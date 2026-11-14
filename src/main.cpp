#include "PCH.h"
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <string>
#include <thread>
#include <unordered_map>
#include <spdlog/sinks/basic_file_sink.h>

#include "SKSEMenuFramework.h"

// ---------------------------------------------------------------------------
// v1.6.0 — DEĞİŞİKLİK ÖZETİ
//  * Kılıflama (sheathe) isteği ARTIK HİÇBİR ZAMAN savaş dışında engellenmez.
//    Eski sürüm kılıflamayı engelleyip silahı elde "donduruyordu".
//  * Döngü tespiti artık sadece gerçek "çek -> kılıfla" döngülerini sayar
//    (silah gerçekten kDrawn iken). Eskiden AI'ın rutin çağrıları sayılıyordu.
//  * Engelleme, kılıflama yerine savaş dışı "tekrar çekme" üzerine uygulanır.
//  * Takipçiler / komutlu aktörler (IsPlayerTeammate) tamamen muaf (varsayılan).
//  * Savaş içi engel: aynı çekişte en fazla 1 kez engellenir (AI tekrar
//    denerse geçer) -> silah değiştirme akışı kilitlenmez.
//  * "Donmuş silah" düzeltmesi: oyuncu silah çekmişken / savaştayken / NPC
//    sinsi iken çalışmaz; zorla kılıflama sonrası 60 sn bekleme süresi vardır.
//  * Dış mekan (Exterior) ve iç mekan (Interior) hücre/worldspace ayrımı düzeltildi.
//  * Menüyü kPostLoad sonrası kaydediyoruz (header'daki static GetModuleHandle
//    sorunu için), slider'larda INI sadece bırakınca yazılır.
//  * State tablosu temizlenir / budanır (bellek sızıntısı yok).
// ---------------------------------------------------------------------------

