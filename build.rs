// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Steventechno and the Bonk! contributors
//! Compiles cpp/host.cpp against Qt6 and links it into the Rust binary.
//!
//! Qt6 is located, in order, through:
//!   1. the QMAKE environment variable (path to a Qt6 qmake),
//!   2. pkg-config (Qt6Widgets / Qt6OpenGLWidgets),
//!   3. qmake6, qmake-qt6 or qmake on PATH (must report Qt 6).

use std::env;
use std::path::PathBuf;
use std::process::Command;

const MODULES: [&str; 5] = ["Core", "Gui", "Widgets", "OpenGL", "OpenGLWidgets"];

struct QtFlags {
    cflags: Vec<String>,
    link_dirs: Vec<String>,
    libs: Vec<String>,
}

fn run(cmd: &str, args: &[&str]) -> Option<String> {
    let out = Command::new(cmd).args(args).output().ok()?;
    if !out.status.success() {
        return None;
    }
    Some(String::from_utf8_lossy(&out.stdout).trim().to_string())
}

fn from_qmake(qmake: &str) -> Option<QtFlags> {
    let version = run(qmake, &["-query", "QT_VERSION"])?;
    if !version.starts_with('6') {
        return None;
    }
    let headers = run(qmake, &["-query", "QT_INSTALL_HEADERS"])?;
    let libdir = run(qmake, &["-query", "QT_INSTALL_LIBS"])?;
    let mut cflags = vec![format!("-I{headers}")];
    for m in MODULES {
        cflags.push(format!("-I{headers}/Qt{m}"));
    }
    Some(QtFlags {
        cflags,
        link_dirs: vec![libdir],
        libs: MODULES.iter().rev().map(|m| format!("Qt6{m}")).collect(),
    })
}

fn from_pkg_config() -> Option<QtFlags> {
    let out = run("pkg-config", &["--cflags", "--libs", "Qt6Widgets", "Qt6OpenGLWidgets", "Qt6OpenGL"])?;
    let mut f = QtFlags { cflags: vec![], link_dirs: vec![], libs: vec![] };
    for tok in out.split_whitespace() {
        if let Some(d) = tok.strip_prefix("-L") {
            f.link_dirs.push(d.to_string());
        } else if let Some(l) = tok.strip_prefix("-l") {
            f.libs.push(l.to_string());
        } else if tok.starts_with("-I") || tok.starts_with("-D") || tok.starts_with("-isystem") || tok.starts_with("-std") {
            f.cflags.push(tok.to_string());
        }
    }
    if f.libs.is_empty() {
        return None;
    }
    Some(f)
}

fn find_qt() -> QtFlags {
    if let Ok(q) = env::var("QMAKE") {
        return from_qmake(&q).unwrap_or_else(|| panic!("QMAKE={q} is not a working Qt 6 qmake"));
    }
    if let Some(f) = from_pkg_config() {
        return f;
    }
    for q in ["qmake6", "qmake-qt6", "qmake"] {
        if let Some(f) = from_qmake(q) {
            return f;
        }
    }
    panic!(
        "\n\nCould not find Qt 6 development files.\n\
         Install them (Arch: qt6-base, Fedora: qt6-qtbase-devel, \
         Debian/Kubuntu: qt6-base-dev qt6-base-dev-tools, openSUSE: qt6-base-devel)\n\
         or set QMAKE=/path/to/qt6/bin/qmake\n\n"
    );
}

fn main() {
    println!("cargo:rerun-if-changed=cpp/host.cpp");
    println!("cargo:rerun-if-changed=cpp/guide.inc");
    println!("cargo:rerun-if-changed=cpp/importer.inc");
    println!("cargo:rerun-if-changed=cpp/spawnlist.inc");
    println!("cargo:rerun-if-changed=cpp/prefabs.inc");
    println!("cargo:rerun-if-changed=cpp/workshop.inc");
    println!("cargo:rerun-if-changed=assets");
    println!("cargo:rerun-if-changed=build.rs");
    println!("cargo:rerun-if-env-changed=QMAKE");
    println!("cargo:rerun-if-env-changed=CXX");
    println!("cargo:rerun-if-env-changed=CXXFLAGS");

    let qt = find_qt();
    let out = PathBuf::from(env::var("OUT_DIR").unwrap());
    let obj = out.join("host.o");
    let lib = out.join("libbonk_qt.a");
    let cxx = env::var("CXX").unwrap_or_else(|_| "c++".to_string());

    let status = Command::new(&cxx)
        .args(["-std=c++17", "-O2", "-fPIC", "-Wall", "-c", "cpp/host.cpp", "-o"])
        .arg(&obj)
        .args(&qt.cflags)
        .args(env::var("CXXFLAGS").unwrap_or_default().split_whitespace())
        // Lets `cargo run` find the tool icons in the source tree; installed
        // builds use the icons/ folder next to the binary instead.
        .arg(format!("-DBONK_ASSET_DIR=\"{}/assets\"", env::var("CARGO_MANIFEST_DIR").unwrap()))
        .arg(format!("-DBONK_VERSION=\"{}\"", env::var("CARGO_PKG_VERSION").unwrap()))
        .status()
        .unwrap_or_else(|e| panic!("failed to run C++ compiler '{cxx}': {e}"));
    assert!(status.success(), "compiling cpp/host.cpp failed");

    let _ = std::fs::remove_file(&lib);
    let status = Command::new("ar").arg("crs").arg(&lib).arg(&obj).status().expect("failed to run ar");
    assert!(status.success(), "ar failed");

    println!("cargo:rustc-link-search=native={}", out.display());
    println!("cargo:rustc-link-lib=static=bonk_qt");
    for d in &qt.link_dirs {
        println!("cargo:rustc-link-search=native={d}");
        // Harmless for system Qt, required for a Qt installed in a custom prefix.
        println!("cargo:rustc-link-arg=-Wl,-rpath,{d}");
    }
    for l in &qt.libs {
        println!("cargo:rustc-link-lib=dylib={l}");
    }
    println!("cargo:rustc-link-lib=dylib=stdc++");
}
