#include "entity.h"

namespace KO {

void EntityManager::ScanManager(uintptr_t staticPtr,
                                 const ManagerLayout& layout,
                                 std::vector<Entity>& out) {
    if (layout.arrayOffset == 0 && layout.countOffset == 0) return; // layout henüz bilinmiyor

    uintptr_t mgr = Memory::Deref(staticPtr);
    if (!Memory::IsValidPtr(mgr)) return;

    int32_t count = Memory::Read<int32_t>(mgr + layout.countOffset);
    if (count <= 0 || count > layout.maxCount) return;

    uintptr_t arrBase = Memory::Read<uintptr_t>(mgr + layout.arrayOffset);
    if (!Memory::IsValidPtr(arrBase)) return;

    for (int i = 0; i < count; ++i) {
        uintptr_t entPtr = Memory::Read<uintptr_t>(arrBase + static_cast<uintptr_t>(i) * sizeof(uintptr_t));
        if (!Memory::IsValidPtr(entPtr)) continue;

        Entity e;
        e.base  = entPtr;
        e.index = i;
        if (e.ID() <= 0) continue;
        out.push_back(e);
    }
}

void EntityManager::Update() {
    entities_.clear();
    players_.clear();
    monsters_.clear();

    // Player manager (FLDB)
    ScanManager(Offsets::Static::FLDB, playerMgrLayout, entities_);

    // Monster/NPC manager (SMMB) — ayrı scan, duplicate ID kontrol et
    std::vector<Entity> monsterRaw;
    ScanManager(Offsets::Static::SMMB, monsterMgrLayout, monsterRaw);
    for (auto& e : monsterRaw) {
        bool dup = false;
        for (auto& ex : entities_) if (ex.ID() == e.ID()) { dup = true; break; }
        if (!dup) entities_.push_back(e);
    }

    // Kategorize
    for (auto& e : entities_) {
        if (e.IsNpcByVTable()) monsters_.push_back(e);
        else                   players_.push_back(e);
    }
}

const Entity* EntityManager::NearestMonster(float px, float py, float maxRange) const {
    const Entity* best = nullptr;
    float bestDist = maxRange;
    for (const auto& e : monsters_) {
        if (!e.IsAlive()) continue;
        float d = e.DistanceTo(px, py);
        if (d < bestDist) { bestDist = d; best = &e; }
    }
    return best;
}

const Entity* EntityManager::FindByID(int32_t id) const {
    for (const auto& e : entities_)
        if (e.ID() == id) return &e;
    return nullptr;
}

} // namespace KO
