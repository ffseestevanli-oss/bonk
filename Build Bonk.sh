#!/usr/bin/env bash
# SPDX-License-Identifier: GPL-3.0-or-later
# Copyright (C) 2026 Steventechno and the Bonk! contributors
# ---------------------------------------------------------------------------
#  Build Bonk! Model Prop-inator!
#
#  Drop this script into the extracted Bonk source folder (or the folder
#  that contains it) and double-click it. It will:
#    1. find the Bonk source,
#    2. check for Rust, a C++ compiler and Qt 6 (and offer to install them),
#    3. build Bonk in release mode,
#    4. put the program, its icon and a .desktop launcher in "Built Binaries",
#    5. optionally add Bonk to your application menu.
#
#  On immutable systems (Bazzite, Silverblue/Kinoite, Bluefin, Aurora, SteamOS,
#  openSUSE Aeon/Kalpa/MicroOS, Vanilla OS, ...) nothing is installed on the
#  base system: the build runs inside a distrobox/toolbox container instead.
#
#  Options:  --clean      rebuild from scratch
#            --no-gui     terminal prompts only (no kdialog/zenity windows)
#            --yes        answer "yes" to every question (unattended builds)
#            --container  always build inside a container
#            --native     never use a container
#  Env:      QMAKE=/path/to/qt6/qmake  to use a specific Qt 6
# ---------------------------------------------------------------------------

set -u

SCRIPT_PATH="$(readlink -f "${BASH_SOURCE[0]}")"
BUILDER_DIR="$(dirname "$SCRIPT_PATH")"
OUT_NAME="Built Binaries"
APP_ID="bonk"
APP_NAME="Bonk! Model Prop-inator!"
BOX_NAME="bonk-build"
LEGACY_BOX_NAME="anvil-build"   # reused if it exists from the Anvil days

CLEAN=0
NO_GUI=0
ASSUME_YES=0
FORCE_CONTAINER=0
FORCE_NATIVE=0
INNER=0        # set when this script runs inside the build container
PASS_ARGS=()
for a in "$@"; do
    case "$a" in
        --clean) CLEAN=1; PASS_ARGS+=("$a") ;;
        --no-gui) NO_GUI=1 ;;
        --yes|-y) ASSUME_YES=1 ;;
        --container) FORCE_CONTAINER=1 ;;
        --native) FORCE_NATIVE=1 ;;
        --inner-container-build) INNER=1; NO_GUI=1; ASSUME_YES=1 ;;
        -h|--help) sed -n '2,25p' "$SCRIPT_PATH"; exit 0 ;;
    esac
done

# ---------------------------------------------------------------- terminal ---
# Double-clicked from Dolphin there is no terminal: reopen in one so the build
# output is visible and sudo can ask for a password.
if [ "$INNER" = 0 ] && [ ! -t 1 ] && [ -z "${BONK_BUILDER_IN_TERM:-}" ]; then
    export BONK_BUILDER_IN_TERM=1
    for t in konsole x-terminal-emulator gnome-terminal kgx ptyxis xfce4-terminal xterm; do
        if command -v "$t" >/dev/null 2>&1; then
            case "$t" in
                konsole) exec konsole -p tabtitle="Building Bonk!" -e bash "$SCRIPT_PATH" "$@" ;;
                gnome-terminal|kgx|ptyxis) exec "$t" -- bash "$SCRIPT_PATH" "$@" ;;
                *) exec "$t" -e bash "$SCRIPT_PATH" "$@" ;;
            esac
        fi
    done
    # No terminal emulator: carry on with dialogs only.
fi

# ------------------------------------------------------------------- output ---
if [ -t 1 ]; then
    B=$'\e[1m'; G=$'\e[32m'; Y=$'\e[33m'; R=$'\e[31m'; C=$'\e[36m'; N=$'\e[0m'
else
    B=; G=; Y=; R=; C=; N=
fi
step() { printf '\n%s==> %s%s\n' "$B$C" "$*" "$N"; }
ok()   { printf '  %s✔%s %s\n' "$G" "$N" "$*"; }
warn() { printf '  %s!%s %s\n' "$Y" "$N" "$*"; }
bad()  { printf '  %s✘%s %s\n' "$R" "$N" "$*"; }

GUI=""
if [ "$NO_GUI" = 0 ] && [ -n "${DISPLAY:-}${WAYLAND_DISPLAY:-}" ]; then
    if command -v kdialog >/dev/null 2>&1; then GUI=kdialog
    elif command -v zenity >/dev/null 2>&1; then GUI=zenity
    fi
fi

