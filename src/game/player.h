#pragma once
#include <cstdint>

namespace KO {

struct PlayerData {
    char  name[33];
    int   level;
    int   hp, maxHp;
    int   mp, maxMp;
    int   gold;
    float x, y;
    bool  valid;
};

// PTR_CHR adresindeki pointer'ı dereference edip player verilerini okur.
// Oyun menüsünde / karakter seçim ekranında valid=false döner.
PlayerData ReadPlayer();

} // namespace KO
