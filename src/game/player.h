#pragma once
#include <cstdint>
#include <string>
#include "../core/offsets.h"
#include "../core/memory.h"

namespace KO {

struct Player {
    // player struct base (resolved pointer)
    uintptr_t base = 0;

    bool Valid() const { return Memory::IsValidPtr(base); }

    std::string Name()  const { return Valid() ? Memory::ReadString(base + Offsets::Player::Name)   : ""; }
    int32_t  HP()       const { return Valid() ? Memory::Read<int32_t>(base + Offsets::Player::HP)   : 0; }
    int32_t  MaxHP()    const { return Valid() ? Memory::Read<int32_t>(base + Offsets::Player::MaxHP): 1; }
    int32_t  MP()       const { return Valid() ? Memory::Read<int32_t>(base + Offsets::Player::MP)   : 0; }
    int32_t  MaxMP()    const { return Valid() ? Memory::Read<int32_t>(base + Offsets::Player::MaxMP): 1; }
    float    X()        const { return Valid() ? Memory::Read<float>(base + Offsets::Player::X)      : 0.f; }
    float    Y()        const { return Valid() ? Memory::Read<float>(base + Offsets::Player::Y)      : 0.f; }
    float    Z()        const { return Valid() ? Memory::Read<float>(base + Offsets::Player::Z)      : 0.f; }
    int32_t  Level()    const { return Valid() ? Memory::Read<int32_t>(base + Offsets::Player::Level): 0; }
    int32_t  TargetID() const { return Valid() ? Memory::Read<int32_t>(base + Offsets::Player::TargetID): -1; }
    bool     IsAlive()  const { return Valid() && Memory::Read<int32_t>(base + Offsets::Player::State) == 1; }

    float HPPercent() const { return static_cast<float>(HP()) / static_cast<float>(MaxHP()) * 100.f; }
    float MPPercent() const { return static_cast<float>(MP()) / static_cast<float>(MaxMP()) * 100.f; }

    // Global instance
    static Player& Get() {
        static Player instance;
        return instance;
    }

    // Her frame başında çağır
    static void Update() {
        auto& p = Get();
        // g_myInfo sabit adres ise: base = Memory::Resolve(Offsets::g_myInfo)
        // pointer ise:              base = Memory::Read<uintptr_t>(Memory::Resolve(Offsets::g_myInfo))
        p.base = Memory::Read<uintptr_t>(Memory::Resolve(Offsets::g_myInfo));
    }
};

} // namespace KO
