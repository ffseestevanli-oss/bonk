#!/bin/bash
# Bonk as two AppImages (Qt inside), from one release build:
#   Bonk-<version>-x86_64.AppImage                    the editor
#   Bonk-Spawnlist-Creator-<version>-x86_64.AppImage  the Spawnlist Creator on its own
#   cargo build --release && packaging/appimage/build-appimage.sh
# Needs Qt 6 development files (qmake6) with the xcb and Wayland platform
# plugins and Qt SVG (Ubuntu: qt6-svg-dev), and downloads linuxdeploy and its
# Qt plugin the first time. Writes both into target/appimage.
# (The Flatpak keeps both in one package.)
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

export PATH="$tools:$PATH"
export QMAKE="${QMAKE:-$(command -v qmake6 || command -v qmake)}"
# the icons are SVG: bring Qt's SVG image and icon plugins along
export EXTRA_QT_MODULES="${EXTRA_QT_MODULES-svg}"
export EXTRA_QT_PLUGINS="${EXTRA_QT_PLUGINS-svg;iconengines;imageformats}"
# Wayland sessions run Bonk through XWayland (mouselook needs cursor warping), so xcb
# is the one that matters; the Wayland plugins come along for BONK_WAYLAND=1.
export EXTRA_PLATFORM_PLUGINS="${EXTRA_PLATFORM_PLUGINS-libqwayland-egl.so;libqwayland-generic.so}"
export APPIMAGE_EXTRACT_AND_RUN=1 # no FUSE needed while building

rm -rf "$out"
mkdir -p "$out"

# build <AppDir name> <desktop file> <output name> [creator]
build() {
    local app="$out/$1" desktop="$2" name="$3" creator="${4:-}"
    install -Dm755 "$bin" "$app/usr/bin/bonk"
    # the tool panel's icons and the window icon sit next to the program (where Bonk looks)
    install -d "$app/usr/bin/icons"
    install -m644 "$src"/assets/tools/*.svg "$app/usr/bin/icons/"
    install -m644 "$src/assets/bonk.svg" "$app/usr/bin/bonk.svg"
    install -Dm644 "$src/assets/bonk.svg" "$app/usr/share/icons/hicolor/scalable/apps/$id.svg"
    install -Dm644 "$src/packaging/linux/$desktop" "$app/usr/share/applications/$desktop"
    # the store listing describes the editor: only its AppImage carries it
    [ -z "$creator" ] && install -Dm644 "$src/packaging/linux/$id.metainfo.xml" "$app/usr/share/metainfo/$id.metainfo.xml"
    install -Dm644 "$src/packaging/linux/$id.mime.xml" "$app/usr/share/mime/packages/$id.xml"
    if [ -n "$creator" ]; then
        # AppRun hooks are sourced before the program starts: open the Creator
        install -d "$app/apprun-hooks"
        printf 'export BONK_SPAWNLIST_APP=1\n' > "$app/apprun-hooks/bonk-spawnlist-creator.sh"
    fi
    (cd "$out" && LDAI_OUTPUT="$name" linuxdeploy-x86_64.AppImage --appdir "$app" --plugin qt --output appimage \
        -d "$app/usr/share/applications/$desktop" -i "$app/usr/share/icons/hicolor/scalable/apps/$id.svg")
    echo "==> $out/$name"
}

build AppDir "$id.desktop" "Bonk-$ver-x86_64.AppImage"
build AppDir-creator "$id.SpawnlistCreator.desktop" "Bonk-Spawnlist-Creator-$ver-x86_64.AppImage" creator
