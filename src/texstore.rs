// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Steventechno and the Bonk! contributors
//! Decoded texture images, keyed by material name (e.g. "metal/rusty_panel").
//!
//! The Qt side decodes PNG/JPG files from the project's assets/Textures folder
//! and hands the RGBA pixels over; each viewport's renderer uploads them to
//! its own GL context when the generation number changes.

use std::cell::RefCell;
use std::collections::HashMap;
use std::rc::Rc;

pub struct Tex {
    pub w: u32,
    pub h: u32,
    pub rgba: Rc<Vec<u8>>,
    pub gen: u64,
}

thread_local! {
    static STORE: RefCell<HashMap<String, Tex>> = RefCell::new(HashMap::new());
    static GEN: RefCell<u64> = const { RefCell::new(0) };
    /// Transparency per material for the 3D view: (0 opaque / 1 alpha test / 2 translucent, reference)
    static ALPHA: RefCell<HashMap<String, (u8, f32)>> = RefCell::new(HashMap::new());
}

pub fn set_alpha(name: &str, mode: u8, reference: f32) {
    ALPHA.with(|a| {
        if mode == 0 {
            a.borrow_mut().remove(name);
        } else {
            a.borrow_mut().insert(name.to_string(), (mode.min(2), reference));
        }
    });
}

pub fn alpha(name: &str) -> (u8, f32) {
    ALPHA.with(|a| a.borrow().get(name).copied().unwrap_or((0, 0.5)))
}

fn next_gen() -> u64 {
    GEN.with(|g| {
        *g.borrow_mut() += 1;
        *g.borrow()
    })
}

pub fn set(name: &str, w: u32, h: u32, rgba: Vec<u8>) {
    if w == 0 || h == 0 || rgba.len() < (w * h * 4) as usize {
        return;
    }
    let gen = next_gen();
    STORE.with(|s| s.borrow_mut().insert(name.to_string(), Tex { w, h, rgba: Rc::new(rgba), gen }));
}

pub fn remove(name: &str) {
    STORE.with(|s| s.borrow_mut().remove(name));
}

pub fn clear() {
    STORE.with(|s| s.borrow_mut().clear());
}

/// (width, height, generation, pixels) of a texture, if loaded.
pub fn get(name: &str) -> Option<(u32, u32, u64, Rc<Vec<u8>>)> {
    STORE.with(|s| s.borrow().get(name).map(|t| (t.w, t.h, t.gen, t.rgba.clone())))
}

/// Size of a loaded texture, if any.
pub fn size(name: &str) -> Option<(u32, u32)> {
    STORE.with(|s| s.borrow().get(name).map(|t| (t.w, t.h)))
}
