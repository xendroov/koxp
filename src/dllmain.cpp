#include <Windows.h>
#include "core/hooks.h"
#include "features/bot.h"
#include "features/autoheal.h"
#include "features/autoskill.h"

// Hotkey tanımları
constexpr int HK_TOGGLE_BOT    = VK_F7;   // F7 — bot aç/kapat
constexpr int HK_TOGGLE_HEAL   = VK_F8;   // F8 — autoheal aç/kapat
constexpr int HK_UNLOAD        = VK_F12;  // F12 — DLL'i kaldır

static HMODULE g_hModule = nullptr;
static bool    g_running  = false;

static DWORD WINAPI MainThread(LPVOID) {
    // Hooks kur
    KO::Hooks::Install();

    // Varsayılan skill örneği: slot 0, 2 saniyelik cooldown
    // KO::Features::AutoSkill::Get().cfg.skills.push_back({0, 0x1234, 2000});

    g_running = true;
    while (g_running) {
        // Hotkey kontrol
        if (GetAsyncKeyState(HK_TOGGLE_BOT) & 1) {
            auto& bot = KO::Features::Bot::Get();
            if (bot.IsRunning()) { bot.Stop(); bot.cfg.enabled = false; }
            else                 { bot.cfg.enabled = true; bot.Start(); }
        }

        if (GetAsyncKeyState(HK_TOGGLE_HEAL) & 1)
            KO::Features::AutoHeal::Get().cfg.enabled ^= true;

        if (GetAsyncKeyState(HK_UNLOAD) & 1) {
            g_running = false;
            break;
        }

        // Bot tick (~100 fps)
        KO::Features::Bot::Get().Tick();

        Sleep(10);
    }

    KO::Hooks::Remove();
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