dlg_info() {  # title, text
    case "$GUI" in
        kdialog) kdialog --title "$1" --msgbox "$2" 2>/dev/null ;;
        zenity)  zenity --info --title "$1" --text "$2" --width 420 2>/dev/null ;;
    esac
}
dlg_error() {
    case "$GUI" in
        kdialog) kdialog --title "$1" --error "$2" 2>/dev/null ;;
        zenity)  zenity --error --title "$1" --text "$2" --width 480 2>/dev/null ;;
    esac
}
dlg_notify() {
    [ "$GUI" = kdialog ] && kdialog --title "Bonk! builder" --passivepopup "$1" 4 2>/dev/null &
}
ask() {  # title, question -> 0 = yes
    [ "$ASSUME_YES" = 1 ] && return 0
    case "$GUI" in
        kdialog) kdialog --title "$1" --yesno "$2" 2>/dev/null; return $? ;;
        zenity)  zenity --question --title "$1" --text "$2" --width 420 2>/dev/null; return $? ;;
    esac
    if [ -t 0 ]; then
        local r
        printf '\n%s%s%s [Y/n] ' "$B" "$2" "$N"
        read -r r
        case "$r" in n|N|no|NO) return 1 ;; *) return 0 ;; esac
    fi
    return 1
}

finish() {  # exit code
    if [ "$INNER" = 0 ] && [ -n "${BONK_BUILDER_IN_TERM:-}" ] && [ -t 0 ]; then
        printf '\nPress Enter to close this window...'
        read -r _
    fi
    exit "$1"
}

fail() {  # message
    bad "$1"
    dlg_error "Bonk! build failed" "$1"
    finish 1
}

if [ "$INNER" = 1 ]; then
    printf '%sBonk! builder (inside the build container)%s\n' "$B" "$N"
else
    printf '%sBonk! Model Prop-inator! builder%s\n' "$B" "$N"
fi

# ---------------------------------------------------------- 1. find source ---
step "Looking for the Bonk source"

# Accepts Bonk source (and older Anvil source, which builds the same way).
is_app_src() {
    [ -f "$1/Cargo.toml" ] && [ -f "$1/cpp/host.cpp" ] && [ -f "$1/src/main.rs" ] &&
        grep -Eq '^name *= *"(bonk|anvil)"' "$1/Cargo.toml"
}

SRC=""
if is_app_src "$BUILDER_DIR"; then
    SRC="$BUILDER_DIR"
else
    for d in "$BUILDER_DIR"/*/ "$BUILDER_DIR"/*/*/; do
        d="${d%/}"
        if is_app_src "$d"; then SRC="$d"; break; fi
    done
fi
[ -n "$SRC" ] || fail "Couldn't find the Bonk source next to this script.

Put \"Build Bonk.sh\" inside the extracted Bonk folder (the one containing Cargo.toml, src/ and cpp/) and run it again."

APP_VER="$(sed -n 's/^version *= *"\(.*\)"/\1/p' "$SRC/Cargo.toml" | head -1)"
CRATE="$(sed -n 's/^name *= *"\(.*\)"/\1/p' "$SRC/Cargo.toml" | head -1)"
ok "Found $APP_NAME ${APP_VER:-?} in $SRC"
OUT="$BUILDER_DIR/$OUT_NAME"

# ------------------------------------------------------------- system info ---
OS_IDS=""
OS_NAME=""
OS_VERSION_ID=""
if [ -r /etc/os-release ]; then
    # Read in a subshell so os-release variables (VERSION, NAME, ...) don't leak.
    OS_IDS="$(. /etc/os-release; echo " ${ID:-} ${ID_LIKE:-} ")"
    OS_NAME="$(. /etc/os-release; echo "${PRETTY_NAME:-}")"
    OS_VERSION_ID="$(. /etc/os-release; echo "${VERSION_ID:-}")"
fi

DISTRO="unknown"
case "$OS_IDS" in
    *" arch "*|*" manjaro "*|*" endeavouros "*|*" steamos "*) DISTRO=arch ;;
    *" fedora "*|*" rhel "*) DISTRO=fedora ;;
    *" opensuse"*|*" suse "*|*" sles "*) DISTRO=suse ;;
    *" debian "*|*" ubuntu "*|*" neon "*) DISTRO=debian ;;
esac

in_container() {
    [ -e /run/.containerenv ] || [ -e /.dockerenv ] || [ -n "${container:-}" ] ||
        [ -n "${DISTROBOX_ENTER_PATH:-}" ] || [ -n "${TOOLBOX_PATH:-}" ]
}

