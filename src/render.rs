// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Steventechno and the Bonk! contributors
//! OpenGL renderer for the four viewports.

use crate::doc::{Brush, Entity, ENTITY_COLOR};
use crate::editor::*;
use crate::geom;
use crate::gl::{self, Gl};
use crate::math::*;
use crate::texstore;
use std::cell::RefCell;
use std::collections::HashMap;
use std::ffi::{c_char, c_void, CString};

/// Text to draw on top of a viewport (handled by Qt's QPainter).
pub struct Text {
    pub x: f32,
    pub y: f32,
    /// 0 = left, 1 = center, 2 = right
    pub align: i32,
    pub s: String,
    pub rgba: u32,
}

const FLAT_VS: &str = r#"#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec4 a_col;
uniform mat4 u_mvp;
out vec4 v_col;
void main() { v_col = a_col; gl_Position = u_mvp * vec4(a_pos, 1.0); }
"#;

// u_stripes: diagonal screen-space hatching (used for nodraw faces in 2D views).
const FLAT_FS: &str = r#"#version 330 core
in vec4 v_col;
uniform float u_stripes;
out vec4 o_col;
// Nodraw look: u_nd = (style, intensity, stripe width px, phase); u_nd_col = solid color.
// style 0 solid, 1 RGB cycle (all stripes one hue, cycling), 2 rainbow flow.
uniform vec4 u_nd;
uniform vec3 u_nd_col;
uniform float u_nd_flow; // rainbow flow: px per whole rainbow
vec3 nd_hsv(float h) {
    vec3 k = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    return mix(vec3(1.0), k, 0.85);
}
vec3 nd_colour() {
    float d = gl_FragCoord.x + gl_FragCoord.y;
    if (u_nd.x > 1.5) return nd_hsv(fract(d / max(u_nd_flow, 1.0) - u_nd.w));
    if (u_nd.x > 0.5) return nd_hsv(fract(u_nd.w));
    return u_nd_col;
}
bool nd_band() { return mod(gl_FragCoord.x + gl_FragCoord.y, 2.0 * u_nd.z) < u_nd.z; }
void main() {
    vec4 c = v_col;
    if (u_stripes > 0.5) {
        c = vec4(nd_colour(), 0.18 + 0.4 * u_nd.y);
        if (nd_band()) c.a *= 1.0 - 0.7 * u_nd.y;
    }
    o_col = c;
}
"#;

const LIT_VS: &str = r#"#version 330 core
layout(location = 0) in vec3 a_pos;
layout(location = 1) in vec3 a_nrm;
layout(location = 2) in vec4 a_col;
layout(location = 3) in float a_stripe;
layout(location = 4) in vec2 a_uv;
uniform mat4 u_mvp;
out vec4 v_col;
out float v_stripe;
out vec2 v_uv;
out float v_light;
void main() {
    vec3 l = normalize(vec3(0.35, 0.55, 0.75));
    float d = max(dot(a_nrm, l), 0.0);
    // Slightly different base shade per axis, Hammer-style, so flat faces read clearly.
    float axis = 0.82 + 0.10 * abs(a_nrm.x) + 0.04 * abs(a_nrm.y) + 0.18 * abs(a_nrm.z);
    v_light = (0.42 + 0.50 * d) * axis;
    v_col = vec4(a_col.rgb * v_light, a_col.a);
    v_stripe = a_stripe;
    v_uv = a_uv;
    gl_Position = u_mvp * vec4(a_pos, 1.0);
}
"#;

const LIT_FS: &str = r#"#version 330 core
in vec4 v_col;
in float v_stripe;
in vec2 v_uv;
in float v_light;
uniform float u_use_tex;
uniform sampler2D u_tex;
// textures' transparency: x = 0 opaque, 1 alpha test (y = reference), 2 translucent
uniform vec2 u_alpha;
// moving materials: x frames in the strip, y current frame, zw scroll offset
uniform vec4 u_anim;
// pulse: x brightness, y alpha
uniform vec2 u_pulse;
out vec4 o_col;
// Nodraw look: u_nd = (style, intensity, stripe width px, phase); u_nd_col = solid color.
// style 0 solid, 1 RGB cycle (all stripes one hue, cycling), 2 rainbow flow.
uniform vec4 u_nd;
uniform vec3 u_nd_col;
uniform float u_nd_flow; // rainbow flow: px per whole rainbow
vec3 nd_hsv(float h) {
    vec3 k = clamp(abs(mod(h * 6.0 + vec3(0.0, 4.0, 2.0), 6.0) - 3.0) - 1.0, 0.0, 1.0);
    return mix(vec3(1.0), k, 0.85);
}
vec3 nd_colour() {
    float d = gl_FragCoord.x + gl_FragCoord.y;
    if (u_nd.x > 1.5) return nd_hsv(fract(d / max(u_nd_flow, 1.0) - u_nd.w));
    if (u_nd.x > 0.5) return nd_hsv(fract(u_nd.w));
    return u_nd_col;
}
bool nd_band() { return mod(gl_FragCoord.x + gl_FragCoord.y, 2.0 * u_nd.z) < u_nd.z; }
void main() {
    vec4 c = v_col;
    if (v_stripe > 0.5) {
        // nodraw: the chosen look, lit like everything else; selected ones tinted red
        float sel = v_stripe > 1.5 ? 0.5 : 0.0;
        c.rgb = mix(nd_colour(), vec3(1.0, 0.2, 0.2), sel) * v_light;
        if (nd_band()) c.rgb *= 1.0 - 0.55 * u_nd.y;
    }
    // textured faces: texel x (lighting * tint); the vertex color carries both
    if (u_use_tex > 0.5) {
        vec2 uv = v_uv + u_anim.zw;
        vec4 t;
        if (u_anim.x > 1.5) {
            // one frame of a strip: wrap inside the frame, mip from the unwrapped coordinates
            vec2 f = vec2(uv.x, (fract(uv.y) + u_anim.y) / u_anim.x);
            t = textureGrad(u_tex, f, dFdx(uv) * vec2(1.0, 1.0 / u_anim.x), dFdy(uv) * vec2(1.0, 1.0 / u_anim.x));
        } else {
            t = texture(u_tex, uv);
        }
        if (u_alpha.x > 0.5 && u_alpha.x < 1.5 && t.a < u_alpha.y) discard;
        c.rgb *= t.rgb * 1.25 * u_pulse.x;
        if (u_alpha.x > 1.5) c.a *= t.a;
        c.a *= u_pulse.y;
    }
    o_col = c;
}
"#;

struct Prog {
    id: u32,
    mvp: i32,
    stripes: i32,
    use_tex: i32,
    tex: i32,
    alpha: i32,
    nd: i32,
    nd_col: i32,
    nd_flow: i32,
    anim: i32,
    pulse: i32,
}

pub struct Renderer {
    gl: Gl,
    flat: Prog,
    lit: Prog,
    vao_flat: u32,
    vbo_flat: u32,
    vao_lit: u32,
    vbo_lit: u32,
    /// GL textures uploaded from the texture store: name -> (gl id, generation)
    textures: RefCell<HashMap<String, (u32, u64)>>,
    /// Nodraw look for this frame: (style, intensity, width, phase), color
    nd: std::cell::Cell<([f32; 4], [f32; 3], f32)>,
}

/// Floats per lit vertex: position, normal, rgba, stripe flag, uv.
const LIT_STRIDE: usize = 13;

/// Flat geometry: 7 floats per vertex (xyz rgba).
#[derive(Default)]
struct Flat {
    lines: Vec<f32>,
    tris: Vec<f32>,
    /// depth-tested brush outlines (3D view)
    edges: Vec<f32>,
    /// collision outlines while editing collision: drawn thick, dark under bright, depth-tested
    col_dark: Vec<f32>,
    col_bright: Vec<f32>,
    overlay: Vec<f32>,
}

fn push_v(buf: &mut Vec<f32>, p: [f32; 3], c: [f32; 4]) {
    buf.extend_from_slice(&p);
    buf.extend_from_slice(&c);
}

fn line(buf: &mut Vec<f32>, a: [f32; 3], b: [f32; 3], c: [f32; 4]) {
    push_v(buf, a, c);
    push_v(buf, b, c);
}

fn line3(buf: &mut Vec<f32>, a: V3, b: V3, c: [f32; 4]) {
    line(buf, [a.x, a.y, a.z], [b.x, b.y, b.z], c);
}

fn rect2(buf: &mut Vec<f32>, h0: f32, v0: f32, h1: f32, v1: f32, c: [f32; 4]) {
    line(buf, [h0, v0, 0.0], [h1, v0, 0.0], c);
    line(buf, [h1, v0, 0.0], [h1, v1, 0.0], c);
    line(buf, [h1, v1, 0.0], [h0, v1, 0.0], c);
    line(buf, [h0, v1, 0.0], [h0, v0, 0.0], c);
}

