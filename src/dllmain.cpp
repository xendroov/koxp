#include <Windows.h>
#include "core/log.h"
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

    hkm.Bind(6, "Bot", [&bot]() {
        bot.cfg.enabled = !bot.cfg.enabled;
        if (bot.cfg.enabled) bot.Start();
        else                 bot.Stop();
        KLog("Bot toggled: %s", bot.cfg.enabled ? "ON" : "OFF");
    });

    hkm.Bind(7, "AutoHeal", [&heal]() {
        heal.cfg.enabled = !heal.cfg.enabled;
        KLog("AutoHeal toggled: %s", heal.cfg.enabled ? "ON" : "OFF");
    });

    hkm.Bind(8, "AutoSkill", [&skill]() {
        skill.cfg.enabled = !skill.cfg.enabled;
    });

    hkm.Bind(11, "Cikis", [&]() { g_running = false; });
}

static DWORD WINAPI MainThread(LPVOID) {
    KLog("=== koxp DLL MainThread started ===");

    if (!KO::UI::InstallD3D9Hook()) {
        KLog("D3D9 hook FAILED — devam ediliyor (overlay olmaz)");
        // Hook başarısız olsa da devam et, bot çalışabilir
    }

    SetupHotkeys();
    KLog("Hotkeys ready. F7=Bot F8=Heal F9=Skill F12=Exit");

    g_running = true;
    while (g_running) {
        KO::HotkeyManager::Get().Poll();
        KO::Features::Bot::Get().Tick();
        Sleep(10);
    }

    KO::UI::RemoveD3D9Hook();
    KLog("=== koxp unloading ===");
    FreeLibraryAndExitThread(g_hModule, 0);
    return 0;
}

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        g_hModule = hModule;
        KLog("DllMain ATTACH — creating thread");
        CreateThread(nullptr, 0, MainThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
