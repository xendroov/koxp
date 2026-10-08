#pragma once
#include <cstdint>
#include <string>
#include "../core/offsets.h"
#include "../core/memory.h"

namespace KO {

struct Player {
    uintptr_t base = 0;

    bool Valid() const { return Memory::IsValidPtr(base); }

    // --- Temel stat okuma ---
    int32_t     ID()      const { return R<int32_t> (Offsets::Ent::ID);      }
    std::string Name()    const { return Memory::ReadString(base + Offsets::Ent::Name); }
    int32_t     Nation()  const { return R<int32_t> (Offsets::Ent::Nation);  }
    int32_t     Race()    const { return R<int32_t> (Offsets::Ent::Race);    }
    int32_t     Class()   const { return R<int32_t> (Offsets::Ent::Class);   }
    int32_t     Level()   const { return R<int32_t> (Offsets::Ent::Level);   }
    int32_t     HP()      const { return R<int32_t> (Offsets::Ent::HP);      }
    int32_t     MaxHP()   const { return R<int32_t> (Offsets::Ent::MaxHP);   }
    int32_t     MP()      const { return R<int32_t> (Offsets::Ent::MP);      }
    int32_t     MaxMP()   const { return R<int32_t> (Offsets::Ent::MaxMP);   }
    int32_t     Gold()    const { return R<int32_t> (Offsets::Ent::Gold);    }
    float       X()       const { return R<float>   (Offsets::Ent::PosX);    }
    float       Y()       const { return R<float>   (Offsets::Ent::PosY);    }
    float       Z()       const { return R<float>   (Offsets::Ent::PosZ);    }
    int32_t     TargetID()const { return R<int32_t> (Offsets::Ent::Target);  }

    bool IsAlive() const { return Valid() && HP() > 0; }
    bool IsNpc()   const { return Race() == Offsets::RACE_NPC; }

    float HPPercent() const {
        int mxhp = MaxHP();
        return mxhp > 0 ? (static_cast<float>(HP()) / mxhp * 100.f) : 0.f;
    }
    float MPPercent() const {
        int mxmp = MaxMP();
        return mxmp > 0 ? (static_cast<float>(MP()) / mxmp * 100.f) : 0.f;
    }

    // --- Global instance ---
    static Player& Get() { static Player inst; return inst; }

    // Her frame başında çağır
    // KO_PTR_CHR → *KO_PTR_CHR = player object pointer
    static void Update() {
        auto& p = Get();
        p.base = Memory::Deref(Offsets::Static::PTR_CHR);
    }

private:
    template<typename T>
    T R(uintptr_t off) const {
        if (!Valid()) return T{};
        return Memory::Read<T>(base + off);
    }
};

} // namespace KO
