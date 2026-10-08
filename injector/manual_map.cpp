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

static NtCreateSection_t      pNtCreateSection     = nullptr;
static NtMapViewOfSection_t   pNtMapViewOfSection  = nullptr;
static NtUnmapViewOfSection_t pNtUnmapViewOfSection= nullptr;
static NtCreateThreadEx_t     pNtCreateThreadEx    = nullptr;

static bool LoadNtFns() {
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    pNtCreateSection     = (NtCreateSection_t)    GetProcAddress(ntdll, "NtCreateSection");
    pNtMapViewOfSection  = (NtMapViewOfSection_t) GetProcAddress(ntdll, "NtMapViewOfSection");
    pNtUnmapViewOfSection= (NtUnmapViewOfSection_t)GetProcAddress(ntdll, "NtUnmapViewOfSection");
    pNtCreateThreadEx    = (NtCreateThreadEx_t)   GetProcAddress(ntdll, "NtCreateThreadEx");
    return pNtCreateSection && pNtMapViewOfSection && pNtUnmapViewOfSection && pNtCreateThreadEx;
}

// ─── Shellcode (x86) ─────────────────────────────────────────────────────────
// Called as DWORD WINAPI shell(LPVOID param)  where param = &ShellData in target
// Calls: DllMain(base, DLL_PROCESS_ATTACH, NULL)

#pragma pack(push, 1)
struct ShellData {
    DWORD base;      // mapped image base in target
    DWORD entryRVA;  // DllMain RVA (0 = no entry point)
};
#pragma pack(pop)

static BYTE s_shell[] = {
    // mov eax, [esp+4]     ; eax = ShellData*
    0x8B, 0x44, 0x24, 0x04,
    // mov edx, [eax]       ; edx = base
    0x8B, 0x10,
    // mov ecx, [eax+4]     ; ecx = entryRVA
    0x8B, 0x48, 0x04,
    // mov eax, 1           ; default return TRUE
    0xB8, 0x01, 0x00, 0x00, 0x00,
    // test ecx, ecx
    0x85, 0xC9,
    // jz +9  (skip to ret 4)
    0x74, 0x09,
    // push 0               ; lpvReserved
    0x6A, 0x00,
    // push 1               ; DLL_PROCESS_ATTACH
    0x6A, 0x01,
    // push edx             ; hinstDLL = base
    0x52,
    // add ecx, edx         ; ecx = DllMain absolute address
    0x03, 0xCA,
    // call ecx             ; eax = DllMain result
    0xFF, 0xD1,
    // ret 4
    0xC2, 0x04, 0x00
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
        DWORD  cnt  = (blk->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / 2;
        WORD*  ent  = (WORD*)(blk + 1);
        for (DWORD i = 0; i < cnt; ++i) {
            if ((ent[i] >> 12) == IMAGE_REL_BASED_HIGHLOW) {
                DWORD* patch = (DWORD*)(localBase + blk->VirtualAddress + (ent[i] & 0xFFF));
                *patch += delta;
            }
        }
        blk = (IMAGE_BASE_RELOCATION*)((BYTE*)blk + blk->SizeOfBlock);
    }
}

// Resolves imports in localBase using GetProcAddress from the injector process.
// Safe for system DLLs: ASLR randomises per-boot but keeps them at the SAME VA
// in every 32-bit process on the same boot session.
static bool ResolveImports(BYTE* localBase) {
    auto* nt      = GetNtHdrs(localBase);
    auto& impDir  = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (!impDir.VirtualAddress) return true;

    auto* desc = (IMAGE_IMPORT_DESCRIPTOR*)(localBase + impDir.VirtualAddress);
    for (; desc->Name; ++desc) {
        const char* name = (const char*)(localBase + desc->Name);
        HMODULE hMod = LoadLibraryA(name);
        if (!hMod) {
            std::cout << "[-] Import DLL not found: " << name << "\n";
            return false;
        }

        DWORD origThunkRVA = desc->OriginalFirstThunk ? desc->OriginalFirstThunk : desc->FirstThunk;
        auto* orig = (IMAGE_THUNK_DATA*)(localBase + origThunkRVA);
        auto* iat  = (IMAGE_THUNK_DATA*)(localBase + desc->FirstThunk);

        for (; orig->u1.AddressOfData; ++orig, ++iat) {
            FARPROC fn;
            if (orig->u1.Ordinal & IMAGE_ORDINAL_FLAG32)
                fn = GetProcAddress(hMod, MAKEINTRESOURCEA(orig->u1.Ordinal & 0xFFFF));
            else
                fn = GetProcAddress(hMod, ((IMAGE_IMPORT_BY_NAME*)(localBase + orig->u1.AddressOfData))->Name);

            if (!fn) {
                std::cout << "[-] Import not found in " << name << "\n";
                return false;
            }
            iat->u1.Function = (DWORD)fn;
        }
    }
    return true;
}