fn quad2(buf: &mut Vec<f32>, h0: f32, v0: f32, h1: f32, v1: f32, c: [f32; 4]) {
    for p in [[h0, v0], [h1, v0], [h1, v1], [h0, v0], [h1, v1], [h0, v1]] {
        push_v(buf, [p[0], p[1], 0.0], c);
    }
}

/// A line drawn as a quad, `w` world units wide (core-profile GL has no wide lines).
fn thick2(buf: &mut Vec<f32>, a: (f32, f32), b: (f32, f32), w: f32, c: [f32; 4]) {
    let (dx, dy) = (b.0 - a.0, b.1 - a.1);
    let l = (dx * dx + dy * dy).sqrt().max(1e-6);
    let (nx, ny) = (-dy / l * w * 0.5, dx / l * w * 0.5);
    // extend the ends by half a width so corners join cleanly
    let (ex, ey) = (dx / l * w * 0.5, dy / l * w * 0.5);
    let p = [
        (a.0 - ex + nx, a.1 - ey + ny),
        (b.0 + ex + nx, b.1 + ey + ny),
        (b.0 + ex - nx, b.1 + ey - ny),
        (a.0 - ex - nx, a.1 - ey - ny),
    ];
    for i in [0, 1, 2, 0, 2, 3] {
        push_v(buf, [p[i].0, p[i].1, 0.0], c);
    }
}

fn rgb(c: [f32; 3], a: f32) -> [f32; 4] {
    [c[0], c[1], c[2], a]
}

pub fn box_edges(lo: V3, hi: V3) -> [(V3, V3); 12] {
    let c = |i: u32| v3(if i & 1 != 0 { hi.x } else { lo.x }, if i & 2 != 0 { hi.y } else { lo.y }, if i & 4 != 0 { hi.z } else { lo.z });
    [
        (c(0), c(1)),
        (c(2), c(3)),
        (c(4), c(5)),
        (c(6), c(7)),
        (c(0), c(2)),
        (c(1), c(3)),
        (c(4), c(6)),
        (c(5), c(7)),
        (c(0), c(4)),
        (c(1), c(5)),
        (c(2), c(6)),
        (c(3), c(7)),
    ]
}

const SELECTED: [f32; 3] = [1.0, 0.15, 0.15];
const BLOCK_BOX: [f32; 3] = [1.0, 1.0, 1.0];
const CAMERA: [f32; 3] = [0.35, 0.80, 1.0];
const NODRAW_COL: [f32; 3] = [0.86, 0.74, 0.25];
const COLLISION_COL: [f32; 3] = [1.0, 0.56, 0.16];
const S_EDGE_COL: [f32; 4] = [1.0, 0.84, 0.18, 1.0];
const AXIS_COL: [[f32; 3]; 3] = [[0.95, 0.30, 0.30], [0.35, 0.90, 0.35], [0.40, 0.60, 1.0]];

// ------------------------------------------------------------------ mannequin

/// The scale mannequin: a low-poly figure made of boxes, in local
/// coordinates with the base at the origin, facing +X. 30 wide, 71 tall,
/// so it fits the 32 × 32 × 72 player hull.
const MANNEQUIN: [([f32; 3], [f32; 3]); 8] = [
    ([-4.0, -9.0, 0.0], [4.0, -2.0, 34.0]),    // left leg
    ([-4.0, 2.0, 0.0], [4.0, 9.0, 34.0]),      // right leg
    ([-5.0, -10.0, 34.0], [5.0, 10.0, 58.0]),  // torso
    ([-3.5, -15.0, 30.0], [3.5, -10.5, 57.0]), // left arm
    ([-3.5, 10.5, 30.0], [3.5, 15.0, 57.0]),   // right arm
    ([-2.5, -2.5, 58.0], [2.5, 2.5, 60.0]),    // neck
    ([-5.0, -5.0, 60.0], [5.0, 5.0, 71.0]),    // head
    ([5.0, -3.5, 64.0], [7.0, 3.5, 67.0]),     // visor: shows which way it faces
];

/// The car: about the size of Half-Life 2's jeep (176 × 96 × 80), facing +X.
const CAR: [([f32; 3], [f32; 3]); 14] = [
    ([-80.0, -36.0, 14.0], [80.0, 36.0, 34.0]),  // chassis
    ([-88.0, -30.0, 18.0], [-80.0, 30.0, 30.0]), // rear bumper
    ([40.0, -30.0, 34.0], [86.0, 30.0, 46.0]),   // engine
    ([80.0, -24.0, 20.0], [88.0, 24.0, 32.0]),   // front bumper
    ([-34.0, -26.0, 34.0], [-10.0, 26.0, 50.0]), // seats
    ([-38.0, -34.0, 34.0], [-35.0, -31.0, 80.0]), // roll cage posts
    ([-38.0, 31.0, 34.0], [-35.0, 34.0, 80.0]),
    ([24.0, -34.0, 34.0], [27.0, -31.0, 76.0]),
    ([24.0, 31.0, 34.0], [27.0, 34.0, 76.0]),
    ([-38.0, -34.0, 76.0], [27.0, 34.0, 80.0]),  // roof bar: the top
    ([40.0, -48.0, 0.0], [72.0, -36.0, 32.0]),   // wheels
    ([40.0, 36.0, 0.0], [72.0, 48.0, 32.0]),
    ([-72.0, -48.0, 0.0], [-40.0, -36.0, 32.0]),
    ([-72.0, 36.0, 0.0], [-40.0, 48.0, 32.0]),
];

/// A measurement buddy's parts as world-space boxes (8 corners each).
fn mannequin_parts(b: &Brush) -> Vec<[V3; 8]> {
    let base = b.base();
    let (s, c) = b.rot[2].to_radians().sin_cos();
    let parts: &[([f32; 3], [f32; 3])] = if b.entity == Some(Entity::Car) { &CAR } else { &MANNEQUIN };
    parts
        .iter()
        .map(|(lo, hi)| {
            let mut out = [V3::default(); 8];
            for (i, o) in out.iter_mut().enumerate() {
                let p = [if i & 1 != 0 { hi[0] } else { lo[0] }, if i & 2 != 0 { hi[1] } else { lo[1] }, if i & 4 != 0 { hi[2] } else { lo[2] }];
                *o = base + v3(p[0] * c - p[1] * s, p[0] * s + p[1] * c, p[2]);
            }
            out
        })
        .collect()
}

const CORNER_EDGES: [(usize, usize); 12] = [(0, 1), (2, 3), (4, 5), (6, 7), (0, 2), (1, 3), (4, 6), (5, 7), (0, 4), (1, 5), (2, 6), (3, 7)];
/// Faces of a corner box: (corners CCW from outside, local normal index)
const CORNER_FACES: [[usize; 4]; 6] = [[0, 2, 6, 4], [1, 5, 7, 3], [0, 4, 5, 1], [2, 3, 7, 6], [0, 1, 3, 2], [4, 6, 7, 5]];

fn push_lit(buf: &mut Vec<f32>, p: V3, n: V3, c: [f32; 4], stripe: bool) {
    push_lit_uv(buf, p, n, c, stripe, (0.0, 0.0));
}

fn push_lit_uv(buf: &mut Vec<f32>, p: V3, n: V3, c: [f32; 4], stripe: bool, uv: (f32, f32)) {
    buf.extend_from_slice(&[p.x, p.y, p.z, n.x, n.y, n.z, c[0], c[1], c[2], c[3], if stripe { 1.0 } else { 0.0 }, uv.0, uv.1]);
}

fn push_corner_box(buf: &mut Vec<f32>, k: &[V3; 8], col: [f32; 4]) {
    for f in CORNER_FACES {
        let n = (k[f[1]] - k[f[0]]).cross(k[f[2]] - k[f[0]]).norm();
        for i in [0, 1, 2, 0, 2, 3] {
            push_lit(buf, k[f[i]], n, col, false);
        }
    }
}

impl Renderer {
    /// # Safety
    /// Must be called with the viewport's GL context current.
    pub unsafe fn new(gp: gl::GetProc) -> Result<Renderer, String> {
        let gl = Gl::load(gp)?;
        let flat = program(&gl, FLAT_VS, FLAT_FS)?;
        let lit = program(&gl, LIT_VS, LIT_FS)?;
        let (vao_flat, vbo_flat) = make_vao(&gl, &[3, 4]);
        let (vao_lit, vbo_lit) = make_vao(&gl, &[3, 3, 4, 1, 2]);
        Ok(Renderer { gl, flat, lit, vao_flat, vbo_flat, vao_lit, vbo_lit, textures: RefCell::new(HashMap::new()), nd: std::cell::Cell::new(([0.0, 0.6, 6.0, 0.0], [0.86, 0.74, 0.25], 72.0)) })
    }

