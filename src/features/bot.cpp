#include "bot.h"
#include "autoheal.h"
#include "autoskill.h"
#include "../game/player.h"
#include "../game/entity.h"
#include "../network/packet.h"

namespace KO::Features {

void Bot::Start() { running_ = true; }
void Bot::Stop()  { running_ = false; currentTargetID_ = -1; }

void Bot::SelectTarget() {
    auto& em = EntityManager::Get();
    auto& p  = Player::Get();
    if (!p.Valid()) return;

    const Entity* target = em.NearestMonster(p.X(), p.Y(), cfg.attackRange);
    if (!target) {
        currentTargetID_ = -1;
        return;
    }

    if (target->ID() == currentTargetID_) return; // zaten seçili

    currentTargetID_ = target->ID();

    Packet pkt(Opcode::SELECT_TARGET);
    pkt.Write<int32_t>(currentTargetID_);
    SendToServer(pkt);
}

void Bot::Tick() {
    if (!running_) return;

    // 1. Player & entity state güncelle
    Player::Update();
    EntityManager::Get().Update();

    auto& p = Player::Get();
    if (!p.Valid() || !p.IsAlive()) return;

    // 2. Auto heal (her zaman çalışır)
    AutoHeal::Get().Tick();

    if (!cfg.enabled) return;

    // 3. Hedef seç
    SelectTarget();

    // 4. Hedefe saldır / skill kullan
    AutoSkill::Get().Tick(currentTargetID_);
}

} // namespace KO::Features
