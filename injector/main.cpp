#include <Windows.h>
#include <TlHelp32.h>
#include <cstdio>
#include "../kdrv/kdrv.h"
#include "manual_map.h"

static DWORD FindPID(const wchar_t* name) {
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{ sizeof(pe) };
    DWORD pid = 0;
    if (Process32FirstW(snap, &pe))
        do { if (!_wcsicmp(pe.szExeFile, name)) { pid = pe.th32ProcessID; break; } }
        while (Process32NextW(snap, &pe));
    CloseHandle(snap);
    return pid;
}

// kdrv.sys uzerinden inject
static bool KdrvInject(DWORD pid, const char* dllPath) {
    HANDLE hDev = CreateFileA(KDRV_DEVICE_NAME,
                              GENERIC_READ | GENERIC_WRITE,
                              0, nullptr, OPEN_EXISTING, 0, nullptr);
    if (hDev == INVALID_HANDLE_VALUE) {
        printf("[-] kdrv acilamadi (%lu) — Test Mode acik mi? kdrv.sys yuklendi mi?\n",
               GetLastError());
        return false;
    }

    INJECT_REQ req{};
    req.pid = pid;
    req.loadLibraryA = (unsigned long)(uintptr_t)
        GetProcAddress(GetModuleHandleA("kernel32.dll"), "LoadLibraryA");
    strncpy_s(req.dllPath, dllPath, 259);

    printf("[+] LoadLibraryA @ 0x%08X\n", req.loadLibraryA);

    DWORD ret = 0;
    BOOL ok = DeviceIoControl(hDev, IOCTL_INJECT,
                              &req, sizeof(req),
                              nullptr, 0, &ret, nullptr);
    CloseHandle(hDev);

    if (!ok) { printf("[-] IOCTL FAILED %lu\n", GetLastError()); return false; }
    printf("[+] kdrv inject gonderildi\n");
    return true;
}

int main(int argc, char* argv[]) {
    printf("=== koxp injector ===\n");

    const char* dllPath = (argc > 1) ? argv[1] : "koxp.dll";

    DWORD pid = FindPID(L"KnightOnLine.exe");
    if (!pid) {
        printf("[-] KnightOnLine.exe bulunamadi.\n");
        system("pause"); return 1;
    }
    printf("[+] KnightOnLine.exe PID: %lu\n", pid);

    // Once kdrv ile dene (handle strip'i bypass eder)
    if (KdrvInject(pid, dllPath)) {
        printf("[+] kdrv inject OK — C:\\koxp.log izle\n");
        Sleep(2000);
        system("pause"); return 0;
    }

    // kdrv yoksa manuel map ile dene
    printf("[*] kdrv yok, manuel map deneniyor...\n");
    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) {
        printf("[-] OpenProcess FAILED: %lu\n", GetLastError());
        system("pause"); return 1;
    }
    uintptr_t base = ManualMap(hProc, dllPath);
    CloseHandle(hProc);

    if (!base) { printf("[-] Inject FAILED\n"); system("pause"); return 1; }
    printf("[+] Manuel map OK @ 0x%llX\n", (unsigned long long)base);
    printf("[+] C:\\koxp.log izle\n");
    system("pause");
    return 0;
}
