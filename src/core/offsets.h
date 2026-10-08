#pragma once
#include <cstdint>

// Knight Online 2626 - tüm offsetler
// Module base: 0x00400000 (ASLR yok)
// RVA = absolute - 0x00400000

namespace KO::Offsets {

    // -----------------------------------------------------------------------
    // Statik pointer adresleri (doğrudan dereference edilir)
    // -----------------------------------------------------------------------
    namespace Static {
        constexpr uintptr_t PTR_CHR     = 0x01115574; // → local player object*
        constexpr uintptr_t PTR_PKT     = 0x01115654; // → CGameSocket* (send için)
        constexpr uintptr_t PTR_DLG     = 0x0111563C; // → dialog/UI manager*
        constexpr uintptr_t PTR_RECV1   = 0x01115650; // → recv socket*
        constexpr uintptr_t SMMB        = 0x0111555C; // → seçili monster / yakın entity mgr*
        constexpr uintptr_t FLDB        = 0x01115580; // → field/player manager*  (=KO_PLAYER_MANAGER_RVA+base)
        constexpr uintptr_t ITOB        = 0x01115544; // → item object base*
        constexpr uintptr_t GameProcIntro       = 0x0111562C;
        constexpr uintptr_t CGameProcIntroChrSel= 0x01115640;
    }

    // -----------------------------------------------------------------------
    // Fonksiyon adresleri (absolute, __thiscall)
    // -----------------------------------------------------------------------
    namespace Fn {
        constexpr uintptr_t SendPacket      = 0x007032E0; // CGameSocket::Send
        constexpr uintptr_t RecvPacket      = 0x0084C700; // CGameSocket::Recv
        constexpr uintptr_t FNSB            = 0x00503BE0; // FindNear / utility
        constexpr uintptr_t FMBS            = 0x0050DF80; // FindMonsterBy / utility
        constexpr uintptr_t FxSetBundlePos  = 0x008BBA40;
        constexpr uintptr_t AreaSkillMouseMove = 0x00804990;
        constexpr uintptr_t CameraHook      = 0x007A904A;
    }

    // RVA fonksiyonlar (base + RVA ile çağır)
    namespace FnRVA {
        constexpr uintptr_t CharGetByID     = 0x0010DF80; // (this=FLDB, id) → entity*
        constexpr uintptr_t ScaleSet        = 0x000E2B90;
        constexpr uintptr_t DeathRenderTick = 0x0024052E;
        constexpr uintptr_t DeathTickJoints = 0x00242760;
        constexpr uintptr_t FPS_SecPerFrame = 0x00D8B5E0;
    }

    // -----------------------------------------------------------------------
    // Entity / Player struct offset'leri (player.base veya entity.base'den)
    // -----------------------------------------------------------------------
    namespace Ent {
        constexpr uintptr_t ID          = 0x000006A0; // int32
        constexpr uintptr_t Name        = 0x000006A4; // char[32]
        constexpr uintptr_t Nation      = 0x000006C4; // int32 — 1=Karus,2=ElMorad,100=NPC
        constexpr uintptr_t Race        = 0x000006C0; // int32 — 100=NPC/Monster
        constexpr uintptr_t Class       = 0x000006CC; // int32
        constexpr uintptr_t Level       = 0x000006D0; // int32
        constexpr uintptr_t MaxHP       = 0x000006D4; // int32
        constexpr uintptr_t HP          = 0x000006D8; // int32
        constexpr uintptr_t MaxMP       = 0x00000BEC; // int32
        constexpr uintptr_t MP          = 0x00000BF0; // int32
        constexpr uintptr_t Gold        = 0x00000BFC; // int32
        constexpr uintptr_t PosX        = 0x000003CC; // float
        constexpr uintptr_t PosY        = 0x000003D4; // float
        constexpr uintptr_t PosZ        = 0x00000194; // float (height)
        constexpr uintptr_t Target      = 0x00000660; // int32 (target entity ID)
        constexpr uintptr_t ChrOff      = 0x00000358; // → sub-character object*
    }

    // -----------------------------------------------------------------------
    // Skill struct offset'leri (skill slot object'inden)
    // -----------------------------------------------------------------------
    namespace Skill {
        constexpr uintptr_t ID          = 0x00000010; // uint32
        constexpr uintptr_t SelfAni     = 0x00000060;
        constexpr uintptr_t CastTime    = 0x000000A8; // ms
        constexpr uintptr_t Cooldown    = 0x000000AC; // ms
        constexpr uintptr_t SuccessRate = 0x000000BC;
        constexpr uintptr_t BlindSilence= 0x000000C0;
        constexpr uintptr_t Range       = 0x000000C8; // float
    }

    // -----------------------------------------------------------------------
    // Scale offsets (entity base'den)
    // -----------------------------------------------------------------------
    namespace Scale {
        constexpr uintptr_t X = 0x00000068;
        constexpr uintptr_t Y = 0x0000006C;
        constexpr uintptr_t Z = 0x00000070;
    }

    // -----------------------------------------------------------------------
    // Camera
    // -----------------------------------------------------------------------
    namespace Camera {
        constexpr uintptr_t DistanceOff = 0x000001AC;
    }

    // -----------------------------------------------------------------------
    // GameProc magic / skill area
    // -----------------------------------------------------------------------
    namespace Magic {
        constexpr uintptr_t MgrOff          = 0x00000468; // GameProc → MagicMgr
        constexpr uintptr_t RegionStateOff  = 0x000000D0;
        constexpr uintptr_t RegionSkillOff  = 0x000000D4;
        constexpr uintptr_t RegionFxIDOff   = 0x000001BC;
        constexpr uintptr_t MouseSkillPosOff= 0x00000760;
    }

    // -----------------------------------------------------------------------
    // Sabitler
    // -----------------------------------------------------------------------
    constexpr int32_t RACE_NPC = 100;  // bu race değerine sahip entity NPC/monster

    // NPC vtable RVA (NPC mi player mi ayırt etmek için)
    constexpr uintptr_t NPC_VTABLE_RVA = 0x00C1E3D4;

} // namespace KO::Offsets
