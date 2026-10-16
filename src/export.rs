// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Steventechno and the Bonk! contributors
//! Model export: Valve SMD files for studiomdl.
//!
//! The reference SMD holds what renders: faces on visual layers that aren't
//! nodraw (mannequins skipped), positioned relative to the model origin, with
//! each face's texture alignment turned into UVs. The physics SMD holds the
//! collision layers' brushes, one convex piece each, relative to the model
//! origin (or a layer's own origin).

use crate::doc::{Brush, LayerKind};
use crate::editor::Editor;
use crate::geom;
use crate::math::*;
use crate::texstore;
use std::fmt::Write;

/// Material names in the SMD (and VMT file names): the texture's path with
/// "/" turned into "_", since every VMT of a model sits in one folder.
pub fn flat_material(key: &str) -> String {
    if key.is_empty() {
        "none".into()
    } else {
        key.replace('/', "_")
    }
}

fn header(o: &mut String) {
    o.push_str("version 1\nnodes\n0 \"root\" -1\nend\nskeleton\ntime 0\n0 0 0 0 0 0 0\nend\ntriangles\n");
}

fn vert(o: &mut String, p: V3, n: V3, uv: (f32, f32)) {
    let _ = writeln!(o, "0 {:.6} {:.6} {:.6} {:.6} {:.6} {:.6} {:.6} {:.6}", p.x, p.y, p.z, n.x, n.y, n.z, uv.0, uv.1);
}

fn tri_ok(a: V3, b: V3, c: V3) -> bool {
    (b - a).cross(c - a).len() > 1e-6
}

/// The reference (visual) SMD.
pub fn reference_smd(ed: &Editor) -> (String, usize, usize) {
    let mut o = String::new();
    header(&mut o);
    let origin = ed.doc.origin;
    let mut tris = 0;
    let mut untextured = 0;
    for b in ed.doc.brushes.iter().filter(|b| b.entity.is_none() && !ed.doc.is_collision(b)) {
        for f in b.faces.iter().filter(|f| f.material != geom::NODRAW) {
            if f.material.is_empty() {
                untextured += 1;
            }
            let (tw, th) = texstore::size(&f.material).map(|(w, h)| (w as f32, h as f32)).unwrap_or((512.0, 512.0));
            let tm = f.tex.or_default_for(f.normal);
            let mat = flat_material(&f.material);
            for k in 1..f.idx.len().saturating_sub(1) {
                let (a, bb, c) = (b.verts[f.idx[0]], b.verts[f.idx[k]], b.verts[f.idx[k + 1]]);
                if !tri_ok(a, bb, c) {
                    continue;
                }
                o.push_str(&mat);
                o.push('\n');
                for p in [a, bb, c] {
                    let (u, v) = tm.uv(p);
                    // SMD UVs have v going up the image; ours go down it
                    vert(&mut o, p - origin, f.normal, (u / tw, -v / th));
                }
                tris += 1;
            }
        }
    }
    o.push_str("end\n");
    (o, tris, untextured)
}

/// The physics SMD: one convex piece per collision brush. With
/// `from_visual`, the visual brushes stand in for a missing collision layer.
/// Returns (SMD, convex pieces written, pieces skipped for being too thin).
///
/// studiomdl joins triangles into one convex piece only where their corners
/// match exactly (position *and* normal), so every corner of a brush gets one
/// shared normal, pointing out from the brush's center. With per-face normals
/// each face became its own flat "piece". Pieces under half a unit thick are
/// left out: studiomdl calls them 2-dimensional and gives up on the whole
/// collision model, shrink-wrapping it instead.
pub fn physics_smd(ed: &Editor, from_visual: bool) -> (String, usize, usize) {
    let mut o = String::new();
    header(&mut o);
    let mut pieces = 0;
    let mut thin = 0;
    let model_origin = ed.doc.origin;
    let use_brush = |b: &&Brush| {
        b.entity.is_none()
            && if from_visual {
                !ed.doc.is_collision(b) && b.faces.iter().any(|f| f.material != geom::NODRAW)
            } else {
                ed.doc.layer(b.layer).is_some_and(|l| l.kind == LayerKind::Collision)
            }
    };
    for b in ed.doc.brushes.iter().filter(use_brush) {
        let origin = ed.doc.layer(b.layer).and_then(|l| l.origin).unwrap_or(model_origin);
        let (lo, hi) = b.bounds();
        let size = hi - lo;
        if size.x.min(size.y).min(size.z) < 0.5 {
            thin += 1;
            continue;
        }
        let center = (lo + hi) * 0.5;
        let mut any = false;
        for f in &b.faces {
            for k in 1..f.idx.len().saturating_sub(1) {
                let (a, bb, c) = (b.verts[f.idx[0]], b.verts[f.idx[k]], b.verts[f.idx[k + 1]]);
                if !tri_ok(a, bb, c) {
                    continue;
                }
                o.push_str("phy\n");
                for p in [a, bb, c] {
                    vert(&mut o, p - origin, (p - center).norm(), (0.0, 0.0));
                }
                any = true;
            }
        }
        pieces += any as usize;
    }
    o.push_str("end\n");
    (o, pieces, thin)
}

