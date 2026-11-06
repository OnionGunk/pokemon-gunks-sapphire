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
| 2 | Title-screen logo | adapt | The user's "Pokémon Gunk's Sapphire Version" art, keyed and scaled to 182x87 on the logo layer (banner sprites empty). Checked on the title screen and through its animation. | ✅ |

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
| 21 | DexNav key item | adapt | **Screen** (src/gunk_dexnav_screen.c, Gunk's Platinum layout on one GBA screen, blue skin from tools/gunk/dexnav_skin.py): Caught x/y, LAND 2x5 and WATER 1x5 cells (icon in colour if caught with a ball mark, silhouette if seen, "?" if unseen; only seen ones can be chosen), info panel (name, status, Grass/Surfing and level range, search level), search panel (target, chain, hint), A/B footer; dexnav_screen: cursor moves, B closes, 20 opens leave no sprites. **Water**: surfing, a ripple patch 2-4 tiles away (dexnav_water: Tentacool on Route 103). The Pokémon is rolled when the search starts: a shiny one sparkles on the patch and is the one met; a **search bar** at the top shows it (name, level, SHINY, nature, ability, search level, chain, IVs at 31) and hides for menus/messages (dexnav_bar). START on the screen shows an **area info page**: the TV's mass outbreak, ripe berry trees in Hoenn, and on Route 119 today's Feebas tiles (Gunk's Ruby getters; dexnav_info). R explains when it can't search (nothing chosen, not here, no grass, needs Surf). DexNav battles find held items more often (search level, chain, Compound Eyes lead). An egg in slot 1 doesn't count (from Gunk's Ruby b95aa427e, as Gunk's Platinum and LeafGreen). On Route 101: a Wurmple hidden 4 tiles away, a battle with it on stepping in, chain 1 after it. **Legality (PKHeX.Core 26.10.18):** DexNav Pokémon at chains 0/10/20 and search levels 0/50/255, RNG-seeded shinies (with and without bonus rolls), normal wild Pokémon and a catch all VALID (Method 1/2/4). Found and fixed: levels between a species' slots (e.g. 25 for one found at 24 and 26) were possible and invalid; the level now comes from one of its slots. **Speed:** at the most rolls (20) building every roll froze the step into the patch for 45 frames; now each roll only makes CreateMonWithNature's RNG calls and only the winner is built (worst 6.6 frames, mean 5.5). The kept Pokémon matched a model of the rolls, VBlank draws included, on 15 random encounters; 23/23 VALID at chain 255. **Chain:** beating or catching the DexNav Pokémon adds 1; running (or any other end) resets it to 0 (dexnav_rules: chain 5, flee, 0). | ✅ |
| 22 | Mom's gifts | adapt | With the Running Shoes Mom gives the Endless Repel, Exp. All and DexNav (seen in the story run). | ✅ |
| 23 | Following Pokémon | adapt | Sprites from pokeemerald-expansion. Torchic follows the player around Littleroot and through the story run; no long sweep yet. **Talk:** A on it plays its cry and gives a line by friendship (under 50, 50–99, 100–199, 200–254, 255), or a tired line under a quarter of its HP; lines from FireRed v1.2 (follower_talk 6/6; talk sweep of 277 story NPCs same as vanilla). **Object slots:** when all 16 are live (Route 123 with it out), a map object coming into view takes the follower's slot and the follower comes back when one frees; forced full there, the next object was missing before and spawns now. Its local id (254) and graphics id (218) are its own. **From Gunk's Ruby:** mood emote bubbles when talked to (f1808c94e); a Poké Ball opens with its sound when it goes in or comes out (surfing, biking, falling behind); Unown follows as its own letter (9f0b69d5e). | ✅ |
| 24 | Quick Run (R) and Quick Ball (L) in battle | new | R on the action menu runs. In wild battles where balls can be thrown, a bottom-left widget shows a ball, its count (≤ 99) and an L hint; tap L to throw it, hold L + Left / Right to pick another kind; it starts on the kind thrown last (`VAR_GUNK_LAST_BALL`, no save layout change). Checked: Poké Ball ×5 shown, L + Right → Great Ball ×3, tap L threw one (3 → 2, remembered, caught); R ran; no widget without balls. | ✅ |

## Story checks (toward v1.0)

`sapphire-tools/emu/suite/gyms.py`, against vanilla Sapphire: from the state after Mom's gifts, with the earlier
badges set and an overpowered test party, the player is put in front of each leader and plays the fight and the
badge scene. All eight leaders (Roxanne … Wallace) give their badge on both ROMs, and the flags afterwards are
identical (the only differing vars are the friendship step counter and the lottery number, which come from the
two base saves). The Elite Four, the Champion, the Hall of Fame ceremony and the credits run through to the title
screen on both; Gunk's Sapphire writes the Hall of Fame to sectors 30-31 (16-box layout), vanilla to 28-29, and
after a power cycle CONTINUE loads with the game-clear flag set. Not covered yet: the story scenes between the
gyms (Team Aqua, Kyogre) played in order.
