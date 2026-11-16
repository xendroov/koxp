#include <Windows.h>
#include "core/log.h"
#include "core/memory.h"
#include "core/offsets.h"
#include "game/player.h"

static HMODULE g_mod = nullptr;

static DWORD WINAPI MainThread(LPVOID) {
    Sleep(10000); // KO tam acilana kadar bekle
    KLog("=== koxp basladi ===");

    while (true) {
        if (GetAsyncKeyState(VK_F12) & 1) break;

        uintptr_t base = Mem::Deref(KO::Off::PTR_CHR);
        KLog("[dbg] PTR_CHR=0x%08X base=0x%08X", (unsigned)KO::Off::PTR_CHR, (unsigned)base);

        if (base) {
            int   lv  = Mem::Read<int>(base + KO::Off::Chr::Level);
            int   hp  = Mem::Read<int>(base + KO::Off::Chr::HP);
            int   mhp = Mem::Read<int>(base + KO::Off::Chr::MaxHP);
            KLog("[dbg] lv=%d hp=%d maxhp=%d", lv, hp, mhp);
        }

        KO::PlayerData p = KO::ReadPlayer();
        if (p.valid) {
            KLog("[Player] %s | Lv%d | HP %d/%d | MP %d/%d | Gold %d | X%.1f Y%.1f",
                 p.name, p.level,
                 p.hp, p.maxHp,
                 p.mp, p.maxMp,
                 p.gold,
                 p.x, p.y);
        }

        Sleep(3000);
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