# Returns 0 and sets IMMUTABLE_KIND when the base system can't take packages
# through the normal package manager.
IMMUTABLE_KIND=""
detect_immutable() {
    in_container && return 1
    if [ -e /run/ostree-booted ]; then
        IMMUTABLE_KIND="ostree (rpm-ostree / bootc)"; return 0
    fi
    case "$OS_IDS" in
        *" steamos "*|*" holo "*) IMMUTABLE_KIND="SteamOS"; return 0 ;;
        *" bazzite "*|*" bluefin "*|*" aurora "*|*" kinoite "*|*" silverblue "*|*" ublue"*)
            IMMUTABLE_KIND="Universal Blue / Fedora Atomic"; return 0 ;;
        *" vanilla"*) IMMUTABLE_KIND="Vanilla OS"; return 0 ;;
        *" endless"*) IMMUTABLE_KIND="Endless OS"; return 0 ;;
        *" blendos "*) IMMUTABLE_KIND="blendOS"; return 0 ;;
        *" nixos "*) IMMUTABLE_KIND="NixOS"; return 0 ;;
        *" aeon "*|*" kalpa "*|*" opensuse-microos "*|*" opensuse-leap-micro "*)
            IMMUTABLE_KIND="openSUSE transactional (Aeon/Kalpa/MicroOS)"; return 0 ;;
    esac
    if command -v transactional-update >/dev/null 2>&1; then
        IMMUTABLE_KIND="openSUSE transactional (Aeon/Kalpa/MicroOS)"; return 0
    fi
    if command -v abroot >/dev/null 2>&1; then
        IMMUTABLE_KIND="ABRoot"; return 0
    fi
    if command -v findmnt >/dev/null 2>&1 && findmnt -no OPTIONS /usr 2>/dev/null | tr ',' '\n' | grep -qx ro; then
        IMMUTABLE_KIND="read-only /usr"; return 0
    fi
    return 1
}

IMMUTABLE=0
if [ "$INNER" = 0 ] && detect_immutable; then
    IMMUTABLE=1
fi

# ------------------------------------------------------- 2. dependencies ---
pkgs_for() {  # what -> package list for this distro
    case "$DISTRO:$1" in
        arch:rust) echo "rust" ;;
        arch:cxx) echo "base-devel" ;;
        arch:qt) echo "qt6-base pkgconf" ;;
        arch:gl) echo "libglvnd" ;;
        fedora:rust) echo "cargo rust" ;;
        fedora:cxx) echo "gcc-c++" ;;
        fedora:qt) echo "qt6-qtbase-devel pkgconf-pkg-config" ;;
        fedora:gl) echo "libglvnd-devel" ;;
        suse:rust) echo "cargo rust" ;;
        suse:cxx) echo "gcc-c++" ;;
        suse:qt) echo "qt6-base-devel qt6-openglwidgets-devel pkgconf" ;;
        suse:gl) echo "Mesa-libGL-devel" ;;
        debian:rust) echo "cargo rustc" ;;
        debian:cxx) echo "g++" ;;
        debian:qt) echo "qt6-base-dev qt6-base-dev-tools pkg-config" ;;
        debian:gl) echo "libgl-dev" ;;
    esac
}

install_cmd() {
    case "$DISTRO" in
        arch) echo "pacman -Syu --needed --noconfirm $*" ;;
        fedora) echo "dnf install -y $*" ;;
        suse) echo "zypper --non-interactive install $*" ;;
        debian) echo "apt-get install -y $*" ;;
    esac
}

CXX_BIN="${CXX:-}"
find_cxx() {
    [ -n "$CXX_BIN" ] && command -v "$CXX_BIN" >/dev/null 2>&1 && return 0
    for c in c++ g++ clang++; do
        if command -v "$c" >/dev/null 2>&1; then CXX_BIN="$c"; return 0; fi
    done
    return 1
}

QT_HOW=""
QT_VER=""
find_qt() {
    QT_HOW=""; QT_VER=""
    if [ -n "${QMAKE:-}" ]; then
        QT_VER="$("$QMAKE" -query QT_VERSION 2>/dev/null)"
        case "$QT_VER" in 6*) QT_HOW="QMAKE=$QMAKE"; return 0 ;; esac
        return 1
    fi
    if command -v pkg-config >/dev/null 2>&1 &&
        pkg-config --exists Qt6Widgets Qt6OpenGLWidgets Qt6OpenGL 2>/dev/null; then
        QT_VER="$(pkg-config --modversion Qt6Widgets)"; QT_HOW="pkg-config"; return 0
    fi
    for q in qmake6 qmake-qt6 qmake; do
        if command -v "$q" >/dev/null 2>&1; then
            QT_VER="$("$q" -query QT_VERSION 2>/dev/null)"
            case "$QT_VER" in 6*) QT_HOW="$q"; return 0 ;; esac
        fi
    done
    # Fedora/openSUSE keep qmake6 outside PATH sometimes
    for q in /usr/lib64/qt6/bin/qmake /usr/lib/qt6/bin/qmake /usr/lib/x86_64-linux-gnu/qt6/bin/qmake; do
        if [ -x "$q" ]; then
            QT_VER="$("$q" -query QT_VERSION 2>/dev/null)"
            case "$QT_VER" in 6*) export QMAKE="$q"; QT_HOW="$q"; return 0 ;; esac
        fi
    done
    return 1
}

