#include <ntddk.h>
#include <ntimage.h>
#include "kdrv.h"

// ---- manual declarations (not reliably in WDK public headers) ---------------
PVOID   NTAPI PsGetProcessPeb(PEPROCESS Process);
PETHREAD NTAPI PsGetNextProcessThread(PEPROCESS Process, PETHREAD Thread);

// ---- x64 PEB / LDR structures (hand-rolled to avoid ntifs.h version issues) -
//
// PEB_LDR_DATA x64 layout:
//   +0x000 Length                        ULONG
//   +0x004 Initialized                   UCHAR  (+3 pad)
//   +0x008 SsHandle                      PVOID
//   +0x010 InLoadOrderModuleList         LIST_ENTRY
typedef struct _KDRV_PEB_LDR {
    ULONG      Length;
    BYTE       Initialized;
    BYTE       _pad[3];
    PVOID      SsHandle;
    LIST_ENTRY InLoadOrderModuleList;
} KDRV_PEB_LDR;

// PEB x64 layout (only the fields we touch):
//   +0x000..+0x017  reserved
//   +0x018          Ldr   KDRV_PEB_LDR*
typedef struct _KDRV_PEB {
    BYTE          _reserved[0x18];
    KDRV_PEB_LDR *Ldr;
} KDRV_PEB;

// LDR_DATA_TABLE_ENTRY x64 layout:
//   +0x000 InLoadOrderLinks              LIST_ENTRY  (16)
//   +0x010 InMemoryOrderLinks            LIST_ENTRY  (16)
//   +0x020 InInitializationOrderLinks    LIST_ENTRY  (16)
//   +0x030 DllBase                       PVOID
//   +0x038 EntryPoint                    PVOID
//   +0x040 SizeOfImage                   ULONG
//   +0x044 Flags                         ULONG
//   +0x048 FullDllName                   UNICODE_STRING
//   +0x058 BaseDllName                   UNICODE_STRING
typedef struct _KDRV_LDR_ENTRY {
    LIST_ENTRY     InLoadOrderLinks;
    LIST_ENTRY     InMemoryOrderLinks;
    LIST_ENTRY     InInitializationOrderLinks;
    PVOID          DllBase;
    PVOID          EntryPoint;
    ULONG          SizeOfImage;
    ULONG          Flags;
    UNICODE_STRING FullDllName;
    UNICODE_STRING BaseDllName;
} KDRV_LDR_ENTRY;

// -----------------------------------------------------------------------------

#define DEVICE_NAME  L"\\Device\\kdrv"
#define SYMLINK_NAME L"\\DosDevices\\kdrv"

static PDEVICE_OBJECT g_Device = NULL;

typedef NTSTATUS(NTAPI* ZwQueueApcThread_t)(
    HANDLE ThreadHandle, PVOID ApcRoutine,
    PVOID ApcArgument1, PVOID ApcArgument2, PVOID ApcArgument3
);