    fn set_nodraw_look(&self, ed: &Editor) {
        self.nd.set(([ed.nd_style as f32, ed.nd_intensity, ed.nd_width.max(1.0), ed.time * ed.nd_speed], ed.nd_col, ed.nd_flow));
    }

    unsafe fn nd_uniforms(&self, p: &Prog) {
        let g = &self.gl;
        let (a, c, flow) = self.nd.get();
        (g.Uniform4f)(p.nd, a[0], a[1], a[2], a[3]);
        (g.Uniform3f)(p.nd_col, c[0], c[1], c[2]);
        (g.Uniform1f)(p.nd_flow, flow);
    }

    /// GL texture for a material, uploading (or re-uploading) it from the
    /// texture store when it's new or has changed. None if it isn't loaded.
    unsafe fn texture(&self, name: &str) -> Option<u32> {
        let g = &self.gl;
        let (w, h, gen, px) = texstore::get(name)?;
        let mut cache = self.textures.borrow_mut();
        if let Some(&(id, cgen)) = cache.get(name) {
            if cgen == gen {
                return Some(id);
            }
            (g.DeleteTextures)(1, &id);
        }
        let mut id = 0;
        (g.GenTextures)(1, &mut id);
        (g.ActiveTexture)(gl::TEXTURE0);
        (g.BindTexture)(gl::TEXTURE_2D, id);
        (g.PixelStorei)(gl::UNPACK_ALIGNMENT, 1);
        (g.TexImage2D)(gl::TEXTURE_2D, 0, gl::RGBA8, w as i32, h as i32, 0, gl::RGBA, gl::UNSIGNED_BYTE, px.as_ptr() as *const c_void);
        (g.GenerateMipmap)(gl::TEXTURE_2D);
        (g.TexParameteri)(gl::TEXTURE_2D, gl::TEXTURE_MIN_FILTER, gl::LINEAR_MIPMAP_LINEAR);
        (g.TexParameteri)(gl::TEXTURE_2D, gl::TEXTURE_MAG_FILTER, gl::LINEAR);
        (g.TexParameteri)(gl::TEXTURE_2D, gl::TEXTURE_WRAP_S, gl::REPEAT);
        (g.TexParameteri)(gl::TEXTURE_2D, gl::TEXTURE_WRAP_T, gl::REPEAT);
        (g.BindTexture)(gl::TEXTURE_2D, 0);
        cache.insert(name.to_string(), (id, gen));
        Some(id)
    }

    /// Drop GL textures whose images were removed from the store.
    unsafe fn prune_textures(&self) {
        let g = &self.gl;
        self.textures.borrow_mut().retain(|name, (id, _)| {
            let keep = texstore::size(name).is_some();
            if !keep {
                (g.DeleteTextures)(1, id);
            }
            keep
        });
    }

    unsafe fn draw_lit_tex(&self, mvp: &M4, data: &[f32], tex: u32, alpha: (u8, f32), anim: Option<(texstore::Anim, f32)>) {
        if data.is_empty() {
            return;
        }
        let g = &self.gl;
        (g.UseProgram)(self.lit.id);
        (g.Uniform1f)(self.lit.use_tex, 1.0);
        (g.Uniform2f)(self.lit.alpha, alpha.0 as f32, alpha.1);
        match anim {
            Some((a, t)) => {
                let (frame, off, c, al) = a.at(t);
                (g.Uniform4f)(self.lit.anim, a.frames.max(1) as f32, frame as f32, off[0], off[1]);
                (g.Uniform2f)(self.lit.pulse, c, al);
            }
            None => {
                (g.Uniform4f)(self.lit.anim, 1.0, 0.0, 0.0, 0.0);
                (g.Uniform2f)(self.lit.pulse, 1.0, 1.0);
            }
        }
        (g.Uniform1i)(self.lit.tex, 0);
        (g.ActiveTexture)(gl::TEXTURE0);
        (g.BindTexture)(gl::TEXTURE_2D, tex);
        self.draw_lit(mvp, data);
        (g.UseProgram)(self.lit.id);
        (g.Uniform1f)(self.lit.use_tex, 0.0);
        (g.Uniform2f)(self.lit.alpha, 0.0, 0.0);
        (g.BindTexture)(gl::TEXTURE_2D, 0);
    }

    unsafe fn draw_flat_s(&self, mvp: &M4, data: &[f32], mode: u32, stripes: bool) {
        if data.is_empty() {
            return;
        }
        let g = &self.gl;
        (g.UseProgram)(self.flat.id);
        (g.UniformMatrix4fv)(self.flat.mvp, 1, gl::FALSE, mvp.0.as_ptr());
        (g.Uniform1f)(self.flat.stripes, if stripes { 1.0 } else { 0.0 });
        if stripes {
            self.nd_uniforms(&self.flat);
        }
        (g.BindVertexArray)(self.vao_flat);
        (g.BindBuffer)(gl::ARRAY_BUFFER, self.vbo_flat);
        (g.BufferData)(gl::ARRAY_BUFFER, std::mem::size_of_val(data) as isize, data.as_ptr() as *const c_void, gl::STREAM_DRAW);
        (g.DrawArrays)(mode, 0, (data.len() / 7) as i32);
    }

    unsafe fn draw_flat(&self, mvp: &M4, data: &[f32], mode: u32) {
        self.draw_flat_s(mvp, data, mode, false);
    }

    unsafe fn draw_lit(&self, mvp: &M4, data: &[f32]) {
        if data.is_empty() {
            return;
        }
        let g = &self.gl;
        (g.UseProgram)(self.lit.id);
        (g.UniformMatrix4fv)(self.lit.mvp, 1, gl::FALSE, mvp.0.as_ptr());
        self.nd_uniforms(&self.lit);
        (g.BindVertexArray)(self.vao_lit);
        (g.BindBuffer)(gl::ARRAY_BUFFER, self.vbo_lit);
        (g.BufferData)(gl::ARRAY_BUFFER, std::mem::size_of_val(data) as isize, data.as_ptr() as *const c_void, gl::STREAM_DRAW);
        (g.DrawArrays)(gl::TRIANGLES, 0, (data.len() / LIT_STRIDE) as i32);
    }

    unsafe fn finish(&self) {
        let g = &self.gl;
        (g.UseProgram)(0);
        (g.BindVertexArray)(0);
        (g.BindBuffer)(gl::ARRAY_BUFFER, 0);
        (g.Disable)(gl::DEPTH_TEST);
        (g.Disable)(gl::BLEND);
        (g.Disable)(gl::POLYGON_OFFSET_FILL);
        (g.DepthMask)(1);
    }

    // ------------------------------------------------------------------ 2D

