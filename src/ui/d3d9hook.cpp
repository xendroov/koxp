#include "d3d9hook.h"
#include "menu.h"
#include "../core/log.h"
#include <Windows.h>
#include <d3d9.h>
#include <MinHook.h>
#include <imgui.h>
#include <backends/imgui_impl_dx9.h>
#include <backends/imgui_impl_win32.h>

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace KO::UI {

constexpr int VTI_RESET    = 16;
constexpr int VTI_ENDSCENE = 42;

using Reset_t    = HRESULT(__stdcall*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
using EndScene_t = HRESULT(__stdcall*)(IDirect3DDevice9*);

static Reset_t    orig_Reset    = nullptr;
static EndScene_t orig_EndScene = nullptr;
static bool       g_imguiReady  = false;
static HWND       g_hwnd        = nullptr;
static WNDPROC    g_origWndProc = nullptr;

static LRESULT CALLBACK hk_WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp))
        return true;
    return CallWindowProcA(g_origWndProc, hwnd, msg, wp, lp);
}

static HRESULT __stdcall hk_Reset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pp) {
    if (g_imguiReady) ImGui_ImplDX9_InvalidateDeviceObjects();
    HRESULT hr = orig_Reset(dev, pp);
    if (SUCCEEDED(hr) && g_imguiReady) ImGui_ImplDX9_CreateDeviceObjects();
    return hr;
}

static HRESULT __stdcall hk_EndScene(IDirect3DDevice9* dev) {
    if (!g_imguiReady) {
        // KO window class dene, bulamazsa foreground al
        g_hwnd = FindWindowA("KnightOnLine", nullptr);
        if (!g_hwnd) g_hwnd = FindWindowA(nullptr, "KnightOnLine");
        if (!g_hwnd) g_hwnd = GetForegroundWindow();
        KLog("EndScene init — hwnd=%p", g_hwnd);

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

        Menu::Get().ApplyStyle();
        ImGui_ImplWin32_Init(g_hwnd);
        ImGui_ImplDX9_Init(dev);

        g_origWndProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(hk_WndProc))
        );
        g_imguiReady = true;
        KLog("ImGui init OK");
    }

    ImGui_ImplDX9_NewFrame();
    ImGui_ImplWin32_NewFrame();
    ImGui::NewFrame();
    Menu::Get().Render();
    ImGui::EndFrame();
    ImGui::Render();
    ImGui_ImplDX9_RenderDrawData(ImGui::GetDrawData());

    return orig_EndScene(dev);
}

// vtable al — NULLREF dene, başarısız olursa HAL ile küçük pencere
static bool GetDeviceVTable(void** outTable, size_t count) {
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) { KLog("Direct3DCreate9 failed"); return false; }

    HWND wnd = CreateWindowExA(0, "STATIC", "koxp_tmp",
                               WS_POPUP, 0, 0, 2, 2,
                               nullptr, nullptr, GetModuleHandleA(nullptr), nullptr);

    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed         = TRUE;
    pp.SwapEffect       = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow    = wnd;
    pp.BackBufferFormat = D3DFMT_UNKNOWN;
    pp.BackBufferCount  = 1;

    IDirect3DDevice9* dev = nullptr;

    // 1. NULLREF — en hafif yöntem
    HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_NULLREF,
                                    wnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                    &pp, &dev);

    // 2. HAL fallback
    if (FAILED(hr)) {
        KLog("NULLREF failed (0x%08X), trying HAL", hr);
        pp.BackBufferFormat = D3DFMT_X8R8G8B8;
        hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL,
                               wnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                               &pp, &dev);
    }

    if (FAILED(hr) || !dev) {
        KLog("CreateDevice failed (0x%08X)", hr);
        d3d->Release();
        DestroyWindow(wnd);
        return false;
    }

    memcpy(outTable, *reinterpret_cast<void***>(dev), count * sizeof(void*));
    KLog("vtable OK — EndScene=%p", outTable[VTI_ENDSCENE]);

    dev->Release();
    d3d->Release();
    DestroyWindow(wnd);
    return true;
}

bool InstallD3D9Hook() {
    KLog("InstallD3D9Hook start");

    constexpr size_t VT_SIZE = 119;
    static void* vt[VT_SIZE]{};

    if (!GetDeviceVTable(vt, VT_SIZE)) {
        KLog("GetDeviceVTable FAILED");
        return false;
    }

    MH_STATUS ms = MH_Initialize();
    KLog("MH_Initialize: %d", ms);

    ms = MH_CreateHook(vt[VTI_ENDSCENE], &hk_EndScene,
                        reinterpret_cast<void**>(&orig_EndScene));
    KLog("Hook EndScene: %d", ms);

    ms = MH_CreateHook(vt[VTI_RESET], &hk_Reset,
                        reinterpret_cast<void**>(&orig_Reset));
    KLog("Hook Reset: %d", ms);

    ms = MH_EnableHook(MH_ALL_HOOKS);
    KLog("EnableHook: %d", ms);
    return true;
}

void RemoveD3D9Hook() {
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    if (g_imguiReady) {
        if (g_hwnd && g_origWndProc)
            SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC,
                              reinterpret_cast<LONG_PTR>(g_origWndProc));
        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        g_imguiReady = false;
    }
    KLog("Hook removed");
}

} // namespace KO::UI
