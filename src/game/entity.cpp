#include "entity.h"

namespace KO {

void EntityManager::Update() {
    entities_.clear();

    uintptr_t listBase  = Memory::Read<uintptr_t>(Memory::Resolve(Offsets::g_entityList));
    int32_t   count     = Memory::Read<int32_t>(Memory::Resolve(Offsets::g_entityCount));

    if (!Memory::IsValidPtr(listBase) || count <= 0 || count > 500)
        return;

    for (int i = 0; i < count; ++i) {
        Entity e;
        e.base  = listBase + static_cast<uintptr_t>(i) * Offsets::Entity::EntrySize;
        e.index = i;
        if (e.Valid() && e.ID() > 0)
            entities_.push_back(e);
    }
}

const Entity* EntityManager::NearestMonster(float px, float py, float maxRange) const {
    const Entity* best = nullptr;
    float bestDist = maxRange;

    for (const auto& e : entities_) {
        if (!e.IsMonster() || !e.IsAlive()) continue;
        float d = e.DistanceTo(px, py);
        if (d < bestDist) {
            bestDist = d;
            best = &e;
        }
    }
    return best;
}

} // namespace KO
