#include "PCH.h"
#include <unordered_map>
#include <chrono>
#include <filesystem>
#include <fstream>
#include <string>
#include <spdlog/sinks/basic_file_sink.h>

// ---------------------------------------------------------------------------
// Basit INI okuyucu — Windows API'ye bağımlılık olmadan
// ---------------------------------------------------------------------------
static int ReadINIInt(const std::filesystem::path& a_path,
                      std::string_view a_section,
                      std::string_view a_key,
                      int              a_default)
{
    std::ifstream file(a_path);
    if (!file.is_open())
        return a_default;

    std::string currentSection;
    std::string line;
    const std::string section = std::string("[") + std::string(a_section) + "]";

    while (std::getline(file, line)) {
        // Boşlukları temizle
        auto trim = [](std::string s) {
            size_t start = s.find_first_not_of(" \t\r\n");
            size_t end   = s.find_last_not_of(" \t\r\n");
            return (start == std::string::npos) ? std::string{} : s.substr(start, end - start + 1);
        };
        line = trim(line);

        if (line.empty() || line[0] == ';' || line[0] == '#')
            continue;

        if (line[0] == '[') {
            currentSection = line;
            continue;
        }

        if (currentSection != section)
            continue;

        auto eq = line.find('=');
        if (eq == std::string::npos)
            continue;

        std::string key = trim(line.substr(0, eq));
        std::string val = trim(line.substr(eq + 1));
        // Yorum varsa kes
        auto semi = val.find(';');
        if (semi != std::string::npos)
            val = trim(val.substr(0, semi));

        if (key == std::string(a_key)) {
            try { return std::stoi(val); }
            catch (...) { return a_default; }
        }
    }
    return a_default;
}

// ---------------------------------------------------------------------------
// Ayarlar — Data/SKSE/Plugins/CombatEquipFix.ini dosyasından okunur
// ---------------------------------------------------------------------------
namespace Settings
{
    // Savaş öncesi silah döngüsü düzeltmesi (varsayılan: KAPALI)
    bool bFixPreCombatLoop = false;

    // Döngü tespiti için pencere süresi (saniye)
    int iPreCombatWindowSec = 5;

    // Pencere içinde kaç kınına sokma olursa döngü sayılır
    int iPreCombatThreshold = 3;

    // Döngü tespit edilince kaç saniye engellenir
    int iPreCombatBlockSec = 10;

    void Load()
    {
        const auto iniPath = std::filesystem::absolute("Data\\SKSE\\Plugins\\CombatEquipFix.ini");

        bFixPreCombatLoop   = ReadINIInt(iniPath, "General", "bFixPreCombatLoop",   0) != 0;
        iPreCombatWindowSec = ReadINIInt(iniPath, "General", "iPreCombatWindowSec", 5);
        iPreCombatBlockSec  = ReadINIInt(iniPath, "General", "iPreCombatBlockSec", 10);
        iPreCombatThreshold = ReadINIInt(iniPath, "General", "iPreCombatThreshold",  3);

        SKSE::log::info("Settings loaded: bFixPreCombatLoop={} threshold={} window={}s block={}s",
            bFixPreCombatLoop, iPreCombatThreshold, iPreCombatWindowSec, iPreCombatBlockSec);
    }
}

namespace EquipLoopFix
{
    // NPC başına tutulan durum bilgisi
    struct NPCState
    {
        // --- Savaş içi döngü takibi ---
        std::chrono::steady_clock::time_point lastStateChange{};
        bool lastWasBow   { false };
        bool initialized  { false };

        // --- Savaş öncesi döngü takibi ---
        std::chrono::steady_clock::time_point preCombatWindowStart{};
        int  preCombatSheatheCount{ 0 };
        std::chrono::steady_clock::time_point preCombatBlockUntil{};
    };

    // Silah tipinin yay/arbalet olup olmadığını döndürür
    static bool IsBowType(RE::WEAPON_TYPE a_type)
    {
        return a_type == RE::WEAPON_TYPE::kBow ||
               a_type == RE::WEAPON_TYPE::kCrossbow;
    }

    // NPC'nin şu an elinde yay/arbalet tutup tutmadığını döndürür
    static bool IsCurrentlyHoldingBow(RE::Actor* a_actor)
    {
        const auto* rightData = a_actor->GetEquippedEntryData(false);
        if (rightData && rightData->object) {
            const auto* weap = rightData->object->As<RE::TESObjectWEAP>();
            if (weap && IsBowType(weap->GetWeaponType()))
                return true;
        }
        const auto* leftData = a_actor->GetEquippedEntryData(true);
        if (leftData && leftData->object) {
            const auto* weap = leftData->object->As<RE::TESObjectWEAP>();
            if (weap && IsBowType(weap->GetWeaponType()))
                return true;
        }
        return false;
    }

