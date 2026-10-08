#pragma once
#include <string>

namespace KO::UI {

class Menu {
public:
    static Menu& Get() { static Menu inst; return inst; }

    void Render();         // her EndScene'de çağrılır
    void ApplyStyle();     // ImGui tema

private:
    bool showHotkeyEditor_ = false;

    void DrawPlayerPanel();
    void DrawFeaturesPanel();
    void DrawHotkeyPanel();
    void DrawStatusBar();

    // Yardımcılar
    static void ToggleButton(const char* label, bool& state, const char* onText = "ON", const char* offText = "OFF");
    static void HpBar(float pct);
    static void MpBar(float pct);
};

} // namespace KO::UI
