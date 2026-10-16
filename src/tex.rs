// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Steventechno and the Bonk! contributors
//! Per-face texture mapping, Hammer / Valve 220 style.
//!
//! A face's texture coordinates, in texels, are
//!     u = p·U / scale_u + shift_u,   v = p·V / scale_v + shift_v
//! with U, V unit axes (usually lying in the face). Dividing by the image size
//! gives the 0..1 coordinates the renderer uses. Scale is world units per
//! texel (Hammer's default is 0.25).
//!
//! Many operations are easiest as an affine map in texture space applied to
//! the "rows" form (a_u = U/scale_u, b_u = shift_u): uv' = M·uv + t keeps
//! faces that line up still lining up, which is what "treat as one" needs.

use crate::math::*;

pub const DEFAULT_SCALE: f32 = 0.25;

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct TexMap {
    pub u: V3,
    pub v: V3,
    pub shift: [f32; 2],
    pub scale: [f32; 2],
    /// Rotation in degrees, as shown in the face controls (informational: the
    /// axes already include it)
    pub rot: f32,
}

/// Rotate `p` about the unit axis `k` by `deg` degrees (right-handed).
pub fn rotate_about(p: V3, k: V3, deg: f32) -> V3 {
    let (s, c) = deg.to_radians().sin_cos();
    p * c + k.cross(p) * s + k * (k.dot(p) * (1.0 - c))
}

/// The world-aligned base axes for a face normal: project along the
/// dominant axis, image "up" pointing up (+z) on walls.
pub fn base_axes(n: V3) -> (V3, V3) {
    let (ax, ay, az) = (n.x.abs(), n.y.abs(), n.z.abs());
    if az >= ax && az >= ay {
        (v3(1.0, 0.0, 0.0), v3(0.0, -1.0, 0.0))
    } else if ax >= ay {
        (v3(0.0, 1.0, 0.0), v3(0.0, 0.0, -1.0))
    } else {
        (v3(1.0, 0.0, 0.0), v3(0.0, 0.0, -1.0))
    }
}

impl TexMap {
    /// Hammer's default: world-aligned, scale 0.25, no shift.
    pub fn world(n: V3) -> TexMap {
        TexMap::world_scaled(n, [DEFAULT_SCALE, DEFAULT_SCALE])
    }

    pub fn world_scaled(n: V3, scale: [f32; 2]) -> TexMap {
        let (u, v) = base_axes(n);
        TexMap { u, v, shift: [0.0, 0.0], scale, rot: 0.0 }
    }

    /// Texel coordinates of a world point.
    pub fn uv(&self, p: V3) -> (f32, f32) {
        (p.dot(self.u) / nz(self.scale[0]) + self.shift[0], p.dot(self.v) / nz(self.scale[1]) + self.shift[1])
    }

    /// Usable on a face with normal `n`: the axes aren't (nearly) edge-on.
    pub fn valid_for(&self, n: V3) -> bool {
        let c = self.u.cross(self.v);
        c.len() > 1e-3 && (c.dot(n) / c.len()).abs() > 0.08 && self.scale[0].abs() > 1e-5 && self.scale[1].abs() > 1e-5
    }

    /// This mapping if it suits the face, else the world default keeping the scale.
    pub fn or_default_for(&self, n: V3) -> TexMap {
        if self.valid_for(n) {
            *self
        } else {
            TexMap::world_scaled(n, [self.scale[0].abs().max(1e-3), self.scale[1].abs().max(1e-3)])
        }
    }

    fn rows(&self) -> ([V3; 2], [f32; 2]) {
        ([self.u * (1.0 / nz(self.scale[0])), self.v * (1.0 / nz(self.scale[1]))], self.shift)
    }

    fn from_rows(a: [V3; 2], b: [f32; 2], rot: f32) -> TexMap {
        let (lu, lv) = (a[0].len().max(1e-9), a[1].len().max(1e-9));
        TexMap { u: a[0] * (1.0 / lu), v: a[1] * (1.0 / lv), shift: b, scale: [1.0 / lu, 1.0 / lv], rot }
    }