    /// # Safety
    /// GL context of this viewport must be current.
    pub unsafe fn render_2d(&self, ed: &Editor, id: usize, texts: &mut Vec<Text>) {
        // one set per frame: selection lookups stay cheap with thousands of brushes
        let selset: std::collections::HashSet<u32> = ed.doc.selection.iter().copied().collect();
        let is_sel = |id: u32| selset.contains(&id);
        let g = &self.gl;
        let v = ed.view(id);
        let (ha, va, da) = v.kind.axes();
        let bg = ed.cfg.bg;
        let light = ed.cfg.light_bg();
        (g.ClearColor)(bg[0], bg[1], bg[2], 1.0);
        (g.Clear)(gl::COLOR_BUFFER_BIT | gl::DEPTH_BUFFER_BIT);
        (g.Disable)(gl::DEPTH_TEST);
        (g.Enable)(gl::BLEND);
        (g.BlendFunc)(gl::SRC_ALPHA, gl::ONE_MINUS_SRC_ALPHA);

        let hw = v.w * 0.5 / v.zoom;
        let hh = v.h * 0.5 / v.zoom;
        let (l, r, b, t) = (v.cx - hw, v.cx + hw, v.cy - hh, v.cy + hh);
        let mvp = M4::ortho(l, r, b, t, -1.0, 1.0);
        let px = 1.0 / v.zoom; // one pixel in world units
        let mut f = Flat::default();
        let p2 = |p: V3| [p.get(ha), p.get(va), 0.0];

        // Grid. Lines that would be closer than 4px are skipped by doubling the step.
        let mut step = ed.grid;
        while step * v.zoom < 4.0 {
            step *= 2.0;
        }
        // Grid colors are the background pushed toward a contrasting color;
        // the intensity setting scales how far.
        let k = ed.cfg.grid_intensity;
        let con = ed.cfg.contrast();
        let toward = |t: [f32; 3], amt: f32| -> [f32; 4] {
            let a = amt.clamp(0.0, 1.0);
            [bg[0] + (t[0] - bg[0]) * a, bg[1] + (t[1] - bg[1]) * a, bg[2] + (t[2] - bg[2]) * a, 1.0]
        };
        let grid_col = |c: f32| -> [f32; 4] {
            if c.abs() < 1e-3 {
                toward([0.20, 0.62, 0.75], 0.65 * k.min(1.4))
            } else if (c / 1024.0).fract().abs() < 1e-6 {
                toward([0.80, 0.42, 0.25], 0.45 * k)
            } else if (c / 64.0).fract().abs() < 1e-6 && step < 64.0 {
                toward(con, 0.28 * k)
            } else {
                toward(con, 0.15 * k)
            }
        };
        let mut i = (l / step).floor() as i64;
        while (i as f32) * step <= r {
            let x = i as f32 * step;
            line(&mut f.lines, [x, b, 0.0], [x, t, 0.0], grid_col(x));
            i += 1;
        }
        let mut j = (b / step).floor() as i64;
        while (j as f32) * step <= t {
            let y = j as f32 * step;
            line(&mut f.lines, [l, y, 0.0], [r, y, 0.0], grid_col(y));
            j += 1;
        }

        // Nodraw faces turned toward the viewer get a striped yellow fill.
        let toward_viewer = match v.kind {
            ViewKind::Top => v3(0.0, 0.0, 1.0),
            ViewKind::Front => v3(-1.0, 0.0, 0.0),
            _ => v3(0.0, -1.0, 0.0),
        };
        self.set_nodraw_look(ed);
        let mut stripes = Vec::new();
        for br in ed.doc.brushes.iter().filter(|b| ed.nodraw_2d && ed.drawable(b) && b.entity.is_none() && !ed.ghosted(b)) {
            for fc in br.faces.iter().filter(|fc| fc.material == geom::NODRAW && fc.normal.dot(toward_viewer) > 0.05) {
                for k in 1..fc.idx.len().saturating_sub(1) {
                    for vi in [fc.idx[0], fc.idx[k], fc.idx[k + 1]] {
                        push_v(&mut stripes, p2(br.verts[vi]), rgb(NODRAW_COL, 0.45));
                    }
                }
            }
        }

        // Brushes: unselected first so selection draws on top.
        let dark = |c: [f32; 3]| [c[0] * 0.45, c[1] * 0.45, c[2] * 0.45];
        for pass in [false, true] {
            for br in ed.doc.brushes.iter().filter(|br| ed.drawable(br) && is_sel(br.id) == pass) {
                let base = if br.entity.is_some() {
                    ENTITY_COLOR
                } else if ed.doc.is_collision(br) && !ed.collision_solid {
                    COLLISION_COL
                } else {
                    br.color
                };
                let mut c = if pass {
                    rgb(SELECTED, 1.0)
                } else if light {
                    rgb(dark(base), 1.0)
                } else {
                    rgb(base, 1.0)
                };
                if ed.ghosted(br) {
                    c[3] = ed.ghost_opacity.max(0.12);
                }
                if br.entity.is_some() {
                    // the figure's outline, plus its hull box faintly
                    for part in mannequin_parts(br) {
                        for (a, bb) in CORNER_EDGES {
                            line(&mut f.lines, p2(part[a]), p2(part[bb]), c);
                        }
                    }
                    let (lo, hi) = br.bounds();
                    rect2(&mut f.lines, lo.get(ha), lo.get(va), hi.get(ha), hi.get(va), [c[0], c[1], c[2], 0.35]);
                    continue;
                }
                for (i, j) in br.edges() {
                    line(&mut f.lines, p2(br.verts[i]), p2(br.verts[j]), c);
                }
            }
        }

        // Camera marker
        {
            let cam_col = if light { [0.05, 0.40, 0.70] } else { CAMERA };
            let cp = ed.cam.pos;
            let (ch, cv) = (cp.get(ha), cp.get(va));
            let s = 4.0 * px;
            rect2(&mut f.overlay, ch - s, cv - s, ch + s, cv + s, rgb(cam_col, 1.0));
            let fw = ed.cam.forward();
            let (dh, dv) = (fw.get(ha), fw.get(va));
            let dl = (dh * dh + dv * dv).sqrt();
            if dl > 1e-3 {
                let k = 28.0 * px / dl;
                line(&mut f.overlay, [ch, cv, 0.0], [ch + dh * k, cv + dv * k, 0.0], rgb(cam_col, 1.0));
            }
        }

        // Model origin: a small ring with axis arms in the view's two axes
        {
            let o = ed.doc.origin;
            let (oh, ov) = (o.get(ha), o.get(va));
            let strong = ed.tool == Tool::Origin;
            let a = if strong { 1.0 } else { 0.7 };
            let arm = (if strong { 20.0 } else { 14.0 }) * px;
            let w = (if strong { 2.5 } else { 1.5 }) * px;
            thick2(&mut f.tris, (oh, ov), (oh + arm, ov), w, rgb(AXIS_COL[ha], a));
            thick2(&mut f.tris, (oh, ov), (oh, ov + arm), w, rgb(AXIS_COL[va], a));
            circle2(&mut f.overlay, oh, ov, 4.0 * px, rgb(con, a));
            let _ = da;
        }

        // "Dropped here": squares growing out of the dropped bounds and fading away
        if let Some(((lo, hi), t0)) = ed.ripple {
            let t = ((ed.time - t0) / crate::editor::RIPPLE_SECS).clamp(0.0, 1.0);
            let rc = if light { [0.1, 0.35, 0.75] } else { [0.55, 0.85, 1.0] };
            for k in 0..3 {
                let tk = (t * 1.6 - k as f32 * 0.3).clamp(0.0, 1.0);
                if tk <= 0.0 || tk >= 1.0 {
                    continue;
                }
                let grow = tk * 28.0 * px;
                let a = (1.0 - tk) * 0.9;
                let (h0, v0, h1, v1) = (lo.get(ha) - grow, lo.get(va) - grow, hi.get(ha) + grow, hi.get(va) + grow);
                let w = 1.5 * px;
                for (p, q) in [((h0, v0), (h1, v0)), ((h1, v0), (h1, v1)), ((h1, v1), (h0, v1)), ((h0, v1), (h0, v0))] {
                    thick2(&mut f.tris, p, q, w, rgb(rc, a));
                }
            }
        }

        // Selection box: the box, and what it would catch, in yellow
        if let (Tool::Select, Some((lo, hi))) = (ed.tool, ed.sel_box) {
            let ycol = if light { [0.65, 0.5, 0.0, 1.0] } else { [1.0, 0.86, 0.1, 1.0] };
            for id in ed.sel_box_catch() {
                if let Some(br) = ed.doc.brush(id) {
                    if br.entity.is_some() {
                        let (l2, h2) = br.bounds();
                        rect2(&mut f.overlay, l2.get(ha), l2.get(va), h2.get(ha), h2.get(va), ycol);
                        continue;
                    }
                    for (i, j) in br.edges() {
                        line(&mut f.overlay, p2(br.verts[i]), p2(br.verts[j]), ycol);
                    }
                }
            }
            let (h0, v0, h1, v1) = (lo.get(ha), lo.get(va), hi.get(ha), hi.get(va));
            // dashed outline
            let dash = 6.0 * px;
            for (a, b) in [((h0, v0), (h1, v0)), ((h1, v0), (h1, v1)), ((h1, v1), (h0, v1)), ((h0, v1), (h0, v0))] {
                let len = ((b.0 - a.0).powi(2) + (b.1 - a.1).powi(2)).sqrt();
                let n = (len / dash).ceil().max(1.0) as i32;
                for k in (0..n).step_by(2) {
                    let t0 = k as f32 / n as f32;
                    let t1 = ((k + 1) as f32 / n as f32).min(1.0);
                    line(&mut f.overlay, [a.0 + (b.0 - a.0) * t0, a.1 + (b.1 - a.1) * t0, 0.0], [a.0 + (b.0 - a.0) * t1, a.1 + (b.1 - a.1) * t1, 0.0], ycol);
                }
            }
        }

        // Layers with their own origin
        for l in ed.doc.layers.iter().filter(|l| l.visible) {
            if let Some(o) = l.origin {
                let (oh, ov) = (o.get(ha), o.get(va));
                let arm = 12.0 * px;
                thick2(&mut f.tris, (oh, ov), (oh + arm, ov), 1.5 * px, rgb(COLLISION_COL, 1.0));
                thick2(&mut f.tris, (oh, ov), (oh, ov + arm), 1.5 * px, rgb(COLLISION_COL, 1.0));
                circle2(&mut f.overlay, oh, ov, 3.0 * px, rgb(COLLISION_COL, 1.0));
            }
        }

        // Pending block box
        if let Some((lo, hi)) = ed.block_box {
            let c = if light { [0.05, 0.05, 0.05] } else { BLOCK_BOX };
            if let Some(h) = ed.block_preview() {
                // the shape it will become, inside a faint box
                rect2(&mut f.overlay, lo.get(ha), lo.get(va), hi.get(ha), hi.get(va), rgb(c, 0.35));
                for fc in &h.faces {
                    for i in 0..fc.idx.len() {
                        line(&mut f.overlay, p2(h.verts[fc.idx[i]]), p2(h.verts[fc.idx[(i + 1) % fc.idx.len()]]), rgb(c, 1.0));
                    }
                }
            } else {
                rect2(&mut f.overlay, lo.get(ha), lo.get(va), hi.get(ha), hi.get(va), rgb(c, 1.0));
            }
        }

        // Handles + dimensions for the active box
        if let Some((lo, hi)) = ed.active_box() {
            let (h0, v0, h1, v1) = (lo.get(ha), lo.get(va), hi.get(ha), hi.get(va));
            let rotate = ed.block_box.is_none() && ed.handle_mode() == HandleMode::Rotate;
            let small = (h1 - h0) * v.zoom < HANDLE_PX * 6.0 || (v1 - v0) * v.zoom < HANDLE_PX * 6.0;
            let s = HANDLE_PX * px;
            for (hx, hy) in handle_list(small || rotate) {
                let ph = [h0, (h0 + h1) * 0.5, h1][(hx + 1) as usize];
                let pv = [v0, (v0 + v1) * 0.5, v1][(hy + 1) as usize];
                if rotate {
                    // Hammer-style round rotation handles
                    disc2(&mut f.tris, ph, pv, s * 1.2, [1.0, 1.0, 1.0, 1.0]);
                    circle2(&mut f.overlay, ph, pv, s * 1.2, [0.0, 0.0, 0.0, 1.0]);
                } else {
                    quad2(&mut f.tris, ph - s, pv - s, ph + s, pv + s, [1.0, 1.0, 1.0, 1.0]);
                    rect2(&mut f.overlay, ph - s, pv - s, ph + s, pv + s, [0.0, 0.0, 0.0, 1.0]);
                }
            }
            if !rotate {
                let col = if ed.block_box.is_some() {
                    if light { 0x1010_10ff } else { 0xffff_ffff }
                } else if light {
                    0xc020_20ff
                } else {
                    0xff60_60ff
                };
                let (tx, ty) = v.to_screen((h0 + h1) * 0.5, v1);
                texts.push(Text { x: tx, y: ty - 10.0, align: 1, s: fmt_num(h1 - h0), rgba: col });
                let (lx, ly) = v.to_screen(h0, (v0 + v1) * 0.5);
                texts.push(Text { x: lx - 10.0, y: ly + 4.0, align: 2, s: fmt_num(v1 - v0), rgba: col });
            }
        }

        // S held: the edges that will move, drawn bold (4px; 2px when zoomed far out)
        if let Some((sview, (lo, hi), edges)) = ed.s_edges() {
            if sview == id {
                let w = if v.zoom < 0.25 { 2.0 } else { 4.0 } * px;
                let (h0, v0, h1, v1) = (lo.get(ha), lo.get(va), hi.get(ha), hi.get(va));
                for (horizontal, max) in edges {
                    let (a, bb) = if horizontal {
                        let x = if max { h1 } else { h0 };
                        ((x, v0), (x, v1))
                    } else {
                        let y = if max { v1 } else { v0 };
                        ((h0, y), (h1, y))
                    };
                    thick2(&mut f.tris, a, bb, w, S_EDGE_COL);
                }
            }
        }

        // Vertex tool: edge-midpoint and vertex handles of selected brushes
        if ed.tool == Tool::Vertex {
            let s = VERTEX_PX * px;
            let editable = |b: &&Brush| is_sel(b.id) && b.entity.is_none();
            for br in ed.doc.brushes.iter().filter(editable) {
                for (i, j) in br.edges() {
                    let m = (br.verts[i] + br.verts[j]) * 0.5;
                    let (mh, mv) = (m.get(ha), m.get(va));
                    let e = s * 0.7;
                    rect2(&mut f.overlay, mh - e, mv - e, mh + e, mv + e, [1.0, 0.85, 0.3, 1.0]);
                }
            }
            // unselected vertices first, selected on top
            for pass in [false, true] {
                for br in ed.doc.brushes.iter().filter(editable) {
                    for &p in &br.verts {
                        if ed.vertex_selected(br.id, p) != pass {
                            continue;
                        }
                        let (ph, pv) = (p.get(ha), p.get(va));
                        let c = if pass { [1.0, 0.2, 0.2, 1.0] } else { [1.0, 1.0, 1.0, 1.0] };
                        quad2(&mut f.tris, ph - s, pv - s, ph + s, pv + s, c);
                        rect2(&mut f.overlay, ph - s, pv - s, ph + s, pv + s, [0.0, 0.0, 0.0, 1.0]);
                    }
                }
            }
            if let Some((mview, a, b)) = ed.marquee() {
                if mview == id {
                    let (ah, av) = v.to_world(a.0, a.1);
                    let (bh, bv) = v.to_world(b.0, b.1);
                    rect2(&mut f.overlay, ah, av, bh, bv, [1.0, 1.0, 0.4, 1.0]);
                }
            }
        }

        // Bisect: tint the pieces the pending cut would make, plus the cut line itself
        let mut fills = Vec::new();
        if ed.tool == Tool::Bisect {
            if let Some(pv) = ed.bisect_preview() {
                for (_, neg, pos) in &pv.pieces {
                    for (piece, is_pos) in [(neg, false), (pos, true)] {
                        let Some(pc) = piece else { continue };
                        let Some((col, filled)) = bisect_tint(ed.bisect_delete, is_pos == pv.delete_pos, con) else { continue };
                        if filled {
                            let poly = geom::hull2d(&pc.verts.iter().map(|q| (q.get(ha), q.get(va))).collect::<Vec<_>>());
                            for k in 1..poly.len().saturating_sub(1) {
                                for p in [poly[0], poly[k], poly[k + 1]] {
                                    push_v(&mut fills, [p.0, p.1, 0.0], [col[0], col[1], col[2], 0.28]);
                                }
                            }
                        }
                        for (i, j) in pc.edges() {
                            line(&mut f.overlay, p2(pc.verts[i]), p2(pc.verts[j]), rgb(col, 1.0));
                        }
                    }
                }
            }
            if let Some((lv, a, b)) = ed.bisect_line {
                if lv == id {
                    line(&mut f.overlay, [a.0, a.1, 0.0], [b.0, b.1, 0.0], rgb(con, 1.0));
                    for p in [a, b] {
                        disc2(&mut f.tris, p.0, p.1, 5.0 * px, [1.0, 0.82, 0.25, 1.0]);
                        circle2(&mut f.overlay, p.0, p.1, 5.0 * px, [0.0, 0.0, 0.0, 1.0]);
                    }
                }
            }
        }

        self.draw_flat(&mvp, &f.lines, gl::LINES);
        self.draw_flat_s(&mvp, &stripes, gl::TRIANGLES, true);
        self.draw_flat(&mvp, &fills, gl::TRIANGLES);
        self.draw_flat(&mvp, &f.tris, gl::TRIANGLES);
        self.draw_flat(&mvp, &f.overlay, gl::LINES);
        self.finish();
    }