// ---------------------------------------------------------------------------
// Basit INI okuyucu
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

    auto trim = [](std::string s) {
        size_t start = s.find_first_not_of(" \t\r\n");
        size_t end   = s.find_last_not_of(" \t\r\n");
        return (start == std::string::npos) ? std::string{} : s.substr(start, end - start + 1);
    };

    while (std::getline(file, line)) {
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
// Ayarlar — Data/SKSE/Plugins/CombatEquipFix.ini
// ---------------------------------------------------------------------------
namespace Settings
{
    bool bSkipTeammates        = true;  // Takipçiler/komutlu aktörler muaf (ÖNERİLEN)

    bool bFixPreCombatLoop     = true;
    int  iPreCombatWindowSec   = 6;
    int  iPreCombatThreshold   = 3;     // 3 tam "çek->kılıfla" döngüsü
    int  iPreCombatBlockSec    = 5;

    bool bFixCombatLoop        = true;  // Savaş içi hızlı kılıflama (1 kez engeller)

    bool bFixStuckWeapon         = true;
    int  iStuckWeaponTimeoutSec  = 30;
    int  iStuckWeaponCheckRadius = 3000;

    static std::filesystem::path IniPath()
    {
        return std::filesystem::absolute("Data\\SKSE\\Plugins\\CombatEquipFix.ini");
    }

    void Load()
    {
        const auto p = IniPath();

        bSkipTeammates          = ReadINIInt(p, "General", "bSkipTeammates",          1) != 0;

        bFixPreCombatLoop       = ReadINIInt(p, "General", "bFixPreCombatLoop",       1) != 0;
        iPreCombatThreshold     = ReadINIInt(p, "General", "iPreCombatThreshold",     3);
        iPreCombatWindowSec     = ReadINIInt(p, "General", "iPreCombatWindowSec",     6);
        iPreCombatBlockSec      = ReadINIInt(p, "General", "iPreCombatBlockSec",      5);

        bFixCombatLoop          = ReadINIInt(p, "General", "bFixCombatLoop",          1) != 0;

        bFixStuckWeapon         = ReadINIInt(p, "General", "bFixStuckWeapon",         1) != 0;
        iStuckWeaponTimeoutSec  = ReadINIInt(p, "General", "iStuckWeaponTimeoutSec",  30);
        iStuckWeaponCheckRadius = ReadINIInt(p, "General", "iStuckWeaponCheckRadius", 3000);

        SKSE::log::info("Settings: skipTeammates={} | preCombat={} thr={} win={}s block={}s | combat={} | stuck={} timeout={}s radius={}",
            bSkipTeammates, bFixPreCombatLoop, iPreCombatThreshold, iPreCombatWindowSec, iPreCombatBlockSec,
            bFixCombatLoop, bFixStuckWeapon, iStuckWeaponTimeoutSec, iStuckWeaponCheckRadius);
    }

    void Save()
    {
        std::ofstream file(IniPath());
        if (!file.is_open()) return;

        file << "; ============================================================\n";
        file << ";  Combat Equipment State Fix — Configuration File\n";
        file << "; ============================================================\n\n";
        file << "[General]\n";
        file << "bSkipTeammates=" << (bSkipTeammates ? 1 : 0) << "\n\n";
        file << "bFixPreCombatLoop=" << (bFixPreCombatLoop ? 1 : 0) << "\n";
        file << "iPreCombatThreshold=" << iPreCombatThreshold << "\n";
        file << "iPreCombatWindowSec=" << iPreCombatWindowSec << "\n";
        file << "iPreCombatBlockSec=" << iPreCombatBlockSec << "\n\n";
        file << "bFixCombatLoop=" << (bFixCombatLoop ? 1 : 0) << "\n\n";
        file << "bFixStuckWeapon=" << (bFixStuckWeapon ? 1 : 0) << "\n";
        file << "iStuckWeaponTimeoutSec=" << iStuckWeaponTimeoutSec << "\n";
        file << "iStuckWeaponCheckRadius=" << iStuckWeaponCheckRadius << "\n";

        SKSE::log::info("Settings saved to INI file.");
    }
}

namespace MenuFramework
{
    static void Tip(const char* a_text)
    {
        if (ImGuiMCP::IsItemHovered())
            ImGuiMCP::SetTooltip("%s", a_text);
    }

    // Slider: INI'ye sadece kullanıcı sürüklemeyi bitirince yaz (her karede değil)
    static void SaveSlider(int* a_value, const char* a_label, int a_min, int a_max, const char* a_tip)
    {
        ImGuiMCP::SliderInt(a_label, a_value, a_min, a_max);
        if (ImGuiMCP::IsItemDeactivatedAfterEdit())
            Settings::Save();
        Tip(a_tip);
    }

    static void SaveCheckbox(const char* a_label, bool* a_value, const char* a_tip)
    {
        if (ImGuiMCP::Checkbox(a_label, a_value))
            Settings::Save();
        Tip(a_tip);
    }

    void __stdcall RenderSettings()
    {
        ImGuiMCP::Text("Combat Equipment State Fix Settings");
        ImGuiMCP::Separator();

        SaveCheckbox("Skip Followers / Teammates", &Settings::bSkipTeammates,
            "Followers and commanded actors are never touched by this plugin. Recommended.");

        ImGuiMCP::Spacing();
        ImGuiMCP::Separator();

        SaveCheckbox("Fix Pre-Combat Draw/Sheathe Loop", &Settings::bFixPreCombatLoop,
            "Stops NPCs from repeatedly drawing and sheathing weapons outside combat (blocks re-draw during a short cooldown; sheathing is never blocked).");

        if (Settings::bFixPreCombatLoop) {
            SaveSlider(&Settings::iPreCombatThreshold, "Loop Threshold", 2, 10,
                "Number of draw->sheathe cycles inside the window to count as a loop.");
            SaveSlider(&Settings::iPreCombatWindowSec, "Detection Window (sec)", 2, 30,
                "Time window used for counting cycles.");
            SaveSlider(&Settings::iPreCombatBlockSec, "Block Duration (sec)", 2, 30,
                "How long re-drawing is blocked once a loop is detected.");
        }

        ImGuiMCP::Spacing();
        ImGuiMCP::Separator();

        SaveCheckbox("Fix Combat Sheathe Loop", &Settings::bFixCombatLoop,
            "Blocks ONE too-fast sheathe right after a draw in combat. A retry always goes through.");

        ImGuiMCP::Spacing();
        ImGuiMCP::Separator();

        SaveCheckbox("Fix Stuck Drawn Weapon", &Settings::bFixStuckWeapon,
            "Sheathes weapons of non-follower NPCs that stay drawn outside combat for too long.");

        if (Settings::bFixStuckWeapon) {
            SaveSlider(&Settings::iStuckWeaponTimeoutSec, "Stuck Timeout (sec)", 10, 180,
                "Seconds a weapon must stay drawn outside combat before it is force-sheathed.");
            SaveSlider(&Settings::iStuckWeaponCheckRadius, "Check Radius (units)", 500, 10000,
                "Only NPCs within this distance of the player are scanned.");
        }
    }

    void Register()
    {
        // Header'daki 'static auto menuFramework = GetModuleHandleW(...)' DLL yüklenirken
        // çalışır. Eklentimiz alfabetik olarak SKSEMenuFramework'ten önce yüklenirse NULL
        // kalır ve menü sessizce hiç görünmez. kPostLoad sonrası yeniden çözüyoruz.
        menuFramework = GetModuleHandleW(L"SKSEMenuFramework");

        if (menuFramework) {
            SKSEMenuFramework::SetSection("Combat Equipment State Fix");
            SKSEMenuFramework::AddSectionItem("Settings", RenderSettings);
            SKSE::log::info("Registered menu with SKSE Menu Framework.");
        } else {
            SKSE::log::info("SKSE Menu Framework is not loaded. Menu skipped.");
        }
    }
}

namespace EquipLoopFix
{
    using clock_t = std::chrono::steady_clock;

    struct NPCState
    {
        // --- Savaş içi ---
        clock_t::time_point lastDraw{};
        bool lastWasBow        { false };
        int  combatBlockCount  { 0 };       // bu çekiş için kaç kez engellendi

        // --- Savaş dışı döngü ---
        clock_t::time_point cycleWindowStart{};
        int  cycleCount        { 0 };
        clock_t::time_point drawBlockUntil{};

        // --- Donmuş silah ---
        clock_t::time_point drawnSince{};
        clock_t::time_point nextForceAllowed{};

        clock_t::time_point lastTouched{};
    };

    static bool IsBowType(RE::WEAPON_TYPE a_type)
    {
        return a_type == RE::WEAPON_TYPE::kBow || a_type == RE::WEAPON_TYPE::kCrossbow;
    }

    static bool IsCurrentlyHoldingBow(RE::Actor* a_actor)
    {
        for (bool left : { false, true }) {
            const auto* data = a_actor->GetEquippedEntryData(left);
            if (data && data->object) {
                const auto* weap = data->object->As<RE::TESObjectWEAP>();
                if (weap && IsBowType(weap->GetWeaponType()))
                    return true;
            }
        }
        return false;
    }

    static bool IsExempt(RE::Actor* a_actor)
    {
        if (a_actor->IsPlayerRef())
            return true;
        if (Settings::bSkipTeammates && (a_actor->IsPlayerTeammate() || a_actor->IsCommandedActor()))
            return true;
        return false;
    }

    static RE::WEAPON_STATE GetWeaponState(RE::Actor* a_actor)
    {
        const auto* st = a_actor->AsActorState();
        return st ? st->GetWeaponState() : RE::WEAPON_STATE::kSheathed;
    }

    class DrawWeaponHook
    {
        using DrawWeaponMagicHandsFn = void(RE::Actor*, bool);

    public:
        static void Install()
        {
            InstallHook(RE::VTABLE_Actor[0],     "Actor",     DrawWeaponMagicHandsActor,     _DrawWeaponMagicHandsActor);
            InstallHook(RE::VTABLE_Character[0], "Character", DrawWeaponMagicHandsCharacter, _DrawWeaponMagicHandsCharacter);
            SKSE::log::info("Combat Equipment State Fix: Hooks installed.");
        }

        static void ClearAllState()
        {
            _npcState.clear();
            SKSE::log::info("NPC state cleared (save load).");
        }

        // Game main thread'inde çağrılır (SKSE task).
        static void ScanAndFixStuckWeapons()
        {
            const auto now = clock_t::now();

            // 1) Eski kayıtları buda (5 dk dokunulmamış ve takip edilmeyenler)
            for (auto it = _npcState.begin(); it != _npcState.end();) {
                const auto& st = it->second;
                if (st.drawnSince.time_since_epoch().count() == 0 &&
                    (now - st.lastTouched) > std::chrono::minutes(5))
                    it = _npcState.erase(it);
                else
                    ++it;
            }

            if (!Settings::bFixStuckWeapon)
                return;

            auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player)
                return;

            // Oyuncu savaştaysa veya silahı çekikse hiçbir şeye dokunma:
            // çevredeki NPC'lerin silahı çekik olması çok doğal.
            if (player->IsInCombat() || GetWeaponState(player) != RE::WEAPON_STATE::kSheathed)
                return;

            const float radius   = static_cast<float>(Settings::iStuckWeaponCheckRadius);
            const float radiusSq = radius * radius;
            const auto  playerPos = player->GetPosition();
            const auto* playerWS  = player->GetWorldspace();
            const auto  timeout   = std::chrono::seconds(Settings::iStuckWeaponTimeoutSec);

            for (auto& [formID, st] : _npcState)
            {
                if (st.drawnSince.time_since_epoch().count() == 0)
                    continue;
                if ((now - st.drawnSince) < timeout)
                    continue;

                auto* actor = RE::TESForm::LookupByID<RE::Actor>(formID);
                if (!actor || IsExempt(actor) || actor->IsDisabled() || actor->IsDead() || !actor->Is3DLoaded()) {
                    st.drawnSince = {};
                    continue;
                }

                // Dış mekan vs. İç mekan kontrolü:
                const auto* actorWS = actor->GetWorldspace();
                if (playerWS || actorWS) {
                    // Dış mekan: worldspace'ler eşleşmeli
                    if (playerWS != actorWS) {
                        st.drawnSince = {};
                        continue;
                    }
                } else {
                    // İç mekan (interior): worldspace null'dur, aynı hücrede olmalılar
                    if (actor->GetParentCell() != player->GetParentCell()) {
                        st.drawnSince = {};
                        continue;
                    }
                }

                const auto  pos = actor->GetPosition();
                const float dx = pos.x - playerPos.x;
                const float dy = pos.y - playerPos.y;
                const float dz = pos.z - playerPos.z;
                if ((dx * dx + dy * dy + dz * dz) > radiusSq) {
                    st.drawnSince = {};
                    continue;
                }

                // Meşru durumlar: savaş, saldırı, sinsi
                if (actor->IsInCombat() || actor->IsAttacking() || actor->IsSneaking()) {
                    st.drawnSince = {};
                    continue;
                }

                if (now < st.nextForceAllowed) {
                    continue;  // yakın zamanda zorla kılıflandı; AI tekrar çektiyse bekle
                }

                if (GetWeaponState(actor) == RE::WEAPON_STATE::kDrawn) {
                    SKSE::log::info("Stuck weapon: NPC {:X} drawn >{}s. Force-sheathing.",
                        formID, Settings::iStuckWeaponTimeoutSec);
                    st.drawnSince        = {};
                    st.nextForceAllowed  = now + std::chrono::seconds(60);
                    actor->DrawWeaponMagicHands(false);
                } else {
                    st.drawnSince = {};
                }
            }
        }

    private:
        static void InstallHook(REL::VariantID a_id, const char* a_name, DrawWeaponMagicHandsFn* a_hook,
                                REL::Relocation<DrawWeaponMagicHandsFn*>& a_original)
        {
            REL::Relocation<uintptr_t> vtbl(a_id);
            // CommonLib: Actor::DrawWeaponMagicHands = 0xA6 (SE/AE), 0xA8 (VR)
            const auto vfuncIdx = REL::Module::IsVR() ? 0xA8 : 0xA6;
            a_original = vtbl.write_vfunc(vfuncIdx, a_hook);
            SKSE::log::info("Hooked {} vtable at index 0x{:X}.", a_name, vfuncIdx);
        }

        static void DrawWeaponMagicHandsActor(RE::Actor* a_this, bool a_draw)
        {
            Handle(a_this, a_draw, _DrawWeaponMagicHandsActor);
        }

        static void DrawWeaponMagicHandsCharacter(RE::Actor* a_this, bool a_draw)
        {
            Handle(a_this, a_draw, _DrawWeaponMagicHandsCharacter);
        }

        static void Handle(RE::Actor* a_this, bool a_draw,
                           REL::Relocation<DrawWeaponMagicHandsFn*>& a_original)
        {
            if (!a_this || IsExempt(a_this)) {
                a_original(a_this, a_draw);
                return;
            }

            auto& state = _npcState[a_this->GetFormID()];
            const auto now = clock_t::now();
            state.lastTouched = now;

            const bool inCombat = a_this->IsInCombat();
            const auto weapSt   = GetWeaponState(a_this);

            // ===============================================================
            // SAVAŞ İÇİ
            // ===============================================================
            if (inCombat)
            {
                state.cycleCount       = 0;
                state.cycleWindowStart = {};
                state.drawBlockUntil   = {};
                state.drawnSince       = {};

                if (a_draw) {
                    state.lastDraw         = now;
                    state.lastWasBow       = IsCurrentlyHoldingBow(a_this);
                    state.combatBlockCount = 0;
                }
                else if (Settings::bFixCombatLoop && weapSt == RE::WEAPON_STATE::kDrawn)
                {
                    // Sadece gerçekten çekik bir silah için ve sadece ÇOK hızlı kılıflamada,
                    // ve çekiş başına EN FAZLA 1 kez engelle. AI tekrar isterse geçsin ki
                    // silah değiştirme (yay<->kılıç) akışı kilitlenmesin.
                    const auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                                        now - state.lastDraw).count();
                    if (state.lastDraw.time_since_epoch().count() != 0 &&
                        ms < 1500 && state.combatBlockCount < 1)
                    {
                        state.combatBlockCount++;
                        SKSE::log::info("Blocked fast combat sheathe once for NPC {:X} ({}ms)",
                            a_this->GetFormID(), ms);
                        return;
                    }
                }

                a_original(a_this, a_draw);
                return;
            }

            // ===============================================================
            // SAVAŞ DIŞI
            // ===============================================================
            if (Settings::bFixPreCombatLoop)
            {
                if (a_draw)
                {
                    // Döngü soğuma süresindeyse TEKRAR ÇEKMEYİ engelle (kılıflamayı asla engelleme)
                    if (state.drawBlockUntil > now) {
                        SKSE::log::info("Blocked pre-combat re-draw for NPC {:X} (cooldown)",
                            a_this->GetFormID());
                        return;
                    }
                    state.lastDraw = now;
                }
                else if (weapSt == RE::WEAPON_STATE::kDrawn)
                {
                    // Gerçek bir "çek -> kılıfla" döngüsü: son çekişten kısa süre sonra kılıflama
                    const auto sinceDraw = std::chrono::duration_cast<std::chrono::seconds>(
                                               now - state.lastDraw).count();
                    if (state.lastDraw.time_since_epoch().count() != 0 &&
                        sinceDraw < Settings::iPreCombatWindowSec)
                    {
                        const auto windowAge = std::chrono::duration_cast<std::chrono::seconds>(
                                                   now - state.cycleWindowStart).count();
                        if (state.cycleWindowStart.time_since_epoch().count() == 0 ||
                            windowAge > Settings::iPreCombatWindowSec)
                        {
                            state.cycleWindowStart = now;
                            state.cycleCount       = 1;
                        }
                        else
                        {
                            state.cycleCount++;
                        }

                        if (state.cycleCount >= Settings::iPreCombatThreshold)
                        {
                            state.drawBlockUntil   = now + std::chrono::seconds(Settings::iPreCombatBlockSec);
                            state.cycleCount       = 0;
                            state.cycleWindowStart = {};
                            SKSE::log::info("Pre-combat loop detected for NPC {:X}; blocking re-draw for {}s",
                                a_this->GetFormID(), Settings::iPreCombatBlockSec);
                        }
                    }
                }
            }

            // Donmuş silah takibi (sadece zaman damgası)
            if (Settings::bFixStuckWeapon)
            {
                if (a_draw) {
                    if (state.drawnSince.time_since_epoch().count() == 0)
                        state.drawnSince = now;
                } else {
                    state.drawnSince = {};
                }
            }

            a_original(a_this, a_draw);
        }

        static inline REL::Relocation<DrawWeaponMagicHandsFn*> _DrawWeaponMagicHandsActor;
        static inline REL::Relocation<DrawWeaponMagicHandsFn*> _DrawWeaponMagicHandsCharacter;

    public:
        static inline std::unordered_map<RE::FormID, NPCState> _npcState;
    };

    // -----------------------------------------------------------------------
    // Arka plan thread — 5 sn'de bir game thread'e tarama görevi ekler
    // -----------------------------------------------------------------------
    static std::atomic<bool> g_checkerRunning{ false };
    static std::thread       g_checkerThread;

    static void StuckWeaponCheckerThread()
    {
        constexpr int sleepSec = 5;
        while (g_checkerRunning)
        {
            for (int i = 0; i < sleepSec * 10 && g_checkerRunning; ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

            if (!g_checkerRunning)
                break;

            SKSE::GetTaskInterface()->AddTask([]() {
                DrawWeaponHook::ScanAndFixStuckWeapons();
            });
        }
    }

    void StartChecker()
    {
        g_checkerRunning = true;
        g_checkerThread  = std::thread(StuckWeaponCheckerThread);
        SKSE::log::info("Stuck weapon checker thread started.");
    }

    void StopChecker()
    {
        g_checkerRunning = false;
        if (g_checkerThread.joinable())
            g_checkerThread.join();
    }

    static void StopCheckerAtExit()
    {
        StopChecker();
    }
}

