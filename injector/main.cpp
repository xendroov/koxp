#include <Windows.h>
#include <TlHelp32.h>
#include <iostream>
#include <string>

// Yonetici olarak calisip calismadigi kontrol et
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

// Kendi kendini yonetici olarak yeniden baslat (UAC)
static void RelaunchAsAdmin() {
    char path[MAX_PATH]{};
    GetModuleFileNameA(nullptr, path, MAX_PATH);

    // Komut satirini aktar
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
        if (!ok) std::cout << "[-] DLL yuklenemedi — DLL ile injector ayni klasorde olmali\n";
        CloseHandle(hThread);
    } else {
        std::cout << "[-] CreateRemoteThread hatasi: " << GetLastError() << "\n";
    }

    VirtualFreeEx(hProc, mem, 0, MEM_RELEASE);
    CloseHandle(hProc);
    return ok;
}

int main(int argc, char* argv[]) {
    SetConsoleOutputCP(65001);

    std::cout << "=== koxp Injector ===\n";

    // Yonetici degil ise kendini UAC ile yeniden baslat
    if (!IsAdmin()) {
        std::cout << "[!] Yonetici yetkisi yok — UAC isteniyor...\n";
        RelaunchAsAdmin();
        return 0;  // Eski pencere kapanir, yeni pencere yonetici olarak acar
    }
    std::cout << "[+] Yonetici olarak calisiyor\n";

    const wchar_t* candidates[] = {
        L"KnightOnline.exe",
        L"KnightOnLine.exe",
        L"Knight.exe",
        L"ko.exe",
        L"KO.exe",
    };

    std::string dllPath = "koxp.dll";
    if (argc > 1) dllPath = argv[1];

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

    bool ok = Inject(pid, std::string(full));
    if (ok) {
        std::cout << "[+] Inject BASARILI!\n";
        std::cout << "[*] Log: C:\\koxp_log.txt\n";
    } else {
        std::cout << "[-] Inject BASARISIZ.\n";
    }

    system("pause");
    return ok ? 0 : 1;
}
