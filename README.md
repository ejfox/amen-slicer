# minigame — GBA (Butano) skeleton

Bare Butano skeleton: `bn::core::init()` + an empty update loop. No placeholder
mechanic — this is the base for a minimalist game that may later be embedded in a
larger one. Entry point: `src/main.cpp`.

## Build & playtest (the dev loop)

```bash
cd ~/dev/gba/minigame
make -j8 && open -a mGBA minigame.gba
```

Full compile-to-playing loop is seconds. Edit code in `src/`, rerun.

- `make -j8` — build (uses all cores; `-j$(sysctl -n hw.ncpu)` also works)
- `make clean` — wipe `build/` if something gets weird
- Output ROM: `minigame.gba` (name follows the project folder / TARGET)

**Hot loop — `tools/dev.sh`:** run it once from the project root and just *save*.
It watches `src/ include/ graphics/ audio/` and on every save auto-rebuilds and
reloads the ROM into the running mGBA. Zero deps (`make` + `open` + shell); build
errors print and it keeps watching. Ctrl-C to stop. This is the tight edit→see loop.
```bash
tools/dev.sh
```

## pk.h — the p5-flavored layer (`include/pk.h`)

A tiny creative-coding layer over Butano. You write two functions and call
`pk::run()`; it runs at a fixed 60fps.

```cpp
#include "pk.h"
void pk::setup()  { pk::background(pk::color(2,2,6)); }   // once
void pk::update() { /* every frame */ }                    // 60fps
int  main()       { pk::run(); }
```

