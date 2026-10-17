// SPDX-License-Identifier: GPL-3.0-or-later
//! Collision helpers: a simplified physics hull built from the visual
//! brushes, joining brushes, and convex volumes.
//!
//! Simplified collision, by size:
//! - big brushes (walls, floors, roofs, steps) are kept as they are;
//! - details (thin panes, or brushes whose second-largest side is under the
//!   detail size) sitting inside a wall's thickness fill their opening: a
//!   window with a pane or a frame becomes solid wall; an empty hole stays open;
//! - other details that touch form clusters; a cluster that is mostly solid and
//!   big enough becomes one convex piece (stairs → a ramp), the rest are dropped;
//! - finally neighbors whose union is exactly convex are merged into one piece.

use crate::doc::Brush;
use crate::geom;
use crate::math::{v3, V3};
use crate::overlap::shared_volume;

/// Below this thickness a brush is a pane (glass, a decal sheet)
const PANE: f32 = 2.0;
/// Walls are at most this thick
const WALL_MAX: f32 = 64.0;
const TOL: f32 = 0.5;

pub fn volume(b: &Brush) -> f32 {
    let mut v = 0.0;
    for f in &b.faces {
        let p0 = b.verts[f.idx[0]];
        for k in 1..f.idx.len().saturating_sub(1) {
            v += p0.dot(b.verts[f.idx[k]].cross(b.verts[f.idx[k + 1]]));
        }
    }
    (v / 6.0).abs()
}

/// A brush around these points (id 0; nodraw faces), if they have volume.
pub fn hull_brush(points: &[V3]) -> Option<Brush> {
    let h = geom::hull(points)?;
    let mut b = Brush::new_box(0, V3::default(), v3(1.0, 1.0, 1.0), [1.0, 1.0, 1.0]);
    b.verts = h.verts;
    b.faces = h.faces;
    Some(b)
}

fn dims(lo: V3, hi: V3) -> [(f32, usize); 3] {
    let mut d = [(hi.x - lo.x, 0), (hi.y - lo.y, 1), (hi.z - lo.z, 2)];
    d.sort_by(|a, b| a.0.total_cmp(&b.0));
    d
}

fn touch(a: (V3, V3), b: (V3, V3), tol: f32) -> bool {
    (0..3).all(|i| a.0.get(i) <= b.1.get(i) + tol && b.0.get(i) <= a.1.get(i) + tol)
}

/// Exact join: the hull of `a` and `b`, if it adds no volume to their union.
pub fn exact_join(a: &Brush, b: &Brush) -> Option<Brush> {
    let pts: Vec<V3> = a.verts.iter().chain(b.verts.iter()).copied().collect();
    let h = hull_brush(&pts)?;
    let union = volume(a) + volume(b) - shared_volume(a, b);
    (volume(&h) <= union * 1.002 + 1e-3).then_some(h)
}

/// Whether the union of all these brushes is one convex brush (within rounding).
pub fn union_is_convex(brushes: &[&Brush]) -> Option<Brush> {
    let pts: Vec<V3> = brushes.iter().flat_map(|b| b.verts.iter().copied()).collect();
    let h = hull_brush(&pts)?;
    // union volume: sum minus pairwise overlaps (good enough for touching / lightly overlapping parts)
    let mut union: f32 = brushes.iter().map(|b| volume(b)).sum();
    for i in 0..brushes.len() {
        for j in i + 1..brushes.len() {
            union -= shared_volume(brushes[i], brushes[j]);
        }
    }
    (volume(&h) <= union * 1.002 + 1e-3).then_some(h)
}

