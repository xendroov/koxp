#include <ntddk.h>
#include "kdrv.h"

#pragma warning(disable:4013 4100)

// ---- Kernel API bildirimleri ------------------------------------------------
NTSTATUS NTAPI ZwAllocateVirtualMemory(HANDLE,PVOID*,ULONG_PTR,PSIZE_T,ULONG,ULONG);
NTSTATUS NTAPI ZwWriteVirtualMemory(HANDLE,PVOID,PVOID,SIZE_T,PSIZE_T*);

typedef NTSTATUS(NTAPI* ZwQueueApcThread_t)(HANDLE,PVOID,PVOID,PVOID,PVOID);

// x86 process'te WoW64 APC rutini — 32-bit LoadLibraryA'yi cagirmak icin
// PEB'den wow64.dll!Wow64ApcRoutine bulunur
typedef struct { UCHAR _[64]; } KAPC_ST;
VOID NTAPI KeStackAttachProcess(PKPROCESS, KAPC_ST*);
VOID NTAPI KeUnstackDetachProcess(KAPC_ST*);
PVOID NTAPI PsGetProcessPeb(PEPROCESS);

// x64 PEB → Ldr → InLoadOrderModuleList → wow64.dll → Wow64ApcRoutine
#pragma pack(push,8)
typedef struct { ULONG Len; UCHAR Init; UCHAR pad[3]; PVOID Ssh; LIST_ENTRY InLoad; } MY_LDR;
typedef struct { UCHAR r[0x18]; MY_LDR* Ldr; } MY_PEB;
typedef struct {
    LIST_ENTRY Load, Mem, Init;
    PVOID Base, Entry;
    ULONG Size, Flags;
    UNICODE_STRING Full, Base2;
} MY_LDR_ENTRY;
#pragma pack(pop)

static PVOID FindWow64Apc(PVOID rawPeb) {
    MY_PEB* peb = (MY_PEB*)rawPeb;
    if (!peb || !peb->Ldr) return NULL;
    LIST_ENTRY* head = &peb->Ldr->InLoad;
    LIST_ENTRY* cur  = head->Flink;
    while (cur && cur != head) {
        MY_LDR_ENTRY* e = CONTAINING_RECORD(cur, MY_LDR_ENTRY, Load);
        cur = cur->Flink;
        if (!e->Base || !e->Base2.Buffer) continue;
        if (_wcsicmp(e->Base2.Buffer, L"wow64.dll") != 0) continue;
        // Export tablosundan Wow64ApcRoutine bul
        PIMAGE_DOS_HEADER dos = (PIMAGE_DOS_HEADER)e->Base;
        if (dos->e_magic != IMAGE_DOS_SIGNATURE) return NULL;
        PIMAGE_NT_HEADERS64 nt = (PIMAGE_NT_HEADERS64)((UCHAR*)dos + dos->e_lfanew);
        ULONG expRva = nt->OptionalHeader.DataDirectory[0].VirtualAddress;
        if (!expRva) return NULL;
        PIMAGE_EXPORT_DIRECTORY exp = (PIMAGE_EXPORT_DIRECTORY)((UCHAR*)dos + expRva);
        PULONG names = (PULONG)((UCHAR*)dos + exp->AddressOfNames);
        PUSHORT ords = (PUSHORT)((UCHAR*)dos + exp->AddressOfNameOrdinals);
        PULONG fns   = (PULONG)((UCHAR*)dos + exp->AddressOfFunctions);
        for (ULONG i = 0; i < exp->NumberOfNames; i++) {
            if (strcmp((const char*)((UCHAR*)dos + names[i]), "Wow64ApcRoutine") == 0)
                return (PVOID)((UCHAR*)dos + fns[ords[i]]);
        }
        return NULL;
    }
    return NULL;
}

