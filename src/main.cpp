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
// Basit INI okuyucu / yazıcı — Windows API'ye bağımlılık olmadan
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
// Ayarlar — Data/SKSE/Plugins/CombatEquipFix.ini dosyasından okunur / yazılır
// ---------------------------------------------------------------------------
namespace Settings
{
    bool bFixPreCombatLoop    = true;  // Varsayılan olarak AÇIK
    int  iPreCombatWindowSec  = 6;     // Algılama penceresi
    int  iPreCombatThreshold  = 2;     // 2 kez hızlı kılıflama yeterli
    int  iPreCombatBlockSec   = 8;     // Engelleme süresi

    bool bFixStuckWeapon        = true; // Donmuş silah düzeltmesi — Varsayılan AÇIK
    int  iStuckWeaponTimeoutSec = 20;   // NPC'nin kDrawn durumunda kaç saniye kalması sorun sayılır
    int  iStuckWeaponCheckRadius = 3000; // Player'a olan maksimum mesafe (Skyrim unit, ~60m)

    void Load()
    {
        const auto iniPath = std::filesystem::absolute("Data\\SKSE\\Plugins\\CombatEquipFix.ini");

        bFixPreCombatLoop       = ReadINIInt(iniPath, "General", "bFixPreCombatLoop",        1)  != 0;
        iPreCombatWindowSec     = ReadINIInt(iniPath, "General", "iPreCombatWindowSec",      6);
        iPreCombatBlockSec      = ReadINIInt(iniPath, "General", "iPreCombatBlockSec",       8);
        iPreCombatThreshold     = ReadINIInt(iniPath, "General", "iPreCombatThreshold",      2);

        bFixStuckWeapon         = ReadINIInt(iniPath, "General", "bFixStuckWeapon",          1)  != 0;
        iStuckWeaponTimeoutSec  = ReadINIInt(iniPath, "General", "iStuckWeaponTimeoutSec",  20);
        iStuckWeaponCheckRadius = ReadINIInt(iniPath, "General", "iStuckWeaponCheckRadius", 3000);

        SKSE::log::info("Settings loaded: bFixPreCombatLoop={} threshold={} window={}s block={}s | bFixStuckWeapon={} timeout={}s radius={}",
            bFixPreCombatLoop, iPreCombatThreshold, iPreCombatWindowSec, iPreCombatBlockSec,
            bFixStuckWeapon, iStuckWeaponTimeoutSec, iStuckWeaponCheckRadius);
    }

    void Save()
    {
        const auto iniPath = std::filesystem::absolute("Data\\SKSE\\Plugins\\CombatEquipFix.ini");
        std::ofstream file(iniPath);
        if (!file.is_open()) return;

        file << "; ============================================================\n";
        file << ";  Combat Equipment State Fix — Configuration File\n";
        file << "; ============================================================\n\n";
        file << "[General]\n";
        file << "bFixPreCombatLoop=" << (bFixPreCombatLoop ? 1 : 0) << "\n";
        file << "iPreCombatThreshold=" << iPreCombatThreshold << "\n";
        file << "iPreCombatWindowSec=" << iPreCombatWindowSec << "\n";
        file << "iPreCombatBlockSec=" << iPreCombatBlockSec << "\n";
        file << "\n";
        file << "bFixStuckWeapon=" << (bFixStuckWeapon ? 1 : 0) << "\n";
        file << "iStuckWeaponTimeoutSec=" << iStuckWeaponTimeoutSec << "\n";
        file << "iStuckWeaponCheckRadius=" << iStuckWeaponCheckRadius << "\n";

        SKSE::log::info("Settings saved to INI file.");
    }
}

