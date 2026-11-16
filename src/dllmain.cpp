#include <Windows.h>
#include "core/log.h"
#include "game/player.h"

static HMODULE g_mod = nullptr;

static DWORD WINAPI MainThread(LPVOID) {
    Sleep(8000);
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
        // TEST: thread yok, sadece proxy — loader lock deadlock'u engellendi
        // Thread onaylaninca bu satiri ac:
        // CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
