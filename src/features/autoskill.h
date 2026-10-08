#pragma once
#include <cstdint>
#include <vector>

namespace KO::Features {

struct SkillSlot {
    int      slotIndex  = -1;
    uint32_t skillID    = 0;
    int      cooldownMs = 1000;
    int64_t  lastUsed   = 0;
};

struct AutoSkillConfig {
    bool enabled           = true;
    int  attackDelayMs     = 800;   // normal attack delay
    bool useSkills         = true;
    std::vector<SkillSlot> skills;  // kullanılacak skill slotları (öncelik sırasına göre)
};

class AutoSkill {
public:
    static AutoSkill& Get() { static AutoSkill inst; return inst; }

    void Tick(int32_t targetID);

    AutoSkillConfig cfg;

private:
    int64_t lastAttack_ = 0;

    void Attack(int32_t targetID);
    void UseSkill(const SkillSlot& slot, int32_t targetID);
    bool SkillReady(const SkillSlot& slot) const;
    int64_t Now() const;
};

} // namespace KO::Features
