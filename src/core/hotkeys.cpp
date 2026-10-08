#include "hotkeys.h"

namespace KO {

void HotkeyManager::Bind(int fIndex, std::string label, std::function<void()> fn) {
    if (fIndex < 0 || fIndex >= HK_COUNT) return;
    slots_[fIndex].label  = std::move(label);
    slots_[fIndex].action = std::move(fn);
    slots_[fIndex].enabled = true;
}

void HotkeyManager::Unbind(int fIndex) {
    if (fIndex < 0 || fIndex >= HK_COUNT) return;
    slots_[fIndex].label  = {};
    slots_[fIndex].action = {};
}

void HotkeyManager::Poll() {
    for (int i = 0; i < HK_COUNT; ++i) {
        if (!slots_[i].action || !slots_[i].enabled) continue;

        int vk      = VK_F1_BASE + i;
        bool cur    = (GetAsyncKeyState(vk) & 0x8000) != 0;
        bool rising = cur && !prevState_[i];   // sadece basış anında tetikle
        prevState_[i] = cur;

        if (rising) slots_[i].action();
    }
}

} // namespace KO
