#include "manual_map.h"
#include <Windows.h>
#include <winternl.h>
#include <iostream>
#include <fstream>
#include <vector>
#include <string>

// ─── NT native API typedefs ───────────────────────────────────────────────────

using NtCreateSection_t = NTSTATUS(NTAPI*)(
    PHANDLE, ACCESS_MASK, PVOID, PLARGE_INTEGER, ULONG, ULONG, HANDLE);

using NtMapViewOfSection_t = NTSTATUS(NTAPI*)(
    HANDLE, HANDLE, PVOID*, ULONG_PTR, SIZE_T, PLARGE_INTEGER,
    PSIZE_T, DWORD, ULONG, ULONG);

using NtUnmapViewOfSection_t = NTSTATUS(NTAPI*)(HANDLE, PVOID);

using NtCreateThreadEx_t = NTSTATUS(NTAPI*)(
    PHANDLE, ACCESS_MASK, PVOID, HANDLE, PVOID, PVOID,
    ULONG, SIZE_T, SIZE_T, SIZE_T, PVOID);

using NtDelayExecution_t = NTSTATUS(NTAPI*)(BOOLEAN, PLARGE_INTEGER);

static NtCreateSection_t      pNtCreateSection     = nullptr;
static NtMapViewOfSection_t   pNtMapViewOfSection  = nullptr;
static NtUnmapViewOfSection_t pNtUnmapViewOfSection= nullptr;
static NtCreateThreadEx_t     pNtCreateThreadEx    = nullptr;
static NtDelayExecution_t     pNtDelayExecution    = nullptr;

static bool LoadNtFns() {
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    pNtCreateSection      = (NtCreateSection_t)     GetProcAddress(ntdll, "NtCreateSection");
    pNtMapViewOfSection   = (NtMapViewOfSection_t)  GetProcAddress(ntdll, "NtMapViewOfSection");
    pNtUnmapViewOfSection = (NtUnmapViewOfSection_t)GetProcAddress(ntdll, "NtUnmapViewOfSection");
    pNtCreateThreadEx     = (NtCreateThreadEx_t)    GetProcAddress(ntdll, "NtCreateThreadEx");
    pNtDelayExecution     = (NtDelayExecution_t)    GetProcAddress(ntdll, "NtDelayExecution");
    return pNtCreateSection && pNtMapViewOfSection &&
           pNtUnmapViewOfSection && pNtCreateThreadEx && pNtDelayExecution;
}

// ─── Shellcode A: immediate DllMain call (x86) ───────────────────────────────

#pragma pack(push, 1)
struct ShellData {
    DWORD base;
    DWORD entryRVA;
};
#pragma pack(pop)

static BYTE s_shell[] = {
    0x8B, 0x44, 0x24, 0x04,
    0x8B, 0x10,
    0x8B, 0x48, 0x04,
    0xB8, 0x01, 0x00, 0x00, 0x00,
    0x85, 0xC9,
    0x74, 0x09,
    0x6A, 0x00,
    0x6A, 0x01,
    0x52,
    0x03, 0xCA,
    0xFF, 0xD1,
    0xC2, 0x04, 0x00
};

// ─── Shellcode B: NtDelayExecution then DllMain (x86) ────────────────────────
// NtDelayExecution is in ntdll — always mapped even before kernel32 loads.
//
// ShellDataDelayed layout (packed):
//   +0x00 DWORD base
//   +0x04 DWORD entryRVA
//   +0x08 DWORD pNtDelayExecution
//   +0x0C DWORD delay_lo   } LARGE_INTEGER (negative = relative wait)
//   +0x10 DWORD delay_hi   }

#pragma pack(push, 1)
struct ShellDataDelayed {
    DWORD base;
    DWORD entryRVA;
    DWORD pNtDelayExecution;
    DWORD delay_lo;
    DWORD delay_hi;
};
#pragma pack(pop)

