#include <Windows.h>
#include <TlHelp32.h>
#include <shellapi.h>
#include <iostream>
#include <string>
#include "manual_map.h"
#include "../kdrv/kdrv.h"

static bool IsAdmin() {
    BOOL elevated = FALSE;
    HANDLE token = nullptr;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &token)) {
        TOKEN_ELEVATION te{};
        DWORD sz = sizeof(te);
        if (GetTokenInformation(token, TokenElevation, &te, sizeof(te), &sz))
            elevated = te.TokenIsElevated;
        CloseHandle(token);
    }
    return elevated != FALSE;
}

static void RelaunchAsAdmin() {
    char path[MAX_PATH]{};
    GetModuleFileNameA(nullptr, path, MAX_PATH);

    std::string args;
    int argc = 0;
    LPWSTR* argv = CommandLineToArgvW(GetCommandLineW(), &argc);
    for (int i = 1; i < argc; ++i) {
        int len = WideCharToMultiByte(CP_ACP, 0, argv[i], -1, nullptr, 0, nullptr, nullptr);
        std::string s(len, '\0');
        WideCharToMultiByte(CP_ACP, 0, argv[i], -1, s.data(), len, nullptr, nullptr);
        if (i > 1) args += " ";
        args += "\"" + s + "\"";
    }
    LocalFree(argv);

    std::cout << "[*] Yonetici izni isteniyor (UAC)...\n";

    SHELLEXECUTEINFOA sei{};
    sei.cbSize       = sizeof(sei);
    sei.lpVerb       = "runas";
    sei.lpFile       = path;
    sei.lpParameters = args.c_str();
    sei.nShow        = SW_SHOWNORMAL;

    if (!ShellExecuteExA(&sei)) {
        DWORD err = GetLastError();
        if (err == ERROR_CANCELLED)
            std::cout << "[-] UAC iptal edildi.\n";
        else
            std::cout << "[-] ShellExecuteEx hatasi: " << err << "\n";
    }
}

static DWORD FindProcessID(const wchar_t* name) {
    DWORD pid = 0;
    HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snap == INVALID_HANDLE_VALUE) return 0;
    PROCESSENTRY32W pe{ sizeof(pe) };
    if (Process32FirstW(snap, &pe)) {
        do {
            if (_wcsicmp(pe.szExeFile, name) == 0) { pid = pe.th32ProcessID; break; }
        } while (Process32NextW(snap, &pe));
    }
    CloseHandle(snap);
    return pid;
}

// ─── Kernel driver injection ──────────────────────────────────────────────────

static SC_HANDLE g_hSCM = NULL;
static SC_HANDLE g_hSvc = NULL;

static bool LoadDriver(const char* sysPath) {
    g_hSCM = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!g_hSCM) {
        std::cout << "[-] OpenSCManager hatasi: " << GetLastError() << "\n";
        return false;
    }
    SC_HANDLE hOld = OpenServiceA(g_hSCM, "kdrv", SERVICE_ALL_ACCESS);
    if (hOld) {
        SERVICE_STATUS ss{};
        ControlService(hOld, SERVICE_CONTROL_STOP, &ss);
        DeleteService(hOld);
        CloseServiceHandle(hOld);
        Sleep(500);
    }
    g_hSvc = CreateServiceA(g_hSCM, "kdrv", "kdrv",
        SERVICE_ALL_ACCESS, SERVICE_KERNEL_DRIVER,
        SERVICE_DEMAND_START, SERVICE_ERROR_NORMAL,
        sysPath, nullptr, nullptr, nullptr, nullptr, nullptr);
    if (!g_hSvc) {
        std::cout << "[-] CreateService hatasi: " << GetLastError() << "\n";
        CloseServiceHandle(g_hSCM); g_hSCM = nullptr;
        return false;
    }
    if (!StartServiceA(g_hSvc, 0, nullptr)) {
        DWORD err = GetLastError();
        if (err != ERROR_SERVICE_ALREADY_RUNNING) {
            std::cout << "[-] StartService hatasi: " << err << "\n";
            DeleteService(g_hSvc);
            CloseServiceHandle(g_hSvc); g_hSvc = nullptr;
            CloseServiceHandle(g_hSCM); g_hSCM = nullptr;
            return false;
        }
    }
    std::cout << "[+] kdrv.sys yuklendi\n";
    return true;
}

