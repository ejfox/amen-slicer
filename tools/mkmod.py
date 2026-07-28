#!/usr/bin/env python3
"""
mkmod — generate a tiny original ProTracker .mod (4-channel "M.K.") so the game
has looping music. Butano/maxmod imports it as `bn::music_items::<name>`.

    tools/mkmod.py            # -> audio/theme.mod

One square-wave instrument, a short A-minor-pentatonic loop (moody, vulpes-ish).
This is a starting seed — open it in OpenMPT / MilkyTracker / Furnace to actually
compose. Real tracker music is its own craft; this just proves the pipeline.
"""
import os, struct

# ProTracker period table (finetune 0)
OCT2 = {'C':428,'D':381,'E':339,'F':320,'G':285,'A':254,'B':226}
OCT3 = {'C':214,'D':190,'E':170,'F':160,'G':143,'A':127,'B':113}

def cell(period=0, sample=0, effect=0, param=0):
    b0 = (sample & 0xF0) | ((period >> 8) & 0x0F)
    b1 = period & 0xFF
    b2 = ((sample & 0x0F) << 4) | (effect & 0x0F)
    b3 = param & 0xFF
    return bytes([b0, b1, b2, b3])

EMPTY = cell()

def build():
    # --- one 64-row, 4-channel pattern ---
    rows = [[EMPTY, EMPTY, EMPTY, EMPTY] for _ in range(64)]

    # channel 0: bass root movement (A - G - A - E), sample 1
    bass = [(0, OCT2['A']), (16, OCT2['G']), (32, OCT2['A']), (48, OCT2['E'])]
    for r, p in bass:
        rows[r][0] = cell(period=p, sample=1)

    # channel 1: ascending arpeggio every 4 rows
    arp = [OCT2['A'], OCT3['C'], OCT3['E'], OCT3['A']]
    for i, r in enumerate(range(0, 64, 4)):
        rows[r][1] = cell(period=arp[i % len(arp)], sample=1)

    # set tempo/speed on row 0 (Fxx): speed 6 at row 0 keeps it groovy
    # (leave default; ProTracker defaults to speed 6 / 125 BPM)

    pattern = b"".join(b"".join(r) for r in rows)

    # --- square-wave sample: one cycle, looped ---
    amp = 64
    sample_data = bytes((amp & 0xFF) for _ in range(32)) + bytes(((-amp) & 0xFF) for _ in range(32))
    words = len(sample_data) // 2  # 32 words

    out = bytearray()
    out += b"pk theme".ljust(20, b"\0")                 # song title (20)
    # 31 sample headers (30 bytes each)
    for s in range(31):
        name = b"square".ljust(22, b"\0") if s == 0 else b"\0" * 22
        length = words if s == 0 else 0
        finetune = 0
        volume = 64 if s == 0 else 0
        rep_start = 0
        rep_len = words if s == 0 else 1                # loop whole sample
        out += name
        out += struct.pack(">H", length)
        out += bytes([finetune & 0x0F, volume])
        out += struct.pack(">H", rep_start)
        out += struct.pack(">H", rep_len)
    out += bytes([1])                                    # song length (1 pattern)
    out += bytes([127])                                  # restart position
    out += bytes([0]) + bytes(127)                       # order table: pattern 0, rest 0
    out += b"M.K."                                       # 4-channel magic
    out += pattern                                       # pattern 0
    out += sample_data                                   # sample 1 data
    return bytes(out)

os.makedirs("audio", exist_ok=True)
data = build()
with open("audio/theme.mod", "wb") as f:
    f.write(data)
print(f"wrote audio/theme.mod ({len(data)} bytes)")
