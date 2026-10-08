#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <cmath>
#include "../core/offsets.h"
#include "../core/memory.h"

namespace KO {

struct Entity {
    uintptr_t base  = 0;
    int       index = -1;

    bool Valid() const { return Memory::IsValidPtr(base); }

    int32_t     ID()     const { return R<int32_t>(Offsets::Ent::ID);    }
    std::string Name()   const { return Memory::ReadString(base + Offsets::Ent::Name); }
    int32_t     HP()     const { return R<int32_t>(Offsets::Ent::HP);    }
    int32_t     MaxHP()  const { return R<int32_t>(Offsets::Ent::MaxHP); }
    int32_t     MP()     const { return R<int32_t>(Offsets::Ent::MP);    }
    int32_t     Race()   const { return R<int32_t>(Offsets::Ent::Race);  }
    int32_t     Nation() const { return R<int32_t>(Offsets::Ent::Nation);}
    int32_t     Level()  const { return R<int32_t>(Offsets::Ent::Level); }
    float       X()      const { return R<float>  (Offsets::Ent::PosX);  }
    float       Y()      const { return R<float>  (Offsets::Ent::PosY);  }
    float       Z()      const { return R<float>  (Offsets::Ent::PosZ);  }

    bool IsAlive()   const { return Valid() && HP() > 0; }
    bool IsNpc()     const { return Race() == Offsets::RACE_NPC; }
    bool IsPlayer()  const { return Valid() && !IsNpc(); }

    // NPC vtable kontrolü (daha güvenilir NPC tespiti)
    bool IsNpcByVTable() const {
        if (!Valid()) return false;
        uintptr_t vtbl = Memory::Read<uintptr_t>(base);
        return vtbl == (Memory::Base() + Offsets::NPC_VTABLE_RVA);
    }

    float DistanceTo(float px, float py) const {
        float dx = X() - px, dy = Y() - py;
        return std::sqrtf(dx * dx + dy * dy);
    }

    float HPPercent() const {
        int mxhp = MaxHP();
        return mxhp > 0 ? (static_cast<float>(HP()) / mxhp * 100.f) : 0.f;
    }

private:
    template<typename T>
    T R(uintptr_t off) const {
        if (!Valid()) return T{};
        return Memory::Read<T>(base + off);
    }
};

// -----------------------------------------------------------------------
// Entity Manager — FLDB (player manager) + SMMB (monster/npc manager)
// KO 2626: manager object pointer'ları static adreste.
// Her manager'ın içindeki entity array layout → TODO: debugger ile bul.
// -----------------------------------------------------------------------
class EntityManager {
public:
    static EntityManager& Get() { static EntityManager inst; return inst; }

    void Update();

    const std::vector<Entity>& All()     const { return entities_; }
    const std::vector<Entity>& Players() const { return players_;  }
    const std::vector<Entity>& Monsters()const { return monsters_; }

    // En yakın canlı NPC/monster
    const Entity* NearestMonster(float px, float py, float maxRange = 2000.f) const;

    // ID ile bul
    const Entity* FindByID(int32_t id) const;

    // -----------------------------------------------------------------------
    // Entity array layout içindeki offset'ler — debugger ile doğrula
    // FLDB veya SMMB dereference sonrası bu offset'ler kullanılır.
    // -----------------------------------------------------------------------
    struct ManagerLayout {
        uintptr_t arrayOffset = 0x00;  // manager + X = entity pointer array başı
        uintptr_t countOffset = 0x00;  // manager + Y = entity sayısı (int32)
        int       maxCount    = 300;   // güvenlik sınırı
    };
    ManagerLayout playerMgrLayout;
    ManagerLayout monsterMgrLayout;

private:
    std::vector<Entity> entities_;
    std::vector<Entity> players_;
    std::vector<Entity> monsters_;

    void ScanManager(uintptr_t staticPtr, const ManagerLayout& layout,
                     std::vector<Entity>& out);
};

} // namespace KO
