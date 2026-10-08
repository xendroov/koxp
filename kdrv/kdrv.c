#include <ntifs.h>
#include <wdm.h>
#include "kdrv.h"

#define DEVICE_NAME  L"\\Device\\kdrv"
#define SYMLINK_NAME L"\\DosDevices\\kdrv"

// ZwCreateThreadEx is exported from ntoskrnl but not in public WDK headers
typedef NTSTATUS(NTAPI* ZwCreateThreadEx_t)(
    PHANDLE            ThreadHandle,
    ACCESS_MASK        DesiredAccess,
    POBJECT_ATTRIBUTES ObjectAttributes,
    HANDLE             ProcessHandle,
    PVOID              StartRoutine,
    PVOID              Argument,
    ULONG              CreateFlags,
    ULONG_PTR          ZeroBits,
    SIZE_T             StackSize,
    SIZE_T             MaximumStackSize,
    PVOID              AttributeList
);

static PDEVICE_OBJECT g_Device = NULL;

// ─── Injection logic ──────────────────────────────────────────────────────────────────────────────────

static NTSTATUS InjectDll(const INJECT_REQUEST* req) {
    if (!req->pid || !req->loadLibraryA || !req->dllPath[0])
        return STATUS_INVALID_PARAMETER;

    DbgPrint("[kdrv] InjectDll: pid=%u lla=0x%X dll=%s\n",
             req->pid, req->loadLibraryA, req->dllPath);

    // 1. Find EPROCESS for target PID
    PEPROCESS eproc = NULL;
    NTSTATUS st = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)req->pid, &eproc);
    if (!NT_SUCCESS(st)) {
        DbgPrint("[kdrv] PsLookupProcessByProcessId = 0x%X\n", st);
        return st;
    }

    // 2. Open kernel handle to target process (bypasses ObRegisterCallbacks)
    HANDLE hProc = NULL;
    st = ObOpenObjectByPointer(eproc, OBJ_KERNEL_HANDLE, NULL,
                               PROCESS_ALL_ACCESS, *PsProcessType,
                               KernelMode, &hProc);
    if (!NT_SUCCESS(st)) {
        DbgPrint("[kdrv] ObOpenObjectByPointer = 0x%X\n", st);
        ObDereferenceObject(eproc);
        return st;
    }

    // 3. Allocate memory in target process for the DLL path string
    SIZE_T pathLen   = strlen(req->dllPath) + 1;
    SIZE_T allocSize = pathLen;
    PVOID  addr      = NULL;
    st = ZwAllocateVirtualMemory(hProc, &addr, 0, &allocSize,
                                 MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!NT_SUCCESS(st)) {
        DbgPrint("[kdrv] ZwAllocateVirtualMemory = 0x%X\n", st);
        ObDereferenceObject(eproc);
        ZwClose(hProc);
        return st;
    }
    DbgPrint("[kdrv] Path buffer allocated at %p\n", addr);

    // 4. Write DLL path into target by temporarily attaching to its address space
    KAPC_STATE apcState;
    KeStackAttachProcess((PKPROCESS)eproc, &apcState);
    __try {
        RtlCopyMemory(addr, req->dllPath, pathLen);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        st = GetExceptionCode();
        DbgPrint("[kdrv] Write exception: 0x%X\n", st);
    }
    KeUnstackDetachProcess(&apcState);
    ObDereferenceObject(eproc);

    if (!NT_SUCCESS(st)) {
        SIZE_T zero = 0;
        ZwFreeVirtualMemory(hProc, &addr, &zero, MEM_RELEASE);
        ZwClose(hProc);
        return st;
    }
    DbgPrint("[kdrv] DLL path written OK\n");

    // 5. Resolve ZwCreateThreadEx from ntoskrnl at runtime
    UNICODE_STRING fnName;
    RtlInitUnicodeString(&fnName, L"ZwCreateThreadEx");
    ZwCreateThreadEx_t pZwCreateThreadEx =
        (ZwCreateThreadEx_t)MmGetSystemRoutineAddress(&fnName);

    if (!pZwCreateThreadEx) {
        DbgPrint("[kdrv] ZwCreateThreadEx not found in ntoskrnl\n");
        ZwClose(hProc);
        return STATUS_NOT_FOUND;
    }

    // 6. Create remote thread: entry = LoadLibraryA, param = dllPath buffer
    //    The target is a 32-bit (WOW64) process; loadLibraryA is a 32-bit address.
    HANDLE hThread = NULL;
    st = pZwCreateThreadEx(&hThread, THREAD_ALL_ACCESS, NULL, hProc,
                           (PVOID)(ULONG_PTR)req->loadLibraryA, addr,
                           0, 0, 0, 0, NULL);
    DbgPrint("[kdrv] ZwCreateThreadEx = 0x%X  hThread=%p\n", st, hThread);

    if (NT_SUCCESS(st) && hThread) ZwClose(hThread);
    ZwClose(hProc);
    return st;
}

