#!/usr/bin/env python3
"""
mkbreak — synth an amen-style breakbeat and slice it into 16 sixteenth-notes.
Each slice becomes audio/sliceNN.wav -> bn::sound_items::sliceNN, so the GBA can
sequence / rearrange / stutter them. Original synth drums (no copyrighted sample).

    tools/mkbreak.py

Prints the slice length so the sequencer's step timing can match it.
"""
import os, math, random, struct, wave

RATE = 16000
BPM  = 138
STEPS = 16
SIXTEENTH = 60.0 / BPM / 4.0                 # seconds per 16th note
SLICE = round(SIXTEENTH * RATE)              # samples per slice
BUFLEN = SLICE * STEPS
random.seed(303)                             # deterministic (TB-303 wink)

def env(t, rate):        return math.exp(-t * rate)
def noise():             return random.uniform(-1, 1)

def kick(dur=0.16):
    out, ph = [], 0.0
    n = int(dur * RATE)
    for i in range(n):
        t = i / RATE
        f = 45 + 55 * env(t, 42)             # pitch drop 100->45 Hz
        ph += 2 * math.pi * f / RATE
        out.append(math.sin(ph) * env(t, 20))
    return out

def snare(dur=0.14):
    n = int(dur * RATE)
    return [(0.5 * math.sin(2*math.pi*185*(i/RATE)) + 0.8*noise()) * env(i/RATE, 26)
            for i in range(n)]

def chat(dur=0.045):                          # closed hat: short noise tick
    n = int(dur * RATE)
    return [noise() * env(i/RATE, 130) * 0.9 for i in range(n)]

def ohat(dur=0.16):                           # open hat: longer noise
    n = int(dur * RATE)
    return [noise() * env(i/RATE, 16) * 0.8 for i in range(n)]

K, S, C, O = kick(), snare(), chat(), ohat()

# --- an amen-flavored 16-step break (K kick, S snare, c closed hat, o open hat) ---
pattern = {
    'K': ([0, 3, 6, 10],      K, 1.00),
    'S': ([4, 7, 12, 14],     S, 0.90),
    'c': ([0,2,4,6,8,10,12],  C, 0.40),
    'o': ([15],               O, 0.55),
}

buf = [0.0] * BUFLEN
for steps, samp, gain in pattern.values():
    for st in steps:
        start = st * SLICE
        for j, v in enumerate(samp):
            buf[(start + j) % BUFLEN] += v * gain   # wrap tails -> seamless loop

peak = max(1e-6, max(abs(v) for v in buf))
buf = [max(-1.0, min(1.0, v / peak * 0.95)) for v in buf]   # normalize

def to8(x): return max(0, min(255, int(128 + x * 120)))

os.makedirs("audio", exist_ok=True)
FADE = 12                                     # samples of edge fade to kill clicks
for i in range(STEPS):
    chunk = buf[i*SLICE:(i+1)*SLICE][:]
    for j in range(FADE):                     # short in/out fades
        chunk[j]            *= j / FADE
        chunk[-1-j]         *= j / FADE
    w = wave.open(f"audio/slice{i:02d}.wav", "wb")
    w.setnchannels(1); w.setsampwidth(1); w.setframerate(RATE)
    w.writeframes(bytes(to8(x) for x in chunk))
    w.close()

print(f"wrote 16 slices -> audio/slice00..15.wav")
print(f"BPM={BPM}  slice={SLICE} samples ({SIXTEENTH*1000:.1f} ms)  "
      f"frames/step @60fps = {SIXTEENTH*60:.4f}")
