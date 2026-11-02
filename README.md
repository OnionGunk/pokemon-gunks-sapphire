# Pokémon Gunk's Sapphire Version

A quality-of-life hack of Pokémon Sapphire, built on [pret's Ruby and Sapphire decompilation](https://github.com/pret/pokeruby).
It keeps the game's difficulty and story as they are and takes the grind and friction out, for long playthroughs.
It is part of the Pokémon Gunk's line, with [Gunk's Platinum](https://github.com/OnionGunk/pokemon-gunks-platinum),
and shares its code with its sister hack, Gunk's Ruby.

**Work in progress.** No release yet. [`docs/GUNK_CHANGES.md`](docs/GUNK_CHANGES.md) tracks what is done and tested.

## Features

- **Faster play:** instant text, a faster EXP bar, auto-run with the Running Shoes (hold B to walk), faster egg
  hatching.
- **Items:** reusable TMs, two registered items (Select and L), bag sorting (Start), a prompt to use another Repel
  when one runs out, and an Endless Repel key item.
- **Pokémon:** IV and EV view in the summary, an Exp. All key item, vitamins up to 252 EVs, and trade evolutions
  without trading. HM moves work without teaching the move, and every PC has a Move Tutor that remembers and
  forgets moves.
- **DexNav:** a key item that lists the area's Pokémon. Pick one and it hides in the grass nearby. Search levels
  and chains raise the odds of good IVs and shinies, and every encounter is a legal wild Pokémon.
- **A dead clock battery doesn't stop time:** most Sapphire carts' clock batteries have run dry, which freezes
  berries, tides and daily events. When the clock reports a power failure, the game runs its clock from play time
  instead.
- Planned: following Pokémon and more PC boxes.

Save safety comes first: every build passes an emulator test suite that covers save cycles, power cycles, a power
cut at every sector of a save, recovery from either damaged save slot, and saving with a dead clock battery.

## Play it

Releases ship a **BPS patch**, not a ROM. You need your own dump of **Pokémon Sapphire (USA)**, the original 1.0:

```
sha1: 3ccbbd45f8553c36463f13b938e833f652b793e4
```

1. Download the `.bps` file from [Releases](../../releases).
2. Apply it to your ROM with [Rom Patcher JS](https://www.marcrobledo.com/RomPatcher.js/) (in the browser),
   Floating IPS, or `py tools/patch/bps.py apply <sapphire.gba> <patch.bps> <out.gba>`.
3. Play the result in any GBA emulator or flashcart (128 KB flash save with real-time clock, as the original).
   The patcher checks your ROM first and refuses the wrong one.

Saves carry over between releases unless the release notes say otherwise. Back up your save before updating.

## Build it

Follow pret's [`INSTALL.md`](INSTALL.md), then run `make sapphire`. A clean build reproduces the release ROM, and
`tools/patch/bps.py make` makes the patch from it. [`docs/ENGINE.md`](docs/ENGINE.md) explains the base and the
toolchain notes for MSYS2.

## Credits

- [pret](https://github.com/pret/pokeruby) for the Ruby and Sapphire decompilation this hack is built on, and
  [agbcc](https://github.com/pret/agbcc).
- [pokeemerald-expansion](https://github.com/rh-hideout/pokeemerald-expansion) (RHH and contributors) as the
  reference for several features, and for the follower sprites.
- The DexNav idea comes from later Pokémon games and fan hacks such as Pokémon Unbound.
- The feature code is shared with Gunk's Ruby (same decomp), written by Claude for the Gunk's line.

Pokémon is © Nintendo, Creatures Inc. and GAME FREAK inc. This is a non-commercial fan project and contains
no ROM data; you need your own legally dumped copy of the game.