    // ------------------------------------------------------------------ 3D

    /// # Safety
    /// GL context of this viewport must be current.
    /// A spawn icon: the model's visible faces only (nodraw, collision and
    /// buddies left out), on a transparent background, from `eye` looking at
    /// `target`. Draws into whatever framebuffer is bound (the caller sets the viewport).
    pub unsafe fn render_thumb(&self, doc: &crate::doc::Document, eye: V3, target: V3, fov: f32, w: f32, h: f32, grid: bool) {
        let g = &self.gl;
        (g.ClearColor)(0.0, 0.0, 0.0, 0.0);
        (g.Clear)(gl::COLOR_BUFFER_BIT | gl::DEPTH_BUFFER_BIT);
        (g.Enable)(gl::DEPTH_TEST);
        (g.DepthFunc)(gl::LEQUAL);
        (g.Enable)(gl::BLEND);
        (g.BlendFunc)(gl::SRC_ALPHA, gl::ONE_MINUS_SRC_ALPHA);
        // coverage adds up in the alpha channel (the picture goes over a background later)
        (g.BlendFuncSeparate)(gl::SRC_ALPHA, gl::ONE_MINUS_SRC_ALPHA, gl::ONE, gl::ONE_MINUS_SRC_ALPHA);
        let fwd = (target - eye).norm();
        let up = if fwd.z.abs() > 0.999 { v3(0.0, 1.0, 0.0) } else { v3(0.0, 0.0, 1.0) };
        let proj = M4::perspective(fov.to_radians(), w / h.max(1.0), 1.0, 65536.0);
        let mvp = proj.mul(&M4::look_at(eye, target, up));
        let mut lit: Vec<f32> = Vec::new();
        let mut textured: HashMap<String, (u32, Vec<f32>)> = HashMap::new();
        self.prune_textures();
        // what the compiled model has: every visual brush (hidden layers too), untextured in its color
        for br in doc.brushes.iter().filter(|b| b.entity.is_none() && !doc.is_collision(b)) {
            for fc in &br.faces {
                if fc.material == geom::NODRAW {
                    continue;
                }
                if let (Some(tex), Some((tw, th))) = (self.texture(&fc.material), texstore::size(&fc.material)) {
                    let buf = &mut textured.entry(fc.material.clone()).or_insert_with(|| (tex, Vec::new())).1;
                    let tm = fc.tex.or_default_for(fc.normal);
                    for k in 1..fc.idx.len().saturating_sub(1) {
                        for vi in [fc.idx[0], fc.idx[k], fc.idx[k + 1]] {
                            let p = br.verts[vi];
                            let (u, v) = tm.uv(p);
                            push_lit_uv(buf, p, fc.normal, [1.0, 1.0, 1.0, 1.0], false, (u / tw as f32, v / th as f32));
                        }
                    }
                } else {
                    for k in 1..fc.idx.len().saturating_sub(1) {
                        for vi in [fc.idx[0], fc.idx[k], fc.idx[k + 1]] {
                            push_lit(&mut lit, br.verts[vi], fc.normal, rgb(br.color, 1.0), false);
                        }
                    }
                }
            }
        }
        self.draw_lit(&mvp, &lit);
        for (name, (tex, data)) in textured.iter() {
            let a = texstore::alpha(name);
            if a.0 != 2 {
                self.draw_lit_tex(&mvp, data, *tex, a, None);
            }
        }
        if grid {
            // a floor grid around the model's footprint, at z = 0, fading out
            let (mut lo, mut hi) = (v3(f32::MAX, f32::MAX, 0.0), v3(f32::MIN, f32::MIN, 0.0));
            for br in doc.brushes.iter().filter(|b| b.entity.is_none() && !doc.is_collision(b)) {
                let (a, b) = br.bounds();
                lo = lo.min(a);
                hi = hi.max(b);
            }
            if lo.x <= hi.x {
                let size = (hi.x - lo.x).max(hi.y - lo.y).max(64.0);
                let step = [16.0f32, 32.0, 64.0, 128.0, 256.0].into_iter().find(|s| size / s <= 16.0).unwrap_or(512.0);
                let (cx, cy) = ((lo.x + hi.x) * 0.5, (lo.y + hi.y) * 0.5);
                let reach = size * 0.9 + step * 2.0;
                let n = (reach / step).ceil() as i32;
                let (x0, y0) = ((cx / step).round() * step, (cy / step).round() * step);
                let mut lines = Vec::new();
                for i in -n..=n {
                    let fade = 1.0 - (i.abs() as f32 / (n as f32 + 1.0));
                    let c = [1.0, 1.0, 1.0, 0.35 * fade];
                    let x = x0 + i as f32 * step;
                    let y = y0 + i as f32 * step;
                    line(&mut lines, [x, y0 - n as f32 * step, 0.05], [x, y0 + n as f32 * step, 0.05], c);
                    line(&mut lines, [x0 - n as f32 * step, y, 0.05], [x0 + n as f32 * step, y, 0.05], c);
                }
                self.draw_flat(&mvp, &lines, gl::LINES);
            }
        }
        (g.DepthMask)(0);
        for (name, (tex, data)) in textured.iter() {
            let a = texstore::alpha(name);
            if a.0 == 2 {
                self.draw_lit_tex(&mvp, data, *tex, a, None);
            }
        }
        (g.DepthMask)(1);
        (g.BlendFunc)(gl::SRC_ALPHA, gl::ONE_MINUS_SRC_ALPHA);
    }