    class DrawWeaponHook
    {
    public:
        static void Install()
        {
            InstallHook(RE::VTABLE_Actor[0],     "Actor");
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
            if (!a_this || a_this->IsPlayerRef())
            {
                _DrawWeaponMagicHands(a_this, a_draw);
                return;
            }

            auto& state = _npcState[a_this->GetFormID()];
            auto  now   = std::chrono::steady_clock::now();

            // ---------------------------------------------------------------
            // BLOK A — Savaş içi döngü düzeltmesi (her zaman aktif)
            // ---------------------------------------------------------------
            if (a_this->IsInCombat())
            {
                // Savaşa girilince savaş-öncesi sayaçları sıfırla
                state.preCombatSheatheCount = 0;
                state.preCombatWindowStart  = {};
                state.preCombatBlockUntil   = {};

                if (!a_draw)  // Kınına sokma
                {
                    bool currentlyBow = IsCurrentlyHoldingBow(a_this);

                    if (state.initialized)
                    {
                        bool weaponTypeChanging = (currentlyBow != state.lastWasBow);

                        if (weaponTypeChanging)
                        {
                            // Yay ↔ yakın dövüş geçişi → meşru, izin ver
                            SKSE::log::info("Allowed weapon switch ({} -> {}) for NPC {:X}",
                                state.lastWasBow ? "bow" : "melee",
                                currentlyBow     ? "bow" : "melee",
                                a_this->GetFormID());
                            state.lastStateChange = now;
                            state.lastWasBow      = currentlyBow;
                        }
                        else
                        {
                            auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                now - state.lastStateChange).count();

                            if (ms < 2000)
                            {
                                SKSE::log::info("Blocked combat sheathe loop for NPC {:X} ({}ms, {})",
                                    a_this->GetFormID(), ms, currentlyBow ? "bow" : "melee");
                                return;  // ENGELLE
                            }
                            state.lastStateChange = now;
                            state.lastWasBow      = currentlyBow;
                        }
                    }
                    else
                    {
                        state.initialized     = true;
                        state.lastStateChange = now;
                        state.lastWasBow      = IsCurrentlyHoldingBow(a_this);
                    }
                }
                else  // Silah çekme — sadece state güncelle
                {
                    state.lastStateChange = now;
                    state.lastWasBow      = IsCurrentlyHoldingBow(a_this);
                    state.initialized     = true;
                }
            }
            // ---------------------------------------------------------------
            // BLOK B — Savaş öncesi döngü düzeltmesi (INI ile açılır)
            // ---------------------------------------------------------------
            else if (Settings::bFixPreCombatLoop && !a_draw)
            {
                // Hâlâ engelleme süresi içinde mi?
                if (state.preCombatBlockUntil > now)
                {
                    SKSE::log::info("Blocked pre-combat sheathe loop for NPC {:X} (still cooling)",
                        a_this->GetFormID());
                    return;  // ENGELLE
                }

                using ms_t = std::chrono::milliseconds;
                const auto windowMs = std::chrono::seconds(Settings::iPreCombatWindowSec);

                auto windowAge = std::chrono::duration_cast<ms_t>(now - state.preCombatWindowStart);

                if (windowAge > windowMs)
                {
                    // Pencere dolmuş → yeni pencere başlat
                    state.preCombatWindowStart  = now;
                    state.preCombatSheatheCount = 1;
                }
                else
                {
                    state.preCombatSheatheCount++;

                    if (state.preCombatSheatheCount >= Settings::iPreCombatThreshold)
                    {
                        // Döngü tespit edildi → blokla
                        state.preCombatBlockUntil   = now + std::chrono::seconds(Settings::iPreCombatBlockSec);
                        state.preCombatSheatheCount = 0;
                        state.preCombatWindowStart  = {};

                        SKSE::log::info(
                            "Pre-combat sheathe loop detected for NPC {:X}, blocking {}s",
                            a_this->GetFormID(), Settings::iPreCombatBlockSec);
                        return;  // ENGELLE
                    }
                }
            }

            _DrawWeaponMagicHands(a_this, a_draw);
        }

        static inline REL::Relocation<decltype(DrawWeaponMagicHands)> _DrawWeaponMagicHands;
        static inline std::unordered_map<RE::FormID, NPCState>        _npcState;
    };
}

void InitializeLog()
{
    auto path = SKSE::log::log_directory();
    if (!path) return;
    *path /= "CombatEquipFix.log"sv;
    auto sink = std::make_shared<spdlog::sinks::basic_file_sink_mt>(path->string(), true);
    auto log  = std::make_shared<spdlog::logger>("global log"s, std::move(sink));
    log->set_level(spdlog::level::info);
    log->flush_on(spdlog::level::info);
    spdlog::set_default_logger(std::move(log));
    spdlog::set_pattern("[%l] %v"s);
}

SKSEPluginInfo(
    .Version = { 1, 2, 0, 0 },
    .Name    = "Combat Equipment State Fix",
    .Author  = "Antigravity",
    .RuntimeCompatibility = { SKSE::VersionIndependence::AddressLibrary, false }
)

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    InitializeLog();
    SKSE::log::info("Combat Equipment State Fix v1.2.0 loaded.");
    SKSE::Init(a_skse);
    Settings::Load();
    EquipLoopFix::DrawWeaponHook::Install();
    return true;
}