gl_ok() {
    find_cxx || return 1
    printf '#include <GL/gl.h>\nint main(){return 0;}\n' |
        "$CXX_BIN" -x c++ -fsyntax-only ${CXXFLAGS:-} - >/dev/null 2>&1
}

[ -d "$HOME/.cargo/bin" ] && PATH="$HOME/.cargo/bin:$PATH"

check_all() {
    MISSING=()
    MISSING_NAMES=()
    if command -v cargo >/dev/null 2>&1 && command -v rustc >/dev/null 2>&1; then
        ok "Rust: $(rustc --version 2>/dev/null)"
    else
        bad "Rust (cargo/rustc) not found"; MISSING+=(rust); MISSING_NAMES+=("Rust")
    fi
    if find_cxx; then
        ok "C++ compiler: $CXX_BIN"
    else
        bad "C++ compiler not found"; MISSING+=(cxx); MISSING_NAMES+=("C++ compiler")
    fi
    if find_qt; then
        ok "Qt $QT_VER (via $QT_HOW)"
    else
        bad "Qt 6 development files not found"; MISSING+=(qt); MISSING_NAMES+=("Qt 6 development files")
    fi
    if gl_ok; then
        ok "OpenGL headers"
    elif find_cxx; then
        bad "OpenGL headers (GL/gl.h) not found"; MISSING+=(gl); MISSING_NAMES+=("OpenGL headers")
    fi
}

run_as_root() {  # command...
    if [ "$(id -u)" = 0 ]; then
        "$@"
    elif in_container && command -v sudo >/dev/null 2>&1; then
        # distrobox / toolbox give passwordless sudo
        sudo -n "$@" 2>/dev/null || sudo "$@"
    elif [ -t 0 ] && command -v sudo >/dev/null 2>&1; then
        sudo "$@"
    elif command -v pkexec >/dev/null 2>&1; then
        pkexec "$@"
    else
        return 127
    fi
}

# ----------------------------------------------------- container helpers ---
BOX_TOOL=""
pick_box_tool() {
    if command -v distrobox >/dev/null 2>&1 &&
        { command -v podman >/dev/null 2>&1 || command -v docker >/dev/null 2>&1 || command -v lilipod >/dev/null 2>&1; }; then
        BOX_TOOL=distrobox; return 0
    fi
    if command -v toolbox >/dev/null 2>&1; then
        BOX_TOOL=toolbox; return 0
    fi
    return 1
}

# Pick a container image as close to the host as possible, so the program can
# usually run directly on the host afterwards.
box_image() {
    case "$OS_IDS" in
        *" fedora "*|*" bazzite "*|*" bluefin "*|*" aurora "*|*" kinoite "*|*" silverblue "*)
            case "$OS_VERSION_ID" in
                [0-9]*) echo "registry.fedoraproject.org/fedora-toolbox:${OS_VERSION_ID%%.*}" ;;
                *) echo "registry.fedoraproject.org/fedora-toolbox:latest" ;;
            esac ;;
        *" steamos "*|*" arch "*) echo "docker.io/library/archlinux:latest" ;;
        *" opensuse"*|*" suse "*) echo "registry.opensuse.org/opensuse/distrobox:latest" ;;
        *" ubuntu "*|*" debian "*) echo "docker.io/library/ubuntu:24.04" ;;
        *) echo "registry.fedoraproject.org/fedora-toolbox:latest" ;;
    esac
}

box_exists() {
    case "$BOX_TOOL" in
        distrobox) distrobox list 2>/dev/null | sed 's/\x1b\[[0-9;]*m//g' | awk -F'|' '{gsub(/ /,"",$2); print $2}' | grep -qx "$BOX_NAME" ;;
        toolbox) toolbox list -c 2>/dev/null | awk 'NR>1 {print $2}' | grep -qx "$BOX_NAME" ;;
    esac
}

box_create() {
    local img; img="$(box_image)"
    local nv=()
    # NVIDIA hosts: share the driver so OpenGL works if Anvil runs in the container.
    [ -e /proc/driver/nvidia/version ] && [ "$BOX_TOOL" = distrobox ] && nv=(--nvidia)
    step "Creating build container \"$BOX_NAME\" from $img"
    case "$BOX_TOOL" in
        distrobox) distrobox create --yes --name "$BOX_NAME" --image "$img" ${nv[@]+"${nv[@]}"} ;;
        toolbox) toolbox create -y -c "$BOX_NAME" --image "$img" ;;
    esac
}

