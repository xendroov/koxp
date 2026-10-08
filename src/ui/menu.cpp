#include "menu.h"
#include "../game/player.h"
#include "../game/entity.h"
#include "../features/bot.h"
#include "../features/autoheal.h"
#include "../features/autoskill.h"
#include "../core/hotkeys.h"
#include <imgui.h>
#include <string>

namespace KO::UI {

// ------------------------------------------------------------------ style ---
void Menu::ApplyStyle() {
    ImGuiStyle& s = ImGui::GetStyle();
    s.WindowRounding   = 6.f;
    s.FrameRounding    = 4.f;
    s.ScrollbarRounding= 4.f;
    s.GrabRounding     = 4.f;
    s.FramePadding     = {6, 4};
    s.ItemSpacing      = {8, 5};
    s.WindowBorderSize = 1.f;
    s.FrameBorderSize  = 0.f;

    ImVec4* c = s.Colors;
    c[ImGuiCol_WindowBg]          = {0.10f, 0.10f, 0.12f, 0.92f};
    c[ImGuiCol_TitleBg]           = {0.13f, 0.13f, 0.17f, 1.00f};
    c[ImGuiCol_TitleBgActive]     = {0.16f, 0.16f, 0.22f, 1.00f};
    c[ImGuiCol_Header]            = {0.20f, 0.45f, 0.80f, 0.40f};
    c[ImGuiCol_HeaderHovered]     = {0.20f, 0.45f, 0.80f, 0.65f};
    c[ImGuiCol_HeaderActive]      = {0.20f, 0.45f, 0.80f, 0.90f};
    c[ImGuiCol_Button]            = {0.20f, 0.40f, 0.70f, 0.50f};
    c[ImGuiCol_ButtonHovered]     = {0.20f, 0.45f, 0.80f, 0.80f};
    c[ImGuiCol_ButtonActive]      = {0.20f, 0.50f, 0.90f, 1.00f};
    c[ImGuiCol_FrameBg]           = {0.16f, 0.16f, 0.20f, 0.80f};
    c[ImGuiCol_FrameBgHovered]    = {0.22f, 0.22f, 0.28f, 0.90f};
    c[ImGuiCol_SliderGrab]        = {0.30f, 0.60f, 0.90f, 1.00f};
    c[ImGuiCol_SliderGrabActive]  = {0.40f, 0.70f, 1.00f, 1.00f};
    c[ImGuiCol_CheckMark]         = {0.40f, 0.80f, 0.40f, 1.00f};
    c[ImGuiCol_Text]              = {0.90f, 0.90f, 0.90f, 1.00f};
    c[ImGuiCol_TextDisabled]      = {0.50f, 0.50f, 0.50f, 1.00f};
    c[ImGuiCol_Separator]         = {0.30f, 0.30f, 0.40f, 0.80f};
    c[ImGuiCol_Border]            = {0.25f, 0.25f, 0.35f, 1.00f};
    c[ImGuiCol_PopupBg]           = {0.12f, 0.12f, 0.16f, 0.96f};
}

// --------------------------------------------------------------- helpers ---
void Menu::ToggleButton(const char* label, bool& state, const char* onText, const char* offText) {
    if (state) ImGui::PushStyleColor(ImGuiCol_Button, {0.15f, 0.55f, 0.15f, 0.85f});
    else        ImGui::PushStyleColor(ImGuiCol_Button, {0.50f, 0.15f, 0.15f, 0.85f});

    if (ImGui::Button(state ? onText : offText, {50, 22})) state = !state;
    ImGui::PopStyleColor();
    ImGui::SameLine();
    ImGui::Text("%s", label);
}

void Menu::HpBar(float pct) {
    ImVec4 col = (pct > 60.f) ? ImVec4{0.15f, 0.70f, 0.15f, 1.f}
               : (pct > 30.f) ? ImVec4{0.85f, 0.75f, 0.10f, 1.f}
                               : ImVec4{0.85f, 0.15f, 0.10f, 1.f};
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, col);
    char buf[24]; snprintf(buf, sizeof(buf), "HP %.0f%%", pct);
    ImGui::ProgressBar(pct / 100.f, {-1, 14}, buf);
    ImGui::PopStyleColor();
}

