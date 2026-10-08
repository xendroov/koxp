#include "d3d9hook.h"
#include "menu.h"
#include <Windows.h>
#include <d3d9.h>
#include <MinHook.h>
#include <imgui.h>
#include <backends/imgui_impl_dx9.h>
#include <backends/imgui_impl_win32.h>

// ImGui Win32 message handler (imgui_impl_win32.cpp'den)
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND, UINT, WPARAM, LPARAM);

namespace KO::UI {

// vtable index'leri IDirect3DDevice9
constexpr int VTI_RESET     = 16;
constexpr int VTI_ENDSCENE  = 42;

using Reset_t    = HRESULT(__stdcall*)(IDirect3DDevice9*, D3DPRESENT_PARAMETERS*);
using EndScene_t = HRESULT(__stdcall*)(IDirect3DDevice9*);

static Reset_t    orig_Reset    = nullptr;
static EndScene_t orig_EndScene = nullptr;

static bool   g_imguiReady = false;
static HWND   g_hwnd       = nullptr;
static WNDPROC g_origWndProc = nullptr;

// --- WndProc hook ---
static LRESULT CALLBACK hk_WndProc(HWND hwnd, UINT msg, WPARAM wp, LPARAM lp) {
    if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wp, lp))
        return true;
    return CallWindowProcA(g_origWndProc, hwnd, msg, wp, lp);
}

// --- Reset hook: device kaybolunca ImGui resource'larını temizle ---
static HRESULT __stdcall hk_Reset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pp) {
    if (g_imguiReady) {
        ImGui_ImplDX9_InvalidateDeviceObjects();
    }
    HRESULT hr = orig_Reset(dev, pp);
    if (SUCCEEDED(hr) && g_imguiReady) {
        ImGui_ImplDX9_CreateDeviceObjects();
    }
    return hr;
}

// --- EndScene hook: her frame ImGui çiz ---
static HRESULT __stdcall hk_EndScene(IDirect3DDevice9* dev) {
    if (!g_imguiReady) {
        // İlk kez: ImGui'yi başlat
        g_hwnd = FindWindowA("KnightOnLine", nullptr);
        if (!g_hwnd) g_hwnd = GetForegroundWindow();

        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.IniFilename = nullptr;         // imgui.ini yazma
        io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;

        Menu::Get().ApplyStyle();

        ImGui_ImplWin32_Init(g_hwnd);
        ImGui_ImplDX9_Init(dev);

        // WndProc hook
        g_origWndProc = reinterpret_cast<WNDPROC>(
            SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(hk_WndProc))
        );

        g_imguiReady = true;
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

// --- Geçici cihaz oluşturup vtable al ---
static bool GetDeviceVTable(void** outTable, size_t count) {
    IDirect3D9* d3d = Direct3DCreate9(D3D_SDK_VERSION);
    if (!d3d) return false;

    HWND wnd = CreateWindowA("STATIC", "", WS_POPUP, 0, 0, 1, 1,
                              nullptr, nullptr, nullptr, nullptr);

    D3DPRESENT_PARAMETERS pp{};
    pp.Windowed           = TRUE;
    pp.SwapEffect         = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow      = wnd;

    IDirect3DDevice9* dev = nullptr;
    HRESULT hr = d3d->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_NULLREF,
                                    wnd, D3DCREATE_SOFTWARE_VERTEXPROCESSING,
                                    &pp, &dev);
    if (FAILED(hr)) {
        d3d->Release();
        DestroyWindow(wnd);
        return false;
    }

    memcpy(outTable, *reinterpret_cast<void***>(dev), count * sizeof(void*));
    dev->Release();
    d3d->Release();
    DestroyWindow(wnd);
    return true;
}

bool InstallD3D9Hook() {
    constexpr size_t VT_SIZE = 119; // IDirect3DDevice9 method count
    static void* vt[VT_SIZE]{};

    if (!GetDeviceVTable(vt, VT_SIZE)) return false;

    MH_Initialize();

    MH_CreateHook(vt[VTI_ENDSCENE], &hk_EndScene,
                  reinterpret_cast<void**>(&orig_EndScene));

    MH_CreateHook(vt[VTI_RESET], &hk_Reset,
                  reinterpret_cast<void**>(&orig_Reset));

    MH_EnableHook(MH_ALL_HOOKS);
    return true;
}

void RemoveD3D9Hook() {
    MH_DisableHook(MH_ALL_HOOKS);
    MH_Uninitialize();

    if (g_imguiReady) {
        // WndProc geri yükle
        if (g_hwnd && g_origWndProc)
            SetWindowLongPtrA(g_hwnd, GWLP_WNDPROC,
                              reinterpret_cast<LONG_PTR>(g_origWndProc));

        ImGui_ImplDX9_Shutdown();
        ImGui_ImplWin32_Shutdown();
        ImGui::DestroyContext();
        g_imguiReady = false;
    }
}

} // namespace KO::UI