/// Materials the reference SMD uses (texture keys, "" for none), most-used first.
pub fn used_materials(ed: &Editor) -> Vec<(String, usize)> {
    let mut out: Vec<(String, usize)> = Vec::new();
    for b in ed.doc.brushes.iter().filter(|b| b.entity.is_none() && !ed.doc.is_collision(b)) {
        for f in b.faces.iter().filter(|f| f.material != geom::NODRAW) {
            match out.iter_mut().find(|m| m.0 == f.material) {
                Some(m) => m.1 += 1,
                None => out.push((f.material.clone(), 1)),
            }
        }
    }
    out.sort_by(|a, b| b.1.cmp(&a.1));
    out
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::editor::Editor;

    #[test]
    fn reference_and_physics_smd() {
        let mut e = Editor::new();
        e.doc.origin = v3(0.0, 0.0, 0.0);
        let a = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0));
        for f in &mut e.doc.brush_mut(a).unwrap().faces {
            f.material = "walls/red_brick".into();
        }
        e.doc.brush_mut(a).unwrap().faces[0].material = geom::NODRAW.into();
        e.doc.origin = v3(32.0, 32.0, 0.0);
        let (smd, tris, none) = reference_smd(&e);
        assert_eq!(tris, 10);
        assert_eq!(none, 0);
        assert!(smd.starts_with("version 1\nnodes\n0 \"root\" -1\nend\nskeleton\ntime 0\n"));
        assert_eq!(smd.matches("walls_red_brick\n").count(), 10);
        // relative to the origin: x runs -32..32
        assert!(smd.contains("0 -32.000000 ") && smd.contains("0 32.000000 "));
        assert!(smd.trim_end().ends_with("end"));
        // no collision layer: nothing, unless the visual model stands in
        assert_eq!(physics_smd(&e, false).1, 0);
        assert_eq!(physics_smd(&e, true).1, 1);
        crate::bonk_test_add_collision(&mut e);
        let (phy, pieces, _) = physics_smd(&e, false);
        assert_eq!(pieces, 1);
        assert_eq!(phy.matches("phy\n").count(), 12, "collision keeps every face, nodraw included");
        // each corner is written identically on every face it touches, so
        // studiomdl joins the box into one piece: 8 distinct corner lines
        let corners: std::collections::HashSet<&str> = phy.lines().filter(|l| l.split(' ').count() == 9).collect();
        assert_eq!(corners.len(), 8, "{corners:#?}");
        assert_eq!(used_materials(&e), vec![("walls/red_brick".to_string(), 5)]);
        assert_eq!(flat_material("walls/stone_pack/cobble"), "walls_stone_pack_cobble");
    }

    /// Triangles wind counterclockwise seen from outside (as Blender Source
    /// Tools writes them), agreeing with the stored normal.
    #[test]
    fn triangles_face_outward() {
        let mut e = Editor::new();
        let a = e.doc.add_brush(v3(-16.0, -24.0, 0.0), v3(40.0, 8.0, 72.0));
        for f in &mut e.doc.brush_mut(a).unwrap().faces {
            f.material = "x".into();
        }
        let (smd, tris, _) = reference_smd(&e);
        assert_eq!(tris, 12);
        let rows: Vec<Vec<f32>> = smd
            .lines()
            .filter(|l| l.starts_with("0 ") && l.split(' ').count() == 9)
            .map(|l| l.split(' ').skip(1).map(|x| x.parse().unwrap()).collect())
            .collect();
        assert_eq!(rows.len(), 36);
        for t in rows.chunks(3) {
            let p = |r: &Vec<f32>| v3(r[0], r[1], r[2]);
            let n = v3(t[0][3], t[0][4], t[0][5]);
            let c = (p(&t[1]) - p(&t[0])).cross(p(&t[2]) - p(&t[0]));
            assert!(c.dot(n) > 0.0, "triangle winds inward: {:?}", t);
        }
    }
}