box_run() {  # command...
    case "$BOX_TOOL" in
        distrobox) distrobox enter --name "$BOX_NAME" -- "$@" ;;
        toolbox) toolbox run -c "$BOX_NAME" "$@" ;;
    esac
}

# ------------------------------------------------------- output packaging ---
write_icon() {
    # Prefer the icon shipped with the source; fall back to a built-in copy.
    if [ -f "$SRC/assets/bonk.svg" ]; then
        install -m 644 "$SRC/assets/bonk.svg" "$OUT/$APP_ID.svg"
        return
    fi
    cat > "$OUT/$APP_ID.svg" <<'SVG'
<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 128 128" width="128" height="128">
  <defs>
    <linearGradient id="wood" x1="0" y1="-1" x2="0" y2="1" gradientUnits="objectBoundingBox">
      <stop offset="0" stop-color="#f6cf8e"/>
      <stop offset="0.55" stop-color="#d99a52"/>
      <stop offset="1" stop-color="#a8682c"/>
    </linearGradient>
    <linearGradient id="bg" x1="0" y1="0" x2="0" y2="1">
      <stop offset="0" stop-color="#2c313a"/>
      <stop offset="1" stop-color="#1b1e24"/>
    </linearGradient>
  </defs>

  <!-- background tile -->
  <rect x="4" y="4" width="120" height="120" rx="26" fill="url(#bg)"/>

  <!-- impact burst behind the hit -->
  <polygon fill="#ffd23f" stroke="#11141a" stroke-width="2.5" stroke-linejoin="round"
           points="98,26 103,42 118,36 109,50 124,58 107,61 112,77 98,68 88,80 87,64 71,62 84,53 76,38 91,44"/>

  <!-- the cube (a brush!) -->
  <g stroke="#11141a" stroke-width="3" stroke-linejoin="round">
    <polygon fill="#ffb067" points="80,66 104,79 80,92 56,79"/>
    <polygon fill="#e8833a" points="56,79 80,92 80,118 56,105"/>
    <polygon fill="#c4621f" points="80,92 104,79 104,105 80,118"/>
  </g>

  <!-- the bat -->
  <g transform="translate(13 108) rotate(-46)">
    <path fill="url(#wood)" stroke="#11141a" stroke-width="3" stroke-linejoin="round"
          d="M7,-4 L38,-4 C54,-4.5 62,-10.5 73,-11 L94,-11 A11,11 0 0 1 94,11 L73,11 C62,10.5 54,4.5 38,4 L7,4 Z"/>
    <!-- grip tape -->
    <rect x="9" y="-4" width="24" height="8" fill="#3a3f4a" stroke="#11141a" stroke-width="2"/>
    <path d="M13,-4 L17,4 M19,-4 L23,4 M25,-4 L29,4" stroke="#6b7280" stroke-width="1.6"/>
    <!-- knob -->
    <ellipse cx="5" cy="0" rx="4.5" ry="8" fill="#c98b45" stroke="#11141a" stroke-width="3"/>
    <!-- highlight -->
    <path d="M60,-6.5 C66,-8.5 70,-8.5 76,-8.5 L92,-8.5" fill="none" stroke="#fff3d6" stroke-width="2.4" stroke-linecap="round" opacity="0.85"/>
  </g>

  <!-- swing lines -->
  <path d="M44,22 Q52,16 62,16 M36,32 Q44,26 52,25" fill="none" stroke="#e6e9ef" stroke-width="3" stroke-linecap="round" opacity="0.8"/>
</svg>
SVG
}

# .desktop Exec quoting: escape \ " ` $ for the exec layer, then \ again for
# the desktop-entry string layer.
exec_quote() {
    local s="$1"
    s="${s//\\/\\\\}"; s="${s//\"/\\\"}"; s="${s//\`/\\\`}"; s="${s//\$/\\\$}"
    s="${s//\\/\\\\}"
    printf '"%s"' "$s"
}
desktop_str() { local s="$1"; printf '%s' "${s//\\/\\\\}"; }

# How the launcher starts Anvil: directly, or inside the build container.
RUN_MODE="native"
exec_line() {
    local bin="$OUT/$APP_ID"
    if [ "$RUN_MODE" = container ]; then
        case "$BOX_TOOL" in
            distrobox) printf '%s enter --name %s -- %s' "$(exec_quote "$(command -v distrobox)")" "$BOX_NAME" "$(exec_quote "$bin")" ;;
            toolbox) printf '%s run -c %s %s' "$(exec_quote "$(command -v toolbox)")" "$BOX_NAME" "$(exec_quote "$bin")" ;;
        esac
    else
        exec_quote "$bin"
    fi
}

