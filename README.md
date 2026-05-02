# Combat Equipment State Fix (SKSE)

## English
This SKSE plugin resolves a common AI bug where NPCs enter an infinite loop of drawing and sheathing their weapons during combat. This issue is most prominent when NPCs use shields, dual weapons, or modern combat frameworks like MCO, SCAR, BFCO, and EldenRim Arts.

The plugin intercepts weapon state changes and prevents NPCs from sheathing their weapons within 2 seconds of their last draw or state change while in combat. This breaks the AI logic loop without affecting legitimate equipment swaps (like switching to a dagger when close).

**Features:**
- Fixes NPC weapon/shield flickering and sheathing loops during combat.
- Supports both **Actor** and **Character** classes (Full support for Draugr and unique NPCs).
- Supports Skyrim Special Edition **1.5.97**, Anniversary Edition **1.6+**, and **Skyrim VR**.
- No performance impact; lightweight C++ implementation.
- Compatible with all animation and combat overhauls (MCO, SCAR, BFCO, etc.).
- Designed for universal compatibility via CommonLibSSE-NG.

**Requirements:**
- Skyrim Special Edition (1.5.97), AE (1.6+), or Skyrim VR.
- [SKSE64](https://skse.silverlock.org/) / [SKSEVR](https://skse.silverlock.org/)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444) (or [VR Address Library](https://www.nexusmods.com/skyrimvr/mods/58101))

---

## Türkçe
Bu SKSE eklentisi, yapay zekanın (NPC) savaş sırasında sürekli olarak silahlarını kınına sokup tekrar çekme (loop) döngüsüne girmesi sorununu çözer. Bu hata özellikle kalkan, çift el silah (dual-wield) veya MCO, SCAR, BFCO gibi modern dövüş sistemleri kullanan mod listelerinde sıkça görülür.

Eklenti, yapay zekanın silah çekme fonksiyonlarına müdahale eder ve savaş sırasında son işleminden sonraki 2 saniye içinde silahını kınına koymasını engeller. Bu sayede yapay zeka hatası kırılırken, okçuların dibine girince hançer çekmesi gibi meşru davranışlar etkilenmez.

**Özellikler:**
- Savaş sırasında kalkan ve silahların sürekli gidip gelmesini (flickering) engeller.
- Hem **Actor** hem de **Character** sınıflarını destekler (Draugr'lar ve diğer özel NPC'ler dahil tam uyumluluk).
- Skyrim Special Edition **1.5.97**, Anniversary Edition **1.6+** ve **Skyrim VR** desteği.
- Performans kaybı yaratmaz; hafif bir C++ eklentisidir.
- Tüm animasyon ve dövüş modlarıyla (MCO, SCAR, BFCO vb.) tam uyumludur.
- CommonLibSSE-NG aracılığıyla evrensel uyumluluk için tasarlanmıştır.

**Gereksinimler:**
- Skyrim Special Edition (1.5.97), AE (1.6+) veya Skyrim VR.
- [SKSE64](https://skse.silverlock.org/) / [SKSEVR](https://skse.silverlock.org/)
- [Address Library for SKSE Plugins](https://www.nexusmods.com/skyrimspecialedition/mods/32444) (veya [VR Address Library](https://www.nexusmods.com/skyrimvr/mods/58101))

## License / Lisans
Copyright (c) 2026 Arif KULPU. All Rights Reserved. — Tüm Hakları Saklıdır. See LICENSE for details.