static BYTE s_shellDelayed[] = {
    0x8B, 0x44, 0x24, 0x04,   // mov eax, [esp+4]   ; ShellDataDelayed*
    0x50,                      // push eax            ; save ptr (eax/ecx/edx caller-saved)
    0x8D, 0x48, 0x0C,          // lea ecx, [eax+0xC] ; &delay_lo
    0x51,                      // push ecx            ; DelayInterval
    0x6A, 0x00,                // push 0              ; Alertable=FALSE
    0xFF, 0x50, 0x08,          // call [eax+8]        ; pNtDelayExecution(__stdcall, cleans 2 args)
    0x58,                      // pop eax             ; restore ShellDataDelayed*
    0x8B, 0x10,                // mov edx, [eax]      ; base
    0x8B, 0x48, 0x04,          // mov ecx, [eax+4]   ; entryRVA
    0x85, 0xC9,                // test ecx, ecx
    0x74, 0x09,                // jz +9  (skip DllMain)
    0x6A, 0x00,                // push 0
    0x6A, 0x01,                // push 1  (DLL_PROCESS_ATTACH)
    0x52,                      // push edx
    0x03, 0xCA,                // add ecx, edx
    0xFF, 0xD1,                // call ecx
    0xC2, 0x04, 0x00           // ret 4
};

// ─── PE helpers ──────────────────────────────────────────────────────────────

static IMAGE_NT_HEADERS* GetNtHdrs(BYTE* base) {
    auto* dos = (IMAGE_DOS_HEADER*)base;
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return nullptr;
    auto* nt  = (IMAGE_NT_HEADERS*)(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE)  return nullptr;
    return nt;
}

static void ApplyRelocations(BYTE* localBase, DWORD prefBase, DWORD newBase) {
    DWORD delta = newBase - prefBase;
    if (!delta) return;
    auto* nt     = GetNtHdrs(localBase);
    auto& relDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
    if (!relDir.VirtualAddress) return;
    auto* blk = (IMAGE_BASE_RELOCATION*)(localBase + relDir.VirtualAddress);
    while (blk->VirtualAddress) {
        DWORD  cnt = (blk->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / 2;
        WORD*  ent = (WORD*)(blk + 1);
        for (DWORD i = 0; i < cnt; ++i) {
            if ((ent[i] >> 12) == IMAGE_REL_BASED_HIGHLOW) {
                DWORD* patch = (DWORD*)(localBase + blk->VirtualAddress + (ent[i] & 0xFFF));
                *patch += delta;
            }
        }
        blk = (IMAGE_BASE_RELOCATION*)((BYTE*)blk + blk->SizeOfBlock);
    }
}

static bool ResolveImports(BYTE* localBase) {
    auto* nt     = GetNtHdrs(localBase);
    auto& impDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!impDir.VirtualAddress) return true;
    auto* desc = (IMAGE_IMPORT_DESCRIPTOR*)(localBase + impDir.VirtualAddress);
    for (; desc->Name; ++desc) {
        const char* name = (const char*)(localBase + desc->Name);
        HMODULE hMod = LoadLibraryA(name);
        if (!hMod) {
            std::cout << "[-] Import DLL bulunamadi: " << name << "\n";
            return false;
        }
        DWORD origRVA = desc->OriginalFirstThunk ? desc->OriginalFirstThunk : desc->FirstThunk;
        auto* orig = (IMAGE_THUNK_DATA*)(localBase + origRVA);
        auto* iat  = (IMAGE_THUNK_DATA*)(localBase + desc->FirstThunk);
        for (; orig->u1.AddressOfData; ++orig, ++iat) {
            FARPROC fn;
            if (orig->u1.Ordinal & IMAGE_ORDINAL_FLAG32)
                fn = GetProcAddress(hMod, MAKEINTRESOURCEA(orig->u1.Ordinal & 0xFFFF));
            else
                fn = GetProcAddress(hMod, ((IMAGE_IMPORT_BY_NAME*)(localBase + orig->u1.AddressOfData))->Name);
            if (!fn) {
                std::cout << "[-] Import " << name << " icinde bulunamadi\n";
                return false;
            }
            iat->u1.Function = (DWORD)fn;
        }
    }
    return true;
}

// ─── Shared map logic ─────────────────────────────────────────────────────────

struct MapResult {
    PVOID remoteView;
    DWORD entryRVA;
    bool  ok;
};

