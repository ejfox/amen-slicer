#!/usr/bin/env bash
# deploy.sh — build the ROM, boot-test it in mGBA, copy it onto the Miyoo Mini's
# SD card under the right name, CLEAR the stale save-state, and eject.
# One command to go from code -> playtesting on real hardware, without the
# black-screen-on-resume trap (see "why the save-state wipe" below).
#
#   tools/deploy.sh                 # build + test + deploy
#   tools/deploy.sh --no-test       # skip the mGBA boot test
#
# Auto-finds the card at /Volumes/*/Roms/GBA. Override if it can't guess:
#   MIYOO_SD=/Volumes/ONION-V4_3_/Roms/GBA tools/deploy.sh
#
# ── why the save-state wipe ──────────────────────────────────────────────────
# The Miyoo runs GBA via the gpSP core, which auto-saves a resume state on exit
# and auto-LOADS it on launch. After you rebuild, that state is from the OLD
# binary — loading it into the new ROM = black screen with audio still ticking
# ("crashes, doesn't draw"). The ROM is fine; the stale state is poison. So on
# every deploy we delete it and the game boots clean. (Cost us an evening once.)
set -u

export DEVKITPRO=${DEVKITPRO:-/opt/devkitpro}
export DEVKITARM=$DEVKITPRO/devkitARM
export PATH=$DEVKITPRO/tools/bin:$PATH

# On-card ROM name (must match the entry in Roms/GBA/miyoogamelist.xml).
CARD_ROM="${CARD_ROM:-Amen Slicer.gba}"
CORE="${CORE:-gpSP}"                       # Miyoo GBA core → where states/saves live
MGBA="/Applications/mGBA.app/Contents/MacOS/mGBA"

ROM="$(basename "$PWD").gba"               # local build artifact, e.g. amen.gba
JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)
DO_TEST=1; [ "${1:-}" = "--no-test" ] && DO_TEST=0

# 1. build
echo "building ${ROM} ..."
make -j"$JOBS" >/dev/null || { echo "✗ build failed (run 'make' to see errors)"; exit 1; }
[ -f "$ROM" ] || { echo "✗ $ROM not produced"; exit 1; }

# 2. boot-test in mGBA (catches crashes BEFORE the card round-trip)
if [ "$DO_TEST" = 1 ] && [ -x "$MGBA" ]; then
  echo "boot-testing in mGBA (max log level) ..."
  LOG=$(mktemp)
  "$MGBA" -l 511 "$ROM" >"$LOG" 2>&1 &
  MPID=$!; sleep 4; kill $MPID 2>/dev/null
  if grep -qiE 'fatal|guru|undefined instruction|unhandled|abort' "$LOG"; then
    echo "✗ mGBA reported a fatal error — NOT deploying. See:"; grep -iE 'fatal|guru|undefined|unhandled|abort' "$LOG" | head; rm -f "$LOG"; exit 1
  fi
  echo "✓ booted clean in mGBA"; rm -f "$LOG"
fi

# 3. locate the card's GBA folder (override wins)
DEST="${MIYOO_SD:-$(ls -d /Volumes/*/Roms/GBA 2>/dev/null | head -1)}"
if [ -z "$DEST" ]; then
  echo "✗ no Miyoo SD card found. Insert it, or point at it explicitly:"
  echo "    MIYOO_SD=/Volumes/<CARD>/Roms/GBA tools/deploy.sh"
  echo "  mounted volumes: $(ls /Volumes 2>/dev/null | tr '\n' ' ')"
  exit 1
fi
CARDROOT=$(printf '%s' "$DEST" | sed -E 's#(/Volumes/[^/]+)/.*#\1#')

# 4. copy (COPYFILE_DISABLE stops macOS from writing ._AppleDouble junk to exFAT)
COPYFILE_DISABLE=1 cp "$ROM" "$DEST/$CARD_ROM" || { echo "✗ copy failed"; exit 1; }
rm -f "$DEST/._$CARD_ROM"
echo "✓ copied $ROM → $DEST/$CARD_ROM"

# 5. wipe the stale gpSP resume state (see header) — the anti-black-screen step
NAME="${CARD_ROM%.gba}"
STATES="$CARDROOT/Saves/CurrentProfile/states/$CORE"
if compgen -G "$STATES/$NAME.state"* >/dev/null 2>&1; then
  rm -f "$STATES/$NAME.state"*
  echo "✓ cleared stale $CORE save-state → clean boot"
else
  echo "· no $CORE save-state to clear (fresh)"
fi

# 6. eject
if diskutil eject "$CARDROOT" >/dev/null 2>&1; then
  echo "✓ ejected $CARDROOT — reinsert into the Miyoo, it's in the GBA list"
else
  echo "! copied, but couldn't auto-eject $CARDROOT — eject it manually"
fi
echo "done."