    pub unsafe fn render_3d(&self, ed: &Editor) {
        // one set per frame: selection lookups stay cheap with thousands of brushes
        let selset: std::collections::HashSet<u32> = ed.doc.selection.iter().copied().collect();
        let is_sel = |id: u32| selset.contains(&id);
        let g = &self.gl;
        let (w, h) = ed.persp_size;
        let bg = ed.cfg.bg;
        let con = ed.cfg.contrast();
        (g.ClearColor)(bg[0], bg[1], bg[2], 1.0);
        (g.Clear)(gl::COLOR_BUFFER_BIT | gl::DEPTH_BUFFER_BIT);
        (g.Enable)(gl::DEPTH_TEST);
        (g.DepthFunc)(gl::LEQUAL);
        (g.Enable)(gl::BLEND);
        (g.BlendFunc)(gl::SRC_ALPHA, gl::ONE_MINUS_SRC_ALPHA);

        let cam = &ed.cam;
        let proj = M4::perspective(ed.fov.to_radians(), w / h, 1.0, 65536.0);
        let view = M4::look_at(cam.pos, cam.pos + cam.forward(), v3(0.0, 0.0, 1.0));
        let mvp = proj.mul(&view);

        // Ground reference grid on z = 0
        let mut f = Flat::default();
        let ext = 2048.0;
        let mut x = -ext;
        while x <= ext {
            let a = (0.22 * ed.cfg.grid_intensity).clamp(0.0, 1.0);
            let c = if x == 0.0 { [0.0, 0.0, 0.0, 0.0] } else { [con[0], con[1], con[2], a] };
            line(&mut f.lines, [x, -ext, 0.0], [x, ext, 0.0], c);
            line(&mut f.lines, [-ext, x, 0.0], [ext, x, 0.0], c);
            x += 128.0;
        }
        line(&mut f.lines, [0.0, 0.0, 0.0], [ext, 0.0, 0.0], [0.85, 0.25, 0.25, 0.9]);
        line(&mut f.lines, [0.0, 0.0, 0.0], [0.0, ext, 0.0], [0.30, 0.80, 0.30, 0.9]);
        line(&mut f.lines, [0.0, 0.0, 0.0], [0.0, 0.0, 256.0], [0.35, 0.50, 1.0, 0.9]);

        // Faces: opaque first, then collision layers translucent on top
        let mut lit: Vec<f32> = Vec::with_capacity(ed.doc.brushes.len() * 36 * LIT_STRIDE);
        let mut glass: Vec<f32> = Vec::new();
        // textured faces, batched per material
        let mut textured: HashMap<String, (u32, Vec<f32>)> = HashMap::new();
        self.prune_textures();
        self.set_nodraw_look(ed);
        let editing_col = ed.editing_collision();
        for br in ed.doc.brushes.iter().filter(|b| ed.drawable(b)) {
            let sel = is_sel(br.id);
            if ed.ghosted(br) {
                // editing collision: the visual model is a see-through ghost
                let a = ed.ghost_opacity.clamp(0.0, 1.0);
                if a <= 0.01 {
                    continue;
                }
                let col = if br.entity.is_some() { ENTITY_COLOR } else { br.color };
                for fc in &br.faces {
                    for k in 1..fc.idx.len().saturating_sub(1) {
                        for vi in [fc.idx[0], fc.idx[k], fc.idx[k + 1]] {
                            push_lit(&mut glass, br.verts[vi], fc.normal, rgb(col, a * 0.8), false);
                        }
                    }
                }
                for (i, j) in br.edges() {
                    line3(&mut f.edges, br.verts[i], br.verts[j], rgb(col, (a * 1.4).min(1.0)));
                }
                continue;
            }
            if br.entity.is_some() {
                let col = if sel { mix(ENTITY_COLOR, SELECTED, 0.55) } else { ENTITY_COLOR };
                for part in mannequin_parts(br) {
                    push_corner_box(&mut lit, &part, rgb(col, 1.0));
                    for (a, b) in CORNER_EDGES {
                        line3(&mut f.edges, part[a], part[b], rgb(dark(col), 1.0));
                    }
                }
                if sel {
                    let (lo, hi) = br.bounds();
                    for (a, b) in box_edges(lo, hi) {
                        line3(&mut f.overlay, a, b, rgb(SELECTED, 0.8));
                    }
                }
                continue;
            }
            let collision = ed.doc.is_collision(br);
            // View → Collision Look: Solid draws each collision brush opaque in its own color
            let solid = collision && ed.collision_solid;
            for (fi, fc) in br.faces.iter().enumerate() {
                let nodraw = fc.material == geom::NODRAW && !solid;
                let mut col = if solid {
                    br.color
                } else if collision && editing_col {
                    // each piece a slightly different shade, so neighbors read apart
                    let k = 0.72 + 0.4 * ((br.id.wrapping_mul(2654435761) >> 24) as f32 / 255.0);
                    [(COLLISION_COL[0] * k).min(1.0), (COLLISION_COL[1] * k).min(1.0), (COLLISION_COL[2] * k).min(1.0)]
                } else if collision {
                    COLLISION_COL
                } else if nodraw {
                    NODRAW_COL
                } else {
                    br.color
                };
                if sel {
                    col = mix(col, SELECTED, 0.55);
                }
                let fsel = ed.face_selected(br.id, fi);
                if fsel {
                    col = mix(col, [1.0, 0.25, 0.25], 0.75);
                }
                // A project texture: draw the image, world-aligned like Hammer.
                if !collision && !nodraw && !fc.material.is_empty() {
                    if let (Some(tex), Some((tw, th))) = (self.texture(&fc.material), texstore::size(&fc.material)) {
                        let mut tint = [1.0f32, 1.0, 1.0];
                        if sel {
                            tint = mix(tint, SELECTED, 0.45);
                        }
                        if fsel {
                            tint = mix(tint, [1.0, 0.25, 0.25], 0.6);
                        }
                        let buf = &mut textured.entry(fc.material.clone()).or_insert_with(|| (tex, Vec::new())).1;
                        let tm = fc.tex.or_default_for(fc.normal);
                        for k in 1..fc.idx.len().saturating_sub(1) {
                            for vi in [fc.idx[0], fc.idx[k], fc.idx[k + 1]] {
                                let p = br.verts[vi];
                                let (u, v) = tm.uv(p);
                                push_lit_uv(buf, p, fc.normal, rgb(tint, 1.0), false, (u / tw as f32, v / th as f32));
                            }
                        }
                        if fsel {
                            for i in 0..fc.idx.len() {
                                line3(&mut f.overlay, br.verts[fc.idx[i]], br.verts[fc.idx[(i + 1) % fc.idx.len()]], [1.0, 0.85, 0.2, 1.0]);
                            }
                        }
                        continue;
                    }
                }
                let (buf, alpha) = if collision && !editing_col && !solid { (&mut glass, 0.35) } else { (&mut lit, 1.0) };
                for k in 1..fc.idx.len().saturating_sub(1) {
                    for vi in [fc.idx[0], fc.idx[k], fc.idx[k + 1]] {
                        let n = buf.len();
                        push_lit(buf, br.verts[vi], fc.normal, rgb(col, alpha), nodraw);
                        if nodraw && (sel || fsel) {
                            buf[n + 10] = 2.0; // selected nodraw: keep the red tint
                        }
                    }
                }
                if fsel {
                    for i in 0..fc.idx.len() {
                        line3(&mut f.overlay, br.verts[fc.idx[i]], br.verts[fc.idx[(i + 1) % fc.idx.len()]], [1.0, 0.85, 0.2, 1.0]);
                    }
                }
            }
            let base = if collision && !solid { COLLISION_COL } else { br.color };
            let ec = if sel { rgb(SELECTED, 1.0) } else { rgb(dark(base), 1.0) };
            // View → Show Edges in 3D off: only selected brushes (and collision
            // while you edit it) keep their outlines
            let outline = ed.show_edges_3d || sel || (collision && editing_col);
            for (a, b) in br.edges().into_iter().map(|(i, j)| (br.verts[i], br.verts[j])).filter(|_| outline) {
                if collision && editing_col && !sel {
                    // editing collision: thick two-tone edges that hide behind what's in front
                    let bright = if solid { mix(br.color, [1.0, 1.0, 1.0], 0.55) } else { [1.0, 0.93, 0.8] };
                    line3(&mut f.col_dark, a, b, [0.04, 0.03, 0.02, 0.95]);
                    line3(&mut f.col_bright, a, b, rgb(bright, 1.0));
                    continue;
                }
                let target = if sel || (collision && !solid) { &mut f.overlay } else { &mut f.edges };
                line3(target, a, b, if collision && !sel && !solid { rgb(COLLISION_COL, 0.9) } else { ec });
            }
        }
        if let Some((lo, hi)) = ed.block_box {
            if let Some(h) = ed.block_preview() {
                for fc in &h.faces {
                    for i in 0..fc.idx.len() {
                        line3(&mut f.overlay, h.verts[fc.idx[i]], h.verts[fc.idx[(i + 1) % fc.idx.len()]], [1.0, 1.0, 0.3, 1.0]);
                    }
                }
            } else {
                for (a, b) in box_edges(lo, hi) {
                    line3(&mut f.overlay, a, b, [1.0, 1.0, 0.3, 1.0]);
                }
            }
        }
        // Bisect preview: outline the pieces in their tints
        if ed.tool == Tool::Bisect {
            if let Some(pv) = ed.bisect_preview() {
                for (_, neg, pos) in &pv.pieces {
                    for (piece, is_pos) in [(neg, false), (pos, true)] {
                        let (Some(pc), Some((col, _))) = (piece, bisect_tint(ed.bisect_delete, is_pos == pv.delete_pos, con)) else { continue };
                        for (i, j) in pc.edges() {
                            line3(&mut f.overlay, pc.verts[i], pc.verts[j], rgb(col, 1.0));
                        }
                    }
                }
            }
        }
        // Model origin gizmo (always on top)
        {
            let o = ed.doc.origin;
            let len = if ed.tool == Tool::Origin { 32.0 } else { 20.0 };
            for (axis, dir) in [v3(1.0, 0.0, 0.0), v3(0.0, 1.0, 0.0), v3(0.0, 0.0, 1.0)].into_iter().enumerate() {
                line3(&mut f.overlay, o, o + dir * len, rgb(AXIS_COL[axis], 1.0));
            }
        }
        // Selection box and what it catches
        if let (Tool::Select, Some((lo, hi))) = (ed.tool, ed.sel_box) {
            for (a, b) in box_edges(lo, hi) {
                line3(&mut f.overlay, a, b, [1.0, 0.86, 0.1, 0.8]);
            }
            for id in ed.sel_box_catch() {
                if let Some(br) = ed.doc.brush(id) {
                    for (i, j) in br.edges() {
                        line3(&mut f.overlay, br.verts[i], br.verts[j], [1.0, 0.86, 0.1, 1.0]);
                    }
                }
            }
        }
        // Layers with their own origin (e.g. a collision layer): a gizmo in orange
        for l in ed.doc.layers.iter().filter(|l| l.visible) {
            if let Some(o) = l.origin {
                let len = if ed.tool == Tool::Origin && ed.doc.active_layer == l.id { 28.0 } else { 16.0 };
                for dir in [v3(1.0, 0.0, 0.0), v3(0.0, 1.0, 0.0), v3(0.0, 0.0, 1.0)] {
                    line3(&mut f.overlay, o, o + dir * len, rgb(COLLISION_COL, 1.0));
                }
            }
        }
        // Orbit pivot while middle-dragging
        if let Some(p) = ed.orbit_pivot {
            let s = ((p - cam.pos).len() * 0.025).max(2.0);
            for dir in [v3(1.0, 0.0, 0.0), v3(0.0, 1.0, 0.0), v3(0.0, 0.0, 1.0)] {
                line3(&mut f.overlay, p - dir * s, p + dir * s, rgb(con, 0.95));
            }
        }

        (g.DepthMask)(0);
        self.draw_flat(&mvp, &f.lines, gl::LINES);
        (g.DepthMask)(1);
        (g.Enable)(gl::POLYGON_OFFSET_FILL);
        (g.PolygonOffset)(1.0, 1.0);
        self.draw_lit(&mvp, &lit);
        // opaque and cut-out (alpha test) textures first; translucent ones last
        for (name, (tex, data)) in textured.iter() {
            let a = texstore::alpha(name);
            if a.0 != 2 {
                self.draw_lit_tex(&mvp, data, *tex, a, texstore::anim(name).map(|an| (an, ed.time)));
            }
        }
        self.draw_flat(&mvp, &f.edges, gl::LINES);
        // collision edges while editing it: about 3px dark with a 2px bright core. Thick
        // lines by drawing the same lines nudged a pixel at a time (core GL has no wide lines).
        if !f.col_dark.is_empty() {
            let nudge = |dx: f32, dy: f32| {
                let t = M4([1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 0.0, 0.0, 0.0, 1.0, 0.0, 2.0 * dx / w.max(1.0), 2.0 * dy / h.max(1.0), 0.0, 1.0]);
                t.mul(&mvp)
            };
            // the dark outline doesn't write depth, so the bright core lands on top of it
            (g.DepthMask)(0);
            for (dx, dy) in [(-1.5, 0.0), (1.5, 0.0), (0.0, -1.5), (0.0, 1.5), (-1.0, -1.0), (1.0, 1.0), (-1.0, 1.0), (1.0, -1.0)] {
                self.draw_flat(&nudge(dx, dy), &f.col_dark, gl::LINES);
            }
            (g.DepthMask)(1);
            for (dx, dy) in [(0.0, 0.0), (0.5, 0.0), (0.0, 0.5), (0.5, 0.5)] {
                self.draw_flat(&nudge(dx, dy), &f.col_bright, gl::LINES);
            }
        }
        // translucent collision hulls and textures: don't write depth so what's behind shows
        (g.DepthMask)(0);
        for (name, (tex, data)) in textured.iter() {
            let a = texstore::alpha(name);
            if a.0 == 2 {
                self.draw_lit_tex(&mvp, data, *tex, a, texstore::anim(name).map(|an| (an, ed.time)));
            }
        }
        self.draw_lit(&mvp, &glass);
        (g.DepthMask)(1);
        (g.Disable)(gl::POLYGON_OFFSET_FILL);
        // Selection / pending box outlines show through geometry, like Hammer.
        (g.Disable)(gl::DEPTH_TEST);
        self.draw_flat(&mvp, &f.overlay, gl::LINES);

        // Vertex tool: show vertices of selected brushes in the 3D view too
        if ed.tool == Tool::Vertex {
            let o = M4::ortho(0.0, w, h, 0.0, -1.0, 1.0);
            let mut q = Vec::new();
            let mut ol = Vec::new();
            for br in ed.doc.brushes.iter().filter(|b| is_sel(b.id) && b.entity.is_none()) {
                for &p in &br.verts {
                    if let Some(n) = mvp.project(p) {
                        if n.z.abs() > 1.0 {
                            continue;
                        }
                        let (sx, sy) = ((n.x * 0.5 + 0.5) * w, (0.5 - n.y * 0.5) * h);
                        let c = if ed.vertex_selected(br.id, p) { [1.0, 0.2, 0.2, 1.0] } else { [1.0, 1.0, 1.0, 1.0] };
                        let s = VERTEX_PX;
                        quad2(&mut q, sx - s, sy - s, sx + s, sy + s, c);
                        rect2(&mut ol, sx - s, sy - s, sx + s, sy + s, [0.0, 0.0, 0.0, 1.0]);
                    }
                }
            }
            self.draw_flat(&o, &q, gl::TRIANGLES);
            self.draw_flat(&o, &ol, gl::LINES);
        }

        if ed.captured {
            let o = M4::ortho(0.0, w, h, 0.0, -1.0, 1.0);
            let (cx, cy) = ((w * 0.5).floor() + 0.5, (h * 0.5).floor() + 0.5);
            let mut c = Vec::new();
            let col = [con[0], con[1], con[2], 0.9];
            line(&mut c, [cx - 9.0, cy, 0.0], [cx - 3.0, cy, 0.0], col);
            line(&mut c, [cx + 3.0, cy, 0.0], [cx + 9.0, cy, 0.0], col);
            line(&mut c, [cx, cy - 9.0, 0.0], [cx, cy - 3.0, 0.0], col);
            line(&mut c, [cx, cy + 3.0, 0.0], [cx, cy + 9.0, 0.0], col);
            self.draw_flat(&o, &c, gl::LINES);
        }
        self.finish();
    }
}