static void UnloadDriver() {
    if (g_hSvc) {
        SERVICE_STATUS ss{};
        ControlService(g_hSvc, SERVICE_CONTROL_STOP, &ss);
        Sleep(300);
        DeleteService(g_hSvc);
        CloseServiceHandle(g_hSvc); g_hSvc = nullptr;
    }
    if (g_hSCM) { CloseServiceHandle(g_hSCM); g_hSCM = nullptr; }
    std::cout << "[*] kdrv.sys kaldirildi\n";
}

static bool KernelInject(DWORD pid, const std::string& dllPath) {
    HANDLE hDev = CreateFileW(L"\\\\.\\kdrv",
        GENERIC_READ | GENERIC_WRITE, 0, nullptr,
        OPEN_EXISTING, 0, nullptr);
    bool preloaded = (hDev != INVALID_HANDLE_VALUE);

    if (!preloaded) {
        char exeDir[MAX_PATH]{};
        GetModuleFileNameA(nullptr, exeDir, MAX_PATH);
        char* slash = strrchr(exeDir, '\\');
        if (slash) *(slash + 1) = '\0';
        std::string sysPath = std::string(exeDir) + "kdrv.sys";
        if (GetFileAttributesA(sysPath.c_str()) == INVALID_FILE_ATTRIBUTES) {
            std::cout << "[-] kdrv.sys bulunamadi: " << sysPath << "\n";
            return false;
        }
        if (!LoadDriver(sysPath.c_str())) return false;
        hDev = CreateFileW(L"\\\\.\\kdrv",
            GENERIC_READ | GENERIC_WRITE, 0, nullptr,
            OPEN_EXISTING, 0, nullptr);
    } else {
        std::cout << "[+] kdrv zaten yuklü (preloaded)\n";
    }

    if (hDev == INVALID_HANDLE_VALUE) {
        std::cout << "[-] \\\\.\\kdrv acilamadi: " << GetLastError() << "\n";
        if (!preloaded) UnloadDriver();
        return false;
    }

    INJECT_REQUEST req{};
    req.pid = pid;
    HMODULE hK32 = GetModuleHandleA("kernel32.dll");
    req.loadLibraryA = (unsigned long)(ULONG_PTR)GetProcAddress(hK32, "LoadLibraryA");
    strncpy_s(req.dllPath, sizeof(req.dllPath), dllPath.c_str(), _TRUNCATE);

    std::cout << "[*] IOCTL: pid=" << pid
              << " lla=0x" << std::hex << req.loadLibraryA << std::dec
              << "\n    dll=" << req.dllPath << "\n";

    DWORD bytes = 0;
    BOOL  ioOk  = DeviceIoControl(hDev, IOCTL_KDRV_INJECT,
                                  &req, sizeof(req), nullptr, 0, &bytes, nullptr);
    DWORD ioErr = GetLastError();
    CloseHandle(hDev);
    if (!preloaded) UnloadDriver();

    if (!ioOk) { std::cout << "[-] DeviceIoControl hatasi: " << ioErr << "\n"; return false; }
    std::cout << "[+] KernelInject BASARILI!\n";
    return true;
}

// ─── Classic LoadLibrary fallback ─────────────────────────────────────────────

