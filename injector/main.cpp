#include <Windows.h>
#include <TlHelp32.h>
#include <iostream>
#include <string>

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
        std::cout << "[-] OpenProcess hatasi: " << GetLastError() << "\n";
        return false;
    }

    void* mem = VirtualAllocEx(hProc, nullptr, dllPath.size() + 1,
                               MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!mem) {
        std::cout << "[-] VirtualAllocEx hatasi: " << GetLastError() << "\n";
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
        std::cout << "[+] Thread bitti. LoadLibrary sonucu: 0x" << std::hex << exitCode << std::dec << "\n";
        ok = (exitCode != 0);
        if (!ok) std::cout << "[-] DLL yuklenemedi (0 dondu)\n";
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

    // KO 2626 olasi exe isimleri
    const wchar_t* candidates[] = {
        L"KnightOnLine.exe",
        L"KnightOnline.exe",
        L"Knight.exe",
        L"ko.exe",
        L"KO.exe",
    };

    std::string dllPath = "koxp.dll";
    if (argc > 1) dllPath = argv[1];

    char full[MAX_PATH]{};
    GetFullPathNameA(dllPath.c_str(), MAX_PATH, full, nullptr);

    std::cout << "=== koxp Injector ===\n";
    std::cout << "[*] DLL yolu: " << full << "\n";

    // DLL var mi?
    if (GetFileAttributesA(full) == INVALID_FILE_ATTRIBUTES) {
        std::cout << "[-] DLL bulunamadi: " << full << "\n";
        std::cout << "    -> injector.exe ile koxp.dll ayni klasorde olmali!\n";
        system("pause");
        return 1;
    }

    // Process bul
    DWORD pid = 0;
    const wchar_t* foundName = nullptr;
    for (auto name : candidates) {
        pid = FindProcessID(name);
        if (pid) { foundName = name; break; }
    }

    if (!pid) {
        std::cout << "[-] Knight Online process bulunamadi!\n";
        std::cout << "    Aranan isimler: KnightOnLine.exe, KnightOnline.exe, Knight.exe, ko.exe\n";
        std::cout << "\n[!] Gorev Yoneticisi'nden (Ctrl+Shift+Esc) KO'nun exe adini kontrol et.\n";
        system("pause");
        return 1;
    }

    std::wcout << L"[+] Process bulundu: " << foundName << L" (PID: " << pid << L")\n";

    bool ok = Inject(pid, std::string(full));
    if (ok) {
        std::cout << "[+] Inject basarili!\n";
        std::cout << "[*] Log dosyasi: C:\\koxp_log.txt\n";
        std::cout << "[*] Overlay gozukmuyorsa logu kontrol et.\n";
    } else {
        std::cout << "[-] Inject basarisiz.\n";
        std::cout << "    -> x86-Release build kullandığına emin ol\n";
        std::cout << "    -> Yonetici olarak calistir (sag tik → Yönetici)\n";
    }

    system("pause");
    return ok ? 0 : 1;
}