write_desktop() {  # file, icon value
    mkdir -p "$(dirname "$1")"
    cat > "$1" <<EOF
[Desktop Entry]
Type=Application
Version=1.5
Name=$APP_NAME
GenericName=Model Editor
Comment=Hammer-style brush modeler for Source engine models
Exec=$(exec_line) %f
MimeType=application/x-bonk-model;application/x-bonk-project;
Path=$(desktop_str "$OUT")
Icon=$(desktop_str "$2")
Terminal=false
Categories=Graphics;3DGraphics;Development;
Keywords=bonk;hammer;source;model;mdl;brush;prop;
StartupNotify=true
StartupWMClass=bonk
EOF
    chmod 755 "$1"
}

# Can the built program start on this system without the container?
native_runnable() {
    command -v ldd >/dev/null 2>&1 || return 1
    local out
    out="$(ldd "$OUT/$APP_ID" 2>&1)" || return 1
    if printf '%s\n' "$out" | grep -q "not found"; then
        printf '%s\n' "$out" | grep "not found" | sed 's/^/      /' | head -5
        return 1
    fi
    return 0
}

# ===================================================== container route ======
build_in_container() {
    step "Immutable system detected (${IMMUTABLE_KIND:-container requested})"
    printf '  The base system stays untouched: Bonk will be built inside a\n'
    printf '  %s container named "%s".\n' "${BOX_TOOL:-distrobox}" "$BOX_NAME"

    if ! pick_box_tool; then
        fail "This is an immutable system (${IMMUTABLE_KIND:-}), so build tools can't be installed on it directly, and neither distrobox nor toolbox was found.

Install distrobox (e.g. from your distro's app store or https://distrobox.it) and run this again."
    fi
    ok "Using $BOX_TOOL"

    # Reuse the container from the Anvil days instead of downloading another.
    local want="$BOX_NAME"
    BOX_NAME="$LEGACY_BOX_NAME"
    if box_exists; then
        ok "Reusing the existing \"$LEGACY_BOX_NAME\" container"
    else
        BOX_NAME="$want"
    fi

    if box_exists; then
        ok "Container \"$BOX_NAME\" already exists"
    else
        ask "Bonk! builder" "Your system is immutable (${IMMUTABLE_KIND:-}), so packages can't be installed on it directly.

Bonk can be built inside a $BOX_TOOL container instead:
  name:  $BOX_NAME
  image: $(box_image)

This downloads the container image once (a few hundred MB). Continue?" ||
            fail "Build cancelled. Nothing was changed."
        box_create || fail "Creating the \"$BOX_NAME\" container failed. See the terminal output for details."
        ok "Container created"
    fi

    step "Entering the container (the first start can take a few minutes)"
    local inner_args=(--inner-container-build ${PASS_ARGS[@]+"${PASS_ARGS[@]}"})
    box_run bash "$SCRIPT_PATH" "${inner_args[@]}"
    local rc=$?
    [ "$rc" -eq 0 ] && [ -x "$OUT/$APP_ID" ] ||
        fail "The build inside the \"$BOX_NAME\" container failed. The log is in:
  $OUT/build.log"

    step "Checking whether Bonk runs directly on this system"
    if [ "$FORCE_CONTAINER" = 0 ] && native_runnable; then
        RUN_MODE="native"
        ok "Yes: the launcher will start it directly"
    else
        RUN_MODE="container"
        warn "This system lacks a matching Qt 6, so the launcher will start Bonk inside \"$BOX_NAME\""
    fi
}

# ======================================================== native route ======
build_native() {
    step "Checking build tools"
    check_all

    if [ "${#MISSING[@]}" -gt 0 ]; then
        PKGS=""
        for m in "${MISSING[@]}"; do PKGS="$PKGS $(pkgs_for "$m")"; done
        PKGS="$(echo $PKGS)"
        NAMES="$(printf '%s, ' "${MISSING_NAMES[@]}")"; NAMES="${NAMES%, }"

        if [ -z "$PKGS" ]; then
            fail "Missing: $NAMES.

Your distribution wasn't recognised, so please install Rust, a C++ compiler and the Qt 6 base development package yourself, then run this again."
        fi

        CMD="$(install_cmd $PKGS)"
        if ask "Bonk! builder" "Missing: $NAMES.

Install them now with:
  $CMD

(You may be asked for your password.)"; then
            step "Installing $PKGS"
            # shellcheck disable=SC2086
            run_as_root $CMD || fail "Installing packages failed. Command was:
  sudo $CMD"
            hash -r
            step "Re-checking build tools"
            check_all
            [ "${#MISSING[@]}" -eq 0 ] || fail "Still missing after install: $(printf '%s, ' "${MISSING_NAMES[@]}" | sed 's/, $//')"
        else
            fail "Can't build without: $NAMES.

Install them with:
  sudo $CMD"
        fi
    fi

    # Cargo older than 1.78 can't read a version-4 Cargo.lock. Anvil has no crate
    # dependencies, so the lock file can simply be regenerated.
    CARGO_MINOR="$(cargo --version | sed -n 's/^cargo 1\.\([0-9]*\).*/\1/p')"
    if [ -n "$CARGO_MINOR" ] && [ "$CARGO_MINOR" -lt 78 ] && grep -q '^version = 4' "$SRC/Cargo.lock" 2>/dev/null; then
        warn "Older cargo detected; regenerating Cargo.lock"
        rm -f "$SRC/Cargo.lock"
    fi
    if [ -n "$CARGO_MINOR" ] && [ "$CARGO_MINOR" -lt 70 ]; then
        warn "Rust 1.$CARGO_MINOR is quite old; if the build fails, install a newer Rust from https://rustup.rs"
    fi

    step "Building $APP_NAME (release)"
    mkdir -p "$OUT" || fail "Couldn't create \"$OUT\""
    LOG="$OUT/build.log"
    [ "$INNER" = 0 ] && dlg_notify "Building $APP_NAME ${APP_VER}…"

    if [ "$CLEAN" = 1 ]; then
        (cd "$SRC" && cargo clean) >/dev/null 2>&1
    fi

    START=$(date +%s)
    (cd "$SRC" && cargo build --release --color always 2>&1) | tee "$LOG"
    RC=${PIPESTATUS[0]}
    # keep the log readable in a text editor
    sed -i 's/\x1b\[[0-9;]*[mK]//g' "$LOG" 2>/dev/null

    if [ "$RC" -ne 0 ] || [ ! -x "$SRC/target/release/$CRATE" ]; then
        TAIL="$(grep -E 'error|Error|fatal' "$LOG" | head -8)"
        fail "The build failed. The full log is in:
  $LOG

${TAIL}"
    fi
    ok "Built in $(( $(date +%s) - START ))s"

    install -m 755 "$SRC/target/release/$CRATE" "$OUT/$APP_ID" || fail "Couldn't copy the program into \"$OUT\""
    {
        echo "Rust:    $(rustc --version 2>/dev/null)"
        echo "Qt:      $QT_VER (via $QT_HOW)"
        echo "Built on: ${OS_NAME:-unknown}"
    } > "$OUT/.toolchain"
}

# ==================================================================== main ===
if [ "$INNER" = 1 ]; then
    # Running inside the container: build only; the host side writes launchers.
    build_native
    exit 0
fi

USE_CONTAINER=0
if [ "$FORCE_CONTAINER" = 1 ]; then
    USE_CONTAINER=1
elif [ "$IMMUTABLE" = 1 ] && [ "$FORCE_NATIVE" = 0 ]; then
    # If someone has already layered every build dependency onto the image,
    # just build natively.
    step "Checking build tools on the host"
    check_all >/dev/null 2>&1
    if [ "${#MISSING[@]}" -eq 0 ]; then
        ok "Everything needed is already installed on the host"
    else
        warn "Immutable system detected: $IMMUTABLE_KIND"
        USE_CONTAINER=1
    fi
fi

if [ "$USE_CONTAINER" = 1 ]; then
    pick_box_tool
    build_in_container
else
    build_native
fi

# --------------------------------------------------------------- package ---
step "Writing \"$OUT_NAME\""
ok "$APP_ID"
write_icon && ok "$APP_ID.svg"
# Tool panel icons
if [ -d "$SRC/assets/tools" ]; then
    mkdir -p "$OUT/icons"
    install -m 644 "$SRC"/assets/tools/*.svg "$OUT/icons/" && ok "icons/ (tool panel)"
fi
[ -f "$SRC/LICENSE" ] && install -m 644 "$SRC/LICENSE" "$OUT/LICENSE" && ok "LICENSE (GPL-3.0-or-later)"
write_desktop "$OUT/Bonk.desktop" "$OUT/$APP_ID.svg" && ok "Bonk.desktop"
# Clean up launchers left by Anvil-era builds in the same folder.
rm -f "$OUT/Anvil.desktop" "$OUT/anvil" "$OUT/anvil.svg"

{
    echo "$APP_NAME $APP_VER"
    echo "Built:   $(date '+%Y-%m-%d %H:%M')"
    echo "Source:  $SRC"
    cat "$OUT/.toolchain" 2>/dev/null
    echo "Host:    ${OS_NAME:-unknown}${IMMUTABLE_KIND:+ (immutable: $IMMUTABLE_KIND)}"
    if [ "$USE_CONTAINER" = 1 ]; then
        echo "Built in container: $BOX_NAME ($BOX_TOOL, $(box_image))"
        echo "Launch mode: $RUN_MODE"
        [ "$RUN_MODE" = container ] && echo "Note: the launcher needs the \"$BOX_NAME\" container; don't remove it."
    fi
    echo
    echo "Launch Bonk.desktop (or ./$APP_ID) from this folder."
    echo "If you move this folder, run the builder again so the launcher points at the new location."
} > "$OUT/BUILD-INFO.txt"
rm -f "$OUT/.toolchain"
ok "BUILD-INFO.txt"

# ------------------------------------------------------------ app menu ---
MENU_FILE="${XDG_DATA_HOME:-$HOME/.local/share}/applications/$APP_ID.desktop"
ICON_FILE="${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor/scalable/apps/$APP_ID.svg"
MENU_MSG=""
OLD_MENU="${XDG_DATA_HOME:-$HOME/.local/share}/applications/anvil.desktop"
OLD_ICON="${XDG_DATA_HOME:-$HOME/.local/share}/icons/hicolor/scalable/apps/anvil.svg"
if [ -f "$OLD_MENU" ] && grep -q "Hammer-style brush modeler" "$OLD_MENU"; then
    # Anvil was renamed: replace its menu entry with Bonk's.
    rm -f "$OLD_MENU" "$OLD_ICON"
    write_desktop "$MENU_FILE" "$APP_ID"
    ok "Replaced the old Anvil menu entry"
fi
if [ -f "$MENU_FILE" ]; then
    # Already in the menu from an earlier build: keep it pointing at this build.
    write_desktop "$MENU_FILE" "$APP_ID"
    install -Dm 644 "$OUT/$APP_ID.svg" "$ICON_FILE"
    MENU_MSG="The application-menu entry was updated."
    ok "Updated the application-menu entry"
elif ask "Bonk! builder" "$APP_NAME was built successfully.

Add it to your application menu too?"; then
    install -Dm 644 "$OUT/$APP_ID.svg" "$ICON_FILE"
    write_desktop "$MENU_FILE" "$APP_ID"
    MENU_MSG="It was added to your application menu."
    ok "Added to the application menu"
fi
if [ -n "$MENU_MSG" ]; then
    # Register .bpmdl models and .bonkpj projects so Dolphin shows them with the Bonk! icon and
    # opens them in Bonk! on double-click.
    MIME_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/mime"
    mkdir -p "$MIME_DIR/packages"
    cat > "$MIME_DIR/packages/bonk.xml" <<'XML'
<?xml version="1.0" encoding="UTF-8"?>
<mime-info xmlns="http://www.freedesktop.org/standards/shared-mime-info">
  <mime-type type="application/x-bonk-model">
    <comment>Bonk! model</comment>
    <glob pattern="*.bpmdl"/>
    <icon name="bonk"/>
  </mime-type>
  <mime-type type="application/x-bonk-project">
    <comment>Bonk! project</comment>
    <glob pattern="*.bonkpj"/>
    <icon name="bonk"/>
  </mime-type>
</mime-info>
XML
    command -v update-mime-database >/dev/null 2>&1 && update-mime-database "$MIME_DIR" >/dev/null 2>&1
    command -v xdg-mime >/dev/null 2>&1 && xdg-mime default "$APP_ID.desktop" application/x-bonk-model application/x-bonk-project >/dev/null 2>&1
    ok "Registered .bpmdl and .bonkpj files to open with Bonk!"
    command -v kbuildsycoca6 >/dev/null 2>&1 && kbuildsycoca6 >/dev/null 2>&1
    command -v update-desktop-database >/dev/null 2>&1 &&
        update-desktop-database "$(dirname "$MENU_FILE")" >/dev/null 2>&1
fi

step "Done"
printf '  %s %s is in:\n    %s\n' "$APP_NAME" "$APP_VER" "$OUT"
EXTRA=""
[ "$RUN_MODE" = container ] && EXTRA="
It runs inside the \"$BOX_NAME\" container; keep that container installed."
dlg_info "Bonk! is ready" "$APP_NAME ${APP_VER} was built into:
$OUT

Double-click Bonk.desktop there to start it. BONK!${MENU_MSG:+
$MENU_MSG}${EXTRA}"
finish 0
