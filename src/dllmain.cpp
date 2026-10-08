#include <Windows.h>
#include "core/hotkeys.h"
#include "ui/d3d9hook.h"
#include "features/bot.h"
#include "features/autoheal.h"
#include "features/autoskill.h"

static HMODULE g_hModule = nullptr;
static bool    g_running  = false;

static void SetupHotkeys() {
    auto& hkm   = KO::HotkeyManager::Get();
    auto& bot   = KO::Features::Bot::Get();
    auto& heal  = KO::Features::AutoHeal::Get();
    auto& skill = KO::Features::AutoSkill::Get();

    // F7: Bot toggle
    hkm.Bind(6, "Bot", [&bot]() {
        bot.cfg.enabled = !bot.cfg.enabled;
        if (bot.cfg.enabled) bot.Start();
        else                 bot.Stop();
    });

    // F8: AutoHeal toggle
    hkm.Bind(7, "AutoHeal", [&heal]() {
        heal.cfg.enabled = !heal.cfg.enabled;
    });

    // F9: AutoSkill toggle
    hkm.Bind(8, "AutoSkill", [&skill]() {
        skill.cfg.enabled = !skill.cfg.enabled;
    });

    // F12: çıkış
    hkm.Bind(11, "Cikis", [&]() { g_running = false; });
}

static DWORD WINAPI MainThread(LPVOID) {
    // D3D9 overlay hook kur
    KO::UI::InstallD3D9Hook();

    SetupHotkeys();

    g_running = true;
    while (g_running) {
        KO::HotkeyManager::Get().Poll();
        KO::Features::Bot::Get().Tick();
        Sleep(10);
    }

    KO::UI::RemoveD3D9Hook();
    FreeLibraryAndExitThread(g_hModule, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        g_hModule = hModule;
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
