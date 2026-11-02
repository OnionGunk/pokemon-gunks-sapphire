# Engine decision: pret's pokeruby

Gunk's Sapphire is built on [pret/pokeruby](https://github.com/pret/pokeruby) at `5784633ce`. That decomp builds
Ruby and Sapphire from one tree: `make sapphire COMPARE=1` reproduces Sapphire (USA) 1.0 byte for byte
(sha1 `3ccbbd45f8553c36463f13b938e833f652b793e4`; checked here: `pokesapphire.gba: OK`).

## Options considered

| Base | Verdict |
| --- | --- |
| **pret/pokeruby** | **Chosen.** Full C source of Sapphire itself, a matching build, shiftable (code and data can grow), and the same toolchain (agbcc) as the other pret GBA decomps. |
| pret/pokeemerald + pokeemerald-expansion | Emerald, not Sapphire: the story, maps, Battle Frontier and scripts differ. Rebuilding Sapphire's story on it would break "keep the story". Used only as a **reference** for features. |
| Binary patching the retail ROM | No source; every change placed by hand in free space. The decomp makes this unnecessary. |

## One codebase with Gunk's Ruby

Ruby and Sapphire are the same program with a different `GAME_VERSION`. Gunk's Ruby implements the features as
`Gunk's Ruby: …` commits; Gunk's Sapphire cherry-picks them unchanged and adds only what is Sapphire's own: the
ROM header title (`GUNK SA`), this README and these docs. Both hacks therefore behave the same, and each build is
tested on its own game with the save suite in `C:\Projects\sapphire-tools\emu\suite` (see `GUNK_CHANGES.md`).

## Limits of pokeruby

- It is less modernised than pokeemerald: many functions still have `sub_XXXXXXX` names. Features are ported from
  pokeemerald / pokeemerald-expansion by hand.
- The save layout is fixed by the flash: 32 sectors of 4 KB, two 14-sector save slots (SaveBlock2 1, SaveBlock1
  4, PC storage 9), 2 Hall of Fame sectors and 2 unused. See `PC_BOXES_PLAN.md`.
- German text: the Gunk's strings are English only; build the English Sapphire.

## Toolchain (Windows, MSYS2)

- agbcc from [pret/agbcc](https://github.com/pret/agbcc), installed into `tools/agbcc`.
- MSYS2 with `mingw-w64-ucrt-x86_64-arm-none-eabi-gcc`, `make`, `gcc`, and libpng 1.6 built into `/usr/local`.
- Two wrappers first on `PATH` (copies in the Gunk's tool folders):
  - `arm-none-eabi-cpp`: MSYS2's native cpp writes CRLF into pipes, and preproc's enum parser then fails with
    "empty enum is invalid".
  - `arm-none-eabi-ld`: binutils 2.47 rejects pokeruby's linker script ("sym_ewram.ld appears multiple times");
    the wrapper inlines the script's `INCLUDE`s.
- Build: `make sapphire` (vanilla check on the upstream commit: `make sapphire COMPARE=1`).