static bool Inject(DWORD pid, const std::string& dllPath) {
    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) {
        DWORD err = GetLastError();
        std::cout << "[-] OpenProcess hatasi: " << err;
        if (err == 5) std::cout << " (ACCESS DENIED)";
        std::cout << "\n";
        return false;
    }
    void* mem = VirtualAllocEx(hProc, nullptr, dllPath.size() + 1,
                               MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!mem) {
        std::cout << "[-] VirtualAllocEx hatasi: " << GetLastError() << "\n";
        CloseHandle(hProc); return false;
    }
    WriteProcessMemory(hProc, mem, dllPath.c_str(), dllPath.size() + 1, nullptr);
    HANDLE hThread = CreateRemoteThread(hProc, nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(LoadLibraryA), mem, 0, nullptr);
    bool ok = false;
    if (hThread) {
        WaitForSingleObject(hThread, 8000);
        DWORD exitCode = 0;
        GetExitCodeThread(hThread, &exitCode);
        ok = (exitCode != 0);
        std::cout << (ok ? "[+] LoadLibrary BASARILI\n" : "[-] DLL yuklenemedi\n");
        CloseHandle(hThread);
    } else {
        std::cout << "[-] CreateRemoteThread hatasi: " << GetLastError() << "\n";
    }
    VirtualFreeEx(hProc, mem, 0, MEM_RELEASE);
    CloseHandle(hProc);
    return ok;
}

// ─── Suspended-process launch + inject ───────────────────────────────────────

static const char* s_koPaths[] = {
    "C:\\KnightOnline\\KnightOnline.exe",
    "C:\\Program Files (x86)\\KnightOnline\\KnightOnline.exe",
    "C:\\KOGAME\\KnightOnline\\KnightOnline.exe",
    "C:\\KO\\KnightOnline.exe",
    nullptr
};

static bool LaunchAndInject(const std::string& koExePath, const std::string& dllPath) {
    std::cout << "[*] KO baslatiliyor (SUSPENDED): " << koExePath << "\n";

    STARTUPINFOA si{}; si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};

    if (!CreateProcessA(koExePath.c_str(), nullptr,
                        nullptr, nullptr, FALSE,
                        CREATE_SUSPENDED, nullptr, nullptr,
                        &si, &pi)) {
        std::cout << "[-] CreateProcessA hatasi: " << GetLastError() << "\n";
        return false;
    }

    std::cout << "[+] KO SUSPENDED | PID=" << pi.dwProcessId
              << " TID=" << pi.dwThreadId << "\n";

    bool ok = ManualMapDelayed(pi.dwProcessId, dllPath, 4000);

    // Her durumda resume — inject basarisiz olsa bile KO calismali
    if (ResumeThread(pi.hThread) == (DWORD)-1)
        std::cout << "[-] ResumeThread hatasi: " << GetLastError() << "\n";
    else
        std::cout << "[+] Ana thread serbest birakildi\n";

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);

    if (ok)
        std::cout << "[+] LaunchAndInject tamam! DllMain ~4s sonra cagrilacak.\n";
    else
        std::cout << "[-] Inject basarisiz, KO yine de calistirildi.\n";
    return ok;
}

