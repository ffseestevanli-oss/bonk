// SPDX-License-Identifier: GPL-3.0-or-later
//! Find brushes that overlap (share volume), not ones that only touch.

use crate::doc::Brush;
use crate::geom;
use crate::math::V3;

/// One finding: two brushes, how much volume they share (cubic units), and
/// whether they're exact duplicates.
#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Overlap {
    pub a: u32,
    pub b: u32,
    pub volume: f32,
    pub duplicate: bool,
}

/// Shared volume below this doesn't count (rounding on touching faces).
const MIN_VOLUME: f32 = 1.0;
const EPS: f32 = 0.01;

fn inside(b: &Brush, p: V3) -> bool {
    b.faces.iter().all(|f| f.normal.dot(p) <= f.dist + EPS)
}

fn volume(h: &geom::Hull) -> f32 {
    let mut v = 0.0;
    for f in &h.faces {
        let p0 = h.verts[f.idx[0]];
        for k in 1..f.idx.len().saturating_sub(1) {
            v += p0.dot(h.verts[f.idx[k]].cross(h.verts[f.idx[k + 1]]));
        }
    }
    (v / 6.0).abs()
}

fn same_shape(a: &Brush, b: &Brush) -> bool {
    a.verts.len() == b.verts.len() && a.verts.iter().all(|p| b.verts.iter().any(|q| (*p - *q).len() < EPS * 10.0))
}

/// The volume two convex brushes share.
pub fn shared_volume(a: &Brush, b: &Brush) -> f32 {
    let mut pts: Vec<V3> = Vec::new();
    pts.extend(a.verts.iter().copied().filter(|&p| inside(b, p)));
    pts.extend(b.verts.iter().copied().filter(|&p| inside(a, p)));
    // edges of one crossing the faces of the other
    for (x, y) in [(a, b), (b, a)] {
        for (i, j) in x.edges() {
            let (p, q) = (x.verts[i], x.verts[j]);
            for f in &y.faces {
                let (dp, dq) = (f.normal.dot(p) - f.dist, f.normal.dot(q) - f.dist);
                if (dp > 0.0) != (dq > 0.0) && (dp - dq).abs() > 1e-6 {
                    let r = p + (q - p) * (dp / (dp - dq));
                    if inside(y, r) {
                        pts.push(r);
                    }
                }
            }
        }
    }
    if pts.len() < 4 {
        return 0.0;
    }
    geom::hull(&pts).map(|h| volume(&h)).unwrap_or(0.0)
}

/// Every overlapping pair among `brushes`; `group` tells which brushes may be
/// compared (same value = same kind, e.g. visual vs collision). Duplicates
/// first, then the biggest overlaps. At most `limit` results.
pub fn scan(brushes: &[&Brush], group: &dyn Fn(&Brush) -> u8, limit: usize) -> Vec<Overlap> {
    let mut items: Vec<(&Brush, (V3, V3))> = brushes.iter().map(|b| (*b, b.bounds())).collect();
    items.sort_by(|x, y| x.1 .0.x.total_cmp(&y.1 .0.x));
    let mut out = Vec::new();
    for i in 0..items.len() {
        let (a, (alo, ahi)) = items[i];
        for &(b, (blo, bhi)) in &items[i + 1..] {
            if blo.x >= ahi.x - EPS {
                break; // sorted by min x: nothing further along can touch
            }
            if blo.y >= ahi.y - EPS || bhi.y <= alo.y + EPS || blo.z >= ahi.z - EPS || bhi.z <= alo.z + EPS || bhi.x <= alo.x + EPS {
                continue;
            }
            if group(a) != group(b) {
                continue;
            }
            let duplicate = same_shape(a, b);
            let volume = shared_volume(a, b);
            if duplicate || volume >= MIN_VOLUME {
                let (x, y) = if a.id < b.id { (a.id, b.id) } else { (b.id, a.id) };
                out.push(Overlap { a: x, b: y, volume, duplicate });
            }
        }
    }
    out.sort_by(|x, y| y.duplicate.cmp(&x.duplicate).then(y.volume.total_cmp(&x.volume)));
    out.truncate(limit);
    out
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::math::v3;

    fn bx(id: u32, lo: V3, hi: V3) -> Brush {
        Brush::new_box(id, lo, hi, [1.0, 1.0, 1.0])
    }

    #[test]
    fn touching_is_fine_overlap_and_duplicates_are_found() {
        let a = bx(1, v3(0.0, 0.0, 0.0), v3(32.0, 32.0, 32.0));
        let touching = bx(2, v3(32.0, 0.0, 0.0), v3(64.0, 32.0, 32.0));
        let over = bx(3, v3(24.0, 8.0, 8.0), v3(40.0, 24.0, 24.0)); // 8 × 16 × 16 inside a
        let dup = bx(4, v3(0.0, 0.0, 0.0), v3(32.0, 32.0, 32.0));
        assert_eq!(shared_volume(&a, &touching), 0.0);
        assert!((shared_volume(&a, &over) - 8.0 * 16.0 * 16.0).abs() < 1.0, "{}", shared_volume(&a, &over));
        let all = [&a, &touching, &over, &dup];
        let r = scan(&all, &|_| 0, 100);
        assert!(r[0].duplicate && (r[0].a, r[0].b) == (1, 4));
        assert!(r.iter().any(|o| (o.a, o.b) == (1, 3)));
        assert!(!r.iter().any(|o| (o.a, o.b) == (1, 2)), "touching isn't overlapping");
        // different kinds (visual vs collision) aren't compared
        let r = scan(&all, &|b| (b.id == 4) as u8, 100);
        assert!(!r.iter().any(|o| o.duplicate));
    }
}
