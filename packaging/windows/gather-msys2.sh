#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# After `cargo build --release` in an MSYS2 UCRT64 shell: put bonk.exe, every
# MSYS2 DLL it loads, Qt's plugins and the tool icons in target/windows/Bonk,
# and zip it. Run it from the MSYS2 UCRT64 shell.
set -euo pipefail
cd "$(dirname "$0")/../.."

PREFIX=${MINGW_PREFIX:-/ucrt64}
OUT=target/windows/Bonk
rm -rf "$OUT"
mkdir -p "$OUT/icons"
cp target/release/bonk.exe "$OUT/"
cp assets/tools/*.svg "$OUT/icons/"

QTPLUG="$PREFIX/share/qt6/plugins"
for p in platforms styles imageformats iconengines; do
    [ -d "$QTPLUG/$p" ] && cp -r "$QTPLUG/$p" "$OUT/"
done

# The DLLs bonk.exe and the plugins load, from MSYS2 (repeat until nothing new)
for _ in 1 2 3 4; do
    find "$OUT" -name '*.exe' -o -name '*.dll' | while read -r f; do
        ldd "$f" | awk '{print $3}' | grep -i "^$PREFIX/bin/" || true
    done | sort -u | while read -r dll; do
        [ -f "$OUT/$(basename "$dll")" ] || cp "$dll" "$OUT/"
    done
done

(cd target/windows && rm -f Bonk-windows-x86_64.zip && zip -qr Bonk-windows-x86_64.zip Bonk)
echo "Built target/windows/Bonk-windows-x86_64.zip"
