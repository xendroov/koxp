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
        std::cout << "[-] OpenProcess failed: " << GetLastError() << "\n";
        return false;
    }

    void* mem = VirtualAllocEx(hProc, nullptr, dllPath.size() + 1,
                               MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE);
    if (!mem) { CloseHandle(hProc); return false; }

    WriteProcessMemory(hProc, mem, dllPath.c_str(), dllPath.size() + 1, nullptr);

    HANDLE hThread = CreateRemoteThread(
        hProc, nullptr, 0,
        reinterpret_cast<LPTHREAD_START_ROUTINE>(LoadLibraryA),
        mem, 0, nullptr
    );

    if (hThread) {
        WaitForSingleObject(hThread, 5000);
        CloseHandle(hThread);
        std::cout << "[+] Inject OK\n";
    } else {
        std::cout << "[-] CreateRemoteThread failed: " << GetLastError() << "\n";
    }

    VirtualFreeEx(hProc, mem, 0, MEM_RELEASE);
    CloseHandle(hProc);
    return hThread != nullptr;
}

int main(int argc, char* argv[]) {
    const wchar_t* target = L"KnightOnline.exe"; // KO exe adını kontrol et

    std::string dllPath = "koxp.dll";
    if (argc > 1) dllPath = argv[1];

    // Tam yol al
    char full[MAX_PATH];
    GetFullPathNameA(dllPath.c_str(), MAX_PATH, full, nullptr);

    std::cout << "[*] Hedef: KnightOnline.exe\n";
    std::cout << "[*] DLL  : " << full << "\n";

    DWORD pid = FindProcessID(target);
    if (!pid) {
        std::cout << "[-] Process bulunamadi. KO acik mi?\n";
        return 1;
    }
    std::cout << "[*] PID  : " << pid << "\n";

    Inject(pid, std::string(full));
    return 0;
}