    /// Apply uv' = M·uv + t (texel space).
    pub fn affine(&self, m: [[f32; 2]; 2], t: [f32; 2], drot: f32) -> TexMap {
        let (a, b) = self.rows();
        let na = [a[0] * m[0][0] + a[1] * m[0][1], a[0] * m[1][0] + a[1] * m[1][1]];
        let nb = [m[0][0] * b[0] + m[0][1] * b[1] + t[0], m[1][0] * b[0] + m[1][1] * b[1] + t[1]];
        let mut out = TexMap::from_rows(na, nb, wrap_deg(self.rot + drot));
        // keep the sign convention of the scale fields when nothing flipped
        if self.scale[0] < 0.0 && m[0][1] == 0.0 && m[0][0] > 0.0 {
            out.scale[0] = -out.scale[0];
            out.u = -out.u;
        }
        if self.scale[1] < 0.0 && m[1][0] == 0.0 && m[1][1] > 0.0 {
            out.scale[1] = -out.scale[1];
            out.v = -out.v;
        }
        out
    }

    /// Scale the texture by (ku, kv) about the texel point `c`.
    pub fn scaled_about(&self, k: [f32; 2], c: [f32; 2]) -> TexMap {
        let (ku, kv) = (nz(k[0]), nz(k[1]));
        // uv' = (uv - c)/k + c
        self.affine([[1.0 / ku, 0.0], [0.0, 1.0 / kv]], [c[0] - c[0] / ku, c[1] - c[1] / kv], 0.0)
    }

    /// Rotate the texture by `deg` about the texel point `c`.
    pub fn rotated_about(&self, deg: f32, c: [f32; 2]) -> TexMap {
        // Rotating the image by +deg on the face = rotating texel space by -deg.
        let (s, co) = (-deg).to_radians().sin_cos();
        let m = [[co, -s], [s, co]];
        let t = [c[0] - (m[0][0] * c[0] + m[0][1] * c[1]), c[1] - (m[1][0] * c[0] + m[1][1] * c[1])];
        self.affine(m, t, deg)
    }

    /// The same mapping after the geometry moved by `d` (texture lock).
    pub fn translated(&self, d: V3) -> TexMap {
        TexMap {
            shift: [self.shift[0] - d.dot(self.u) / nz(self.scale[0]), self.shift[1] - d.dot(self.v) / nz(self.scale[1])],
            ..*self
        }
    }

    /// The same mapping after the geometry was rotated by `deg` about the
    /// unit axis `k` through `c` (texture lock).
    pub fn rotated_with(&self, k: V3, c: V3, deg: f32) -> TexMap {
        let (a, b) = self.rows();
        // uv'(p) = uv(R⁻¹(p - c) + c) = (R a)·(p - c) + a·c + b
        let na = [rotate_about(a[0], k, deg), rotate_about(a[1], k, deg)];
        let nb = [a[0].dot(c) + b[0] - na[0].dot(c), a[1].dot(c) + b[1] - na[1].dot(c)];
        TexMap::from_rows(na, nb, self.rot)
    }

    /// The same mapping after the geometry was scaled per axis by `k` about
    /// `anchor` (texture lock with "scale texture with brush").
    pub fn scaled_with(&self, k: V3, anchor: V3) -> TexMap {
        let (a, b) = self.rows();
        // uv'(p) = uv(anchor + (p - anchor)/k)
        let inv = |x: V3| v3(x.x / nz(k.x), x.y / nz(k.y), x.z / nz(k.z));
        let na = [inv(a[0]), inv(a[1])];
        let nb = [a[0].dot(anchor) + b[0] - na[0].dot(anchor), a[1].dot(anchor) + b[1] - na[1].dot(anchor)];
        let mut out = TexMap::from_rows(na, nb, self.rot);
        out.rot = self.rot;
        out
    }

    /// Continue this mapping (on a face with normal `ns`) onto a face with
    /// normal `nd`, as one sheet: folded around the shared edge `edge` when
    /// there is one, carried straight over for faces on the same plane, and
    /// projected straight through otherwise.
    pub fn wrapped(&self, ns: V3, nd: V3, edge: Option<(V3, V3)>) -> TexMap {
        if let Some((p0, p1)) = edge {
            let e = (p1 - p0).norm();
            let c = ns.cross(nd);
            let ang = c.dot(e).atan2(ns.dot(nd)).to_degrees();
            if ang.abs() > 1e-3 {
                return self.rotated_with(e, p0, ang).or_default_for(nd);
            }
            return *self;
        }
        if self.valid_for(nd) {
            *self
        } else {
            // edge-on: lay it down along the new face from the same world origin
            let mut t = TexMap::world_scaled(nd, [self.scale[0].abs(), self.scale[1].abs()]);
            t.shift = self.shift;
            t
        }
    }