fn dark(c: [f32; 3]) -> [f32; 3] {
    [c[0] * 0.35, c[1] * 0.35, c[2] * 0.35]
}

/// Tint for a bisect piece: (color, filled). Split mode: the two sides get
/// different tints. Split & Delete: the doomed side is filled red and the kept
/// side is outlined in the contrast color so the cut reads clearly.
fn bisect_tint(delete_mode: bool, doomed_side: bool, contrast: [f32; 3]) -> Option<([f32; 3], bool)> {
    Some(if delete_mode {
        if doomed_side { ([1.0, 0.22, 0.22], true) } else { (contrast, false) }
    } else if doomed_side {
        ([1.0, 0.82, 0.25], true)
    } else {
        ([0.30, 0.80, 1.0], true)
    })
}

fn mix(a: [f32; 3], b: [f32; 3], t: f32) -> [f32; 3] {
    [a[0] + (b[0] - a[0]) * t, a[1] + (b[1] - a[1]) * t, a[2] + (b[2] - a[2]) * t]
}

fn circle2(buf: &mut Vec<f32>, ch: f32, cv: f32, r: f32, c: [f32; 4]) {
    const N: usize = 12;
    for k in 0..N {
        let a0 = k as f32 / N as f32 * std::f32::consts::TAU;
        let a1 = (k + 1) as f32 / N as f32 * std::f32::consts::TAU;
        line(buf, [ch + r * a0.cos(), cv + r * a0.sin(), 0.0], [ch + r * a1.cos(), cv + r * a1.sin(), 0.0], c);
    }
}

