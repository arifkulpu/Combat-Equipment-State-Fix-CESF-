#include "PCH.h"
#include <unordered_map>
#include <chrono>
#include <spdlog/sinks/basic_file_sink.h>

namespace EquipLoopFix
{
    // NPC başına tutulan durum bilgisi
    struct NPCState
    {
        std::chrono::steady_clock::time_point lastStateChange{};
        bool lastWasBow{ false };   // Kınına sokan önceki silah yay mıydı?
        bool initialized{ false };
    };

    // Silah tipinin yay/arbalet olup olmadığını döndürür
    static bool IsBowType(RE::WEAPON_TYPE a_type)
    {
        return a_type == RE::WEAPON_TYPE::kBow ||
               a_type == RE::WEAPON_TYPE::kCrossbow;
    }

    // NPC'nin şu an elindeki silahın tipini döndürür
    // Sağ el → Sol el → Yok (yakın dövüş varsayımı) sıralamasıyla kontrol eder
    static bool IsCurrentlyHoldingBow(RE::Actor* a_actor)
    {
        // Sağ el
        const auto* rightData = a_actor->GetEquippedEntryData(false);
        if (rightData && rightData->object) {
            const auto* weap = rightData->object->As<RE::TESObjectWEAP>();
            if (weap && IsBowType(weap->GetWeaponType())) {
                return true;
            }
        }
        // Sol el (çift-el durumları için)
        const auto* leftData = a_actor->GetEquippedEntryData(true);
        if (leftData && leftData->object) {
            const auto* weap = leftData->object->As<RE::TESObjectWEAP>();
            if (weap && IsBowType(weap->GetWeaponType())) {
                return true;
            }
        }
        return false;
    }

    class DrawWeaponHook
    {
    public:
        static void Install()
        {
            InstallHook(RE::VTABLE_Actor[0], "Actor");
            InstallHook(RE::VTABLE_Character[0], "Character");
            SKSE::log::info("Combat Equipment State Fix: Hooks installed.");
        }

    private:
        static void InstallHook(REL::VariantID a_id, const char* a_name)
        {
            REL::Relocation<uintptr_t> vtbl(a_id);
            _DrawWeaponMagicHands = vtbl.write_vfunc(REL::Relocate(0xA6, 0xA6, 0xA8), DrawWeaponMagicHands);
            SKSE::log::info("Hooked {} vtable.", a_name);
        }

        static void DrawWeaponMagicHands(RE::Actor* a_this, bool a_draw)
        {
            if (a_this && !a_this->IsPlayerRef() && a_this->IsInCombat()) {
                auto& state = _npcState[a_this->GetFormID()];
                auto now = std::chrono::steady_clock::now();

                if (!a_draw) {  // Kınına sokma girişimi
                    bool currentlyBow = IsCurrentlyHoldingBow(a_this);

                    if (state.initialized) {
                        // Silah tipi değişiyor mu? (yay → yakın veya yakın → yay)
                        // Bu meşru bir AI silah değişimidir — engelleme
                        bool weaponTypeChanging = (currentlyBow != state.lastWasBow);

                        if (weaponTypeChanging) {
                            SKSE::log::info(
                                "Allowed weapon switch ({} -> {}) for NPC {:X}",
                                state.lastWasBow ? "bow" : "melee",
                                currentlyBow  ? "bow" : "melee",
                                a_this->GetFormID()
                            );
                            // Geçişe izin ver — state güncelle ve devam et
                            state.lastStateChange = now;
                            state.lastWasBow = currentlyBow;
                        } else {
                            // Aynı silah tipiyle hızlı kınına sokma → LOOP
                            auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(
                                now - state.lastStateChange).count();

                            if (duration < 2000) {
                                SKSE::log::info(
                                    "Blocked redundant sheathe for NPC {:X} ({}ms, weapon:{})",
                                    a_this->GetFormID(), duration,
                                    currentlyBow ? "bow" : "melee"
                                );
                                return;  // Engelle
                            }
                            state.lastStateChange = now;
                            state.lastWasBow = currentlyBow;
                        }
                    } else {
                        // İlk kez görülen NPC — state başlat
                        state.initialized = true;
                        state.lastStateChange = now;
                        state.lastWasBow = currentlyBow;
                    }
                }
                // Silah çekme (a_draw == true) → state sadece güncellenir, hiç engellenmez
                else {
                    bool currentlyBow = IsCurrentlyHoldingBow(a_this);
                    state.lastStateChange = now;
                    state.lastWasBow = currentlyBow;
                    state.initialized = true;
                }
            }

            _DrawWeaponMagicHands(a_this, a_draw);
        }

        static inline REL::Relocation<decltype(DrawWeaponMagicHands)> _DrawWeaponMagicHands;
        static inline std::unordered_map<RE::FormID, NPCState> _npcState;
    };
}

void InitializeLog()
{
    auto path = SKSE::log::log_directory();
    if (!path) return;
    *path /= "CombatEquipFix.log"sv;
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
    auto log = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
    log->set_level(spdlog::level::info);
    log->flush_on(spdlog::level::info);
    spdlog::set_default_logger(std::move(log));
    spdlog::set_pattern("[%l] %v"s);
}

SKSEPluginInfo(
    .Version = { 1, 1, 0, 0 },
    .Name = "Combat Equipment State Fix",
    .Author = "Antigravity",
    .RuntimeCompatibility = { SKSE::VersionIndependence::AddressLibrary, true }
)

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    InitializeLog();
    SKSE::log::info("Combat Equipment State Fix v1.1.0 loaded.");
    SKSE::Init(a_skse);
    EquipLoopFix::DrawWeaponHook::Install();
    return true;
}
