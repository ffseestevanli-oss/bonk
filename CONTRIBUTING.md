# Contributing to Bonk!

Thanks for helping! Bug reports, ideas and pull requests are all welcome.

## Reporting a bug

Use the **Bug report** issue template. The most useful things to include:

- what you did, what you expected, and what happened instead;
- **Help → About Bonk! → System Info** (it copies your versions and paths to the clipboard);
- if Bonk crashed: `~/.local/share/bonk/crash.log` (Bonk mentions it the next time it starts);
- if a compile went wrong: tick **Show studiomdl's log** in the compile window and paste it.

A `.bpmdl` that shows the problem helps a lot. Textures usually aren't needed.

## Building and testing

```sh
cargo build --release   # target/release/bonk
cargo test              # the whole test suite (no display needed)
```

You need Rust and the Qt 6 base development package; see the README for each distro.
`QMAKE=/path/to/qt6/bin/qmake` points the build at a specific Qt.

## How the code is laid out

- **`src/`** is Rust and holds all the editor logic and rendering: brushes, tools, undo, file
  formats, the OpenGL renderer and the importers/exporters. Most of it is covered by tests in the
  same file (`#[cfg(test)] mod tests`).
- **`cpp/host.cpp`** is the Qt 6 window: menus, docks, dialogs, settings, compiling. It calls
  into Rust through the `bonk_*` functions declared at its top and defined in `src/main.rs`.
  No moc is needed: signals are connected with lambdas.
- **`build.rs`** finds Qt and compiles the C++ side into a static library.

## Guidelines

- **No new dependencies** without a good reason. Bonk builds with nothing but Rust and Qt 6,
  which keeps the one-click builder and the future Flatpak simple.
- **US English** in everything users see (color, center, behavior…).
- **Write it the way users talk:** dialogs and the guide explain what happens in plain words.
- **Add a test** for editor or file-format changes, and run `cargo test` before sending.
- **Update the guide** (`cpp/guide.inc`) and `CHANGELOG.md` when you change how something works.
- New source files start with the license header used in the existing ones.

By contributing you agree that your contributions are licensed under the project's license,
GPL-3.0-or-later.
