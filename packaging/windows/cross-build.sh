#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Cross-compile Bonk for Windows from Linux with MinGW-w64 and a MinGW build of
# Qt 6, then gather bonk.exe, its DLLs and Qt's plugins into one folder.
#
#   QT_WIN=/path/to/qt6-mingw packaging/windows/cross-build.sh
#
# Needs: the x86_64-pc-windows-gnu Rust target (rustup target add
# x86_64-pc-windows-gnu), mingw-w64 (posix threads), and Qt 6 built for MinGW
# with the same version as the Linux Qt's host tools. Output: target/windows/Bonk/
set -euo pipefail
cd "$(dirname "$0")/../.."

: "${QT_WIN:?set QT_WIN to the MinGW Qt 6 install (the folder with bin/, lib/, plugins/)}"
TARGET=x86_64-pc-windows-gnu
export QMAKE="$QT_WIN/bin/qmake"
export CXX="${CXX:-x86_64-w64-mingw32-g++-posix}"
export AR="${AR:-x86_64-w64-mingw32-ar}"
export CARGO_TARGET_X86_64_PC_WINDOWS_GNU_LINKER="${CARGO_TARGET_X86_64_PC_WINDOWS_GNU_LINKER:-x86_64-w64-mingw32-gcc-posix}"

cargo build --release --target "$TARGET"

OUT=target/windows/Bonk
rm -rf "$OUT"
mkdir -p "$OUT"
cp "target/$TARGET/release/bonk.exe" "$OUT/"
mkdir -p "$OUT/icons"
cp assets/tools/*.svg "$OUT/icons/"

# Qt and its plugins (platforms is required; the rest are nice to have)
for m in Core Gui Widgets OpenGL OpenGLWidgets Svg; do
    [ -f "$QT_WIN/bin/Qt6$m.dll" ] && cp "$QT_WIN/bin/Qt6$m.dll" "$OUT/"
done
for p in platforms styles imageformats iconengines; do
    [ -d "$QT_WIN/plugins/$p" ] && cp -r "$QT_WIN/plugins/$p" "$OUT/"
done
# drop debug-only and unneeded plugins if any
find "$OUT" -name '*.debug' -delete

# MinGW's runtime DLLs
GCC_DIR=$(dirname "$("$CXX" -print-libgcc-file-name)")
for dll in libstdc++-6.dll libgcc_s_seh-1.dll; do
    cp "$GCC_DIR/$dll" "$OUT/" 2>/dev/null || cp "$("$CXX" -print-file-name=$dll)" "$OUT/"
done
WINPTHREAD=$("$CXX" -print-file-name=libwinpthread-1.dll)
[ -f "$WINPTHREAD" ] || WINPTHREAD=/usr/x86_64-w64-mingw32/lib/libwinpthread-1.dll
cp "$WINPTHREAD" "$OUT/"

echo "Built $OUT"
ls "$OUT"
