#include "autoheal.h"
#include "../game/player.h"
#include "../network/packet.h"
#include <chrono>

namespace KO::Features {

int64_t AutoHeal::Now() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

void AutoHeal::UseHpPot() {
    Packet pkt(Opcode::USE_ITEM);
    pkt.Write<uint8_t>(cfg.hpPotSlot);
    SendToServer(pkt);
    lastHpPot_ = Now();
}

void AutoHeal::UseMpPot() {
    Packet pkt(Opcode::USE_ITEM);
    pkt.Write<uint8_t>(cfg.mpPotSlot);
    SendToServer(pkt);
    lastMpPot_ = Now();
}

void AutoHeal::Tick() {
    if (!cfg.enabled) return;

    auto& p = Player::Get();
    if (!p.Valid() || !p.IsAlive()) return;

    int64_t now = Now();

    if (p.HPPercent() < cfg.hpThreshold && (now - lastHpPot_) >= cfg.hpPotDelay)
        UseHpPot();

    if (p.MPPercent() < cfg.mpThreshold && (now - lastMpPot_) >= cfg.mpPotDelay)
        UseMpPot();
}

} // namespace KO::Features
