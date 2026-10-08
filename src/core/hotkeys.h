#pragma once
#include <Windows.h>
#include <functional>
#include <string>
#include <array>

namespace KO {

// F1(0x70) .. F12(0x7B) = 12 slot
constexpr int HK_COUNT    = 12;
constexpr int VK_F1_BASE  = VK_F1; // 0x70

struct HotkeySlot {
    std::string            label;
    std::function<void()>  action;
    bool                   enabled = true;
};

class HotkeyManager {
public:
    static HotkeyManager& Get() { static HotkeyManager inst; return inst; }

    // F1=index 0, F2=1, ..., F12=11
    void Bind(int fIndex, std::string label, std::function<void()> fn);
    void Unbind(int fIndex);

    void Poll();   // her frame çağır (GetAsyncKeyState tabanlı)

    const std::array<HotkeySlot, HK_COUNT>& Slots() const { return slots_; }
    HotkeySlot& Slot(int fIndex) { return slots_[fIndex]; }

private:
    std::array<HotkeySlot, HK_COUNT> slots_{};
    std::array<bool, HK_COUNT> prevState_{};
};

} // namespace KO
