# Bonk on Windows

Bonk is Linux first; the Windows build is a functional port. On Windows Bonk
runs Garry's Mod's `studiomdl.exe`, `gmad.exe` and `gmpublish.exe` directly (no
Proton), and finds Steam through the registry (`HKCU\Software\Valve\Steam`),
falling back to `C:\Program Files (x86)\Steam`. Settings → Garry's Mod →
Garry's Mod folder still overrides it. The crash log is
`%LOCALAPPDATA%\bonk\crash.log`, settings live in the registry
(`HKCU\Software\bonk\bonk`), and projects default to `%USERPROFILE%\Bonk Projects`.

Only the GNU (MinGW) toolchain is supported: `build.rs` compiles the Qt host
with GCC-style flags.

## Download

Every push builds `bonk-windows-x86_64` on GitHub Actions (the run's
Artifacts). Unzip it anywhere and run `Bonk\bonk.exe`; keep the DLLs and the
`platforms`, `styles`, `imageformats`, `iconengines` and `icons` folders next
to it.

## Build on Windows (MSYS2)

1. Install [MSYS2](https://www.msys2.org) and open the **UCRT64** shell.
2. Install the tools and Qt:

   ```sh
   pacman -S --needed git zip mingw-w64-ucrt-x86_64-gcc mingw-w64-ucrt-x86_64-rust \
       mingw-w64-ucrt-x86_64-pkgconf mingw-w64-ucrt-x86_64-qt6-base mingw-w64-ucrt-x86_64-qt6-svg
   ```

3. Build, then gather the program with its DLLs:

   ```sh
   cargo build --release
   packaging/windows/gather-msys2.sh   # → target/windows/Bonk and Bonk-windows-x86_64.zip
   ```

`cargo run --release` works from the UCRT64 shell too (the DLLs are on its PATH).

## Cross-compile from Linux

You need `rustup target add x86_64-pc-windows-gnu`, mingw-w64 (with the
`-posix` thread model compilers) and a Qt 6 built for MinGW whose version
matches the Linux Qt's host tools (qtbase, plus qtsvg for the tool icons).
Then:

```sh
QT_WIN=/path/to/qt6-mingw packaging/windows/cross-build.sh   # → target/windows/Bonk
```

The result runs under Wine for a quick check:

```sh
cd target/windows/Bonk && wine bonk.exe
```

Building Qt for MinGW from Linux (qtbase 6.4.2 from Ubuntu 24.04's sources, with
its matching `qt6-base-dev-tools` as the host tools) went like this:

```sh
cmake -G Ninja qtbase-everywhere-src-6.4.2 \
  -DCMAKE_TOOLCHAIN_FILE=mingw.cmake -DQT_HOST_PATH=/usr \
  -DQT_HOST_PATH_CMAKE_DIR=/usr/lib/x86_64-linux-gnu/cmake \
  -DCMAKE_INSTALL_PREFIX=/opt/qt-mingw -DCMAKE_BUILD_TYPE=Release \
  -DQT_BUILD_EXAMPLES=OFF -DQT_BUILD_TESTS=OFF -DFEATURE_sql=OFF -DFEATURE_dbus=OFF \
  -DFEATURE_printsupport=OFF -DFEATURE_testlib=OFF -DFEATURE_tabletevent=OFF \
  -DFEATURE_system_zlib=OFF -DFEATURE_system_png=OFF -DFEATURE_system_jpeg=OFF \
  -DFEATURE_system_freetype=OFF -DFEATURE_system_harfbuzz=OFF -DFEATURE_system_pcre2=OFF
cmake --build . && cmake --install .
# qtsvg: /opt/qt-mingw/bin/qt-configure-module qtsvg-everywhere-src-6.4.2, build, install
```

where `mingw.cmake` sets `CMAKE_SYSTEM_NAME Windows` and the
`x86_64-w64-mingw32-gcc-posix` / `g++-posix` / `windres` compilers.
(`FEATURE_tabletevent=OFF` only because Debian's source drops Wacom's `wintab.h`.)

## Known gaps

- Text is the "doesn't need to look pretty" kind: the Linux look and fonts aren't tuned for Windows.
- Paths with non-ASCII characters are passed to the Rust side as UTF-8; tested only lightly.
- No installer, Start menu entry or `.bpmdl` file association yet.