// ---- Inject -----------------------------------------------------------------
static NTSTATUS DoInject(const INJECT_REQ* req) {
    if (!req->pid || !req->loadLibraryA || !req->dllPath[0])
        return STATUS_INVALID_PARAMETER;

    DbgPrint("[kdrv] inject pid=%lu lla=0x%X dll=%s\n",
             req->pid, req->loadLibraryA, req->dllPath);

    PEPROCESS ep = NULL;
    NTSTATUS st = PsLookupProcessByProcessId((HANDLE)(ULONG_PTR)req->pid, &ep);
    if (!NT_SUCCESS(st)) { DbgPrint("[kdrv] lookup=%X\n", st); return st; }

    // Kernel handle — handle strip'ten etkilenmez
    HANDLE hProc = NULL;
    st = ObOpenObjectByPointer(ep, OBJ_KERNEL_HANDLE, NULL,
                               PROCESS_ALL_ACCESS, *PsProcessType,
                               KernelMode, &hProc);
    if (!NT_SUCCESS(st)) {
        ObDereferenceObject(ep);
        DbgPrint("[kdrv] ObOpen=%X\n", st);
        return st;
    }

    // Hedef process'e path buffer ayir
    SIZE_T pathLen = strlen(req->dllPath) + 1;
    SIZE_T allocSz = pathLen;
    PVOID pathBuf = NULL;
    st = ZwAllocateVirtualMemory(hProc, &pathBuf, 0, &allocSz,
                                 MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!NT_SUCCESS(st)) {
        DbgPrint("[kdrv] alloc=%X\n", st);
        ZwClose(hProc); ObDereferenceObject(ep); return st;
    }

    // Path yaz + Wow64ApcRoutine bul
    PVOID wow64apc = NULL;
    PVOID rawPeb   = PsGetProcessPeb(ep);
    KAPC_ST apc; RtlZeroMemory(&apc, sizeof(apc));
    KeStackAttachProcess((PKPROCESS)ep, &apc);
    __try {
        RtlCopyMemory(pathBuf, req->dllPath, pathLen);
        wow64apc = FindWow64Apc(rawPeb);
    } __except(EXCEPTION_EXECUTE_HANDLER) { st = GetExceptionCode(); }
    KeUnstackDetachProcess(&apc);

    DbgPrint("[kdrv] pathBuf=%p wow64apc=%p\n", pathBuf, wow64apc);

    if (!NT_SUCCESS(st) || !wow64apc) {
        SIZE_T z = 0; ZwFreeVirtualMemory(hProc, &pathBuf, &z, MEM_RELEASE);
        ZwClose(hProc); ObDereferenceObject(ep);
        return !NT_SUCCESS(st) ? st : STATUS_NOT_FOUND;
    }

    // ZwQueueApcThread ile LoadLibraryA'yi cagir
    UNICODE_STRING fnQ; RtlInitUnicodeString(&fnQ, L"ZwQueueApcThread");
    ZwQueueApcThread_t pQueue =
        (ZwQueueApcThread_t)MmGetSystemRoutineAddress(&fnQ);
    if (!pQueue) {
        DbgPrint("[kdrv] ZwQueueApcThread yok\n");
        ZwClose(hProc); ObDereferenceObject(ep);
        return STATUS_NOT_FOUND;
    }

    // Thread'leri iterate et, alertable thread bul
    NTSTATUS injectSt = STATUS_NOT_FOUND;
    PETHREAD pt = NULL;
    int tried = 0;
    while ((pt = PsGetNextProcessThread(ep, pt)) != NULL) {
        tried++;
        HANDLE hThr = NULL;
        if (!NT_SUCCESS(ObOpenObjectByPointer(pt, OBJ_KERNEL_HANDLE, NULL,
                THREAD_ALL_ACCESS, *PsThreadType, KernelMode, &hThr))) continue;

        NTSTATUS ts = pQueue(hThr, wow64apc,
                             (PVOID)(ULONG_PTR)req->loadLibraryA,
                             pathBuf, NULL);
        DbgPrint("[kdrv] QueueApc thr#%d=%X\n", tried, ts);
        ZwClose(hThr);

        if (NT_SUCCESS(ts)) {
            injectSt = STATUS_SUCCESS;
            ObDereferenceObject(pt);
            break;
        }
    }

    ZwClose(hProc);
    ObDereferenceObject(ep);
    DbgPrint("[kdrv] done=%X tried=%d\n", injectSt, tried);
    return injectSt;
}

