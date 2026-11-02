# More PC boxes: plan

Ruby has **14 boxes** (420 Pokémon). The National Dex is 386, so a living dex fits, but with little room to spare.
This plan raises it to **16 boxes** (480), the most the flash chip allows without dropping a feature.

## Why 16 is the ceiling

The 128 KiB flash chip has 32 sectors of 4 KiB, each holding 3968 bytes of data plus a footer (`src/save.c`):

| Sectors | Vanilla use |
|---|---|
| 0-13 | save slot A: 1 SaveBlock2 + 4 SaveBlock1 + 9 PokemonStorage chunks |
| 14-27 | save slot B (saves alternate between A and B) |
| 28-29 | Hall of Fame |
| 30-31 | **unused** (only `unref_*` functions in `save.c` touch them) |

`struct PokemonStorage` is `1 (+3 padding) + 14 × 30 × 80 + 14 × 9 + 14` = 33,744 bytes in 9 chunks (35,712 bytes of room).
Each extra box costs `30 × 80 + 9 + 1` = 2,410 bytes.

Giving each slot one more sector (15 per slot) uses sectors 30-31 and makes 10 storage chunks (39,680 bytes):
`4 + 16 × 2,410 = 38,564` fits; 17 boxes (40,974) does not. The leftover space in SaveBlock1 and SaveBlock2
(about 2 KB together, part of it now used by the DexNav search levels) is less than one box and lives in
other chunks, so 16 is the ceiling.

## Save layout after the change

| Sectors | Gunk's Ruby |
|---|---|
| 0-14 | save slot A: SaveBlock2, SaveBlock1 × 4, PokemonStorage × 10 |
| 15-29 | save slot B |
| 30-31 | Hall of Fame |

Everything else about saving stays: both slots alternate, chunk rotation by `saveCounter`, per-sector checksums,
and the "previous save loaded" fallback all work the same, just with 15 sectors per slot
(`NUM_SECTORS_PER_SAVE_SLOT`, `ALL_SECTORS` follow from it).

**Old saves don't load** (vanilla Ruby saves and earlier Gunk's Ruby builds): the slots and the Hall of Fame are in new
places. That's acceptable before the real playthrough starts; it goes in the release notes.

## Code changes

1. `include/pokemon.h`: `TOTAL_BOXES_COUNT 16` and `struct PokemonStorage` sized by it.
2. `src/save.c`: `NUM_SECTORS_PER_SAVE_SLOT 15`, `HALL_OF_FAME_SECTOR 30`, a 10th `gPokemonStorage` chunk, and a
   compile-time check that `sizeof(struct PokemonStorage) <= 10 × 3968`.
3. The PC (`src/pokemon_storage_system*.c`) uses a literal `14` in two meanings: the number of boxes, and the
   **party's pseudo-box id**. Every `14` there gets audited and replaced by `TOTAL_BOXES_COUNT` (box loops, wrap
   in box selection, the box list) or `PARTY_BOX` (= `TOTAL_BOXES_COUNT`, the party). The party id moves from 14 to
   16 with the box count, so both readings stay right.
4. Other box users: `CopyMonToPC` / `SendMonToPC` (caught Pokémon going to the PC), the "box is full" checks in
   battle and daycare, Lanette's/Bill's PC messages, the box-name defaults (BOX1-BOX16) and default wallpapers.
5. Record mixing and link code never send box data, so link compatibility is unaffected.

## Tests

- Save suite (save cycles, crash after save, torn save at every frame of the write, damaged-slot recovery) on the
  new layout.
- `full_boxes`: all 480 slots filled through RAM, save, power cycle, compare every box with the cart.
- In the emulator: boxes 15-16 in the PC (move, withdraw, deposit, rename, wallpaper), wrapping from 16 to 1, a catch
  with a full party going to box 15/16, the Hall of Fame record surviving a save.

## Sapphire note: a layout that keeps old saves loading

This plan is shared with Gunk's Ruby (same code). One alternative from the Sapphire side, not yet decided:
keep sectors 0-29 exactly as vanilla and give each slot its sixteenth-box sector **outside** the rotation —
slot A's extension in sector 30, slot B's in sector 31, written **after** the slot's 14 normal sectors.

- A slot is valid when its 14 normal chunks are valid **and** its extension carries the same save counter —
  except when **no** slot has a valid extension (a vanilla or pre-boxes save), which then loads with boxes 15-16
  empty. The Hall of Fame stays in 28-29.
- Torn saves still fall back: the extension is written last, so a cut before it leaves the new slot without a
  matching extension while the older slot has one, and the older slot loads.
- Space: the extension sector gives 3,968 bytes; with the 1,968 spare in the 9th storage chunk that is
  5,936 bytes = **2 more boxes** (4,820), the same 16 boxes.
- Cost: the load/validate code in `save.c` gets the extension rule (small), and the save suite gains the
  "legacy save" and "cut before the extension" cases.

Benefit: vanilla Sapphire/Ruby saves and earlier Gunk's builds keep loading.