// ---------------------------------------------------------------------------
// SKSE mesaj işleyici
// ---------------------------------------------------------------------------
static void OnSKSEMessage(SKSE::MessagingInterface::Message* a_msg)
{
    if (!a_msg) return;

    switch (a_msg->type)
    {
    case SKSE::MessagingInterface::kPostLoad:
        // Tüm DLL'ler yüklendi -> Menu Framework artık çözülebilir
        MenuFramework::Register();
        break;
    case SKSE::MessagingInterface::kPostLoadGame:
    case SKSE::MessagingInterface::kNewGame:
        EquipLoopFix::DrawWeaponHook::ClearAllState();
        break;
    default:
        break;
    }
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
    .Version = { 1, 6, 0, 0 },
    .Name    = "Combat Equipment State Fix",
    .Author  = "Antigravity",
    .RuntimeCompatibility = { SKSE::VersionIndependence::AddressLibrary, false }
)

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    SKSE::Init(a_skse);
    InitializeLog();
    SKSE::log::info("Combat Equipment State Fix v1.6.0 loaded for runtime {}.",
        REL::Module::get().version().string());
    Settings::Load();

    if (const auto messaging = SKSE::GetMessagingInterface())
        messaging->RegisterListener(OnSKSEMessage);

    EquipLoopFix::DrawWeaponHook::Install();
    EquipLoopFix::StartChecker();
    std::atexit(EquipLoopFix::StopCheckerAtExit);
    return true;
}