static MapResult DoMap(HANDLE hProc, const std::string& dllPath) {
    MapResult r{};
    std::ifstream f(dllPath, std::ios::binary | std::ios::ate);
    if (!f) { std::cout << "[-] DLL acilamadi\n"; return r; }
    std::vector<BYTE> raw((size_t)f.tellg());
    f.seekg(0); f.read((char*)raw.data(), raw.size()); f.close();

    BYTE* rawBase = raw.data();
    auto* nt = GetNtHdrs(rawBase);
    if (!nt) { std::cout << "[-] Gecerli PE degil\n"; return r; }

    DWORD imageSize = nt->OptionalHeader.SizeOfImage;
    DWORD prefBase  = nt->OptionalHeader.ImageBase;
    r.entryRVA      = nt->OptionalHeader.AddressOfEntryPoint;

    std::cout << "[*] ImageSize=0x" << std::hex << imageSize
              << " PrefBase=0x" << prefBase
              << " EntryRVA=0x" << r.entryRVA << std::dec << "\n";

    LARGE_INTEGER secSz{}; secSz.QuadPart = imageSize;
    HANDLE hSec = nullptr;
    NTSTATUS st = pNtCreateSection(&hSec, SECTION_ALL_ACCESS, nullptr,
                                   &secSz, PAGE_EXECUTE_READWRITE, 0x08000000, nullptr);
    if (st || !hSec) {
        std::cout << "[-] NtCreateSection hatasi: 0x" << std::hex << st << std::dec << "\n";
        return r;
    }

    PVOID localView = nullptr; SIZE_T viewSz = 0;
    st = pNtMapViewOfSection(hSec, GetCurrentProcess(), &localView,
                             0, 0, nullptr, &viewSz, 2, 0, PAGE_EXECUTE_READWRITE);
    if (st) {
        std::cout << "[-] NtMapViewOfSection (local) hatasi: 0x" << std::hex << st << std::dec << "\n";
        CloseHandle(hSec); return r;
    }

    st = pNtMapViewOfSection(hSec, hProc, &r.remoteView,
                             0, 0, nullptr, &viewSz, 2, 0, PAGE_EXECUTE_READWRITE);
    if (st) {
        std::cout << "[-] NtMapViewOfSection (remote) hatasi: 0x" << std::hex << st << std::dec << "\n";
        pNtUnmapViewOfSection(GetCurrentProcess(), localView);
        CloseHandle(hSec); return r;
    }
    std::cout << "[+] Remote view: 0x" << std::hex << (DWORD)r.remoteView << std::dec << "\n";

    memcpy(localView, rawBase, nt->OptionalHeader.SizeOfHeaders);
    auto* sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i)
        if (sec[i].SizeOfRawData)
            memcpy((BYTE*)localView + sec[i].VirtualAddress,
                   rawBase + sec[i].PointerToRawData, sec[i].SizeOfRawData);
    std::cout << "[+] " << nt->FileHeader.NumberOfSections << " section kopyalandi\n";

    ApplyRelocations((BYTE*)localView, prefBase, (DWORD)r.remoteView);

    if (!ResolveImports((BYTE*)localView)) {
        pNtUnmapViewOfSection(GetCurrentProcess(), localView);
        pNtUnmapViewOfSection(hProc, r.remoteView);
        CloseHandle(hSec); return r;
    }

    pNtUnmapViewOfSection(GetCurrentProcess(), localView);
    CloseHandle(hSec);
    r.ok = true;
    return r;
}

// ─── ManualMap: immediate DllMain ─────────────────────────────────────────────

bool ManualMap(DWORD pid, const std::string& dllPath) {
    std::cout << "[*] Manual mapper basliyor\n";
    if (!LoadNtFns()) { std::cout << "[-] ntdll fonksiyonlari bulunamadi\n"; return false; }

    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) { std::cout << "[-] OpenProcess hatasi: " << GetLastError() << "\n"; return false; }

    MapResult m = DoMap(hProc, dllPath);
    if (!m.ok) { CloseHandle(hProc); return false; }

    SIZE_T shellAlloc = sizeof(s_shell) + sizeof(ShellData);
    PVOID  shellMem   = VirtualAllocEx(hProc, nullptr, shellAlloc,
                                       MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!shellMem) {
        std::cout << "[-] VirtualAllocEx hatasi: " << GetLastError() << "\n";
        pNtUnmapViewOfSection(hProc, m.remoteView);
        CloseHandle(hProc); return false;
    }

    ShellData sd{ (DWORD)m.remoteView, m.entryRVA };
    PVOID sdAddr = (BYTE*)shellMem + sizeof(s_shell);
    WriteProcessMemory(hProc, shellMem, s_shell,  sizeof(s_shell),  nullptr);
    WriteProcessMemory(hProc, sdAddr,   &sd,       sizeof(sd),       nullptr);

    HANDLE hThread = nullptr;
    NTSTATUS st = pNtCreateThreadEx(&hThread, THREAD_ALL_ACCESS, nullptr, hProc,
                                    shellMem, sdAddr, 0, 0, 0, 0, nullptr);
    if (st || !hThread) {
        std::cout << "[-] NtCreateThreadEx hatasi: 0x" << std::hex << st << std::dec << "\n";
        VirtualFreeEx(hProc, shellMem, 0, MEM_RELEASE);
        pNtUnmapViewOfSection(hProc, m.remoteView);
        CloseHandle(hProc); return false;
    }

    WaitForSingleObject(hThread, 6000);
    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);
    VirtualFreeEx(hProc, shellMem, 0, MEM_RELEASE);
    CloseHandle(hProc);

    bool ok = (exitCode != 0);
    std::cout << (ok ? "[+] Manual Map BASARILI!\n" : "[-] DllMain FALSE dondu.\n");
    return ok;
}