// ─── IRP dispatch routines ──────────────────────────────────────────────────────────────────────

static NTSTATUS DispatchCreateClose(PDEVICE_OBJECT DevObj, PIRP Irp) {
    UNREFERENCED_PARAMETER(DevObj);
    Irp->IoStatus.Status      = STATUS_SUCCESS;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return STATUS_SUCCESS;
}

static NTSTATUS DispatchDeviceControl(PDEVICE_OBJECT DevObj, PIRP Irp) {
    UNREFERENCED_PARAMETER(DevObj);
    PIO_STACK_LOCATION stack = IoGetCurrentIrpStackLocation(Irp);
    NTSTATUS           st    = STATUS_INVALID_DEVICE_REQUEST;

    if (stack->Parameters.DeviceIoControl.IoControlCode == IOCTL_KDRV_INJECT) {
        ULONG inLen = stack->Parameters.DeviceIoControl.InputBufferLength;
        if (inLen >= sizeof(INJECT_REQUEST))
            st = InjectDll((const INJECT_REQUEST*)Irp->AssociatedIrp.SystemBuffer);
        else
            st = STATUS_BUFFER_TOO_SMALL;
    }

    Irp->IoStatus.Status      = st;
    Irp->IoStatus.Information = 0;
    IoCompleteRequest(Irp, IO_NO_INCREMENT);
    return st;
}

// ─── Driver entry / unload ───────────────────────────────────────────────────────────────────

static VOID DriverUnload(PDRIVER_OBJECT DriverObj) {
    UNREFERENCED_PARAMETER(DriverObj);
    UNICODE_STRING sym;
    RtlInitUnicodeString(&sym, SYMLINK_NAME);
    IoDeleteSymbolicLink(&sym);
    if (g_Device) IoDeleteDevice(g_Device);
    DbgPrint("[kdrv] Unloaded\n");
}

NTSTATUS DriverEntry(PDRIVER_OBJECT DriverObj, PUNICODE_STRING RegPath) {
    UNREFERENCED_PARAMETER(RegPath);
    DbgPrint("[kdrv] DriverEntry\n");

    UNICODE_STRING devName;
    RtlInitUnicodeString(&devName, DEVICE_NAME);

    NTSTATUS st = IoCreateDevice(DriverObj, 0, &devName,
                                 FILE_DEVICE_UNKNOWN,
                                 FILE_DEVICE_SECURE_OPEN,
                                 FALSE, &g_Device);
    if (!NT_SUCCESS(st)) { DbgPrint("[kdrv] IoCreateDevice = 0x%X\n", st); return st; }

    UNICODE_STRING sym;
    RtlInitUnicodeString(&sym, SYMLINK_NAME);
    st = IoCreateSymbolicLink(&sym, &devName);
    if (!NT_SUCCESS(st)) {
        DbgPrint("[kdrv] IoCreateSymbolicLink = 0x%X\n", st);
        IoDeleteDevice(g_Device);
        return st;
    }

    DriverObj->DriverUnload                         = DriverUnload;
    DriverObj->MajorFunction[IRP_MJ_CREATE]         = DispatchCreateClose;
    DriverObj->MajorFunction[IRP_MJ_CLOSE]          = DispatchCreateClose;
    DriverObj->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchDeviceControl;
    g_Device->Flags &= ~DO_DEVICE_INITIALIZING;

    DbgPrint("[kdrv] Ready — \\Device\\kdrv / \\\\.\\kdrv\n");
    return STATUS_SUCCESS;
}
