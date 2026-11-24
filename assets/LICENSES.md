# Graphics & Audio Asset Licenses

All external assets referenced in [manifest.json](manifest.json) and fetched by [tools/fetch-assets.py](../tools/fetch-assets.py) are published under **CC0 1.0 Universal (Public Domain Dedication)** — free for personal, open-source, and commercial use with no attribution requirement.

## Asset Sources

1. **Poly Haven** (`https://polyhaven.com`) — **CC0 1.0 Universal**
   - 2K PBR texture sets (`stone_wall_02`, `stone_brick_wall_001`, `slate_floor_02`, `stone_floor`, `stone_wall_04`, `rock_face`, `cobblestone_floor_08`, `dark_wood`, `rusty_metal_02`).
   - 2K photorealistic 3D models (`Lantern_01`, `antique_estoc`, `boulder_01`, `brass_goblets`).

2. **KayKit by Kay Lousberg** (`https://github.com/KayKit-Game-Assets`) — **CC0 1.0 Universal**
   - *KayKit Dungeon Remastered 1.0*: modular floors, walls, pillars, doorways, iron gates, stairs, rubble, spike traps, wall torches, banners, barrels, crates, wall shelves, candles, potions, gold coins, chests, food, keys.
   - *KayKit Character Pack: Skeletons 1.0*: rigged `Skeleton_Warrior`, `Skeleton_Mage`, `Skeleton_Minion`, `Skeleton_Rogue`.
   - *KayKit Character Pack: Adventures 1.0*: rigged `Rogue_Hooded`, `Rogue`, `Mage`, `Knight`, `Barbarian`, plus spellbook, dagger, axe, staff, wand, shield, arrows.

3. **Quaternius Ultimate Monsters** (mirrored via `511action/descent-3d-assets`) — **CC0 1.0 Universal**
   - Rigged & animated `Orc`, `Demon`, `BlueDemon`, `CreepCreature`, plus CC0 combat/footstep `.wav` effects.

4. **CC0 Monsters Catalog** (`https://github.com/oosawak/cc0-monsters`, sourced from OpenGameArt & Grab3D CC0 releases) — **CC0 1.0 Universal**
   - `mouse`, `giant_rat`, `house_cat`, `town_dog`, `red_fox`, `slime`, `spider`, `bat`, `cobra`, `crocodile`, `wolf`, `zombie_rigged`, `forest_monster`, `stone_golem_rigged`, `gargoyle`, `mimic`, `baby_dragon`, `dragon`, `crystal_dragon`, `wyvern`, `werewolf_rigged`, `gorilla`, `yeti_rigged`, `hydra`, `raven`, `eagle`, `griffin`, `phoenix`, `horror_monster`, `kraken`, `sabertooth`, `mammoth`, `unicorn`, `spirit_stag`, `bear`, `hornet`, `auroch_bull`.

## Updating or Replacing Assets

Raw downloaded binaries are stored locally in `assets/raw/` (gitignored to keep the repository lightweight) and imported into Unreal Engine via [tools/import-assets-to-unreal.py](../tools/import-assets-to-unreal.py). You can freely swap any mesh or material inside `/Game/Data/DA_DungeonVisualCatalog` in Unreal Editor.
