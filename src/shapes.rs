// SPDX-License-Identifier: GPL-3.0-or-later
//! Primitive shapes for the Block tool: each one is a single convex brush
//! that fills the drawn box.

use crate::math::{v3, V3};
use std::f32::consts::{PI, TAU};

#[derive(Clone, Copy, Debug, PartialEq, Eq, Default)]
pub enum Shape {
    #[default]
    Block,
    /// An n-sided prism standing along the view's depth axis
    Cylinder,
    /// A ramp: full footprint at the bottom, rising toward the view's +horizontal
    Wedge,
    /// An n-sided cone, point at the top of the depth axis
    Spike,
    /// An ellipsoid, poles on the depth axis
    Orb,
    /// A half-ring of brushes in the view's plane (an arch), standing on the box's bottom
    Arch,
}

impl Shape {
    pub fn from_index(i: i32) -> Shape {
        match i {
            1 => Shape::Cylinder,
            2 => Shape::Wedge,
            3 => Shape::Spike,
            4 => Shape::Orb,
            5 => Shape::Arch,
            _ => Shape::Block,
        }
    }

    pub fn index(self) -> i32 {
        match self {
            Shape::Block => 0,
            Shape::Cylinder => 1,
            Shape::Wedge => 2,
            Shape::Spike => 3,
            Shape::Orb => 4,
            Shape::Arch => 5,
        }
    }

    /// The faces count this shape actually uses (orbs are kept modest: every
    /// ring multiplies the triangle count).
    pub fn clamp_faces(self, n: u32) -> u32 {
        match self {
            Shape::Orb => n.clamp(4, 16),
            Shape::Arch => n.clamp(2, 32),
            _ => n.clamp(3, 32),
        }
    }
}

/// The brushes `shape` makes filling (lo, hi): one for every shape but the
/// arch, which is `faces` segments around a half ring `thickness` thick, in
/// the plane of the view's horizontal (`across`) and the remaining axis,
/// running through the depth (`axis`).
pub fn pieces(shape: Shape, lo: V3, hi: V3, axis: usize, across: usize, faces: u32, thickness: f32) -> Vec<Vec<V3>> {
    if shape != Shape::Arch {
        return vec![points(shape, lo, hi, axis, across, faces)];
    }
    let (lo, hi) = (lo.min(hi), lo.max(hi));
    let n = shape.clamp_faces(faces) as usize;
    let h = if across == axis { (axis + 1) % 3 } else { across };
    let up = 3 - axis - h;
    let (ch, rx) = ((lo.get(h) + hi.get(h)) * 0.5, (hi.get(h) - lo.get(h)) * 0.5);
    let (base, ry) = (lo.get(up), hi.get(up) - lo.get(up));
    let t = thickness.max(1.0).min(rx * 0.95).min(ry * 0.95);
    let at = |a: f32, r: (f32, f32)| (ch + r.0 * a.cos(), base + r.1 * a.sin());
    let mut out = Vec::with_capacity(n);
    for i in 0..n {
        let (a0, a1) = (PI * (1.0 - i as f32 / n as f32), PI * (1.0 - (i + 1) as f32 / n as f32));
        let mut pts = Vec::with_capacity(8);
        for (ph, pv) in [at(a0, (rx, ry)), at(a1, (rx, ry)), at(a1, (rx - t, ry - t)), at(a0, (rx - t, ry - t))] {
            for d in [lo.get(axis), hi.get(axis)] {
                let mut p = V3::default();
                p.set(h, ph);
                p.set(up, pv);
                p.set(axis, d);
                pts.push(p);
            }
        }
        out.push(pts);
    }
    out
}

