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

// ─── Kernel driver injection (bypasses Xigncode3 ObRegisterCallbacks) ─────────

static SC_HANDLE g_hSCM = NULL;
static SC_HANDLE g_hSvc = NULL;

static bool LoadDriver(const char* sysPath) {
    g_hSCM = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_ALL_ACCESS);
    if (!g_hSCM) {
        std::cout << "[-] OpenSCManager hatasi: " << GetLastError() << "\n";
        return false;
    }

    // Onceki cokme kalintisini temizle
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
    if (g_hSCM) {
        CloseServiceHandle(g_hSCM); g_hSCM = nullptr;
    }
    std::cout << "[*] kdrv.sys kaldirildi\n";
}

static bool KernelInject(DWORD pid, const std::string& dllPath) {
    // Driver zaten yukluyse (preload modu) direkt kullan
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
            std::cout << "    -> kdrv.sys ve injector.exe ayni klasorde olmali\n";
            return false;
        }
        std::cout << "[*] kdrv.sys bulundu: " << sysPath << "\n";

        if (!LoadDriver(sysPath.c_str())) return false;

        hDev = CreateFileW(L"\\\\.\\kdrv",
            GENERIC_READ | GENERIC_WRITE, 0, nullptr,
            OPEN_EXISTING, 0, nullptr);
    } else {
        std::cout << "[+] kdrv zaten yukl\u00fc (preloaded mod)\n";
    }

    if (hDev == INVALID_HANDLE_VALUE) {
        std::cout << "[-] \\\\.\\kdrv acilamadi: " << GetLastError() << "\n";
        if (!preloaded) UnloadDriver();
        return false;
    }

    INJECT_REQUEST req{};
    req.pid = pid;

    // LoadLibraryA adresi 32-bit surec icin gecerli (ayni boot oturumunda sabittir)
    HMODULE hK32 = GetModuleHandleA("kernel32.dll");
    req.loadLibraryA = (unsigned long)(ULONG_PTR)GetProcAddress(hK32, "LoadLibraryA");

    strncpy_s(req.dllPath, sizeof(req.dllPath), dllPath.c_str(), _TRUNCATE);

    std::cout << "[*] IOCTL gonderiliyor: pid=" << pid
              << " lla=0x" << std::hex << req.loadLibraryA << std::dec
              << "\n    dll=" << req.dllPath << "\n";

    DWORD bytes = 0;
    BOOL  ioOk  = DeviceIoControl(hDev, IOCTL_KDRV_INJECT,
                                  &req, sizeof(req),
                                  nullptr, 0, &bytes, nullptr);
    DWORD ioErr = GetLastError();

    CloseHandle(hDev);
    if (!preloaded) UnloadDriver();

    if (!ioOk) {
        std::cout << "[-] DeviceIoControl hatasi: " << ioErr << "\n";
        return false;
    }
    std::cout << "[+] KernelInject BASARILI!\n";
    return true;
}

// ─── Classic LoadLibrary inject (fallback) ─────────────────────────────────

static bool Inject(DWORD pid, const std::string& dllPath) {
    HANDLE hProc = OpenProcess(PROCESS_ALL_ACCESS, FALSE, pid);
    if (!hProc) {
        DWORD err = GetLastError();
        std::cout << "[-] OpenProcess hatasi: " << err;
        if (err == 5) std::cout << " (ACCESS DENIED - injector yonetici degil!)";
        std::cout << "\n";
        return false;
    }

    void* mem = VirtualAllocEx(hProc, nullptr, dllPath.size() + 1,
                               MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!mem) {
        DWORD err = GetLastError();
        std::cout << "[-] VirtualAllocEx hatasi: " << err;
        if (err == 5) std::cout << " (ACCESS DENIED)";
        std::cout << "\n";
        CloseHandle(hProc);
        return false;
    }

    WriteProcessMemory(hProc, mem, dllPath.c_str(), dllPath.size() + 1, nullptr);

    HANDLE hThread = CreateRemoteThread(
        hProc, nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(LoadLibraryA),
        mem, 0, nullptr
    );

    bool ok = false;
    if (hThread) {
        WaitForSingleObject(hThread, 8000);
        DWORD exitCode = 0;
        GetExitCodeThread(hThread, &exitCode);
        std::cout << "[+] Thread bitti. LoadLibrary sonucu: 0x"
                  << std::hex << exitCode << std::dec << "\n";
        ok = (exitCode != 0);
        if (!ok) std::cout << "[-] DLL yuklenemedi\n";
        CloseHandle(hThread);
    } else {
        std::cout << "[-] CreateRemoteThread hatasi: " << GetLastError() << "\n";
    }

    VirtualFreeEx(hProc, mem, 0, MEM_RELEASE);
    CloseHandle(hProc);
    return ok;
}

