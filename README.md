# AMEN SLICER

A breakbeat slicer / performance instrument for the **Game Boy Advance**. Thirteen
real amen breaks, each chopped into 16 sixteenth-notes, on a sync-locked clock —
with live repitch, beat-repeat stutters, breakdown FX, a downsample/bitcrush, and
a reactive generative visualizer. Atomic-purple on OLED black.

Built in C++ with the [Butano](https://github.com/GValiente/butano) engine and a
small p5.js-flavored layer (`include/pk.h`). Runs on real hardware and the Miyoo Mini.

## Controls

| Button | Action |
|---|---|
| **Up / Down** | tempo **±1 BPM** per tap (hold to scroll); repitches the break to hit it |
| **Left / Right** | previous / next flavor — **tempo stays locked** across flavors |
| **A / B / L / R** | stutter **1/16 · 1/32 · 1/8 · 1/64** (hold) |
| **hold stutter + arrow** | breakdown FX — **Up** = build, **Down** = tape-stop, **L/R** = pitch |
| **START** | tap the loop back to the **1** (align to an external downbeat) |
| **SELECT** | toggle **EXPLAIN ⇄ ZEN** view |
| **SELECT + START** | toggle **CRUSH** (downsample / bitcrush) |

**Zen combos:** A+B → glyphs become **+ signs**, L+R → mandala turns **inside-out**,
A+B+L+R → **chaos**. Downbeats and drops fire shockwaves; a tape-stop freezes the
whole visual with the audio.

## Flavors

`clean · winston · 45rpm · 33rpm · cleaned · vinyl · tape · 2000xl · crush · soft ·
freak (170) · skull (165) · toptape (150)` — original vinyl/cassette/CD rips plus
retempo'd remixes, each at its own native BPM.

## Sync — how the clock actually works

BPM is the **canonical value**, not a side-effect. Tap Up/Down to set an integer
BPM; the engine derives the sample repitch from it (`speed = bpm × step / BPM_K`),
so the master interval is `BPM_K / bpm` frames per slice — **independent of which
break is loaded**. That's why switching flavors holds tempo instead of jumping.

The **master phase advances at a constant rate**, gated to nothing. FX (tape-stop,
beat-repeat, build) drive only the *visuals* — the musical grid keeps true time
underneath them. So when you release an FX, playback **snaps back onto the current
beat** instead of drifting. This is the whole point for live use: it stays on the
grid no matter what you throw at it.

Workflow with other gear: dial the same BPM with Up/Down, then tap **START** on
their downbeat to align the "1". (It's a free-running clock — like a drum machine
with no MIDI-in. True external clock lock would need link-cable MIDI, not
implemented.)

> **History:** until Aug 2026 the clock advanced by a `vmotion` factor that FX
> ramped to zero, so the grid itself froze during FX and *permanently* lost time —
> "the FX falls out of time." Fixed by locking the clock to a constant rate and
> making `vmotion` visual-only. See commit `b754b32`.

## Build & run

```bash
make -j8 && open -a mGBA amen.gba     # or: tools/dev.sh for a save->reload hot loop
```
Deploy to the Miyoo Mini: `tools/deploy.sh` — builds, **boot-tests in mGBA**,
copies to the SD card as `Amen Slicer.gba`, **wipes the stale gpSP save-state**
(see Troubleshooting), and ejects. One command, no black screens.

Requires devkitPro `gba-dev` + the Butano repo at `../butano`. Release builds use
LTO (`-flto`, set in the Makefile).

## Troubleshooting

**Black screen with audio ticking after a rebuild ("crashes, doesn't draw").**
Not a code bug — the Miyoo's **gpSP** core auto-saves a resume state on exit and
auto-loads it on launch. After you rebuild, that state is from the *old* binary and
poisons the new one. Fix: delete it (or just don't "resume" on the device):

```
/Volumes/<CARD>/Saves/CurrentProfile/states/gpSP/Amen Slicer.state*
```

`tools/deploy.sh` does this automatically. If you copy the ROM by hand, wipe the
state yourself. Verify the ROM is actually fine first with `open -a mGBA amen.gba` —
mGBA boots fresh (no auto-state), so if it runs there, it's the state, not the code.

**Game not showing in the GBA list after adding it.** The launcher reads
`Roms/GBA/miyoogamelist.xml` (authoritative) — a loose ROM not listed there is
invisible, and rebuilding the cache won't help. Add a `<game>` entry, or delete the
XML to fall back to a raw folder scan.

## Tools (`tools/`)

- **`make_banks.py`** — slices the amen sources into 13 banks × 16 slices (clean +
  crushed), bakes each bank's tempo, amplitude envelope, and waveform sprite, and
  generates `include/banks.h`. Edit the `BANKS` list to swap sources.
- `slice_sample.py` · `png2bmp.py` · `mksfx.py` · `mkmod.py` — supporting asset tools.
- `dev.sh` (hot loop) · `deploy.sh` (to Miyoo).

## Credits & license

- **Code** (`src/`, `include/pk.h`, `tools/`): © EJ Fox — MIT (see `LICENSE`).
- **Engine:** [Butano](https://github.com/GValiente/butano) by Gustavo Valiente (zlib).
- **Amen break samples:** from *Rhythm Lab — The Ultimate Amen Breaks Pack*,
  originally **"Amen, Brother" by The Winstons (1969)**. These are **not** included
  in this repo (git-ignored) — they're sliced locally from the owner's copy via
  `make_banks.py`. A built ROM contains this copyrighted audio, so **it's for
  personal / educational use** and not for redistribution without clearance.
