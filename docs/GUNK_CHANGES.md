# Pokémon Gunk's Sapphire Version: change list

Gunk's Sapphire and Gunk's Ruby are one codebase on pret's pokeruby (see `ENGINE.md`). The features are Gunk's
Ruby's commits (`Gunk's Ruby: …`), cherry-picked unchanged; Gunk's Sapphire adds its title and docs, and
sends back fixes its tests find (`Gunk's Ruby fix: …`). Gunk's Ruby's own change list has the implementation
notes for each feature; this list has the **verdicts for Sapphire and what was checked on the Sapphire build**.

- **port**: same feature as Gunk's Platinum. **adapt**: same goal, different shape on the GBA. **skip**: not needed.
- Status: ✅ checked in the emulator on Gunk's Sapphire, 🟡 built and passes the save suite, not yet exercised,
  ⏳ planned.

Checks: `C:\Projects\sapphire-tools\emu\suite` — `suite.py` (save safety), `features.py` (in-game feature
checks), `story.py` (scripted play from a new game through Mom's gifts).

## Identity

| # | Change | Verdict | Sapphire | Status |
|---|---|---|---|---|
| 1 | ROM header title `GUNK SA` (game code stays `AXPE`: 128 KB flash + RTC) | port | `TITLE` in `config.mk`. | ✅ |
| 2 | Title-screen logo | adapt | Needs art. | ⏳ |

## Save safety

| # | Change | Verdict | Sapphire | Status |
|---|---|---|---|---|
| 3 | Save suite | port | **save_cycles** (3 saves, power cycle, CONTINUE lands on the last), **torn_save** (the flash cut at every sector of a save's write order: not written, erased, half-programmed; each must CONTINUE into the older save, or the new one when complete, and keep running), **backup_recovery** (either slot with a bad checksum or erased: the other loads), **dry_rtc** (the RTC reports a power failure: CONTINUE, save, power cycle, CONTINUE). Gunk's Sapphire 4/4 (45 cut points with 15 sectors per slot); vanilla Sapphire 4/4 too, as the baseline. | ✅ |
| 4 | Dead clock battery: time keeps moving | new | With the RTC reporting a power failure, vanilla stops the clock at 2000-01-01, which stops berries, tides and daily events. The game now runs the clock from play time instead; the battery warning stays. Save side checked by dry_rtc. **dead_rtc_clock**: play time 10 h → 40 h moves the game clock 30 h and the berry clock with it. Found and fixed here: a battery that dies *after* the clock was set on a working RTC put the play-time clock years behind the last berry update, so berries and daily events stopped; the clock now carries on from the last berry update (sent to Ruby). | ✅ |
| 5 | 16 PC boxes (14 in vanilla) | adapt | 15 sectors per save slot, Hall of Fame moved to sectors 30-31 (`PC_BOXES_PLAN.md`). **Vanilla saves don't load.** The save suite passes on this layout. In Oldale's PC, MOVE POKéMON wraps BOX1 → BOX16 → BOX15 and back. | ✅ |

## Quality of life

| # | Change | Verdict | What was checked on Sapphire | Status |
|---|---|---|---|---|
| 6 | INSTANT text; new games on FAST | port | A field message prints in 3 frames (65 at FAST); INSTANT is picked and saved in Options; new games start on FAST. | ✅ |
| 7 | EXP bar twice as fast | port | Now also at early levels (Gunk's Ruby 66372b6de: levels spanning < 64 EXP took the fixed-point path, which ignored the step; Ruby measured 0.80 frames/px vs vanilla 1.42). Battles on Route 101 checked after it; speed not measured here. | ✅ |
| 8 | Auto-run with the Running Shoes, hold B to walk | port | Outdoors: 3 tiles in 32 frames without B, 2 holding B, 2 without the shoes. (Sapphire doesn't allow running indoors.) | ✅ |
| 9 | Repel prompt | port | YES uses another Super Repel (200 steps, one fewer in the bag), NO leaves both, none left shows the original message. | ✅ |
| 10 | Endless Repel key item | adapt | Switched from the bag and from L; the ON/OFF message now waits for a button (fix sent to Ruby). | ✅ |
| 11 | Reusable TMs | port | TM10 taught from the bag: Hidden Power learned, TM10 still there. | ✅ |
| 12 | Two registered items (Select and L) | adapt | L and Select each use their item. | ✅ |
| 13 | Bag sorting (Start) | port | The Items pocket sorted by item number. | ✅ |
| 14 | IV/EV view (A on the summary's skills page) | adapt | A cycles stats → IVs → EVs → stats; the IVs shown match the party data. | ✅ |
| 15 | Exp. All key item | adapt | Route 101 wild battles with two Pokémon: the one that didn't battle gets half the battler's EXP (e.g. +17 / +8) and the EVs. | ✅ |
| 16 | Vitamins to 252 EVs | port | 26 HP Ups: HP EVs 0 → 252; the 27th has no effect and isn't used up. | ✅ |
| 17 | Trade evolutions | port | Kadabra Lv 35 + Rare Candy → Alakazam; holding an Everstone it stays Kadabra; Onix + Metal Coat from the bag → Steelix. (The Metal Coat case found the trade items had no item effect; Ruby fixed it in 8f3197f19.) | ✅ |
| 18 | HM moves without teaching | port | With the Stone Badge and HM01 in the bag, Torchic's party menu offers CUT without knowing it; without HM01 it doesn't. | ✅ |
| 19 | Move Tutor in every PC | adapt | Oldale's PC → MOVE TUTOR → FORGET: Torchic forgot Growl (no Heart Scale). REMEMBER not exercised. | ✅ |
| 20 | Eggs hatch twice as fast | port | Two egg cycles per 256 steps: an egg's cycles went 20 → 18 at one step count. | ✅ |
| 21 | DexNav key item | adapt | On Route 101 from Select: the list (unseen as ?????), a Wurmple hidden 4 tiles away, a battle with it on stepping in, chain 1 after it. **Legality (PKHeX.Core 26.10.18):** DexNav Pokémon at chains 0/10/20 and search levels 0/50/255, RNG-seeded shinies (with and without bonus rolls), normal wild Pokémon and a catch all VALID (Method 1/2/4). Found and fixed: levels between a species' slots (e.g. 25 for one found at 24 and 26) were possible and invalid; the level now comes from one of its slots. | ✅ |
| 22 | Mom's gifts | adapt | With the Running Shoes Mom gives the Endless Repel, Exp. All and DexNav (seen in the story run). | ✅ |
| 23 | Following Pokémon | adapt | Sprites from pokeemerald-expansion. Torchic follows the player around Littleroot and through the story run; no long sweep yet. | ✅ |