// ─── Entry point ──────────────────────────────────────────────────────────

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(65001);

    std::cout << "=== koxp Injector ===\n";

    if (!IsAdmin()) {
        std::cout << "[!] Yonetici yetkisi yok \u2014 UAC isteniyor...\n";
        RelaunchAsAdmin();
        return 0;
    }
    std::cout << "[+] Yonetici olarak calisiyor\n";

    // --preload: driver'i oyun acilmadan yukle, inject yapma
    bool preloadMode = false;
    std::string dllPath = "koxp.dll";
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--preload" || a == "-p") preloadMode = true;
        else dllPath = a;
    }

    if (preloadMode) {
        // Zaten yuklu mu kontrol et — oyun acildiktan sonra tekrar --preload
        // calistirilirsa driver'i oldurme, sadece bildir ve cik
        HANDLE hTest = CreateFileW(L"\\\\.\\kdrv",
            GENERIC_READ | GENERIC_WRITE, 0, nullptr,
            OPEN_EXISTING, 0, nullptr);
        if (hTest != INVALID_HANDLE_VALUE) {
            CloseHandle(hTest);
            std::cout << "[+] kdrv.sys zaten aktif! Inject icin 'injector.exe' calistirin.\n";
            system("pause"); return 0;
        }

        char exeDir[MAX_PATH]{};
        GetModuleFileNameA(nullptr, exeDir, MAX_PATH);
        char* sl = strrchr(exeDir, '\\');
        if (sl) *(sl + 1) = '\0';
        std::string sysPath = std::string(exeDir) + "kdrv.sys";

        std::cout << "[*] Preload modu \u2014 kdrv.sys oyun acilmadan yukleniyor...\n";
        if (!LoadDriver(sysPath.c_str())) {
            std::cout << "[-] Preload basarisiz.\n";
            system("pause"); return 1;
        }
        // SCM handle'larini kapat ama servisi calistirmaya devam et
        if (g_hSvc) { CloseServiceHandle(g_hSvc); g_hSvc = nullptr; }
        if (g_hSCM) { CloseServiceHandle(g_hSCM); g_hSCM = nullptr; }
        std::cout << "[+] kdrv.sys aktif!\n";
        std::cout << "[*] Simdi KnightOnline.exe'yi baslatin.\n";
        std::cout << "[*] Oyun acildiktan sonra bu injector'i tekrar calistirin.\n";
        system("pause"); return 0;
    }

    const wchar_t* candidates[] = {
        L"KnightOnline.exe",
        L"KnightOnLine.exe",
        L"Knight.exe",
        L"ko.exe",
        L"KO.exe",
    };

    char full[MAX_PATH]{};
    GetFullPathNameA(dllPath.c_str(), MAX_PATH, full, nullptr);
    std::cout << "[*] DLL: " << full << "\n";

    if (GetFileAttributesA(full) == INVALID_FILE_ATTRIBUTES) {
        std::cout << "[-] koxp.dll bulunamadi!\n";
        std::cout << "    -> injector.exe ve koxp.dll ayni klasorde olmali\n";
        system("pause");
        return 1;
    }

    DWORD pid = 0;
    const wchar_t* foundName = nullptr;
    for (auto name : candidates) {
        pid = FindProcessID(name);
        if (pid) { foundName = name; break; }
    }

    if (!pid) {
        std::cout << "[-] Knight Online sureci bulunamadi!\n";
        std::cout << "    Aranan: KnightOnline.exe / KnightOnLine.exe / Knight.exe\n";
        system("pause");
        return 1;
    }

    std::wcout << L"[+] Process: " << foundName << L" (PID: " << pid << L")\n";

    // 1. Kernel driver inject (Xigncode3 ObRegisterCallbacks'i atiyor)
    bool ok = KernelInject(pid, std::string(full));

    // 2. Manual mapper (Xigncode NtAllocateVirtualMemory hook'unu atiyor)
    if (!ok) {
        std::cout << "[!] KernelInject basarisiz, manual map deneniyor...\n";
        ok = ManualMap(pid, std::string(full));
    }

    // 3. Klasik LoadLibrary inject (fallback)
    if (!ok) {
        std::cout << "[!] ManualMap basarisiz, klasik inject deneniyor...\n";
        ok = Inject(pid, std::string(full));
    }

    if (ok) {
        std::cout << "[+] Inject BASARILI!\n";
        std::cout << "[*] Log: C:\\koxp_log.txt\n";
    } else {
        std::cout << "[-] Inject BASARISIZ.\n";
    }

    system("pause");
    return ok ? 0 : 1;
}
