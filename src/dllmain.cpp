#include <Windows.h>
#include "core/log.h"
#include "game/player.h"

static HMODULE g_mod = nullptr;

static DWORD WINAPI MainThread(LPVOID) {
    KLog("=== koxp basladi ===");

    while (true) {
        // F12 basılıysa çık
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

        Sleep(1000); // saniyede bir log
    }

    KLog("=== koxp kapaniyor ===");
    FreeLibraryAndExitThread(g_mod, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hMod, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hMod);
        g_mod = hMod;
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
