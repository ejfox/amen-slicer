#!/usr/bin/env python3
"""
png2bmp — convert an Aseprite/PNG export into a Butano-ready indexed BMP.

Butano's graphics importer only accepts indexed .bmp, with palette index 0 =
transparent. Aseprite exports PNG (with alpha). This bridges the two:

    tools/png2bmp.py art/hero.png graphics/hero.bmp
    tools/png2bmp.py art/hero.png                 # -> graphics/hero.bmp
    tools/png2bmp.py art/hero.png --colors 16 --transparent ff00ff

- Fully-transparent pixels (alpha 0) OR the --transparent color become index 0.
- The remaining opaque colors are quantized to (--colors - 1) entries, indices 1+.
- Output palette size stays <= 16 (4bpp) by default so sprites are cheap.

Then add a sibling <name>.json describing the asset, e.g.:
    { "type": "sprite", "height": 16 }     # frames stack vertically, 16px each
    { "type": "regular_bg" }

Requires Pillow (`pip install pillow`).
"""
import sys, os, argparse
from PIL import Image


def parse_hex(s):
    s = s.lstrip("#")
    return (int(s[0:2], 16), int(s[2:4], 16), int(s[4:6], 16))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("input")
    ap.add_argument("output", nargs="?")
    ap.add_argument("--colors", type=int, default=16,
                    help="total palette size incl. transparent (<=16 for 4bpp, <=256 for 8bpp)")
    ap.add_argument("--transparent", default=None,
                    help="RRGGBB treated as transparent (in addition to alpha 0)")
    args = ap.parse_args()

    out = args.output or os.path.join("graphics",
                                      os.path.splitext(os.path.basename(args.input))[0] + ".bmp")

    src = Image.open(args.input).convert("RGBA")
    w, h = src.size
    px = src.load()
    key = parse_hex(args.transparent) if args.transparent else None

    # Collect opaque pixels and quantize them to (colors-1) entries.
    opaque = Image.new("RGB", (w, h), (0, 0, 0))
    op = opaque.load()
    is_transparent = [[False] * w for _ in range(h)]
    for y in range(h):
        for x in range(w):
            r, g, b, a = px[x, y]
            if a == 0 or (key and (r, g, b) == key):
                is_transparent[y][x] = True
            else:
                op[x, y] = (r, g, b)

    q = opaque.quantize(colors=max(1, args.colors - 1), method=Image.MEDIANCUT)
    qpal = q.getpalette()[: (args.colors - 1) * 3]
    qidx = q.load()

    # Build final P image: index 0 transparent, opaque colors shifted to 1..N.
    out_img = Image.new("P", (w, h), 0)
    oi = out_img.load()
    for y in range(h):
        for x in range(w):
            oi[x, y] = 0 if is_transparent[y][x] else qidx[x, y] + 1

    palette = [255, 0, 255] + qpal                    # index 0 = magenta (transparent slot)
    palette += [0, 0, 0] * (256 - len(palette) // 3)  # pad to 256 entries
    out_img.putpalette(palette)

    os.makedirs(os.path.dirname(out) or ".", exist_ok=True)
    out_img.save(out)
    used = {oi[x, y] for y in range(h) for x in range(w)}
    print(f"{args.input} -> {out}  ({w}x{h}, {len(used)} colors incl. transparent)")


if __name__ == "__main__":
    main()