/// Merge neighbors whose union is exactly convex, until nothing more merges.
pub fn merge_exact(mut pieces: Vec<Brush>) -> Vec<Brush> {
    // every piece gets a stamp; a pair of stamps that didn't join is never tried again
    let mut next_stamp: u64 = 0;
    let mut stamps: Vec<u64> = pieces.iter().map(|_| {
        next_stamp += 1;
        next_stamp
    }).collect();
    let mut failed: std::collections::HashSet<(u64, u64)> = Default::default();
    for _pass in 0..24 {
        let mut merged_any = false;
        let mut bounds: Vec<(V3, V3)> = pieces.iter().map(|p| p.bounds()).collect();
        let mut order: Vec<usize> = (0..pieces.len()).collect();
        order.sort_by(|&a, &b| bounds[a].0.x.total_cmp(&bounds[b].0.x));
        let mut dead = vec![false; pieces.len()];
        for oi in 0..order.len() {
            let i = order[oi];
            if dead[i] {
                continue;
            }
            for &j in &order[oi + 1..] {
                if bounds[j].0.x > bounds[i].1.x + TOL {
                    break;
                }
                if dead[j] || !touch(bounds[i], bounds[j], 0.01) {
                    continue;
                }
                let key = (stamps[i].min(stamps[j]), stamps[i].max(stamps[j]));
                if failed.contains(&key) {
                    continue;
                }
                if let Some(h) = exact_join(&pieces[i], &pieces[j]) {
                    bounds[i] = h.bounds();
                    pieces[i] = h;
                    next_stamp += 1;
                    stamps[i] = next_stamp;
                    dead[j] = true;
                    merged_any = true;
                } else {
                    failed.insert(key);
                }
            }
        }
        let mut k = 0;
        pieces.retain(|_| {
            k += 1;
            !dead[k - 1]
        });
        let mut k = 0;
        stamps.retain(|_| {
            k += 1;
            !dead[k - 1]
        });
        if !merged_any {
            break;
        }
    }
    pieces
}

struct Uf(Vec<usize>);
impl Uf {
    fn find(&mut self, i: usize) -> usize {
        let mut r = i;
        while self.0[r] != r {
            r = self.0[r];
        }
        let mut i = i;
        while self.0[i] != r {
            let n = self.0[i];
            self.0[i] = r;
            i = n;
        }
        r
    }
    fn join(&mut self, a: usize, b: usize) {
        let (a, b) = (self.find(a), self.find(b));
        if a != b {
            self.0[a] = b;
        }
    }
}

