#!/usr/bin/env python3
"""
mksfx — a tiny sfxr-style sound-effect synth. Generates 8-bit mono WAVs that
Butano/maxmod imports as `bn::sound_items::<name>` (just drop them in audio/).

    tools/mksfx.py            # (re)generate the starter set into audio/

This is deliberately minimal — for real sound design use an interactive synth
(jsfxr / bfxr / ChipTone) and export WAV here instead. Each effect below is a
readable template: tweak freq/decay/sweep and re-run.
"""
import os, math, struct, wave

RATE = 16000

def square(t, freq, duty=0.5):
    return 1.0 if (t * freq) % 1.0 < duty else -1.0

def render(samples):
    """samples: list of floats in [-1,1] -> 8-bit unsigned bytes."""
    return bytes(max(0, min(255, int(128 + s * 120))) for s in samples)

def tone(freq_fn, dur, amp_fn, duty=0.5):
    n = int(RATE * dur)
    out = []
    for i in range(n):
        t = i / RATE
        out.append(square(t, freq_fn(t), duty) * amp_fn(t))
    return out

def save(name, samples):
    os.makedirs("audio", exist_ok=True)
    path = f"audio/{name}.wav"
    w = wave.open(path, "wb")
    w.setnchannels(1); w.setsampwidth(1); w.setframerate(RATE)
    w.writeframes(render(samples))
    w.close()
    print(f"  audio/{name}.wav  ({len(samples)/RATE:.2f}s)")

def decay(dur, curve=3.0):
    return lambda t: max(0.0, 1.0 - (t / dur)) ** curve

print("mksfx ->")
# blip: short high square, quick decay (menu/confirm)
save("blip", tone(lambda t: 900, 0.08, decay(0.08)))

# coin: low note then a higher sustained note (classic pickup)
lo = tone(lambda t: 780, 0.05, lambda t: 1.0)
hi = tone(lambda t: 1180, 0.28, decay(0.28, 1.5))
save("coin", lo + hi)

# jump: rising pitch sweep
save("jump", tone(lambda t: 400 + 1400 * t / 0.22, 0.22, decay(0.22, 1.2)))

# hit: descending noisy-ish square (bump/scatter)
save("hit", tone(lambda t: 500 - 900 * t / 0.15, 0.15, decay(0.15, 2.0), duty=0.25))
