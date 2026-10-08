#pragma once
#include <cstdint>

namespace KO::Features {

struct AutoHealConfig {
    bool  enabled       = true;
    float hpThreshold   = 70.f;  // HP % altına düşünce pot kullan
    float mpThreshold   = 40.f;  // MP % altına düşünce pot kullan
    int   hpPotDelay    = 1500;  // ms
    int   mpPotDelay    = 1500;
};

class AutoHeal {
public:
    static AutoHeal& Get() { static AutoHeal inst; return inst; }

    void Tick();  // bot loop'tan her iterasyonda çağır

    AutoHealConfig cfg;

private:
    int64_t lastHpPot_ = 0;
    int64_t lastMpPot_ = 0;

    void UseHpPot();
    void UseMpPot();
    int64_t Now() const;
};

} // namespace KO::Features
