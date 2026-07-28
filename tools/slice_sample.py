#!/usr/bin/env python3
"""
slice_sample — chop a real audio file into N equal slices for the GBA slicer.
Uses ffmpeg to decode/mono/resample/trim, then writes audio/sliceNN.wav (8-bit
mono) that Butano/maxmod imports as bn::sound_items::sliceNN.

    tools/slice_sample.py INPUT [--start S] [--dur D] [--slices 16] [--rate 16000]

e.g. first bar of a 4-bar amen (6.9674s / 4 = 1.7419s):
    tools/slice_sample.py "AmenVN_4barOrig.wav" --dur 1.7419 --slices 16

Prints the per-step frame count so the sequencer clock can match it.
"""
import subprocess, wave, os, argparse
import numpy as np

ap = argparse.ArgumentParser()
ap.add_argument("input")
ap.add_argument("--start", type=float, default=0.0)
ap.add_argument("--dur",   type=float, default=None)   # None = whole file
ap.add_argument("--slices", type=int,  default=16)
ap.add_argument("--rate",  type=int,   default=16000)
ap.add_argument("--gain",  type=float, default=0.95)
a = ap.parse_args()

cmd = ["ffmpeg", "-v", "error", "-ss", str(a.start)]
if a.dur is not None: cmd += ["-t", str(a.dur)]
cmd += ["-i", a.input, "-ac", "1", "-ar", str(a.rate), "-f", "s16le", "-"]
raw = subprocess.run(cmd, capture_output=True).stdout
x = np.frombuffer(raw, dtype=np.int16).astype(np.float32) / 32768.0
if x.size == 0:
    raise SystemExit("ffmpeg produced no audio — check the path/args")

peak = max(1e-6, float(np.abs(x).max()))
x = np.clip(x / peak * a.gain, -1.0, 1.0)

os.makedirs("audio", exist_ok=True)
# clear any previous slices so a shorter run doesn't leave stragglers
for old in os.listdir("audio"):
    if old.startswith("slice") and old.endswith(".wav"):
        os.remove(os.path.join("audio", old))

slen = x.size // a.slices
FADE = 16
for i in range(a.slices):
    chunk = x[i*slen:(i+1)*slen].copy()
    chunk[:FADE]  *= np.linspace(0, 1, FADE)      # de-click edges
    chunk[-FADE:] *= np.linspace(1, 0, FADE)
    b = np.clip(np.round(128 + chunk * 120), 0, 255).astype(np.uint8).tobytes()
    w = wave.open(f"audio/slice{i:02d}.wav", "wb")
    w.setnchannels(1); w.setsampwidth(1); w.setframerate(a.rate)
    w.writeframes(b); w.close()

print(f"wrote {a.slices} slices from '{os.path.basename(a.input)}'")
print(f"slice = {slen} samples ({slen/a.rate*1000:.1f} ms)  "
      f"frames/step @60fps = {slen/a.rate*60:.4f}")