// ─── ManualMap ───────────────────────────────────────────────────────────────

bool ManualMap(DWORD pid, const std::string& dllPath) {
    std::cout << "[*] Manual mapper starting (NtCreateSection method)\n";

    if (!LoadNtFns()) {
        std::cout << "[-] Failed to resolve ntdll functions\n";
        return false;
    }

    // Read DLL into buffer
    std::ifstream f(dllPath, std::ios::binary | std::ios::ate);
    if (!f) { std::cout << "[-] Cannot open DLL\n"; return false; }
    std::vector<BYTE> raw((size_t)f.tellg());
    f.seekg(0); f.read((char*)raw.data(), raw.size());
    f.close();

    BYTE* rawBase = raw.data();
    auto* nt = GetNtHdrs(rawBase);
    if (!nt) { std::cout << "[-] Not a valid PE\n"; return false; }

    DWORD imageSize = nt->OptionalHeader.SizeOfImage;
    DWORD prefBase  = nt->OptionalHeader.ImageBase;
    DWORD entryRVA  = nt->OptionalHeader.AddressOfEntryPoint;

    std::cout << "[*] ImageSize=0x" << std::hex << imageSize
              << " PreferredBase=0x" << prefBase
              << " EntryRVA=0x"      << entryRVA << std::dec << "\n";

    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) {
        std::cout << "[-] OpenProcess failed: " << GetLastError() << "\n";
        return false;
    }

    // ── 1. Create anonymous shared section ────────────────────────────────────
    LARGE_INTEGER secSz{}; secSz.QuadPart = imageSize;
    HANDLE hSec = nullptr;
    NTSTATUS st = pNtCreateSection(&hSec, SECTION_ALL_ACCESS, nullptr,
                                   &secSz, PAGE_EXECUTE_READWRITE,
                                   0x08000000 /*SEC_COMMIT*/, nullptr);
    if (st || !hSec) {
        std::cout << "[-] NtCreateSection failed: 0x" << std::hex << st << std::dec << "\n";
        CloseHandle(hProc); return false;
    }
    std::cout << "[+] NtCreateSection OK\n";

    // ── 2. Map in our process (write view) ────────────────────────────────────
    PVOID localView = nullptr; SIZE_T viewSz = 0;
    st = pNtMapViewOfSection(hSec, GetCurrentProcess(), &localView,
                             0, 0, nullptr, &viewSz, 2/*ViewUnmap*/, 0, PAGE_EXECUTE_READWRITE);
    if (st) {
        std::cout << "[-] NtMapViewOfSection (local) failed: 0x" << std::hex << st << std::dec << "\n";
        CloseHandle(hSec); CloseHandle(hProc); return false;
    }
    std::cout << "[+] Local view: 0x" << std::hex << (DWORD)localView << std::dec << "\n";

    // ── 3. Map in target process (execute view, shared physical pages) ─────────
    PVOID remoteView = nullptr; viewSz = 0;
    st = pNtMapViewOfSection(hSec, hProc, &remoteView,
                             0, 0, nullptr, &viewSz, 2/*ViewUnmap*/, 0, PAGE_EXECUTE_READWRITE);
    if (st) {
        std::cout << "[-] NtMapViewOfSection (remote) failed: 0x" << std::hex << st << std::dec << "\n";
        pNtUnmapViewOfSection(GetCurrentProcess(), localView);
        CloseHandle(hSec); CloseHandle(hProc); return false;
    }
    std::cout << "[+] Remote view: 0x" << std::hex << (DWORD)remoteView << std::dec << "\n";

    // ── 4. Copy PE headers + sections into local view ─────────────────────────
    memcpy(localView, rawBase, nt->OptionalHeader.SizeOfHeaders);
    auto* sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i) {
        if (sec[i].SizeOfRawData) {
            memcpy((BYTE*)localView + sec[i].VirtualAddress,
                   rawBase + sec[i].PointerToRawData,
                   sec[i].SizeOfRawData);
        }
    }
    std::cout << "[+] " << nt->FileHeader.NumberOfSections << " sections copied\n";

    // ── 5. Fix relocations (based on actual remote address) ───────────────────
    ApplyRelocations((BYTE*)localView, prefBase, (DWORD)remoteView);
    std::cout << "[+] Relocations applied\n";

    // ── 6. Resolve imports in local view (shared → visible in remote) ─────────
    if (!ResolveImports((BYTE*)localView)) {
        pNtUnmapViewOfSection(GetCurrentProcess(), localView);
        pNtUnmapViewOfSection(hProc, remoteView);
        CloseHandle(hSec); CloseHandle(hProc); return false;
    }
    std::cout << "[+] Imports resolved\n";

    // Local view no longer needed (writes are already visible via shared pages)
    pNtUnmapViewOfSection(GetCurrentProcess(), localView);
    CloseHandle(hSec);

    // ── 7. Write shellcode + ShellData into target ────────────────────────────
    SIZE_T shellAlloc = sizeof(s_shell) + sizeof(ShellData);
    PVOID  shellMem   = VirtualAllocEx(hProc, nullptr, shellAlloc,
                                       MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE);
    if (!shellMem) {
        // Xigncode sometimes blocks shellcode alloc too; log and fail cleanly
        std::cout << "[-] VirtualAllocEx (shellcode) failed: " << GetLastError() << "\n";
        pNtUnmapViewOfSection(hProc, remoteView);
        CloseHandle(hProc); return false;
    }

    ShellData sd{ (DWORD)remoteView, entryRVA };
    PVOID sdAddr = (BYTE*)shellMem + sizeof(s_shell);
    WriteProcessMemory(hProc, shellMem, s_shell,  sizeof(s_shell),   nullptr);
    WriteProcessMemory(hProc, sdAddr,   &sd,       sizeof(sd),        nullptr);
    std::cout << "[+] Shellcode written at 0x" << std::hex << (DWORD)shellMem << std::dec << "\n";

    // ── 8. Create remote thread via NtCreateThreadEx ──────────────────────────
    HANDLE hThread = nullptr;
    st = pNtCreateThreadEx(&hThread, THREAD_ALL_ACCESS, nullptr, hProc,
                           shellMem, sdAddr, 0, 0, 0, 0, nullptr);
    if (st || !hThread) {
        std::cout << "[-] NtCreateThreadEx failed: 0x" << std::hex << st << std::dec << "\n";
        VirtualFreeEx(hProc, shellMem, 0, MEM_RELEASE);
        pNtUnmapViewOfSection(hProc, remoteView);
        CloseHandle(hProc); return false;
    }

    WaitForSingleObject(hThread, 6000);
    DWORD exitCode = 0;
    GetExitCodeThread(hThread, &exitCode);
    CloseHandle(hThread);

    VirtualFreeEx(hProc, shellMem, 0, MEM_RELEASE);
    CloseHandle(hProc);

    std::cout << "[+] Thread exit code: 0x" << std::hex << exitCode << std::dec << "\n";
    bool ok = (exitCode != 0);
    if (!ok) std::cout << "[-] DllMain returned FALSE or crashed\n";
    else     std::cout << "[+] Manual Map BASARILI!\n";
    return ok;
}