Coordinates are **center-origin** (Butano's): x in [-120,120], y in [-80,80],
y grows downward. Handy bounds: `pk::left/right/top/bottom`, `pk::center`.

- **Layout:** `pk::safe_left/right/top/bottom` — an 8px inset (`pk::SAFE`) so UI
  clears the Miyoo Mini's overscan. `pk::col(i,n)` / `pk::row(j,m)` /
  `pk::cell(i,n,j,m)` lay things out on an even grid inside the safe area.
  `Box::pos()` and `Text::print()` both take a `cell(...)` point directly.
- **Layering:** `Text` draws on top (z-order 0); `Box` actors sit behind it
  (z-order 1) so HUD text is never covered. `Box::layer(z)` overrides it
  (higher z = further back).

- **Time:** `pk::frame` (frame count), `pk::seconds()`, `pk::every(n)`
- **Input:** `pk::down/pressed/released(pk::key::A)`, `pk::dx()`, `pk::dy()` (d-pad → -1/0/1)
- **Math:** `random()`, `rnd(a,b)`, `rndi(a,b)`, `chance(p)`, `map()`, `lerp()`,
  `clamp()`, `sin(deg)`, `cos(deg)`, `wave(period,lo,hi)`
- **Color:** `pk::hex(0xRRGGBB)` → GBA 15-bit color, plus the **vulpes palette**
  as named constants: `pk::vulpes::bg` (black), `base`/`pink` (`#e60067`),
  `teal`, `amber`, `fg`, `magenta`, `red`, `chartreuse`, … (from vulpes.nvim,
  squeezed into GBA's 5-bit gamut).
- **Draw:** `pk::background(color)`; `pk::Text` (`clear()`, `print(x,y,str)`,
  `tint(color)` each frame); `pk::Box` — a movable 8x8 sprite actor
  (`pos/move/size/angle/show/fill`). Note: all `Box`es share one palette, so
  `fill()` sets a single global accent color (intentional, minimalist).

GBA is tile/sprite hardware, not a pixel canvas, so there's no immediate-mode
`rect()/ellipse()`. Shapes = sprite actors (kept white; set the mood with
`background()`), and text is real hardware text. `src/main.cpp` is a full demo:
a TITLE⇄PLAY state machine (START toggles), a d-pad-steered player box, and
oscillating floaters (A scatters them).

## Asset pipeline (Aseprite → Butano)

Butano's importer only takes **indexed `.bmp`** (index 0 = transparent); Aseprite
exports **PNG**. `tools/png2bmp.py` bridges them.

1. Draw in Aseprite (or anything), export a PNG with a **transparent background**.
   Stack animation frames vertically (e.g. 16×64 = four 16px frames).
2. Convert: `python3 tools/png2bmp.py art/thing.png graphics/thing.bmp`
   (quantizes to ≤16 colors, maps alpha-0 pixels to palette index 0).
3. Describe it: `graphics/thing.json` → `{ "type": "sprite", "height": 16 }`
   (omit `height` for a single-frame sprite; `"type": "regular_bg"` for backgrounds).
4. Use it: `#include "bn_sprite_items_thing.h"` →
   `bn::sprite_items::thing.create_sprite(x, y)`. Animate with
   `bn::create_sprite_animate_action_forever(sprite, wait, item.tiles_item(), 0,1,2,3)`
   and call `.update()` each frame.

Worked example in the repo: `art/coin.png` → `graphics/coin.bmp` → a 4-frame
spinning coin (see `src/main.cpp`, top-right). `art/` holds PNG sources;
`graphics/` holds the generated BMPs Butano compiles.

> [!tip] Design *to* the palette. GBA sprites are 16 colors (4bpp). Pick a small
> palette and commit — that constraint is the retro look.

## Audio pipeline

Drop files in `audio/` — no JSON needed. maxmod converts them at build time:
- **`*.wav`** (8-bit mono) → `bn::sound_items::<name>` — sound effects
- **`*.mod/.xm/.it/.s3m`** (tracker modules) → `bn::music_items::<name>` — music

Play through `pk`:
```cpp
#include "bn_sound_items.h"
#include "bn_music_items.h"
pk::sfx(bn::sound_items::blip, 0.7);            // one-shot
pk::music(bn::music_items::theme, 0.5);        // looping bg music
pk::music_stop();  pk::music_volume(0.3);
```

Generators in `tools/` (regenerate anytime):
- `python3 tools/mksfx.py` — sfxr-style SFX synth → `audio/{blip,coin,jump,hit}.wav`
- `python3 tools/mkmod.py` — a starter chiptune → `audio/theme.mod`

> [!note] These generators are seeds. For real sound design use interactive tools:
> **jsfxr / bfxr / ChipTone** (web) for SFX, **OpenMPT / MilkyTracker / Furnace**
> for tracker music — then export/save into `audio/` and rebuild.

## Editor / LSP setup (VSCode + Neovim)

Code intelligence (completion, go-to-def, hover, errors) is **clangd**, configured
by the project's `.clangd` file — which gives clangd the ARM cross-compile include
paths, defines, and `-std=c++23`. Both editors read `.clangd` automatically.

- **VSCode:** install the recommended **clangd** extension (prompted on open, or
  see `.vscode/extensions.json`). `.vscode/settings.json` points at Homebrew's
  clangd and disables MS C/C++ IntelliSense so they don't clash. `.vscode/tasks.json`
  has **build**, **build + run (mGBA)**, **hot loop**, and **clean** (⌘⇧B = build).
- **Neovim (LazyVim):** the `lang.clangd` extra is enabled — open nvim once and let
  Mason install `clangd`; it picks up `.clangd` for free.

> [!note] Build once before relying on the LSP. The generated headers
> (`bn_sprite_items_*.h`, `bn_music_items.h`, …) live in `build/`, which `.clangd`
> adds to the include path — they only exist after a build. Verified with
> `clangd --check=src/main.cpp` → **0 errors**. If the toolchain version bumps
> past 16.1.0, update the paths in `.clangd` (regenerate flags via `make -n`).

## Embedding in a larger game

Longer term this may live inside a bigger **Unity** game as a minigame. The plan
(run the real ROM via a libretro mGBA core rendered to a RenderTexture) and the
contract that keeps the ROM embed-ready are in [`EMBEDDING.md`](EMBEDDING.md).
Nothing to do now — just don't break the contract (fixed 240×160, standard
buttons, plain `.gba`), which `pk` already honors.

## Reference

- ~70 small examples in `~/dev/gba/butano/examples/` — sprites, text, affine
  transforms, blending, mosaic: the whole minimalist-aesthetic toolkit.
- Butano docs: https://gvaliente.github.io/butano/

## Toolchain (already set up on this iMac)

- devkitPro / devkitARM (`arm-none-eabi-gcc`), env in `/etc/profile.d/`:
  `DEVKITPRO=/opt/devkitpro`, `DEVKITARM=$DEVKITPRO/devkitARM`,
  `PATH` includes `$DEVKITPRO/tools/bin`.
- mGBA emulator: `open -a mGBA <rom>.gba` (alias `gba` added to ~/.zshrc).
- Makefile is pinned to the absolute Butano lib path
  (`/Users/ejfox/dev/gba/butano/butano`) and `PYTHON := python3`.

## Getting it onto the Miyoo Mini

**One command** (build + copy + eject) once the SD card is mounted:
```bash
tools/deploy.sh
# card mounts under an odd name? point at it:
MIYOO_SD=/Volumes/ONION/Roms/GBA tools/deploy.sh
```
It auto-finds `/Volumes/*/Roms/GBA`, copies the fresh ROM, and ejects the card so
you can pull it straight out. If no card is inserted it prints what to do instead
of failing silently.

Or do it by hand:

1. Mount the Miyoo's SD card on the iMac.
2. Copy the built ROM into `Roms/GBA/`:
   ```bash
   cp ~/dev/gba/minigame/minigame.gba /Volumes/<SDCARD>/Roms/GBA/
   ```
3. Eject, reinsert into the Miyoo — it appears in the GBA games list like any
   other ROM.
4. **Core note:** Onion OS defaults GBA to the `gpSP` core (fast but occasionally
   glitchy with homebrew). If the game misbehaves on the Miyoo but was fine in
   mGBA: open the game, press `Menu + Select` for the RetroArch menu, and switch
   the core to `mGBA`. For a minimalist game the mGBA core runs full speed.
