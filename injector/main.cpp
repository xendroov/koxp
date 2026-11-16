#include <Windows.h>
#include <TlHelp32.h>
#include <cstdio>
#include "manual_map.h"

static DWORD FindPID(const wchar_t* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{ sizeof(pe) };
    DWORD pid = 0;
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, name) == 0) { pid = pe.th32ProcessID; break; }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

int main(int argc, char* argv[]) {
    printf("=== koxp injector ===\n");

    const char* dllPath = (argc > 1) ? argv[1] : "koxp.dll";

    // KnightOnLine.exe bul
    DWORD pid = FindPID(L"KnightOnLine.exe");
    if (!pid) {
        printf("[-] KnightOnLine.exe bulunamadi. Once oyunu ac.\n");
        return 1;
    }
    printf("[+] KnightOnLine.exe PID: %lu\n", pid);

    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) {
        printf("[-] OpenProcess FAILED: %lu\n", GetLastError());
        return 1;
    }

    uintptr_t base = ManualMap(hProc, dllPath);
    CloseHandle(hProc);

    if (!base) { printf("[-] Inject FAILED\n"); return 1; }
    printf("[+] Basarili. C:\\koxp.log izle.\n");
    return 0;
}