// Walk 64-bit InLoadOrderModuleList of the attached process.
// Must be called inside KeStackAttachProcess / KeUnstackDetachProcess.
static PVOID FindExportAttached(PVOID rawPeb, const WCHAR* dll, const char* fn) {
    KDRV_PEB* peb = (KDRV_PEB*)rawPeb;
    if (!peb) return NULL;
    __try {
        if (!peb->Ldr) return NULL;
        LIST_ENTRY* head  = &peb->Ldr->InLoadOrderModuleList;
        LIST_ENTRY* entry = head->Flink;
        while (entry && entry != head) {
            KDRV_LDR_ENTRY* ldr =
                CONTAINING_RECORD(entry, KDRV_LDR_ENTRY, InLoadOrderLinks);
            entry = entry->Flink;
            if (!ldr->DllBase || !ldr->BaseDllName.Buffer) continue;
            if (_wcsicmp(ldr->BaseDllName.Buffer, dll) != 0) continue;

            PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)ldr->DllBase;
            if (dos->e_magic != IMAGE_DOS_SIGNATURE) return NULL;
            PIMAGE_NT_HEADERS64 nt =
                (PIMAGE_NT_HEADERS64)((UCHAR*)dos + dos->e_lfanew);
            if (nt->Signature != IMAGE_NT_SIGNATURE) return NULL;

            ULONG expRva = nt->OptionalHeader
                .DataDirectory[IMAGE_DIRECTORY_ENTRY_EXPORT].VirtualAddress;
            if (!expRva) return NULL;

            PIMAGE_EXPORT_DIRECTORY exp =
                (PIMAGE_EXPORT_DIRECTORY)((UCHAR*)dos + expRva);
            PULONG  names = (PULONG )((UCHAR*)dos + exp->AddressOfNames);
            PUSHORT ords  = (PUSHORT)((UCHAR*)dos + exp->AddressOfNameOrdinals);
            PULONG  funcs = (PULONG )((UCHAR*)dos + exp->AddressOfFunctions);

            for (ULONG i = 0; i < exp->NumberOfNames; i++) {
                if (strcmp((const char*)((UCHAR*)dos + names[i]), fn) == 0)
                    return (PVOID)((UCHAR*)dos + funcs[ords[i]]);
            }
            return NULL;
        }
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        DbgPrint("[kdrv] FindExportAttached exception: 0x%X\n", GetExceptionCode());
    }
    return NULL;
}

// ---- Injection logic --------------------------------------------------------

