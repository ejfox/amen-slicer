#!/usr/bin/env bash
# deploy.sh — build the ROM, copy it onto the Miyoo Mini's SD card, and eject.
# One command to go from code -> playtesting on real hardware.
#
#   tools/deploy.sh
#
# Auto-finds the card at /Volumes/*/Roms/GBA. If your card mounts under a name
# it can't guess, point at it directly:
#   MIYOO_SD=/Volumes/ONION/Roms/GBA tools/deploy.sh
set -u

export DEVKITPRO=${DEVKITPRO:-/opt/devkitpro}
export DEVKITARM=$DEVKITPRO/devkitARM
export PATH=$DEVKITPRO/tools/bin:$PATH

ROM="$(basename "$PWD").gba"
JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)

# 1. build
echo "building ${ROM} ..."
make -j"$JOBS" >/dev/null || { echo "✗ build failed (run 'make' to see errors)"; exit 1; }
[ -f "$ROM" ] || { echo "✗ $ROM not produced"; exit 1; }

# 2. locate the card's GBA folder (override wins)
DEST="${MIYOO_SD:-$(ls -d /Volumes/*/Roms/GBA 2>/dev/null | head -1)}"
if [ -z "$DEST" ]; then
  echo "✗ no Miyoo SD card found."
  echo "  Insert the card and retry, or point at it explicitly:"
  echo "    MIYOO_SD=/Volumes/<CARD>/Roms/GBA tools/deploy.sh"
  echo "  mounted volumes: $(ls /Volumes 2>/dev/null | tr '\n' ' ')"
  exit 1
fi
mkdir -p "$DEST"

# 3. copy
cp "$ROM" "$DEST"/ || { echo "✗ copy failed"; exit 1; }
echo "✓ copied $ROM → $DEST"

# 4. eject (only real /Volumes cards; skip for test dirs)
case "$DEST" in
  /Volumes/*)
    CARD=$(printf '%s' "$DEST" | sed -E 's#(/Volumes/[^/]+)/.*#\1#')
    if diskutil eject "$CARD" >/dev/null 2>&1; then
      echo "✓ ejected $CARD — reinsert into the Miyoo, it's in the GBA list"
    else
      echo "! copied, but couldn't auto-eject $CARD — eject it manually, then reinsert"
    fi
    ;;
  *)
    echo "  (dest not under /Volumes — skipped eject)"
    ;;
esac
echo "done."
