# Combat Equipment State Fix (SKSE)

## English
This SKSE plugin resolves AI bugs where NPCs enter an infinite loop of drawing and sheathing their weapons — both during combat and before it begins. This issue is most prominent when NPCs use shields, dual weapons, or modern combat frameworks like MCO, SCAR, BFCO, and EldenRim Arts.

The plugin intercepts weapon state changes and applies smart filtering to distinguish between a genuine AI loop and legitimate behavior (such as a Draugr switching from bow to melee when the player gets close, or a bandit drawing a dagger at short range).

> **Does this mod affect NPC aggression or hostility?**
> No. This mod only fixes the weapon draw/sheathe animation loop. It does not change when or why NPCs decide to fight. Bandits will still enter combat stance exactly as in vanilla when you approach them.

**Features:**
- Fixes NPC weapon/shield flickering and sheathing loops during combat.
- Supports both **Actor** and **Character** classes (full support for Draugr and unique NPCs).
- Correctly allows Draugr and dual-weapon NPCs to switch between bow and melee depending on distance.
- Optional fix for pre-combat sheathing loops (configurable via INI, off by default).
- Supports Skyrim Special Edition **1.5.97**, **1.6.353**, Anniversary Edition **1.6.629+** / **1.6.1170**, and **Skyrim VR**.
- No performance impact; lightweight C++ implementation.
- Compatible with all animation and combat overhauls (MCO, SCAR, BFCO, etc.).

**Requirements:**
- Skyrim Special Edition (1.5.97 / 1.6.353 / 1.6.1170), or Skyrim VR.
- [SKSE64](https://skse.silverlock.org/) / [SKSEVR](https://skse.silverlock.org/)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444) (or [VR Address Library](https://www.nexusmods.com/skyrimvr/mods/58101))

**Configuration (optional):**
The file `Data/SKSE/Plugins/CombatEquipFix.ini` can be used to enable additional fixes:
```ini
[General]
bFixPreCombatLoop=0   ; Set to 1 to fix weapon loops that occur before combat starts
```

---

## Türkçe
Bu SKSE eklentisi, yapay zekanın (NPC) savaş sırasında **veya savaş başlamadan önce** sürekli olarak silahlarını kınına sokup tekrar çekme (loop) döngüsüne girmesi sorununu çözer. Bu hata özellikle kalkan, çift el silah (dual-wield) veya MCO, SCAR, BFCO gibi modern dövüş sistemleri kullanan mod listelerinde sıkça görülür.

Eklenti, silah çekme fonksiyonlarını hook eder ve gerçek bir AI hatasını (döngü) ile meşru bir silah geçişini (Draugr'un oyuncuya yaklaşınca yayı bırakıp kılıç çekmesi gibi) akıllıca birbirinden ayırt eder.

> **Bu mod haydutların ya da düşman NPC'lerin düşmanlık derecesini etkiliyor mu?**
> Hayır. Bu mod yalnızca silah çekme/kınına sokma animasyon döngüsünü düzeltir. NPC'lerin ne zaman ve neden savaşmaya karar verdiğini hiçbir şekilde değiştirmez. Haydutlar yanınıza geldiğinde tamamen vanilla davranışıyla savaş pozisyonuna geçecektir.

**Özellikler:**
- Savaş sırasında kalkan ve silahların sürekli gidip gelmesini (flickering) engeller.
- Hem **Actor** hem de **Character** sınıflarını destekler (Draugr'lar ve diğer özel NPC'ler dahil tam uyumluluk).
- Yay+yakın dövüş silahı taşıyan NPC'lerin (Draugr gibi) mesafeye göre silah değiştirmesine doğru şekilde izin verir.
- Savaş başlamadan önce yaşanan silah döngüsünü düzelten isteğe bağlı özellik (INI ile açılır, varsayılan olarak kapalı).
- Skyrim Special Edition **1.5.97**, **1.6.353**, Anniversary Edition **1.6.629+** / **1.6.1170** ve **Skyrim VR** desteği.
- Performans kaybı yaratmaz; hafif bir C++ eklentisidir.
- Tüm animasyon ve dövüş modlarıyla (MCO, SCAR, BFCO vb.) tam uyumludur.

**Gereksinimler:**
- Skyrim Special Edition (1.5.97 / 1.6.353 / 1.6.1170) veya Skyrim VR.
- [SKSE64](https://skse.silverlock.org/) / [SKSEVR](https://skse.silverlock.org/)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444) (veya [VR Address Library](https://www.nexusmods.com/skyrimvr/mods/58101))

**Yapılandırma (isteğe bağlı):**
`Data/SKSE/Plugins/CombatEquipFix.ini` dosyası ile ek düzeltmeler etkinleştirilebilir:
```ini
[General]
bFixPreCombatLoop=0   ; Savaş başlamadan önce yaşanan döngüyü düzeltmek için 1 yapın
```

---

## Changelog

### v1.2.0
- **Yeni:** Savaş öncesi silah döngüsü düzeltmesi eklendi (INI ile açılır, varsayılan: kapalı).
- **Yeni:** `CombatEquipFix.ini` yapılandırma dosyası eklendi.
- **Dahili:** Windows API bağımlılığı kaldırıldı; INI okuma saf C++ ile yeniden yazıldı.

### v1.1.0
- **Düzeltme:** Hem yay hem yakın dövüş silahı taşıyan NPC'lerde (Draugr gibi) mesafeye göre silah geçişi artık doğru çalışıyor.
- **Düzeltme:** Yay ↔ yakın dövüş geçişleri meşru AI kararı olarak tanınıyor ve engellenmeden geçiyor.
- **Düzeltme:** Her NPC için silah tipi (`lastWasBow`) takibi eklendi.

### v1.0.1
- **Düzeltme:** `RuntimeCompatibility` bayrağındaki `true` parametresi `false` olarak düzeltildi.
- **Düzeltme:** Skyrim SE **1.6.353** ile yaşanan "unsupported version independence method" hatası giderildi.

### v1.0.0
- İlk sürüm. Savaş sırasında silah/kalkan kınına sokma döngüsü düzeltmesi.
- SE 1.5.97, AE 1.6+ ve VR desteği.

---

## License / Lisans
Copyright (c) 2026 Arif KULPU. All Rights Reserved. — Tüm Hakları Saklıdır. See LICENSE for details.
