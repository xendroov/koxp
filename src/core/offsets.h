#pragma once
#include <cstdint>

// Knight Online 2626 — static base 0x00400000, ASLR yok
namespace KO::Off {

    // PTR_CHR: bu adresteki 4 byte → player object base pointer
    constexpr uintptr_t PTR_CHR  = 0x01115574;

    // Player object offset'leri (base'den)
    namespace Chr {
        constexpr uintptr_t Name   = 0x6A4;  // char[32]
        constexpr uintptr_t Level  = 0x6D0;  // int32
        constexpr uintptr_t MaxHP  = 0x6D4;  // int32
        constexpr uintptr_t HP     = 0x6D8;  // int32
        constexpr uintptr_t MaxMP  = 0xBEC;  // int32
        constexpr uintptr_t MP     = 0xBF0;  // int32
        constexpr uintptr_t Gold   = 0xBFC;  // int32
        constexpr uintptr_t PosX   = 0x3CC;  // float
        constexpr uintptr_t PosY   = 0x3D4;  // float
    }
}