void Menu::MpBar(float pct) {
    ImGui::PushStyleColor(ImGuiCol_PlotHistogram, ImVec4{0.15f, 0.40f, 0.85f, 1.f});
    char buf[24]; snprintf(buf, sizeof(buf), "MP %.0f%%", pct);
    ImGui::ProgressBar(pct / 100.f, {-1, 14}, buf);
    ImGui::PopStyleColor();
}

// --------------------------------------------------------- player panel ---
void Menu::DrawPlayerPanel() {
    auto& p = Player::Get();

    ImGui::PushStyleColor(ImGuiCol_Header, {0.18f, 0.18f, 0.25f, 1.f});
    if (ImGui::CollapsingHeader("  Karakter", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();
        if (p.Valid()) {
            ImGui::Text("%-10s  Lv.%-3d  %s",
                p.Name().c_str(), p.Level(),
                p.IsAlive() ? "" : "[DEAD]");
            ImGui::Spacing();
            HpBar(p.HPPercent());
            MpBar(p.MPPercent());

            // Hedef
            auto& em = EntityManager::Get();
            int32_t tid = p.TargetID();
            const Entity* tgt = nullptr;
            for (auto& e : em.All())
                if (e.ID() == tid) { tgt = &e; break; }

            ImGui::Spacing();
            if (tgt && tgt->Valid())
                ImGui::Text("Hedef: %s  HP:%d/%d", tgt->Name().c_str(), tgt->HP(), tgt->MaxHP());
            else
                ImGui::TextDisabled("Hedef: -");
        } else {
            ImGui::TextDisabled("Oyuna gir...");
        }
        ImGui::Spacing();
    }
    ImGui::PopStyleColor();
}

// ------------------------------------------------------- features panel ---
void Menu::DrawFeaturesPanel() {
    auto& bot   = Features::Bot::Get();
    auto& heal  = Features::AutoHeal::Get();
    auto& skill = Features::AutoSkill::Get();

    ImGui::PushStyleColor(ImGuiCol_Header, {0.18f, 0.18f, 0.25f, 1.f});
    if (ImGui::CollapsingHeader("  Ozellikler", ImGuiTreeNodeFlags_DefaultOpen)) {
        ImGui::Spacing();

        // --- Bot ---
        ImGui::PushID("bot");
        ToggleButton("[F7] Bot", bot.cfg.enabled);
        if (bot.cfg.enabled && !bot.IsRunning()) bot.Start();
        if (!bot.cfg.enabled && bot.IsRunning())  bot.Stop();

        ImGui::Indent(10.f);
        ImGui::SliderFloat("Menzil", &bot.cfg.attackRange, 500.f, 5000.f, "%.0f");
        ImGui::Unindent(10.f);
        ImGui::PopID();

        ImGui::Separator();

        // --- AutoHeal ---
        ImGui::PushID("heal");
        ToggleButton("[F8] AutoHeal", heal.cfg.enabled);
        ImGui::Indent(10.f);
        ImGui::SliderFloat("HP Pot %", &heal.cfg.hpThreshold, 10.f, 95.f, "%.0f%%");
        ImGui::SliderFloat("MP Pot %", &heal.cfg.mpThreshold, 10.f, 95.f, "%.0f%%");
        ImGui::SliderInt("HP Gecikme",  &heal.cfg.hpPotDelay, 500, 5000, "%d ms");
        ImGui::SliderInt("MP Gecikme",  &heal.cfg.mpPotDelay, 500, 5000, "%d ms");
        ImGui::Unindent(10.f);
        ImGui::PopID();

        ImGui::Separator();

        // --- AutoSkill ---
        ImGui::PushID("skill");
        ToggleButton("[F9] AutoSkill", skill.cfg.enabled);
        ImGui::Indent(10.f);
        ImGui::SliderInt("Saldiri Aralik", &skill.cfg.attackDelayMs, 200, 3000, "%d ms");
        ImGui::Checkbox("Skill Kullan", &skill.cfg.useSkills);

        // Skill slot listesi
        if (ImGui::BeginTable("skills", 3,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableSetupColumn("Slot",  0, 36.f);
            ImGui::TableSetupColumn("ID",    0, 70.f);
            ImGui::TableSetupColumn("CD(ms)",0, 70.f);
            ImGui::TableHeadersRow();

            for (int i = 0; i < static_cast<int>(skill.cfg.skills.size()); ++i) {
                auto& sl = skill.cfg.skills[i];
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("%d", sl.slotIndex);
                ImGui::TableSetColumnIndex(1); ImGui::Text("0x%04X", sl.skillID);
                ImGui::TableSetColumnIndex(2); ImGui::Text("%d", sl.cooldownMs);
            }
            ImGui::EndTable();
        }

        if (ImGui::Button("+ Skill Ekle")) {
            skill.cfg.skills.push_back({(int)skill.cfg.skills.size(), 0, 2000});
        }
        ImGui::Unindent(10.f);
        ImGui::PopID();

        ImGui::Spacing();
    }
    ImGui::PopStyleColor();
}

// ---------------------------------------------------- entity debug panel ---
void Menu::DrawEntityDebugPanel() {
    auto& em = EntityManager::Get();

    ImGui::PushStyleColor(ImGuiCol_Header, {0.18f, 0.18f, 0.25f, 1.f});
    if (ImGui::CollapsingHeader("  Entity Debug")) {
        ImGui::Spacing();

        // Manager layout ayarları
        auto& pml = em.playerMgrLayout;
        auto& mml = em.monsterMgrLayout;

        ImGui::Text("Player Manager (FLDB):");
        ImGui::PushID("pml");
        ImGui::InputScalar("Array Off", ImGuiDataType_U32, &pml.arrayOffset,
                           nullptr, nullptr, "0x%08X", ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::InputScalar("Count Off", ImGuiDataType_U32, &pml.countOffset,
                           nullptr, nullptr, "0x%08X", ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::PopID();

        ImGui::Separator();
        ImGui::Text("Monster Manager (SMMB):");
        ImGui::PushID("mml");
        ImGui::InputScalar("Array Off", ImGuiDataType_U32, &mml.arrayOffset,
                           nullptr, nullptr, "0x%08X", ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::InputScalar("Count Off", ImGuiDataType_U32, &mml.countOffset,
                           nullptr, nullptr, "0x%08X", ImGuiInputTextFlags_CharsHexadecimal);
        ImGui::PopID();

        ImGui::Separator();
        ImGui::Text("Entities: %d  Players: %d  Monsters: %d",
            (int)em.All().size(), (int)em.Players().size(), (int)em.Monsters().size());

        if (ImGui::BeginTable("ents", 5,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg |
            ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit,
            {0, 120}))
        {
            ImGui::TableSetupColumn("ID",   0, 50.f);
            ImGui::TableSetupColumn("Name", 0, 80.f);
            ImGui::TableSetupColumn("HP",   0, 70.f);
            ImGui::TableSetupColumn("Race", 0, 40.f);
            ImGui::TableSetupColumn("Dist", 0, 50.f);
            ImGui::TableHeadersRow();

            auto& p = Player::Get();
            for (const auto& e : em.All()) {
                ImGui::TableNextRow();
                ImGui::TableSetColumnIndex(0); ImGui::Text("%d", e.ID());
                ImGui::TableSetColumnIndex(1); ImGui::Text("%.12s", e.Name().c_str());
                ImGui::TableSetColumnIndex(2); ImGui::Text("%d/%d", e.HP(), e.MaxHP());
                ImGui::TableSetColumnIndex(3); ImGui::Text("%d", e.Race());
                ImGui::TableSetColumnIndex(4);
                if (p.Valid()) ImGui::Text("%.0f", e.DistanceTo(p.X(), p.Y()));
            }
            ImGui::EndTable();
        }
        ImGui::Spacing();
    }
    ImGui::PopStyleColor();
}

// --------------------------------------------------------- hotkey panel ---
void Menu::DrawHotkeyPanel() {
    auto& hkm = HotkeyManager::Get();

    ImGui::PushStyleColor(ImGuiCol_Header, {0.18f, 0.18f, 0.25f, 1.f});
    if (ImGui::CollapsingHeader("  Kisayollar (F1-F12)")) {
        ImGui::Spacing();
        if (ImGui::BeginTable("hk", 4,
            ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableSetupColumn("Tus",   0, 36.f);
            ImGui::TableSetupColumn("Atama", 0, 100.f);
            ImGui::TableSetupColumn("Tus",   0, 36.f);
            ImGui::TableSetupColumn("Atama", 0, 100.f);
            ImGui::TableHeadersRow();

            for (int i = 0; i < HK_COUNT; i += 2) {
                ImGui::TableNextRow();
                for (int col = 0; col < 2; ++col) {
                    int idx = i + col;
                    if (idx >= HK_COUNT) break;
                    auto& sl = hkm.Slot(idx);

                    ImGui::TableSetColumnIndex(col * 2);
                    ImGui::PushStyleColor(ImGuiCol_Text, {0.60f, 0.80f, 1.00f, 1.f});
                    ImGui::Text("F%d", idx + 1);
                    ImGui::PopStyleColor();

                    ImGui::TableSetColumnIndex(col * 2 + 1);
                    if (sl.label.empty())
                        ImGui::TextDisabled("-");
                    else
                        ImGui::Text("%s", sl.label.c_str());
                }
            }
            ImGui::EndTable();
        }
        ImGui::Spacing();
    }
    ImGui::PopStyleColor();
}

// --------------------------------------------------------- status bar ---
void Menu::DrawStatusBar() {
    auto& bot   = Features::Bot::Get();
    auto& heal  = Features::AutoHeal::Get();
    auto& skill = Features::AutoSkill::Get();

    auto dot = [](bool on) -> const char* { return on ? "[ON] " : "[OFF]"; };

    ImGui::Separator();
    ImGui::PushStyleColor(ImGuiCol_Text, {0.50f, 0.50f, 0.60f, 1.f});
    ImGui::Text("Bot:%s  Heal:%s  Skill:%s",
        dot(bot.cfg.enabled), dot(heal.cfg.enabled), dot(skill.cfg.enabled));
    ImGui::PopStyleColor();
}

// ------------------------------------------------------------- render ---
void Menu::Render() {
    // Her zaman ekranın sol üstünde, sabit boyut
    const ImGuiIO& io = ImGui::GetIO();
    ImGui::SetNextWindowPos({10, 10}, ImGuiCond_Always);
    ImGui::SetNextWindowSize({310, 0}, ImGuiCond_Always);
    ImGui::SetNextWindowBgAlpha(0.92f);

    ImGuiWindowFlags flags =
        ImGuiWindowFlags_NoMove |
        ImGuiWindowFlags_NoResize |
        ImGuiWindowFlags_NoCollapse |
        ImGuiWindowFlags_NoBringToFrontOnFocus |
        ImGuiWindowFlags_AlwaysAutoResize;

    if (!ImGui::Begin("KOXP  v1.0  |  KO 2626", nullptr, flags)) {
        ImGui::End();
        return;
    }

    DrawPlayerPanel();
    DrawFeaturesPanel();
    DrawEntityDebugPanel();
    DrawHotkeyPanel();
    DrawStatusBar();

    ImGui::End();
}

} // namespace KO::UI