namespace MenuFramework
{
    void __stdcall RenderSettings()
    {
        ImGuiMCP::Text("Combat Equipment State Fix Settings");
        ImGuiMCP::Separator();

        bool changed = false;

        if (ImGuiMCP::Checkbox("Fix Pre-Combat Sheathing Loop", &Settings::bFixPreCombatLoop)) {
            changed = true;
        }
        if (ImGuiMCP::IsItemHovered()) {
            ImGuiMCP::SetTooltip("Prevents NPCs from repeatedly sheathing/drawing weapons before combat starts.");
        }

        if (Settings::bFixPreCombatLoop) {
            ImGuiMCP::Spacing();
            ImGuiMCP::Text("Pre-Combat Loop Tuning:");

            if (ImGuiMCP::SliderInt("Sheathe Threshold", &Settings::iPreCombatThreshold, 2, 10)) {
                changed = true;
            }
            if (ImGuiMCP::IsItemHovered()) {
                ImGuiMCP::SetTooltip("Number of sheathings within the detection window to consider it a loop.");
            }

            if (ImGuiMCP::SliderInt("Detection Window (sec)", &Settings::iPreCombatWindowSec, 2, 30)) {
                changed = true;
            }
            if (ImGuiMCP::IsItemHovered()) {
                ImGuiMCP::SetTooltip("Time window in seconds to count sheathings.");
            }

            if (ImGuiMCP::SliderInt("Block Duration (sec)", &Settings::iPreCombatBlockSec, 3, 60)) {
                changed = true;
            }
            if (ImGuiMCP::IsItemHovered()) {
                ImGuiMCP::SetTooltip("Duration in seconds to block sheathing once a loop is detected.");
            }
        }

        ImGuiMCP::Spacing();
        ImGuiMCP::Separator();

        if (ImGuiMCP::Checkbox("Fix Stuck Drawn Weapon", &Settings::bFixStuckWeapon)) {
            changed = true;
        }
        if (ImGuiMCP::IsItemHovered()) {
            ImGuiMCP::SetTooltip("Force-sheathes weapons held by NPCs who are not attacking or sheathing after the timeout.");
        }

        if (Settings::bFixStuckWeapon) {
            ImGuiMCP::Spacing();
            if (ImGuiMCP::SliderInt("Stuck Timeout (sec)", &Settings::iStuckWeaponTimeoutSec, 5, 120)) {
                changed = true;
            }
            if (ImGuiMCP::IsItemHovered()) {
                ImGuiMCP::SetTooltip("Seconds the NPC must be out of combat with weapon drawn before it is force-sheathed.");
            }

            if (ImGuiMCP::SliderInt("Check Radius (units)", &Settings::iStuckWeaponCheckRadius, 500, 10000)) {
                changed = true;
            }
            if (ImGuiMCP::IsItemHovered()) {
                ImGuiMCP::SetTooltip("Only NPCs within this distance of the player are scanned. 3000 = ~60m. NPCs outside are skipped.");
            }
        }

        if (changed) {
            Settings::Save();
        }
    }