static NTSTATUS InjectDll(const INJECT_REQUEST* req) {
    if (!req->pid || !req->loadLibraryA || !req->dllPath[0])
        return STATUS_INVALID_PARAMETER;

    DbgPrint("[kdrv] InjectDll: pid=%u lla=0x%X dll=%s\n",
             req->pid, req->loadLibraryA, req->dllPath);

    // 1. EPROCESS (ref-counted; keep alive through thread enumeration)
    PEPROCESS eproc = NULL;
    NTSTATUS st = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)req->pid, &eproc);
    if (!NT_SUCCESS(st)) { DbgPrint("[kdrv] Lookup=0x%X\n", st); return st; }

    // 2. Kernel-mode handle -- bypasses ObRegisterCallbacks
    HANDLE hProc = NULL;
    st = ObOpenObjectByPointer(eproc, OBJ_KERNEL_HANDLE, NULL,
                               PROCESS_ALL_ACCESS, *PsProcessType,
                               KernelMode, &hProc);
    if (!NT_SUCCESS(st)) {
        DbgPrint("[kdrv] ObOpen(proc)=0x%X\n", st);
        ObDereferenceObject(eproc); return st;
    }

    // 3. Allocate RW memory in target for the DLL path string
    SIZE_T pathLen   = strlen(req->dllPath) + 1;
    SIZE_T allocSize = pathLen;
    PVOID  addr      = NULL;
    st = ZwAllocateVirtualMemory(hProc, &addr, 0, &allocSize,
                                 MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!NT_SUCCESS(st)) {
        DbgPrint("[kdrv] ZwAllocVirt=0x%X\n", st);
        ZwClose(hProc); ObDereferenceObject(eproc); return st;
    }
    DbgPrint("[kdrv] path buf @ %p\n", addr);

    // 4. Attach: write DLL path + find wow64!Wow64ApcRoutine
    //
    //    Target is a 32-bit WoW64 process.  A kernel APC runs in 64-bit mode;
    //    we cannot call a 32-bit LoadLibraryA directly.  Instead we queue:
    //      ZwQueueApcThread(thread, Wow64ApcRoutine, LoadLibraryA32, pathAddr, NULL)
    //    Wow64ApcRoutine is exported by wow64.dll in every WoW64 process and
    //    handles the 64->32 mode switch, then calls LoadLibraryA32(pathAddr).
    PVOID      wow64Apc = NULL;
    PVOID      rawPeb   = PsGetProcessPeb(eproc);
    KAPC_STATE apcState;
    KeStackAttachProcess((PKPROCESS)eproc, &apcState);
    __try {
        RtlCopyMemory(addr, req->dllPath, pathLen);
        wow64Apc = FindExportAttached(rawPeb, L"wow64.dll", "Wow64ApcRoutine");
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        st = GetExceptionCode();
        DbgPrint("[kdrv] attach exception: 0x%X\n", st);
    }
    KeUnstackDetachProcess(&apcState);

    DbgPrint("[kdrv] path written, Wow64ApcRoutine=%p\n", wow64Apc);

    if (!NT_SUCCESS(st) || !wow64Apc) {
        SIZE_T z = 0; ZwFreeVirtualMemory(hProc, &addr, &z, MEM_RELEASE);
        ZwClose(hProc); ObDereferenceObject(eproc);
        return !NT_SUCCESS(st) ? st : STATUS_NOT_FOUND;
    }

    // 5. Resolve ZwQueueApcThread at runtime
    UNICODE_STRING fnQ;
    RtlInitUnicodeString(&fnQ, L"ZwQueueApcThread");
    ZwQueueApcThread_t pQueue =
        (ZwQueueApcThread_t)MmGetSystemRoutineAddress(&fnQ);
    if (!pQueue) {
        DbgPrint("[kdrv] ZwQueueApcThread not exported\n");
        SIZE_T z = 0; ZwFreeVirtualMemory(hProc, &addr, &z, MEM_RELEASE);
        ZwClose(hProc); ObDereferenceObject(eproc);
        return STATUS_NOT_FOUND;
    }

    // 6. Enumerate threads; queue WoW64-safe APC to first that accepts
    NTSTATUS injectSt = STATUS_NOT_FOUND;
    PETHREAD pThread  = NULL;
    int      tried    = 0;

    while ((pThread = PsGetNextProcessThread(eproc, pThread)) != NULL) {
        tried++;
        HANDLE hThr = NULL;
        NTSTATUS ts = ObOpenObjectByPointer(pThread, OBJ_KERNEL_HANDLE, NULL,
                                             THREAD_ALL_ACCESS, *PsThreadType,
                                             KernelMode, &hThr);
        if (!NT_SUCCESS(ts)) {
            DbgPrint("[kdrv] ObOpen(thr#%d)=0x%X\n", tried, ts); continue;
        }

        ts = pQueue(hThr,
                    wow64Apc,
                    (PVOID)(ULONG_PTR)req->loadLibraryA,  // fn32 for Wow64ApcRoutine
                    addr,                                  // arg  for LoadLibraryA
                    NULL);
        DbgPrint("[kdrv] ZwQueueApcThread(#%d)=0x%X\n", tried, ts);
        ZwClose(hThr);

        if (NT_SUCCESS(ts)) {
            injectSt = STATUS_SUCCESS;
            ObDereferenceObject(pThread);  // stop early -- manual deref required
            break;
        }
    }

    ZwClose(hProc);
    ObDereferenceObject(eproc);
    DbgPrint("[kdrv] inject=>0x%X tried=%d threads\n", injectSt, tried);
    return injectSt;
}

// ---- IRP dispatch ------------------------------------------------------------

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

// ---- Driver entry / unload --------------------------------------------------

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
    if (!NT_SUCCESS(st)) { DbgPrint("[kdrv] IoCreateDevice=0x%X\n", st); return st; }

    UNICODE_STRING sym;
    RtlInitUnicodeString(&sym, SYMLINK_NAME);
    st = IoCreateSymbolicLink(&sym, &devName);
    if (!NT_SUCCESS(st)) {
        DbgPrint("[kdrv] IoCreateSymbolicLink=0x%X\n", st);
        IoDeleteDevice(g_Device); return st;
    }

    DriverObj->DriverUnload                         = DriverUnload;
    DriverObj->MajorFunction[IRP_MJ_CREATE]         = DispatchCreateClose;
    DriverObj->MajorFunction[IRP_MJ_CLOSE]          = DispatchCreateClose;
    DriverObj->MajorFunction[IRP_MJ_DEVICE_CONTROL] = DispatchDeviceControl;
    g_Device->Flags &= ~DO_DEVICE_INITIALIZING;

    DbgPrint("[kdrv] Ready\n");
    return STATUS_SUCCESS;
}
