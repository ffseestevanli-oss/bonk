// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Steventechno and the Bonk! contributors
//! Convex geometry: 3D convex hulls for brushes, 2D hulls for picking.

use crate::math::*;
pub use crate::tex::TexMap;

/// A planar convex face. `idx` lists vertex indices counter-clockwise when
/// seen from outside; `normal` points outward; `dist` = normal · (any vertex).
#[derive(Clone, Debug, PartialEq)]
pub struct Face {
    pub idx: Vec<usize>,
    pub normal: V3,
    pub dist: f32,
    /// Material path, e.g. "walls/red_brick" (fresh faces are nodraw)
    pub material: String,
    /// How the material is laid on the face
    pub tex: TexMap,
}

/// Source's nodraw tool texture: faces with it are never rendered.
pub const NODRAW: &str = "tools/toolsnodraw";

pub struct Hull {
    pub verts: Vec<V3>,
    pub faces: Vec<Face>,
}

type D3 = [f64; 3];

fn d(p: V3) -> D3 {
    [p.x as f64, p.y as f64, p.z as f64]
}
fn sub(a: &D3, b: &D3) -> D3 {
    [a[0] - b[0], a[1] - b[1], a[2] - b[2]]
}
fn dot(a: &D3, b: &D3) -> f64 {
    a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
}
fn cross(a: &D3, b: &D3) -> D3 {
    [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
}
fn len(a: &D3) -> f64 {
    dot(a, a).sqrt()
}
fn scale(a: &D3, s: f64) -> D3 {
    [a[0] * s, a[1] * s, a[2] * s]
}

/// Tolerance used for "on the plane" tests, scaled to the size of the point set.
fn tolerance(pts: &[D3]) -> (f64, f64) {
    let mut lo = pts[0];
    let mut hi = pts[0];
    for p in pts {
        for a in 0..3 {
            lo[a] = lo[a].min(p[a]);
            hi[a] = hi[a].max(p[a]);
        }
    }
    let size = len(&sub(&hi, &lo)).max(1.0);
    (1e-3f64.max(size * 1e-6), size)
}

/// Convex hull of a point set. Returns None if the points don't span a volume.
/// Points that end up inside or on the boundary (not corners) are dropped.
pub fn hull(points: &[V3]) -> Option<Hull> {
    let mut pts: Vec<D3> = Vec::with_capacity(points.len());
    let mut src: Vec<V3> = Vec::with_capacity(points.len());
    for &p in points {
        let q = d(p);
        if !pts.iter().any(|r| len(&sub(r, &q)) < 1e-4) {
            pts.push(q);
            src.push(p);
        }
    }
    let n = pts.len();
    if n < 4 {
        return None;
    }
    let (eps, size) = tolerance(&pts);

    // Candidate support planes from every triple of points.
    let mut faces: Vec<(D3, f64, Vec<usize>)> = Vec::new();
    let mut seen: Vec<Vec<usize>> = Vec::new();
    for i in 0..n {
        for j in i + 1..n {
            for k in j + 1..n {
                let nrm = cross(&sub(&pts[j], &pts[i]), &sub(&pts[k], &pts[i]));
                let l = len(&nrm);
                if l < 1e-9 * size * size {
                    continue;
                }
                let nrm = scale(&nrm, 1.0 / l);
                let dd = dot(&nrm, &pts[i]);
                let (mut pos, mut neg) = (0, 0);
                for p in &pts {
                    let s = dot(&nrm, p) - dd;
                    if s > eps {
                        pos += 1;
                    } else if s < -eps {
                        neg += 1;
                    }
                    if pos > 0 && neg > 0 {
                        break;
                    }
                }
                let (nn, nd) = if pos == 0 && neg > 0 {
                    (nrm, dd)
                } else if neg == 0 && pos > 0 {
                    (scale(&nrm, -1.0), -dd)
                } else {
                    continue;
                };
                let on: Vec<usize> = (0..n).filter(|&m| (dot(&nn, &pts[m]) - nd).abs() <= eps).collect();
                let poly = order_polygon(&pts, &on, &nn, eps * size);
                if poly.len() < 3 {
                    continue;
                }
                let mut key = poly.clone();
                key.sort_unstable();
                if seen.contains(&key) {
                    continue;
                }
                seen.push(key);
                faces.push((nn, nd, poly));
            }
        }
    }
    if faces.len() < 4 {
        return None;
    }

    // Compact to the vertices actually used by faces.
    let mut remap = vec![usize::MAX; n];
    let mut verts = Vec::new();
    for (_, _, poly) in &faces {
        for &i in poly {
            if remap[i] == usize::MAX {
                remap[i] = verts.len();
                verts.push(src[i]);
            }
        }
    }
    let faces = faces
        .into_iter()
        .map(|(_, _, poly)| {
            let idx: Vec<usize> = poly.iter().map(|&i| remap[i]).collect();
            let (normal, dist) = newell(&verts, &idx);
            Face { idx, normal, dist, material: NODRAW.to_string(), tex: TexMap::world(normal) }
        })
        .collect();
    Some(Hull { verts, faces })
}

/// Order coplanar points counter-clockwise around `normal`, keeping only corners.
fn order_polygon(pts: &[D3], on: &[usize], normal: &D3, area_eps: f64) -> Vec<usize> {
    let a: D3 = if normal[0].abs() < 0.9 { [1.0, 0.0, 0.0] } else { [0.0, 1.0, 0.0] };
    let u = {
        let c = cross(&a, normal);
        scale(&c, 1.0 / len(&c))
    };
    let w = cross(normal, &u); // u × w = normal
    let mut p2: Vec<(f64, f64, usize)> = on.iter().map(|&i| (dot(&pts[i], &u), dot(&pts[i], &w), i)).collect();
    p2.sort_by(|a, b| a.0.partial_cmp(&b.0).unwrap().then(a.1.partial_cmp(&b.1).unwrap()));
    let cr = |o: &(f64, f64, usize), a: &(f64, f64, usize), b: &(f64, f64, usize)| (a.0 - o.0) * (b.1 - o.1) - (a.1 - o.1) * (b.0 - o.0);
    let mut lower: Vec<(f64, f64, usize)> = Vec::new();
    for p in &p2 {
        while lower.len() >= 2 && cr(&lower[lower.len() - 2], &lower[lower.len() - 1], p) <= area_eps {
            lower.pop();
        }
        lower.push(*p);
    }
    let mut upper: Vec<(f64, f64, usize)> = Vec::new();
    for p in p2.iter().rev() {
        while upper.len() >= 2 && cr(&upper[upper.len() - 2], &upper[upper.len() - 1], p) <= area_eps {
            upper.pop();
        }
        upper.push(*p);
    }
    lower.pop();
    upper.pop();
    lower.extend(upper);
    lower.into_iter().map(|p| p.2).collect()
}

/// Newell's method: robust polygon normal and plane distance.
pub fn newell(verts: &[V3], idx: &[usize]) -> (V3, f32) {
    let mut n = [0.0f64; 3];
    let mut c = [0.0f64; 3];
    for i in 0..idx.len() {
        let a = d(verts[idx[i]]);
        let b = d(verts[idx[(i + 1) % idx.len()]]);
        n[0] += (a[1] - b[1]) * (a[2] + b[2]);
        n[1] += (a[2] - b[2]) * (a[0] + b[0]);
        n[2] += (a[0] - b[0]) * (a[1] + b[1]);
        for k in 0..3 {
            c[k] += a[k];
        }
    }
    let l = len(&n).max(1e-12);
    let n = scale(&n, 1.0 / l);
    let c = scale(&c, 1.0 / idx.len() as f64);
    (v3(n[0] as f32, n[1] as f32, n[2] as f32), dot(&n, &c) as f32)
}

/// True if every point is a corner of the hull (or sits exactly on another
/// point, i.e. was merged). Points that would end up inside the solid or
/// flattened onto a face/edge fail this test: moving a vertex there would
/// silently delete it or make the brush concave.
pub fn all_points_are_corners(points: &[V3], h: &Hull) -> bool {
    points.iter().all(|&p| h.verts.iter().any(|&q| (q - p).len() < 1e-3))
}

/// Volume of a convex solid (faces CCW from outside).
pub fn volume(verts: &[V3], faces: &[Face]) -> f32 {
    if verts.is_empty() {
        return 0.0;
    }
    let c = verts.iter().fold(V3::default(), |a, &p| a + p) * (1.0 / verts.len() as f32);
    let mut v = 0.0f64;
    for f in faces {
        let p0 = verts[f.idx[0]] - c;
        for k in 1..f.idx.len().saturating_sub(1) {
            let p1 = verts[f.idx[k]] - c;
            let p2 = verts[f.idx[k + 1]] - c;
            v += p0.dot(p1.cross(p2)) as f64 / 6.0;
        }
    }
    v.abs() as f32
}

/// Cut a convex solid by the plane n·p = d. Returns the (negative side,
/// positive side) point sets, or None if the plane doesn't pass through it.
pub fn split_points(verts: &[V3], edges: &[(usize, usize)], n: V3, d: f32) -> Option<(Vec<V3>, Vec<V3>)> {
    let eps = 1e-3;
    let s: Vec<f32> = verts.iter().map(|&p| n.dot(p) - d).collect();
    let (has_pos, has_neg) = (s.iter().any(|&v| v > eps), s.iter().any(|&v| v < -eps));
    if !(has_pos && has_neg) {
        return None;
    }
    let mut neg = Vec::new();
    let mut pos = Vec::new();
    for (i, &p) in verts.iter().enumerate() {
        if s[i] <= eps {
            neg.push(p);
        }
        if s[i] >= -eps {
            pos.push(p);
        }
    }
    for &(i, j) in edges {
        if (s[i] < -eps && s[j] > eps) || (s[i] > eps && s[j] < -eps) {
            let t = s[i] / (s[i] - s[j]);
            let q = verts[i] + (verts[j] - verts[i]) * t;
            neg.push(q);
            pos.push(q);
        }
    }
    Some((neg, pos))
}

/// Ray / convex solid intersection: entry distance and the face it enters
/// through (None for the face when the ray starts inside).
pub fn ray_convex_face(o: V3, dir: V3, faces: &[Face]) -> Option<(f32, Option<usize>)> {
    let mut t0 = 0.0f32;
    let mut t1 = f32::INFINITY;
    let mut entry = None;
    for (i, f) in faces.iter().enumerate() {
        let denom = f.normal.dot(dir);
        let dist = f.normal.dot(o) - f.dist;
        if denom.abs() < 1e-9 {
            if dist > 0.0 {
                return None;
            }
        } else {
            let t = -dist / denom;
            if denom < 0.0 {
                if t > t0 {
                    t0 = t;
                    entry = Some(i);
                }
            } else {
                t1 = t1.min(t);
            }
            if t0 > t1 {
                return None;
            }
        }
    }
    Some((t0, entry))
}

/// Ray / convex solid intersection (entry distance).
#[allow(dead_code)]
pub fn ray_convex(o: V3, dir: V3, faces: &[Face]) -> Option<f32> {
    let mut t0 = 0.0f32;
    let mut t1 = f32::INFINITY;
    for f in faces {
        let denom = f.normal.dot(dir);
        let dist = f.normal.dot(o) - f.dist;
        if denom.abs() < 1e-9 {
            if dist > 0.0 {
                return None;
            }
        } else {
            let t = -dist / denom;
            if denom < 0.0 {
                t0 = t0.max(t);
            } else {
                t1 = t1.min(t);
            }
            if t0 > t1 {
                return None;
            }
        }
    }
    Some(t0)
}

// ------------------------------------------------------------------ 2D

/// Convex hull of 2D points, counter-clockwise.
pub fn hull2d(points: &[(f32, f32)]) -> Vec<(f32, f32)> {
    let mut p = points.to_vec();
    p.sort_by(|a, b| a.0.partial_cmp(&b.0).unwrap().then(a.1.partial_cmp(&b.1).unwrap()));
    p.dedup();
    if p.len() < 3 {
        return p;
    }
    let cr = |o: (f32, f32), a: (f32, f32), b: (f32, f32)| (a.0 - o.0) * (b.1 - o.1) - (a.1 - o.1) * (b.0 - o.0);
    let mut lower: Vec<(f32, f32)> = Vec::new();
    for &q in &p {
        while lower.len() >= 2 && cr(lower[lower.len() - 2], lower[lower.len() - 1], q) <= 0.0 {
            lower.pop();
        }
        lower.push(q);
    }
    let mut upper: Vec<(f32, f32)> = Vec::new();
    for &q in p.iter().rev() {
        while upper.len() >= 2 && cr(upper[upper.len() - 2], upper[upper.len() - 1], q) <= 0.0 {
            upper.pop();
        }
        upper.push(q);
    }
    lower.pop();
    upper.pop();
    lower.extend(upper);
    lower
}

/// Point inside a CCW convex polygon, with an outward tolerance.
pub fn in_convex2d(poly: &[(f32, f32)], p: (f32, f32), tol: f32) -> bool {
    if poly.len() < 3 {
        return false;
    }
    for i in 0..poly.len() {
        let a = poly[i];
        let b = poly[(i + 1) % poly.len()];
        let (ex, ey) = (b.0 - a.0, b.1 - a.1);
        let l = (ex * ex + ey * ey).sqrt().max(1e-9);
        // signed distance to the left of edge a->b (inside is positive)
        let s = (ex * (p.1 - a.1) - ey * (p.0 - a.0)) / l;
        if s < -tol {
            return false;
        }
    }
    true
}

pub fn area2d(poly: &[(f32, f32)]) -> f32 {
    let mut a = 0.0;
    for i in 0..poly.len() {
        let p = poly[i];
        let q = poly[(i + 1) % poly.len()];
        a += p.0 * q.1 - q.0 * p.1;
    }
    a.abs() * 0.5
}

#[cfg(test)]
mod tests {
    use super::*;

    fn cube(s: f32) -> Vec<V3> {
        let mut v = Vec::new();
        for i in 0..8 {
            v.push(v3(if i & 1 != 0 { s } else { 0.0 }, if i & 2 != 0 { s } else { 0.0 }, if i & 4 != 0 { s } else { 0.0 }));
        }
        v
    }

    #[test]
    fn cube_hull() {
        let h = hull(&cube(64.0)).unwrap();
        assert_eq!(h.verts.len(), 8);
        assert_eq!(h.faces.len(), 6);
        for f in &h.faces {
            assert_eq!(f.idx.len(), 4);
            // every vertex is on or behind every face
            for p in &h.verts {
                assert!(f.normal.dot(*p) - f.dist < 1e-3);
            }
        }
    }

    #[test]
    fn faces_wind_ccw_from_outside() {
        let h = hull(&cube(10.0)).unwrap();
        for f in &h.faces {
            let a = h.verts[f.idx[0]];
            let b = h.verts[f.idx[1]];
            let c = h.verts[f.idx[2]];
            assert!((b - a).cross(c - a).dot(f.normal) > 0.0);
        }
    }

    #[test]
    fn wedge_by_merging_vertices() {
        // drop the two top vertices at y=64 down onto the bottom ones
        let pts: Vec<V3> = cube(64.0).into_iter().map(|p| if p.z > 0.0 && p.y > 0.0 { v3(p.x, p.y, 0.0) } else { p }).collect();
        let h = hull(&pts).unwrap();
        assert_eq!(h.verts.len(), 6);
        assert_eq!(h.faces.len(), 5);
    }

    #[test]
    fn interior_point_detected_and_flat_rejected() {
        let mut pts = cube(64.0);
        pts[7] = v3(32.0, 32.0, 32.0);
        let h = hull(&pts).unwrap();
        assert!(!all_points_are_corners(&pts, &h));
        assert!(all_points_are_corners(&cube(64.0), &hull(&cube(64.0)).unwrap()));
        let flat: Vec<V3> = cube(64.0).into_iter().map(|p| v3(p.x, p.y, 0.0)).collect();
        assert!(hull(&flat).is_none());
    }

    #[test]
    fn volume_and_split() {
        let h = hull(&cube(64.0)).unwrap();
        assert!((volume(&h.verts, &h.faces) - 64.0f32.powi(3)).abs() < 1.0);
        // edges of the cube hull
        let mut edges = Vec::new();
        for f in &h.faces {
            for i in 0..f.idx.len() {
                let (a, b) = (f.idx[i], f.idx[(i + 1) % f.idx.len()]);
                if !edges.contains(&(a.min(b), a.max(b))) {
                    edges.push((a.min(b), a.max(b)));
                }
            }
        }
        // cut at x = 16
        let (neg, pos) = split_points(&h.verts, &edges, v3(1.0, 0.0, 0.0), 16.0).unwrap();
        let (hn, hp) = (hull(&neg).unwrap(), hull(&pos).unwrap());
        assert!((volume(&hn.verts, &hn.faces) - 16.0 * 64.0 * 64.0).abs() < 1.0);
        assert!((volume(&hp.verts, &hp.faces) - 48.0 * 64.0 * 64.0).abs() < 1.0);
        // a plane that misses the solid gives nothing
        assert!(split_points(&h.verts, &edges, v3(1.0, 0.0, 0.0), 100.0).is_none());
        // diagonal cut -> two wedges with 6 vertices each
        let n = v3(1.0, 1.0, 0.0).norm();
        let (a, b) = split_points(&h.verts, &edges, n, n.dot(v3(32.0, 32.0, 0.0))).unwrap();
        assert_eq!(hull(&a).unwrap().verts.len(), 6);
        assert_eq!(hull(&b).unwrap().verts.len(), 6);
    }

    #[test]
    fn ray_hits_convex() {
        let h = hull(&cube(64.0)).unwrap();
        let t = ray_convex(v3(-100.0, 32.0, 32.0), v3(1.0, 0.0, 0.0), &h.faces).unwrap();
        assert!((t - 100.0).abs() < 1e-3);
        assert!(ray_convex(v3(-100.0, 100.0, 32.0), v3(1.0, 0.0, 0.0), &h.faces).is_none());
    }

    #[test]
    fn point_in_2d_hull() {
        let poly = hull2d(&[(0.0, 0.0), (10.0, 0.0), (10.0, 10.0), (0.0, 10.0), (5.0, 5.0)]);
        assert_eq!(poly.len(), 4);
        assert!(in_convex2d(&poly, (5.0, 5.0), 0.0));
        assert!(!in_convex2d(&poly, (11.0, 5.0), 0.5));
        assert!((area2d(&poly) - 100.0).abs() < 1e-3);
    }
}
