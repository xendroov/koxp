#include "autoskill.h"
#include "../network/packet.h"
#include <chrono>

namespace KO::Features {

int64_t AutoSkill::Now() const {
    return std::chrono::duration_cast<std::chrono::milliseconds>(
        std::chrono::steady_clock::now().time_since_epoch()
    ).count();
}

bool AutoSkill::SkillReady(const SkillSlot& slot) const {
    return (Now() - slot.lastUsed) >= slot.cooldownMs;
}

void AutoSkill::Attack(int32_t targetID) {
    // Normal attack paketi
    Packet pkt(Opcode::ATTACK);
    pkt.Write<int32_t>(targetID);
    SendToServer(pkt);
    lastAttack_ = Now();
}

void AutoSkill::UseSkill(const SkillSlot& slot, int32_t targetID) {
    Packet pkt(Opcode::USE_SKILL);
    pkt.Write<uint32_t>(slot.skillID);
    pkt.Write<int32_t>(targetID);
    SendToServer(pkt);
    const_cast<SkillSlot&>(slot).lastUsed = Now();
}

void AutoSkill::Tick(int32_t targetID) {
    if (!cfg.enabled || targetID < 0) return;

    int64_t now = Now();

    // Önce skill dene
    if (cfg.useSkills) {
        for (auto& slot : cfg.skills) {
            if (SkillReady(slot)) {
                UseSkill(slot, targetID);
                return;
            }
        }
    }

    // Skill yoksa normal attack
    if ((now - lastAttack_) >= cfg.attackDelayMs)
        Attack(targetID);
}

} // namespace KO::Features
