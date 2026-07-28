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
| **Up / Down** | tempo / stretch (repitch — pitch *and* tempo; live BPM readout) |
| **Left / Right** | previous / next flavor |
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

## Sync

Free-running clock matched to a target BPM (like a drum machine with no MIDI-in).
A **master phase always advances on the grid**, so stutters/FX never knock the loop
off-time. Set the same BPM as your other gear with Up/Down, then tap **START** on
their downbeat to lock in. (True external clock lock would need link-cable MIDI —
not implemented.)

## Build & run

```bash
make -j8 && open -a mGBA amen.gba     # or: tools/dev.sh for a save->reload hot loop
```
Deploy to the Miyoo Mini: `tools/deploy.sh` (build + copy to the SD card + eject).

Requires devkitPro `gba-dev` + the Butano repo at `../butano`. Release builds use
LTO (`-flto`, set in the Makefile).

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