// ─── Entry point ──────────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(65001);
    std::cout << "=== koxp Injector ===\n";

    if (!IsAdmin()) {
        std::cout << "[!] Yonetici yetkisi yok — UAC isteniyor...\n";
        RelaunchAsAdmin();
        return 0;
    }
    std::cout << "[+] Yonetici olarak calisiyor\n";

    bool        preloadMode = false;
    bool        launchMode  = false;
    std::string dllPath     = "koxp.dll";
    std::string koExePath;

    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--preload" || a == "-p") {
            preloadMode = true;
        } else if (a == "--launch" || a == "-l") {
            launchMode = true;
            if (i + 1 < argc && argv[i + 1][0] != '-')
                koExePath = argv[++i];
        } else {
            dllPath = a;
        }
    }

    // Preload modu
    if (preloadMode) {
        HANDLE hTest = CreateFileW(L"\\\\.\\kdrv",
            GENERIC_READ | GENERIC_WRITE, 0, nullptr, OPEN_EXISTING, 0, nullptr);
        if (hTest != INVALID_HANDLE_VALUE) {
            CloseHandle(hTest);
            std::cout << "[+] kdrv.sys zaten aktif!\n";
            system("pause"); return 0;
        }
        char exeDir[MAX_PATH]{};
        GetModuleFileNameA(nullptr, exeDir, MAX_PATH);
        char* sl = strrchr(exeDir, '\\'); if (sl) *(sl + 1) = '\0';
        std::string sysPath = std::string(exeDir) + "kdrv.sys";
        std::cout << "[*] Preload modu — kdrv.sys yukleniyor...\n";
        if (!LoadDriver(sysPath.c_str())) { std::cout << "[-] Preload basarisiz.\n"; system("pause"); return 1; }
        if (g_hSvc) { CloseServiceHandle(g_hSvc); g_hSvc = nullptr; }
        if (g_hSCM) { CloseServiceHandle(g_hSCM); g_hSCM = nullptr; }
        std::cout << "[+] kdrv.sys aktif! Simdi KnightOnline.exe'yi baslatın.\n";
        system("pause"); return 0;
    }

    // DLL yolunu coz
    char full[MAX_PATH]{};
    GetFullPathNameA(dllPath.c_str(), MAX_PATH, full, nullptr);
    std::cout << "[*] DLL: " << full << "\n";
    if (GetFileAttributesA(full) == INVALID_FILE_ATTRIBUTES) {
        std::cout << "[-] koxp.dll bulunamadi! injector.exe ile ayni klasorde olmali.\n";
        system("pause"); return 1;
    }

    // Launch modu (suspended process — en temiz bypass)
    if (launchMode) {
        if (koExePath.empty()) {
            for (int i = 0; s_koPaths[i]; ++i) {
                if (GetFileAttributesA(s_koPaths[i]) != INVALID_FILE_ATTRIBUTES) {
                    koExePath = s_koPaths[i]; break;
                }
            }
        }
        if (koExePath.empty()) {
            std::cout << "[-] KnightOnline.exe bulunamadi!\n"
                      << "    Kullanim: injector.exe --launch \"C:\\KO\\KnightOnline.exe\"\n";
            system("pause"); return 1;
        }
        bool ok = LaunchAndInject(koExePath, std::string(full));
        if (ok) std::cout << "[*] Log: C:\\koxp_log.txt\n";
        system("pause");
        return ok ? 0 : 1;
    }

    // Normal mod — calisip olan KO'ya inject et
    const wchar_t* candidates[] = {
        L"KnightOnline.exe", L"KnightOnLine.exe",
        L"Knight.exe", L"ko.exe", L"KO.exe",
    };
    DWORD pid = 0; const wchar_t* foundName = nullptr;
    for (auto name : candidates) {
        pid = FindProcessID(name);
        if (pid) { foundName = name; break; }
    }
    if (!pid) {
        std::cout << "[-] Knight Online sureci bulunamadi!\n"
                  << "    Ipucu: --launch ile KO'yu buradan baslatabilirsin.\n";
        system("pause"); return 1;
    }
    std::wcout << L"[+] Process: " << foundName << L" (PID: " << pid << L")\n";

    bool ok = KernelInject(pid, std::string(full));
    if (!ok) { std::cout << "[!] KernelInject basarisiz, manual map deneniyor...\n"; ok = ManualMap(pid, std::string(full)); }
    if (!ok) { std::cout << "[!] ManualMap basarisiz, klasik inject deneniyor...\n"; ok = Inject(pid, std::string(full)); }

    std::cout << (ok ? "[+] Inject BASARILI!\n[*] Log: C:\\koxp_log.txt\n" : "[-] Inject BASARISIZ.\n");
    system("pause");
    return ok ? 0 : 1;
}