// ─── ManualMapDelayed: for suspended-process injection ────────────────────────

bool ManualMapDelayed(DWORD pid, const std::string& dllPath, DWORD delayMs) {
    std::cout << "[*] ManualMapDelayed basliyor (delayMs=" << delayMs << ")\n";
    if (!LoadNtFns()) { std::cout << "[-] ntdll fonksiyonlari bulunamadi\n"; return false; }

    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) { std::cout << "[-] OpenProcess hatasi: " << GetLastError() << "\n"; return false; }

    MapResult m = DoMap(hProc, dllPath);
    if (!m.ok) { CloseHandle(hProc); return false; }

    // Negative LARGE_INTEGER = relative wait in 100ns units
    LONGLONG units    = -(LONGLONG)delayMs * 10000LL;
    DWORD    delay_lo = (DWORD)((ULONGLONG)units & 0xFFFFFFFF);
    DWORD    delay_hi = (DWORD)(((ULONGLONG)units >> 32) & 0xFFFFFFFF);

    DWORD pNtDelay = (DWORD)(ULONG_PTR)pNtDelayExecution;

    SIZE_T shellAlloc = sizeof(s_shellDelayed) + sizeof(ShellDataDelayed);
    PVOID  shellMem   = VirtualAllocEx(hProc, nullptr, shellAlloc,
                                       MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!shellMem) {
        std::cout << "[-] VirtualAllocEx (shellcode) hatasi: " << GetLastError() << "\n";
        pNtUnmapViewOfSection(hProc, m.remoteView);
        CloseHandle(hProc); return false;
    }

    ShellDataDelayed sd{};
    sd.base              = (DWORD)m.remoteView;
    sd.entryRVA          = m.entryRVA;
    sd.pNtDelayExecution = pNtDelay;
    sd.delay_lo          = delay_lo;
    sd.delay_hi          = delay_hi;

    PVOID sdAddr = (BYTE*)shellMem + sizeof(s_shellDelayed);
    WriteProcessMemory(hProc, shellMem, s_shellDelayed, sizeof(s_shellDelayed), nullptr);
    WriteProcessMemory(hProc, sdAddr,   &sd,            sizeof(sd),            nullptr);

    std::cout << "[*] Shellcode@0x" << std::hex << (DWORD)shellMem
              << "  NtDelayExecution@0x" << pNtDelay << std::dec << "\n";

    HANDLE hThread = nullptr;
    NTSTATUS st = pNtCreateThreadEx(&hThread, THREAD_ALL_ACCESS, nullptr, hProc,
                                    shellMem, sdAddr, 0, 0, 0, 0, nullptr);
    if (st || !hThread) {
        std::cout << "[-] NtCreateThreadEx hatasi: 0x" << std::hex << st << std::dec << "\n";
        VirtualFreeEx(hProc, shellMem, 0, MEM_RELEASE);
        pNtUnmapViewOfSection(hProc, m.remoteView);
        CloseHandle(hProc); return false;
    }

    // Hemen don -- thread delayMs sonra uyaniyor, DllMain'i cagiriyor.
    // shellMem kasitli olarak serbest birakilmiyor (thread hala orada calisacak).
    CloseHandle(hThread);
    CloseHandle(hProc);

    std::cout << "[+] Shellcode thread aktif. ResumeThread sonrasi " << delayMs / 1000
              << "s icinde DllMain cagrilacak.\n";
    return true;
}