/// The simplified collision pieces for these visual brushes.
pub fn simplify(brushes: &[&Brush], detail: f32) -> Vec<Brush> {
    let t0 = std::time::Instant::now();
    let dbg = std::env::var_os("BONK_DEBUG_SIMPLIFY").is_some();
    let detail = detail.max(1.0);
    let info: Vec<((V3, V3), [(f32, usize); 3])> = brushes.iter().map(|b| {
        let bb = b.bounds();
        (bb, dims(bb.0, bb.1))
    }).collect();
    let is_detail = |i: usize| info[i].1[0].0 < PANE || info[i].1[1].0 < detail;
    let mut pieces: Vec<Brush> = Vec::new();
    let big: Vec<usize> = (0..brushes.len()).filter(|&i| !is_detail(i)).collect();
    let details: Vec<usize> = (0..brushes.len()).filter(|&i| is_detail(i)).collect();
    for &i in &big {
        let mut b = brushes[i].clone();
        b.group = None;
        pieces.push(b);
    }
    // details inside a wall's thickness: which wall (and its thin axis) hosts each one
    let host: Vec<Option<(usize, usize)>> = details
        .iter()
        .map(|&d| {
            let (dl, dh) = info[d].0;
            big.iter().find_map(|&w| {
                let ((wl, wh), wd) = info[w];
                let (thick, axis) = wd[0];
                if thick > WALL_MAX || axis == 2 {
                    return None; // walls stand up: floors and roofs don't host windows
                }
                let inside = dl.get(axis) >= wl.get(axis) - TOL && dh.get(axis) <= wh.get(axis) + TOL;
                (inside && touch((dl, dh), (wl, wh), TOL)).then_some((w, axis))
            })
        })
        .collect();
    // a big thin sheet that isn't in a wall (paving, a floor plate) stays as it is
    let sheet = |k: usize| host[k].is_none() && info[details[k]].1[1].0 >= detail;
    for k in 0..details.len() {
        if sheet(k) {
            let mut b = brushes[details[k]].clone();
            b.group = None;
            pieces.push(b);
        }
    }
    // cluster details that touch (hosted ones only with others in the same wall slab)
    let mut uf = Uf((0..details.len()).collect());
    for a in 0..details.len() {
        if sheet(a) {
            continue;
        }
        for b in a + 1..details.len() {
            if sheet(b) {
                continue;
            }
            let same_kind = match (host[a], host[b]) {
                (Some((wa, xa)), Some((wb, xb))) => xa == xb && {
                    let (la, ha) = info[wa].0;
                    let (lb, hb) = info[wb].0;
                    (la.get(xa) - lb.get(xb)).abs() < TOL && (ha.get(xa) - hb.get(xb)).abs() < TOL
                },
                (None, None) => true,
                _ => false,
            };
            if same_kind && touch(info[details[a]].0, info[details[b]].0, TOL) {
                uf.join(a, b);
            }
        }
    }
    let mut groups: std::collections::BTreeMap<usize, Vec<usize>> = Default::default();
    for k in (0..details.len()).filter(|&k| !sheet(k)) {
        let r = uf.find(k);
        groups.entry(r).or_default().push(k);
    }
    for (_, members) in groups {
        let (mut lo, mut hi) = info[details[members[0]]].0;
        for &m in &members {
            let (l, h) = info[details[m]].0;
            lo = lo.min(l);
            hi = hi.max(h);
        }
        if let Some((w, axis)) = members.iter().find_map(|&m| host[m]) {
            // a window (pane, frame…) in a wall: fill the opening, wall-thick
            let (wl, wh) = info[w].0;
            lo.set(axis, wl.get(axis));
            hi.set(axis, wh.get(axis));
            let corners: Vec<V3> = (0..8)
                .map(|i| v3(if i & 1 != 0 { hi.x } else { lo.x }, if i & 2 != 0 { hi.y } else { lo.y }, if i & 4 != 0 { hi.z } else { lo.z }))
                .collect();
            if let Some(b) = hull_brush(&corners) {
                pieces.push(b);
            }
            continue;
        }
        // a flat cluster (paving, a floor sheet): its parts stay as they are (a hull
        // would fill the corner of an L); the merge pass joins what lines up
        if dims(lo, hi)[0].0 < detail {
            for &m in &members {
                let b = brushes[details[m]];
                let (l, h) = info[details[m]].0;
                if dims(l, h)[2].0 >= detail {
                    let mut b = b.clone();
                    b.group = None;
                    pieces.push(b);
                }
            }
            continue;
        }
        // loose details: one piece if they're mostly solid and big enough
        let filled: f32 = members.iter().map(|&m| volume(brushes[details[m]])).sum();
        if members.len() > 48 {
            // a big cluster: judge it by its box (hulls of thousands of points are slow,
            // and a cluster that big is a block of furniture or trim anyway)
            let bv = (hi.x - lo.x) * (hi.y - lo.y) * (hi.z - lo.z);
            if bv > 0.0 && filled / bv >= 0.5 && dims(lo, hi)[2].0 >= detail {
                let corners: Vec<V3> = (0..8)
                    .map(|i| v3(if i & 1 != 0 { hi.x } else { lo.x }, if i & 2 != 0 { hi.y } else { lo.y }, if i & 4 != 0 { hi.z } else { lo.z }))
                    .collect();
                if let Some(b) = hull_brush(&corners) {
                    pieces.push(b);
                }
            }
            continue;
        }
        let pts: Vec<V3> = members.iter().flat_map(|&m| brushes[details[m]].verts.iter().copied()).collect();
        let Some(h) = hull_brush(&pts) else { continue };
        let hv = volume(&h);
        let (hl, hh) = h.bounds();
        if hv > 0.0 && filled / hv >= 0.5 && dims(hl, hh)[2].0 >= detail {
            pieces.push(h);
        }
    }
    if dbg {
        eprintln!("simplify: {} pieces before merging, {:?}", pieces.len(), t0.elapsed());
    }
    let out = merge_exact(pieces);
    if dbg {
        eprintln!("simplify: {} after merging, {:?}", out.len(), t0.elapsed());
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;

    fn bx(lo: V3, hi: V3) -> Brush {
        Brush::new_box(0, lo, hi, [1.0; 3])
    }

    #[test]
    fn exact_join_only_when_the_union_is_convex() {
        let a = bx(v3(0.0, 0.0, 0.0), v3(32.0, 32.0, 32.0));
        let b = bx(v3(32.0, 0.0, 0.0), v3(64.0, 32.0, 32.0));
        let c = bx(v3(32.0, 32.0, 0.0), v3(64.0, 64.0, 32.0)); // makes an L with a
        assert!(exact_join(&a, &b).is_some());
        assert!(exact_join(&a, &c).is_none());
        let j = exact_join(&a, &b).unwrap();
        assert_eq!(j.bounds(), (v3(0.0, 0.0, 0.0), v3(64.0, 32.0, 32.0)));
        assert_eq!(j.faces.len(), 6);
    }

    #[test]
    fn simplify_fills_windows_keeps_open_doors_and_ramps_stairs() {
        let mut v: Vec<Brush> = Vec::new();
        // a wall along X, 8 thick (y 0..8), 128 tall, with a window opening x 64..96, z 48..80
        // and a door opening x 160..192, z 0..96 (empty)
        v.push(bx(v3(0.0, 0.0, 0.0), v3(64.0, 8.0, 128.0)));
        v.push(bx(v3(96.0, 0.0, 0.0), v3(160.0, 8.0, 128.0)));
        v.push(bx(v3(64.0, 0.0, 0.0), v3(96.0, 8.0, 48.0)));
        v.push(bx(v3(64.0, 0.0, 80.0), v3(96.0, 8.0, 128.0)));
        v.push(bx(v3(160.0, 0.0, 96.0), v3(192.0, 8.0, 128.0)));
        v.push(bx(v3(192.0, 0.0, 0.0), v3(256.0, 8.0, 128.0)));
        // the window's pane and a frame bar
        v.push(bx(v3(64.0, 3.0, 48.0), v3(96.0, 4.0, 80.0)));
        v.push(bx(v3(78.0, 2.0, 48.0), v3(82.0, 6.0, 80.0)));
        // stairs: 4 steps of 8 × 64 × (8, 16, 24, 32) in front of the wall
        for s in 0..4 {
            let x0 = 300.0 + s as f32 * 8.0;
            v.push(bx(v3(x0, 32.0, 0.0), v3(x0 + 8.0, 96.0, 8.0 * (s + 1) as f32)));
        }
        // a lone tiny knob: dropped
        v.push(bx(v3(500.0, 500.0, 0.0), v3(503.0, 503.0, 3.0)));
        let refs: Vec<&Brush> = v.iter().collect();
        let out = simplify(&refs, 33.0);
        let solid_at = |p: V3| out.iter().any(|b| b.faces.iter().all(|f| f.normal.dot(p) <= f.dist + 1e-3));
        assert!(solid_at(v3(80.0, 4.0, 64.0)), "the window is filled");
        assert!(!solid_at(v3(176.0, 4.0, 48.0)), "the empty door stays open");
        assert!(solid_at(v3(320.0, 64.0, 10.0)), "the stairs are a ramp");
        assert!(!solid_at(v3(501.0, 501.0, 1.0)), "tiny loose details go");
        assert!(out.len() < v.len(), "fewer pieces: {} from {}", out.len(), v.len());
        // the wall merged around the filled window: x 0..160 is one slab now
        assert!(out.iter().any(|b| b.bounds() == (v3(0.0, 0.0, 0.0), v3(160.0, 8.0, 128.0))), "{:?}", out.iter().map(|b| b.bounds()).collect::<Vec<_>>());
    }
}
