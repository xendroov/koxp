#include "player.h"
#include "../core/memory.h"
#include "../core/offsets.h"
#include <cstring>

namespace KO {

PlayerData ReadPlayer() {
    PlayerData d{};

    uintptr_t base = Mem::Deref(KO::Off::PTR_CHR);
    if (!base) return d;

    d.level = Mem::Read<int>(base + KO::Off::Chr::Level);
    d.hp    = Mem::Read<int>(base + KO::Off::Chr::HP);
    d.maxHp = Mem::Read<int>(base + KO::Off::Chr::MaxHP);
    d.mp    = Mem::Read<int>(base + KO::Off::Chr::MP);
    d.maxMp = Mem::Read<int>(base + KO::Off::Chr::MaxMP);
    d.gold  = Mem::Read<int>(base + KO::Off::Chr::Gold);
    d.x     = Mem::Read<float>(base + KO::Off::Chr::PosX);
    d.y     = Mem::Read<float>(base + KO::Off::Chr::PosY);

    // İsim: null-terminate güvenli kopyala
    __try {
        const char* src = reinterpret_cast<const char*>(base + KO::Off::Chr::Name);
        strncpy_s(d.name, sizeof(d.name), src, 32);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        d.name[0] = '\0';
    }

    // Basit geçerlilik kontrolü: HP/level makul aralıkta mı?
    d.valid = (d.level > 0 && d.level <= 200 &&
               d.hp > 0    && d.hp <= 500000 &&
               d.maxHp > 0);
    return d;
}

} // namespace KO