    void Register()
    {
        if (SKSEMenuFramework::IsInstalled()) {
            SKSEMenuFramework::SetSection("Combat Equipment State Fix");
            SKSEMenuFramework::AddSectionItem("Settings", RenderSettings);
            SKSE::log::info("Registered menu with SKSE Menu Framework.");
        } else {
            SKSE::log::info("SKSE Menu Framework is not installed. Menu skip.");
        }
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

        // --- Donmuş silah takibi ---
        // Hook'ta sadece bu zaman damgası yazılır.
        // Periyodik background checker tarafından okunur.
        std::chrono::steady_clock::time_point drawnSince{};
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
        using DrawWeaponMagicHandsFn = void(RE::Actor*, bool);

    public:
        static void Install()
        {
            InstallHook(RE::VTABLE_Actor[0],     "Actor",     DrawWeaponMagicHandsActor,     _DrawWeaponMagicHandsActor);
            InstallHook(RE::VTABLE_Character[0], "Character", DrawWeaponMagicHandsCharacter, _DrawWeaponMagicHandsCharacter);
            SKSE::log::info("Combat Equipment State Fix: Hooks installed.");
        }

        // Tüm NPC state'lerini temizle (kayıt yüklenince çağrılır)
        static void ClearAllState()
        {
            for (auto& [id, st] : _npcState) {
                st.drawnSince              = {};
                st.preCombatSheatheCount   = 0;
                st.preCombatWindowStart    = {};
                st.preCombatBlockUntil     = {};
                st.lastStateChange         = {};
                st.initialized             = false;
            }
            SKSE::log::info("NPC state cleared (save load).");
        }

        // Tüm takip edilen NPC'leri tara, player yakınında donmuş olanları kılıfla.
        // Game main thread'inden çağrılmalıdır (SKSE task içinden).
        static void ScanAndFixStuckWeapons()
        {
            if (!Settings::bFixStuckWeapon)
                return;

            // Player referansını al
            const auto* player = RE::PlayerCharacter::GetSingleton();
            if (!player)
                return;

            const float   radius    = static_cast<float>(Settings::iStuckWeaponCheckRadius);
            const float   radiusSq  = radius * radius;
            const auto    playerPos = player->GetPosition();
            const auto*   playerWS  = player->GetWorldspace();

            const auto now     = std::chrono::steady_clock::now();
            const auto timeout = std::chrono::seconds(Settings::iStuckWeaponTimeoutSec);

            for (auto& [formID, st] : _npcState)
            {
                // drawnSince sıfırsa bu NPC takip edilmiyor
                if (st.drawnSince.time_since_epoch().count() == 0)
                    continue;

                // Timeout dolmadıysa atla
                if ((now - st.drawnSince) < timeout)
                    continue;

                // Actor'ü doğrula
                auto* actor = RE::TESForm::LookupByID<RE::Actor>(formID);
                if (!actor || actor->IsPlayerRef() || actor->IsDisabled()) {
                    st.drawnSince = {};
                    continue;
                }

                // ----------------------------------------------------------
                // Mesafe filtresi: farklı worldspace veya çok uzaktaki NPC'ler
                // unload edilmiştir, state'leri zaten sıfırlanmıştır.
                // drawnSince'i temizle ve geç.
                // ----------------------------------------------------------
                const auto* actorWS = actor->GetWorldspace();
                if (actorWS != playerWS) {
                    st.drawnSince = {};
                    continue;
                }

                const auto  actorPos  = actor->GetPosition();
                const float dx        = actorPos.x - playerPos.x;
                const float dy        = actorPos.y - playerPos.y;
                const float dz        = actorPos.z - playerPos.z;
                const float distSq    = dx*dx + dy*dy + dz*dz;

                if (distSq > radiusSq) {
                    // Menzil dışı — uzakta, doğal reset bekleniyor
                    st.drawnSince = {};
                    continue;
                }

                // ----------------------------------------------------------
                // Yakın menzildeki NPC — tam kontrol
                // ----------------------------------------------------------

                // Savaştaysa veya saldırıyorsa meşru durum — takibi sıfırla
                if (actor->IsInCombat() || actor->IsAttacking()) {
                    st.drawnSince = {};
                    continue;
                }

                // Silah durumunu kontrol et
                const auto* actorState = actor->AsActorState();
                if (!actorState) {
                    st.drawnSince = {};
                    continue;
                }
                const auto weapSt = actorState->GetWeaponState();

                if (weapSt == RE::WEAPON_STATE::kDrawn)
                {
                    const auto sec  = std::chrono::duration_cast<std::chrono::seconds>(
                                          now - st.drawnSince).count();
                    const float dist = sqrtf(distSq);
                    SKSE::log::info(
                        "Stuck weapon: NPC {:X} drawn for {}s (dist={:.0f}). Force-sheathing.",
                        formID, sec, dist);
                    st.drawnSince = {};
                    actor->DrawWeaponMagicHands(false);
                }
                else
                {
                    // Kılıflanıyor / kılıflanmış — zaten düzelmiş
                    st.drawnSince = {};
                }
            }
        }

    private:
        static void InstallHook(REL::VariantID a_id, const char* a_name, DrawWeaponMagicHandsFn* a_hook,
                                REL::Relocation<DrawWeaponMagicHandsFn*>& a_original)
        {
            REL::Relocation<uintptr_t> vtbl(a_id);
            // VR vtable index 0xA8, SE/AE (1.5.x, 1.6.x, 1.7.x+) index 0xA6
            const auto vfuncIdx = REL::Module::IsVR() ? 0xA8 : 0xA6;
            a_original = vtbl.write_vfunc(vfuncIdx, a_hook);
            SKSE::log::info("Hooked {} vtable at index 0x{:X}.", a_name, vfuncIdx);
        }

        static void DrawWeaponMagicHandsActor(RE::Actor* a_this, bool a_draw)
        {
            DrawWeaponMagicHands(a_this, a_draw, _DrawWeaponMagicHandsActor);
        }

        static void DrawWeaponMagicHandsCharacter(RE::Actor* a_this, bool a_draw)
        {
            DrawWeaponMagicHands(a_this, a_draw, _DrawWeaponMagicHandsCharacter);
        }

        static void DrawWeaponMagicHands(RE::Actor* a_this, bool a_draw,
                                         REL::Relocation<DrawWeaponMagicHandsFn*>& a_original)
        {
            if (!a_this || a_this->IsPlayerRef())
            {
                a_original(a_this, a_draw);
                return;
            }

            auto& state = _npcState[a_this->GetFormID()];
            auto  now   = std::chrono::steady_clock::now();

            // ---------------------------------------------------------------
            // BLOK A — Savaş içi döngü düzeltmesi (her zaman aktif)
            // ---------------------------------------------------------------
            if (a_this->IsInCombat())
            {
                // Savaşa girilince savaş-öncesi sayaçları ve stuck takibini sıfırla
                state.preCombatSheatheCount = 0;
                state.preCombatWindowStart  = {};
                state.preCombatBlockUntil   = {};
                state.drawnSince            = {};  // Savaştaysa stuck değil

                if (!a_draw)  // Kınına sokma
                {
                    bool currentlyBow = IsCurrentlyHoldingBow(a_this);

                    if (state.initialized)
                    {
                        bool weaponTypeChanging = (currentlyBow != state.lastWasBow);

                        if (weaponTypeChanging)
                        {
                            // Yay <-> yakın dövüş geçişi -> meşru, izin ver
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
            // BLOK B — Savaş dışı silah döngüsü düzeltmesi (Varsayılan Açık)
            // ---------------------------------------------------------------
            else if (Settings::bFixPreCombatLoop)
            {
                // 1) Eğer engelleme süresi aktifse, kınına sokma veya çekme isteklerini engelle
                if (!a_draw && state.preCombatBlockUntil > now)
                {
                    SKSE::log::info("Blocked pre-combat loop action ({}) for NPC {:X} (cooling down)",
                        a_draw ? "draw" : "sheathe", a_this->GetFormID());
                    return;  // ENGELLE
                }

                // 2) Kınına sokma sıklığı takibi
                if (!a_draw)
                {
                    using ms_t = std::chrono::milliseconds;
                    const auto windowMs = std::chrono::seconds(Settings::iPreCombatWindowSec);

                    auto windowAge = std::chrono::duration_cast<ms_t>(now - state.preCombatWindowStart);

                    if (windowAge > windowMs)
                    {
                        // Pencere süresi aşılmış -> yeni pencere başlat
                        state.preCombatWindowStart  = now;
                        state.preCombatSheatheCount = 1;
                    }
                    else
                    {
                        state.preCombatSheatheCount++;

                        if (state.preCombatSheatheCount >= Settings::iPreCombatThreshold)
                        {
                            // Döngü tespit edildi -> geçici süreyle blokla
                            state.preCombatBlockUntil   = now + std::chrono::seconds(Settings::iPreCombatBlockSec);
                            state.preCombatSheatheCount = 0;
                            state.preCombatWindowStart  = {};

                            SKSE::log::info(
                                "Pre-combat loop detected for NPC {:X}, enforcing cooldown of {}s",
                                a_this->GetFormID(), Settings::iPreCombatBlockSec);
                        }
                    }
                }
            }

            // ---------------------------------------------------------------
            // BLOK C — Donmuş silah takibi (sadece zaman damgası)
            // Gerçek kontrol ve düzeltme background thread tarafından yapılır.
            // ---------------------------------------------------------------
            if (Settings::bFixStuckWeapon)
            {
                if (a_draw && !a_this->IsInCombat())
                {
                    // Savaş dışında silah çekildiyse zaman damgasını kaydet
                    if (state.drawnSince.time_since_epoch().count() == 0)
                        state.drawnSince = now;
                }
                else
                {
                    // Kılıflama veya savaş başladı: takibi temizle
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
    // Background thread — her N saniyede bir game thread'e tek bir task ekler.
    // Task, takip listesindeki tüm NPC'leri tek geçişte kontrol eder.
    // -----------------------------------------------------------------------
    static std::atomic<bool> g_checkerRunning{ false };
    static std::thread        g_checkerThread;

    static void StuckWeaponCheckerThread()
    {
        while (g_checkerRunning)
        {
            // Timeout süresinin yarısı kadar bekle (daha duyarlı tespit); en az 5 saniye
            // Keep settings access on the game thread; the menu may edit them concurrently.
            constexpr int sleepSec = 5;

            // Küçük dilimler halinde uyku: g_checkerRunning false olunca çabuk çık
            for (int i = 0; i < sleepSec * 10 && g_checkerRunning; ++i)
                std::this_thread::sleep_for(std::chrono::milliseconds(100));

            if (!g_checkerRunning)
                break;

            // Game main thread'e tek bir tarama görevi ekle
            SKSE::GetTaskInterface()->AddTask([]()
            {
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
        SKSE::log::info("Stuck weapon checker thread stopped.");
    }

    static void StopCheckerAtExit()
    {
        StopChecker();
    }
}

// ---------------------------------------------------------------------------
// SKSE mesaj işleyici — kayıt yüklenince state temizlenir
// ---------------------------------------------------------------------------
static void OnSKSEMessage(SKSE::MessagingInterface::Message* a_msg)
{
    if (!a_msg) return;

    switch (a_msg->type)
    {
    case SKSE::MessagingInterface::kPostLoadGame:
    case SKSE::MessagingInterface::kNewGame:
        // Kayıt yüklenince / yeni oyun başlayınca eski zaman damgalarını temizle
        EquipLoopFix::DrawWeaponHook::ClearAllState();
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
    .Version = { 1, 5, 0, 0 },
    .Name    = "Combat Equipment State Fix",
    .Author  = "Antigravity",
    .RuntimeCompatibility = { SKSE::VersionIndependence::AddressLibrary, false }
)

SKSEPluginLoad(const SKSE::LoadInterface* a_skse)
{
    InitializeLog();
    const auto runtimeVer = REL::Module::get().version().string();
    SKSE::log::info("Combat Equipment State Fix v1.5.0 loaded for runtime {}.", runtimeVer);
    SKSE::Init(a_skse);
    Settings::Load();

    // SKSE mesaj dinleyicisi — kayıt yüklenince state sıfırlanır
    const auto messaging = SKSE::GetMessagingInterface();
    if (messaging)
        messaging->RegisterListener(OnSKSEMessage);

    EquipLoopFix::DrawWeaponHook::Install();
    EquipLoopFix::StartChecker();
    std::atexit(EquipLoopFix::StopCheckerAtExit);
    MenuFramework::Register();
    return true;
}