    /// Texel bounds of a set of points: (min, max).
    pub fn bounds(&self, pts: &[V3]) -> ([f32; 2], [f32; 2]) {
        let mut lo = [f32::MAX; 2];
        let mut hi = [f32::MIN; 2];
        for &p in pts {
            let (u, v) = self.uv(p);
            lo = [lo[0].min(u), lo[1].min(v)];
            hi = [hi[0].max(u), hi[1].max(v)];
        }
        (lo, hi)
    }
}

fn nz(x: f32) -> f32 {
    if x.abs() < 1e-6 {
        1e-6_f32.copysign(x)
    } else {
        x
    }
}

pub fn wrap_deg(a: f32) -> f32 {
    let mut a = a % 360.0;
    if a > 180.0 {
        a -= 360.0;
    }
    if a <= -180.0 {
        a += 360.0;
    }
    if a.abs() < 1e-4 {
        0.0
    } else {
        a
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn close(a: (f32, f32), b: (f32, f32)) -> bool {
        (a.0 - b.0).abs() < 1e-3 && (a.1 - b.1).abs() < 1e-3
    }

    #[test]
    fn default_matches_hammer_world_alignment() {
        let t = TexMap::world(v3(0.0, 0.0, 1.0));
        assert!(close(t.uv(v3(128.0, 64.0, 0.0)), (512.0, -256.0)));
        let w = TexMap::world(v3(1.0, 0.0, 0.0));
        assert!(close(w.uv(v3(0.0, 32.0, 64.0)), (128.0, -256.0)));
    }

    #[test]
    fn texture_lock_translate_and_rotate_keep_texels_on_the_points() {
        let t = TexMap::world(v3(0.0, 0.0, 1.0));
        let p = v3(10.0, 20.0, 0.0);
        let d = v3(33.0, -7.0, 5.0);
        assert!(close(t.translated(d).uv(p + d), t.uv(p)));
        let k = v3(0.0, 0.0, 1.0);
        let c = v3(5.0, 5.0, 0.0);
        let r = t.rotated_with(k, c, 30.0);
        let p2 = rotate_about(p - c, k, 30.0) + c;
        assert!(close(r.uv(p2), t.uv(p)));
        let s = t.scaled_with(v3(2.0, 3.0, 1.0), c);
        let p3 = c + v3((p - c).x * 2.0, (p - c).y * 3.0, 0.0);
        assert!(close(s.uv(p3), t.uv(p)));
    }

    #[test]
    fn wrapping_around_a_corner_is_seamless_on_the_edge() {
        // floor-ish top face to a +x wall sharing the edge x=64, z=0
        let top = TexMap { shift: [7.0, 3.0], ..TexMap::world(v3(0.0, 0.0, 1.0)) };
        let ns = v3(0.0, 0.0, 1.0);
        let nd = v3(1.0, 0.0, 0.0);
        let e = (v3(64.0, 0.0, 0.0), v3(64.0, 64.0, 0.0));
        let w = top.wrapped(ns, nd, Some(e));
        for y in [0.0, 17.0, 64.0] {
            let p = v3(64.0, y, 0.0);
            assert!(close(w.uv(p), top.uv(p)));
        }
        // and continues the same size: 16 units down the wall = 64 texels away from the edge
        let a = w.uv(v3(64.0, 10.0, 0.0));
        let b = w.uv(v3(64.0, 10.0, -16.0));
        assert!(((a.0 - b.0).powi(2) + (a.1 - b.1).powi(2)).sqrt() - 64.0 < 1e-3);
        assert!(w.valid_for(nd));
    }

    #[test]
    fn affine_ops_scale_and_rotate_about_a_point() {
        let t = TexMap::world(v3(0.0, 0.0, 1.0));
        let p = v3(40.0, -24.0, 0.0);
        let c = [100.0, 50.0];
        let s = t.scaled_about([2.0, 2.0], c);
        assert!((s.scale[0] - 0.5).abs() < 1e-5);
        let (u0, v0) = t.uv(p);
        let (u1, v1) = s.uv(p);
        assert!(((u1 - c[0]) - (u0 - c[0]) / 2.0).abs() < 1e-3 && ((v1 - c[1]) - (v0 - c[1]) / 2.0).abs() < 1e-3);
        let r = t.rotated_about(90.0, c);
        assert!((r.rot - 90.0).abs() < 1e-4);
        assert!((r.scale[0] - 0.25).abs() < 1e-5 && (r.scale[1] - 0.25).abs() < 1e-5);
    }
}
