#include "manual_map.h"
#include <cstdio>
#include <vector>

// Remote thread shellcode — DllMain'i çağırır, sonra çıkar
// DWORD WINAPI stub(LPVOID param) { DllMain(base, DLL_PROCESS_ATTACH, 0); return 0; }
struct ShellParams {
    uintptr_t base;
    uintptr_t dllMain;
};

// x86 shellcode:
//   push 0          ; lpvReserved = NULL
//   push 1          ; fdwReason   = DLL_PROCESS_ATTACH
//   push [param+0]  ; hinstDLL    = base
//   call [param+4]  ; DllMain
//   ret
static const uint8_t k_stub[] = {
    0x55,                          // push ebp
    0x89, 0xE5,                    // mov ebp, esp
    0x8B, 0x45, 0x08,              // mov eax, [ebp+8]  ; param*
    0x6A, 0x00,                    // push 0
    0x6A, 0x01,                    // push 1
    0xFF, 0x30,                    // push [eax]        ; base
    0xFF, 0x50, 0x04,              // call [eax+4]      ; dllMain
    0x5D,                          // pop ebp
    0xC2, 0x04, 0x00               // ret 4
};

uintptr_t ManualMap(HANDLE hProc, const char* dllPath) {
    // 1. DLL'i dosyadan oku
    FILE* f = fopen(dllPath, "rb");
    if (!f) { printf("[-] DLL acilamadi: %s\n", dllPath); return 0; }
    fseek(f, 0, SEEK_END);
    size_t fileSize = ftell(f);
    rewind(f);
    std::vector<uint8_t> buf(fileSize);
    fread(buf.data(), 1, fileSize, f);
    fclose(f);

    // 2. PE header dogrula
    auto* dos = reinterpret_cast<IMAGE_DOS_HEADER*>(buf.data());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) { printf("[-] Gecersiz MZ\n"); return 0; }
    auto* nt = reinterpret_cast<IMAGE_NT_HEADERS*>(buf.data() + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) { printf("[-] Gecersiz PE\n"); return 0; }
    if (nt->FileHeader.Machine != IMAGE_FILE_MACHINE_I386) {
        printf("[-] 64-bit DLL, x86 gerekli\n"); return 0;
    }

    DWORD imgSize    = nt->OptionalHeader.SizeOfImage;
    uintptr_t prefBase = nt->OptionalHeader.ImageBase;

    // 3. Remote bellek ayir
    auto* remBase = reinterpret_cast<uint8_t*>(
        VirtualAllocEx(hProc, reinterpret_cast<LPVOID>(prefBase),
                       imgSize, MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!remBase) {
        remBase = reinterpret_cast<uint8_t*>(
            VirtualAllocEx(hProc, nullptr, imgSize,
                           MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    }
    if (!remBase) { printf("[-] VirtualAllocEx FAILED %lu\n", GetLastError()); return 0; }
    printf("[+] Remote bellek: %p\n", remBase);

    // 4. PE header + section'lari kopyala
    WriteProcessMemory(hProc, remBase, buf.data(),
                       nt->OptionalHeader.SizeOfHeaders, nullptr);

    auto* sec = IMAGE_FIRST_SECTION(nt);
    for (WORD i = 0; i < nt->FileHeader.NumberOfSections; ++i, ++sec) {
        if (!sec->SizeOfRawData) continue;
        WriteProcessMemory(hProc,
                           remBase + sec->VirtualAddress,
                           buf.data() + sec->PointerToRawData,
                           sec->SizeOfRawData, nullptr);
    }

    // 5. Relocation (base farki varsa)
    uintptr_t delta = reinterpret_cast<uintptr_t>(remBase) - prefBase;
    if (delta) {
        auto& relDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_BASERELOC];
        auto* reloc  = reinterpret_cast<IMAGE_BASE_RELOCATION*>(buf.data() + relDir.VirtualAddress);
        DWORD done   = 0;
        while (done < relDir.Size && reloc->VirtualAddress) {
            DWORD count = (reloc->SizeOfBlock - sizeof(IMAGE_BASE_RELOCATION)) / 2;
            auto* entry = reinterpret_cast<WORD*>(reloc + 1);
            for (DWORD i = 0; i < count; ++i) {
                if ((entry[i] >> 12) == IMAGE_REL_BASED_HIGHLOW) {
                    uintptr_t target = reinterpret_cast<uintptr_t>(remBase)
                                       + reloc->VirtualAddress + (entry[i] & 0xFFF);
                    uintptr_t val = 0;
                    ReadProcessMemory(hProc, reinterpret_cast<LPCVOID>(target),
                                      &val, 4, nullptr);
                    val += delta;
                    WriteProcessMemory(hProc, reinterpret_cast<LPVOID>(target),
                                       &val, 4, nullptr);
                }
            }
            done  += reloc->SizeOfBlock;
            reloc  = reinterpret_cast<IMAGE_BASE_RELOCATION*>(
                         reinterpret_cast<uint8_t*>(reloc) + reloc->SizeOfBlock);
        }
        printf("[+] Relocation tamam (delta=0x%X)\n", (unsigned)delta);
    }

    // 6. Import tablosunu coz
    auto& impDir = nt->OptionalHeader.DataDirectory[IMAGE_DIRECTORY_ENTRY_IMPORT];
    if (impDir.VirtualAddress) {
        auto* desc = reinterpret_cast<IMAGE_IMPORT_DESCRIPTOR*>(
                         buf.data() + impDir.VirtualAddress);
        for (; desc->Name; ++desc) {
            const char* dllName = reinterpret_cast<const char*>(
                                      buf.data() + desc->Name);
            HMODULE hDep = LoadLibraryA(dllName);
            if (!hDep) { printf("[-] Import DLL yuklenemedi: %s\n", dllName); continue; }

            auto* origThunk = reinterpret_cast<IMAGE_THUNK_DATA*>(
                                  buf.data() + desc->OriginalFirstThunk);
            auto* iatThunk  = reinterpret_cast<IMAGE_THUNK_DATA*>(
                                  buf.data() + desc->FirstThunk);

            for (; origThunk->u1.AddressOfData; ++origThunk, ++iatThunk) {
                uintptr_t fnAddr = 0;
                if (IMAGE_SNAP_BY_ORDINAL(origThunk->u1.Ordinal)) {
                    fnAddr = reinterpret_cast<uintptr_t>(
                                 GetProcAddress(hDep,
                                     MAKEINTRESOURCEA(IMAGE_ORDINAL(origThunk->u1.Ordinal))));
                } else {
                    auto* ibn = reinterpret_cast<IMAGE_IMPORT_BY_NAME*>(
                                    buf.data() + origThunk->u1.AddressOfData);
                    fnAddr = reinterpret_cast<uintptr_t>(GetProcAddress(hDep, ibn->Name));
                }
                uintptr_t remIat = reinterpret_cast<uintptr_t>(remBase)
                                   + desc->FirstThunk
                                   + reinterpret_cast<uintptr_t>(iatThunk)
                                   - reinterpret_cast<uintptr_t>(buf.data() + desc->FirstThunk);
                WriteProcessMemory(hProc, reinterpret_cast<LPVOID>(remIat),
                                   &fnAddr, 4, nullptr);
            }
        }
        printf("[+] Import tablosu cozuldu\n");
    }

    // 7. PE header'i sifirla (bellek tarayici imzasini gizle)
    uint8_t zeros[0x1000] = {};
    WriteProcessMemory(hProc, remBase, zeros, sizeof(zeros), nullptr);

    // 8. DllMain'i calistir
    uintptr_t dllMain = reinterpret_cast<uintptr_t>(remBase)
                        + nt->OptionalHeader.AddressOfEntryPoint;

    ShellParams params{ reinterpret_cast<uintptr_t>(remBase), dllMain };

    // Shellcode + params icin remote bellek
    SIZE_T shellSize = sizeof(k_stub) + sizeof(ShellParams);
    auto* remShell = reinterpret_cast<uint8_t*>(
        VirtualAllocEx(hProc, nullptr, shellSize,
                       MEM_COMMIT | MEM_RESERVE, PAGE_EXECUTE_READWRITE));
    if (!remShell) { printf("[-] Shellcode alloc FAILED\n"); return 0; }

    WriteProcessMemory(hProc, remShell, k_stub, sizeof(k_stub), nullptr);
    WriteProcessMemory(hProc, remShell + sizeof(k_stub),
                       &params, sizeof(params), nullptr);

    HANDLE hThread = CreateRemoteThread(hProc, nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(remShell),
        remShell + sizeof(k_stub), 0, nullptr);
    if (!hThread) { printf("[-] CreateRemoteThread FAILED %lu\n", GetLastError()); return 0; }

    WaitForSingleObject(hThread, 5000);
    CloseHandle(hThread);
    VirtualFreeEx(hProc, remShell, 0, MEM_RELEASE);

    printf("[+] Inject tamam @ %p\n", remBase);
    return reinterpret_cast<uintptr_t>(remBase);
}
