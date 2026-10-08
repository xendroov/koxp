#pragma once
#include <cstdint>

// Knight Online 2626 offsets
// CheatEngine ile scan ederek doldur

namespace KO::Offsets {

    // --- Global pointer'lar ---
    constexpr uintptr_t g_myInfo        = 0x00000000; // local player struct*
    constexpr uintptr_t g_entityList    = 0x00000000; // entity array base*
    constexpr uintptr_t g_entityCount   = 0x00000000; // nearby entity count
    constexpr uintptr_t g_gameState     = 0x00000000; // 0=menu, 1=select, 2=game

    // --- Packet fonksiyonları ---
    constexpr uintptr_t fn_SendPacket   = 0x00000000; // CGameSocket::Send
    constexpr uintptr_t fn_RecvPacket   = 0x00000000; // CGameSocket::Recv

    // --- Player struct offset'leri (g_myInfo base'inden) ---
    namespace Player {
        constexpr uintptr_t Name        = 0x00; // char[32]
        constexpr uintptr_t HP          = 0x00; // int32
        constexpr uintptr_t MaxHP       = 0x00; // int32
        constexpr uintptr_t MP          = 0x00; // int32
        constexpr uintptr_t MaxMP       = 0x00; // int32
        constexpr uintptr_t X           = 0x00; // float
        constexpr uintptr_t Y           = 0x00; // float
        constexpr uintptr_t Z           = 0x00; // float
        constexpr uintptr_t ZoneID      = 0x00; // int32
        constexpr uintptr_t Level       = 0x00; // int32
        constexpr uintptr_t State       = 0x00; // int32 - alive/dead/etc
        constexpr uintptr_t TargetID    = 0x00; // int32
        constexpr uintptr_t HpPotItem   = 0x00; // inventory slot index
        constexpr uintptr_t MpPotItem   = 0x00; // inventory slot index
    }

    // --- Entity struct offset'leri (entity array element'inden) ---
    namespace Entity {
        constexpr uintptr_t EntrySize   = 0x00; // her element'in byte boyutu
        constexpr uintptr_t ID          = 0x00; // int32
        constexpr uintptr_t Name        = 0x00; // char[32]
        constexpr uintptr_t HP          = 0x00; // int32
        constexpr uintptr_t MaxHP       = 0x00; // int32
        constexpr uintptr_t X           = 0x00; // float
        constexpr uintptr_t Y           = 0x00; // float
        constexpr uintptr_t Z           = 0x00; // float
        constexpr uintptr_t Type        = 0x00; // 0=player, 1=monster, 2=npc
        constexpr uintptr_t State       = 0x00; // alive/dead
        constexpr uintptr_t NationID    = 0x00; // nation/team id
    }

    // --- Skill bar / item slot offset'leri ---
    namespace Skills {
        constexpr uintptr_t SlotBase    = 0x00; // skill slot array
        constexpr uintptr_t SlotSize    = 0x00; // her slot'un boyutu
        constexpr uintptr_t SkillID     = 0x00; // skill ID offset per slot
        constexpr uintptr_t Cooldown    = 0x00; // skill cooldown offset
    }

} // namespace KO::Offsets
