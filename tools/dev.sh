#!/usr/bin/env bash
# dev.sh — hot loop for GBA dev. Watches sources, rebuilds on save, reloads the
# ROM into a running mGBA. Zero dependencies: just make + open + shell.
#
#   tools/dev.sh        # run from the project root; Ctrl-C to stop
#
# Save a file in src/ include/ graphics/ audio/ (or the Makefile) and the game
# rebuilds and reloads in mGBA automatically. Build errors print and it keeps
# watching — fix, save, and it retries.
set -u

export DEVKITPRO=${DEVKITPRO:-/opt/devkitpro}
export DEVKITARM=$DEVKITPRO/devkitARM
export PATH=$DEVKITPRO/tools/bin:$PATH

ROM="$(basename "$PWD").gba"
WATCH="src include graphics audio Makefile"
JOBS=$(sysctl -n hw.ncpu 2>/dev/null || echo 4)

# fingerprint of all watched files' mtime+size — changes when you save
sig() { find $WATCH -type f -exec stat -f '%m %z %N' {} + 2>/dev/null | sort | md5; }

echo "pk hot loop → watching: $WATCH"
echo "edit & save to rebuild+reload $ROM · Ctrl-C to stop"

last=""
while true; do
  cur=$(sig)
  if [ "$cur" != "$last" ]; then
    last=$cur
    echo "──▶ $(date +%H:%M:%S) building…"
    if make -j"$JOBS"; then
      open -a mGBA "$ROM" && echo "    ✓ reloaded $ROM"
    else
      echo "    ✗ build failed — fix & save to retry"
    fi
  fi
  sleep 0.5
done
