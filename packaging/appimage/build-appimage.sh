#!/bin/bash
# Bonk as an AppImage (Qt inside), from a release build:
#   cargo build --release && packaging/appimage/build-appimage.sh
# Needs Qt 6 development files (qmake6) with the xcb and Wayland platform
# plugins, and downloads linuxdeploy and its Qt plugin the first time.
# Writes Bonk-<version>-x86_64.AppImage in target/appimage.
set -euo pipefail
src="$(cd "$(dirname "$0")/../.." && pwd)"
id="io.github.ffseestevanli_oss.bonk"
ver="$(sed -n 's/^version *= *"\(.*\)"/\1/p' "$src/Cargo.toml" | head -1)"
out="$src/target/appimage"
tools="$src/target/appimage-tools"
bin="$src/target/release/bonk"
[ -x "$bin" ] || { echo "Build Bonk first: cargo build --release" >&2; exit 1; }

mkdir -p "$tools"
fetch() { # name url
    [ -x "$tools/$1" ] && return
    curl -fsSL -o "$tools/$1" "$2"
    chmod +x "$tools/$1"
}
fetch linuxdeploy-x86_64.AppImage https://github.com/linuxdeploy/linuxdeploy/releases/download/continuous/linuxdeploy-x86_64.AppImage
fetch linuxdeploy-plugin-qt-x86_64.AppImage https://github.com/linuxdeploy/linuxdeploy-plugin-qt/releases/download/continuous/linuxdeploy-plugin-qt-x86_64.AppImage
fetch linuxdeploy-plugin-appimage-x86_64.AppImage https://github.com/linuxdeploy/linuxdeploy-plugin-appimage/releases/download/continuous/linuxdeploy-plugin-appimage-x86_64.AppImage

rm -rf "$out"
app="$out/AppDir"
install -Dm755 "$bin" "$app/usr/bin/bonk"
install -Dm644 "$src/assets/bonk.svg" "$app/usr/share/icons/hicolor/scalable/apps/$id.svg"
install -Dm644 "$src/packaging/linux/$id.desktop" "$app/usr/share/applications/$id.desktop"
install -Dm644 "$src/packaging/linux/$id.SpawnlistCreator.desktop" "$app/usr/share/applications/$id.SpawnlistCreator.desktop"
install -Dm644 "$src/packaging/linux/$id.metainfo.xml" "$app/usr/share/metainfo/$id.metainfo.xml"
install -Dm644 "$src/packaging/linux/$id.mime.xml" "$app/usr/share/mime/packages/$id.xml"

export PATH="$tools:$PATH"
export QMAKE="${QMAKE:-$(command -v qmake6 || command -v qmake)}"
# Wayland sessions run Bonk through XWayland (mouselook needs cursor warping), so xcb
# is the one that matters; the Wayland plugins come along for BONK_WAYLAND=1.
export EXTRA_PLATFORM_PLUGINS="${EXTRA_PLATFORM_PLUGINS-libqwayland-egl.so;libqwayland-generic.so}"
export LDAI_OUTPUT="Bonk-$ver-x86_64.AppImage"
export APPIMAGE_EXTRACT_AND_RUN=1 # no FUSE needed while building
cd "$out"
linuxdeploy-x86_64.AppImage --appdir "$app" --plugin qt --output appimage \
    -d "$app/usr/share/applications/$id.desktop" -i "$app/usr/share/icons/hicolor/scalable/apps/$id.svg"
echo "==> $out/$LDAI_OUTPUT"
