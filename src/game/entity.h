#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <cmath>
#include "../core/offsets.h"
#include "../core/memory.h"

namespace KO {

enum class EntityType : int32_t {
    Player  = 0,
    Monster = 1,
    Npc     = 2,
};

struct Entity {
    uintptr_t base = 0;
    int       index = -1;

    bool Valid()    const { return Memory::IsValidPtr(base); }
    int32_t  ID()   const { return Valid() ? Memory::Read<int32_t>(base + Offsets::Entity::ID)    : -1; }
    std::string Name() const { return Valid() ? Memory::ReadString(base + Offsets::Entity::Name) : ""; }
    int32_t  HP()   const { return Valid() ? Memory::Read<int32_t>(base + Offsets::Entity::HP)   : 0; }
    int32_t  MaxHP() const { return Valid() ? Memory::Read<int32_t>(base + Offsets::Entity::MaxHP):1; }
    float    X()    const { return Valid() ? Memory::Read<float>(base + Offsets::Entity::X)      : 0.f; }
    float    Y()    const { return Valid() ? Memory::Read<float>(base + Offsets::Entity::Y)      : 0.f; }
    float    Z()    const { return Valid() ? Memory::Read<float>(base + Offsets::Entity::Z)      : 0.f; }
    EntityType Type() const { return Valid() ? static_cast<EntityType>(Memory::Read<int32_t>(base + Offsets::Entity::Type)) : EntityType::Npc; }
    bool IsAlive()  const { return Valid() && Memory::Read<int32_t>(base + Offsets::Entity::State) == 1; }
    bool IsMonster() const { return Type() == EntityType::Monster; }

    float DistanceTo(float px, float py) const {
        float dx = X() - px, dy = Y() - py;
        return std::sqrtf(dx * dx + dy * dy);
    }
};

// --- EntityManager ---
class EntityManager {
public:
    static EntityManager& Get() {
        static EntityManager inst;
        return inst;
    }

    void Update();

    const std::vector<Entity>& All() const { return entities_; }

    // En yakın canlı monster (belirli range içinde)
    const Entity* NearestMonster(float px, float py, float maxRange = 2000.f) const;

private:
    std::vector<Entity> entities_;
};

} // namespace KO