PETHREAD NTAPI PsGetNextProcessThread(PEPROCESS, PETHREAD);

// ---- IRP dispatch -----------------------------------------------------------
static NTSTATUS OnCreateClose(PDEVICE_OBJECT d, PIRP irp) {
    UNREFERENCED_PARAMETER(d);
    irp->IoStatus.Status = STATUS_SUCCESS; irp->IoStatus.Information = 0;
    IoCompleteRequest(irp, IO_NO_INCREMENT); return STATUS_SUCCESS;
}
static NTSTATUS OnIoctl(PDEVICE_OBJECT d, PIRP irp) {
    UNREFERENCED_PARAMETER(d);
    PIO_STACK_LOCATION stk = IoGetCurrentIrpStackLocation(irp);
    NTSTATUS st = STATUS_INVALID_DEVICE_REQUEST;
    if (stk->Parameters.DeviceIoControl.IoControlCode == IOCTL_INJECT) {
        ULONG len = stk->Parameters.DeviceIoControl.InputBufferLength;
        st = (len >= sizeof(INJECT_REQ))
             ? DoInject((const INJECT_REQ*)irp->AssociatedIrp.SystemBuffer)
             : STATUS_BUFFER_TOO_SMALL;
    }
    irp->IoStatus.Status = st; irp->IoStatus.Information = 0;
    IoCompleteRequest(irp, IO_NO_INCREMENT); return st;
}

// ---- Driver entry/unload ----------------------------------------------------
#define DEV_NAME L"\\Device\\kdrv"
#define SYM_NAME L"\\DosDevices\\kdrv"
static PDEVICE_OBJECT g_dev = NULL;

static VOID Unload(PDRIVER_OBJECT drv) {
    UNREFERENCED_PARAMETER(drv);
    UNICODE_STRING sym; RtlInitUnicodeString(&sym, SYM_NAME);
    IoDeleteSymbolicLink(&sym);
    if (g_dev) IoDeleteDevice(g_dev);
    DbgPrint("[kdrv] unloaded\n");
}

NTSTATUS DriverEntry(PDRIVER_OBJECT drv, PUNICODE_STRING reg) {
    UNREFERENCED_PARAMETER(reg);
    DbgPrint("[kdrv] DriverEntry\n");
    UNICODE_STRING devName; RtlInitUnicodeString(&devName, DEV_NAME);
    NTSTATUS st = IoCreateDevice(drv, 0, &devName, FILE_DEVICE_UNKNOWN,
                                 FILE_DEVICE_SECURE_OPEN, FALSE, &g_dev);
    if (!NT_SUCCESS(st)) return st;
    UNICODE_STRING sym; RtlInitUnicodeString(&sym, SYM_NAME);
    st = IoCreateSymbolicLink(&sym, &devName);
    if (!NT_SUCCESS(st)) { IoDeleteDevice(g_dev); return st; }
    drv->DriverUnload                         = Unload;
    drv->MajorFunction[IRP_MJ_CREATE]         = OnCreateClose;
    drv->MajorFunction[IRP_MJ_CLOSE]          = OnCreateClose;
    drv->MajorFunction[IRP_MJ_DEVICE_CONTROL] = OnIoctl;
    g_dev->Flags &= ~DO_DEVICE_INITIALIZING;
    DbgPrint("[kdrv] ready\n");
    return STATUS_SUCCESS;
}
