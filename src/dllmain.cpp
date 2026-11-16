#include <Windows.h>
#include "core/log.h"
#include "game/player.h"

extern HMODULE g_realWinmm; // winmm_proxy.cpp'den

static HMODULE g_mod = nullptr;

static DWORD WINAPI MainThread(LPVOID) {
    Sleep(8000); // KO tam acilana kadar bekle
    KLog("=== koxp basladi ===");

    while (true) {
        if (GetAsyncKeyState(VK_F12) & 1) break;

        KO::PlayerData p = KO::ReadPlayer();
        if (p.valid) {
            KLog("[Player] %s | Lv%d | HP %d/%d | MP %d/%d | Gold %d | X%.1f Y%.1f",
                 p.name, p.level,
                 p.hp, p.maxHp,
                 p.mp, p.maxMp,
                 p.gold,
                 p.x, p.y);
        }

        Sleep(1000);
    }

    KLog("=== koxp kapaniyor ===");
    FreeLibraryAndExitThread(g_mod, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hMod);
        g_mod = hMod;

        // Gercek winmm.dll'i hemen yukle (proxy fonksiyonlar hazir olsun)
        if (!g_realWinmm) {
            char path[MAX_PATH];
            GetSystemDirectoryA(path, sizeof(path));
            lstrcatA(path, "\\winmm.dll");
            g_realWinmm = LoadLibraryA(path);
        }

        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