/// The corner points of `shape` filling (lo, hi). `axis` is the axis the
/// round shapes stand along; `across` is the view's horizontal axis (the way a
/// wedge rises). Wedges always sit on their bottom (Z) face.
pub fn points(shape: Shape, lo: V3, hi: V3, axis: usize, across: usize, faces: u32) -> Vec<V3> {
    let (lo, hi) = (lo.min(hi), lo.max(hi));
    let n = shape.clamp_faces(faces) as usize;
    // the two axes around `axis`, in a fixed order
    let (u, w) = match axis {
        0 => (1, 2),
        1 => (0, 2),
        _ => (0, 1),
    };
    let cu = (lo.get(u) + hi.get(u)) * 0.5;
    let cw = (lo.get(w) + hi.get(w)) * 0.5;
    let ru = (hi.get(u) - lo.get(u)) * 0.5;
    let rw = (hi.get(w) - lo.get(w)) * 0.5;
    let at = |a: f32, pu: f32, pw: f32| {
        let mut p = V3::default();
        p.set(axis, a);
        p.set(u, pu);
        p.set(w, pw);
        p
    };
    // ring points: a flat side faces each way along u for even counts
    let ring = |k: usize| {
        let t = TAU * k as f32 / n as f32 + if n % 2 == 0 { PI / n as f32 } else { -PI / 2.0 };
        (cu + ru * t.cos(), cw + rw * t.sin())
    };
    match shape {
        Shape::Block | Shape::Arch => (0..8)
            .map(|i| v3(if i & 1 != 0 { hi.x } else { lo.x }, if i & 2 != 0 { hi.y } else { lo.y }, if i & 4 != 0 { hi.z } else { lo.z }))
            .collect(),
        Shape::Cylinder => {
            let mut pts = Vec::with_capacity(2 * n);
            for k in 0..n {
                let (pu, pw) = ring(k);
                pts.push(at(lo.get(axis), pu, pw));
                pts.push(at(hi.get(axis), pu, pw));
            }
            pts
        }
        Shape::Spike => {
            let mut pts: Vec<V3> = (0..n).map(|k| {
                let (pu, pw) = ring(k);
                at(lo.get(axis), pu, pw)
            }).collect();
            pts.push(at(hi.get(axis), cu, cw));
            pts
        }
        Shape::Wedge => {
            let rise = if across == 2 { 0 } else { across };
            let other = 3 - rise - 2;
            let mut pts = Vec::with_capacity(6);
            for &r in &[lo.get(rise), hi.get(rise)] {
                for &o in &[lo.get(other), hi.get(other)] {
                    let mut p = V3::default();
                    p.set(rise, r);
                    p.set(other, o);
                    p.z = lo.z;
                    pts.push(p);
                }
            }
            for &o in &[lo.get(other), hi.get(other)] {
                let mut p = V3::default();
                p.set(rise, hi.get(rise));
                p.set(other, o);
                p.z = hi.z;
                pts.push(p);
            }
            pts
        }
        Shape::Orb => {
            let rings = (n / 2).max(2);
            let ca = (lo.get(axis) + hi.get(axis)) * 0.5;
            let ra = (hi.get(axis) - lo.get(axis)) * 0.5;
            let mut pts = vec![at(lo.get(axis), cu, cw), at(hi.get(axis), cu, cw)];
            for r in 1..rings {
                let phi = -PI / 2.0 + PI * r as f32 / rings as f32;
                let (s, c) = phi.sin_cos();
                for k in 0..n {
                    let (pu, pw) = ring(k);
                    pts.push(at(ca + ra * s, cu + (pu - cu) * c, cw + (pw - cw) * c));
                }
            }
            pts
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::geom;

    #[test]
    fn every_shape_is_a_valid_brush_filling_its_box() {
        let (lo, hi) = (v3(0.0, 0.0, 0.0), v3(64.0, 32.0, 48.0));
        for s in 0..5 {
            let shape = Shape::from_index(s);
            for faces in [3u32, 4, 8, 12, 32] {
                let pts = points(shape, lo, hi, 2, 0, faces);
                let h = geom::hull(&pts).unwrap_or_else(|| panic!("{shape:?} {faces}"));
                let mut b0 = h.verts[0];
                let mut b1 = h.verts[0];
                for v in &h.verts {
                    b0 = b0.min(*v);
                    b1 = b1.max(*v);
                }
                assert!(b0.x >= -0.01 && b0.y >= -0.01 && b0.z >= -0.01 && b1.x <= 64.01 && b1.y <= 32.01 && b1.z <= 48.01, "{shape:?} {faces}: {b0:?} {b1:?}");
                assert!((b0.z - 0.0).abs() < 0.01 && (b1.z - 48.0).abs() < 0.01, "{shape:?} spans the depth");
            }
        }
        // a cylinder with 8 faces: 8 sides + 2 caps
        let h = geom::hull(&points(Shape::Cylinder, lo, hi, 2, 0, 8)).unwrap();
        assert_eq!(h.faces.len(), 10);
        // a spike: 6 sides + base
        let h = geom::hull(&points(Shape::Spike, lo, hi, 2, 0, 6)).unwrap();
        assert_eq!(h.faces.len(), 7);
        // a wedge: 5 faces
        let h = geom::hull(&points(Shape::Wedge, lo, hi, 2, 0, 0)).unwrap();
        assert_eq!(h.faces.len(), 5);
        // a cylinder drawn in the front view (depth = X) lies along X
        let h = geom::hull(&points(Shape::Cylinder, lo, hi, 0, 1, 8)).unwrap();
        assert_eq!(h.faces.iter().filter(|f| f.normal.x.abs() > 0.99).count(), 2);
    }

    #[test]
    fn an_arch_is_a_half_ring_of_convex_brushes() {
        let (lo, hi) = (v3(0.0, 0.0, 0.0), v3(128.0, 16.0, 96.0));
        // drawn in the front view: depth = Y, across = X, so the arch rises in Z
        let ps = pieces(Shape::Arch, lo, hi, 1, 0, 6, 16.0);
        assert_eq!(ps.len(), 6);
        for p in &ps {
            let h = geom::hull(p).expect("each segment is a solid");
            assert_eq!(h.faces.len(), 6);
            for v in &h.verts {
                assert!(v.x >= -0.01 && v.x <= 128.01 && v.z >= -0.01 && v.z <= 96.01 && (v.y == 0.0 || v.y == 16.0));
            }
        }
        // its feet stand on the bottom, its crown touches the top
        let all: Vec<V3> = ps.iter().flatten().copied().collect();
        assert!(all.iter().any(|v| v.z.abs() < 0.01 && v.x.abs() < 0.01));
        assert!(all.iter().any(|v| (v.z - 96.0).abs() < 0.01));
        // the other shapes are one brush
        assert_eq!(pieces(Shape::Cylinder, lo, hi, 2, 0, 8, 16.0).len(), 1);
    }
}
