#include "PCH.h"
#include <unordered_map>
#include <chrono>
#include <spdlog/sinks/basic_file_sink.h>

namespace EquipLoopFix
{
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
                auto it = _lastStateChange.find(a_this->GetFormID());
                auto now = std::chrono::steady_clock::now();

                if (!a_draw) { // Sheathing attempt
                    if (it != _lastStateChange.end()) {
                        auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->second).count();
                        if (duration < 2000) {
                            SKSE::log::info("Blocked redundant sheathe for NPC {:X} ({}ms since last state change)", a_this->GetFormID(), duration);
                            return; 
                        }
                    }
                }
                _lastStateChange[a_this->GetFormID()] = now;
            }

            _DrawWeaponMagicHands(a_this, a_draw);
        }

        static inline REL::Relocation<decltype(DrawWeaponMagicHands)> _DrawWeaponMagicHands;
        static inline std::unordered_map<RE::FormID, std::chrono::steady_clock::time_point> _lastStateChange;
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
    .Version = { 1, 0, 0, 0 },
    .Name = "Combat Equipment State Fix",
    .Author = "Antigravity",
    .RuntimeCompatibility = { SKSE::VersionIndependence::AddressLibrary, true }
)

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    InitializeLog();
    SKSE::log::info("Combat Equipment State Fix loaded.");
    SKSE::Init(a_skse);
    EquipLoopFix::DrawWeaponHook::Install();
    return true;
}
