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
    /// Moving materials for the 3D view (scrolling, pulsing, animated frames)
    static ANIM: RefCell<HashMap<String, Anim>> = RefCell::new(HashMap::new());
}

/// How a material moves, as the game would show it.
#[derive(Clone, Copy, Debug, PartialEq, Default)]
pub struct Anim {
    /// Frames stacked top to bottom in the image (1 = still)
    pub frames: u32,
    pub fps: f32,
    /// TextureScroll: texture widths per second along (u, v)
    pub scroll: [f32; 2],
    /// Sine: (min, max, period seconds, on alpha instead of brightness); period 0 = off
    pub pulse: [f32; 3],
    pub pulse_alpha: bool,
}

impl Anim {
    pub fn is_still(&self) -> bool {
        self.frames <= 1 && self.scroll == [0.0, 0.0] && self.pulse[2] <= 0.0
    }

    /// (frame, uv offset, brightness multiplier, alpha multiplier) at time `t`
    pub fn at(&self, t: f32) -> (u32, [f32; 2], f32, f32) {
        let frame = if self.frames > 1 { ((t * self.fps.max(0.0)).floor() as u64 % self.frames as u64) as u32 } else { 0 };
        let off = [(self.scroll[0] * t).fract(), (self.scroll[1] * t).fract()];
        let (mut c, mut a) = (1.0, 1.0);
        if self.pulse[2] > 0.0 {
            let k = (std::f32::consts::TAU * t / self.pulse[2]).sin() * 0.5 + 0.5;
            let v = self.pulse[0] + (self.pulse[1] - self.pulse[0]) * k;
            if self.pulse_alpha {
                a = v.clamp(0.0, 1.0);
            } else {
                c = v.max(0.0);
            }
        }
        (frame, off, c, a)
    }
}

pub fn set_anim(name: &str, a: Anim) {
    ANIM.with(|m| {
        if a.is_still() {
            m.borrow_mut().remove(name);
        } else {
            m.borrow_mut().insert(name.to_string(), a);
        }
    });
}

pub fn anim(name: &str) -> Option<Anim> {
    ANIM.with(|m| m.borrow().get(name).copied())
}

pub fn any_anim() -> bool {
    ANIM.with(|m| !m.borrow().is_empty())
}

fn frames_of(name: &str) -> u32 {
    anim(name).map(|a| a.frames.max(1)).unwrap_or(1)
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

/// Size of a loaded texture, if any: one frame of an animation strip.
pub fn size(name: &str) -> Option<(u32, u32)> {
    let n = frames_of(name);
    STORE.with(|s| s.borrow().get(name).map(|t| (t.w, (t.h / n).max(1))))
}