fn disc2(buf: &mut Vec<f32>, ch: f32, cv: f32, r: f32, c: [f32; 4]) {
    const N: usize = 12;
    for k in 0..N {
        let a0 = k as f32 / N as f32 * std::f32::consts::TAU;
        let a1 = (k + 1) as f32 / N as f32 * std::f32::consts::TAU;
        push_v(buf, [ch, cv, 0.0], c);
        push_v(buf, [ch + r * a0.cos(), cv + r * a0.sin(), 0.0], c);
        push_v(buf, [ch + r * a1.cos(), cv + r * a1.sin(), 0.0], c);
    }
}

unsafe fn make_vao(g: &Gl, sizes: &[i32]) -> (u32, u32) {
    let mut vao = 0;
    let mut vbo = 0;
    (g.GenVertexArrays)(1, &mut vao);
    (g.GenBuffers)(1, &mut vbo);
    (g.BindVertexArray)(vao);
    (g.BindBuffer)(gl::ARRAY_BUFFER, vbo);
    let stride: i32 = sizes.iter().sum::<i32>() * 4;
    let mut off = 0;
    for (i, &s) in sizes.iter().enumerate() {
        (g.EnableVertexAttribArray)(i as u32);
        (g.VertexAttribPointer)(i as u32, s, gl::FLOAT, gl::FALSE, stride, off as *const c_void);
        off += s as usize * 4;
    }
    (g.BindVertexArray)(0);
    (g.BindBuffer)(gl::ARRAY_BUFFER, 0);
    (vao, vbo)
}

unsafe fn shader(g: &Gl, kind: u32, src: &str) -> Result<u32, String> {
    let s = (g.CreateShader)(kind);
    let c = CString::new(src).unwrap();
    let p = c.as_ptr();
    (g.ShaderSource)(s, 1, &p, std::ptr::null());
    (g.CompileShader)(s);
    let mut ok = 0;
    (g.GetShaderiv)(s, gl::COMPILE_STATUS, &mut ok);
    if ok == 0 {
        let mut buf = vec![0u8; 4096];
        let mut n = 0;
        (g.GetShaderInfoLog)(s, buf.len() as i32, &mut n, buf.as_mut_ptr() as *mut c_char);
        return Err(format!("shader compile failed: {}", String::from_utf8_lossy(&buf[..n.max(0) as usize])));
    }
    Ok(s)
}

unsafe fn program(g: &Gl, vs: &str, fs: &str) -> Result<Prog, String> {
    let v = shader(g, gl::VERTEX_SHADER, vs)?;
    let f = shader(g, gl::FRAGMENT_SHADER, fs)?;
    let p = (g.CreateProgram)();
    (g.AttachShader)(p, v);
    (g.AttachShader)(p, f);
    (g.LinkProgram)(p);
    (g.DeleteShader)(v);
    (g.DeleteShader)(f);
    let mut ok = 0;
    (g.GetProgramiv)(p, gl::LINK_STATUS, &mut ok);
    if ok == 0 {
        let mut buf = vec![0u8; 4096];
        let mut n = 0;
        (g.GetProgramInfoLog)(p, buf.len() as i32, &mut n, buf.as_mut_ptr() as *mut c_char);
        return Err(format!("program link failed: {}", String::from_utf8_lossy(&buf[..n.max(0) as usize])));
    }
    let loc = |n: &str| {
        let c = CString::new(n).unwrap();
        (g.GetUniformLocation)(p, c.as_ptr())
    };
    Ok(Prog { id: p, mvp: loc("u_mvp"), stripes: loc("u_stripes"), use_tex: loc("u_use_tex"), tex: loc("u_tex"), alpha: loc("u_alpha"), nd: loc("u_nd"), nd_col: loc("u_nd_col"), nd_flow: loc("u_nd_flow"), anim: loc("u_anim"), pulse: loc("u_pulse") })
}
