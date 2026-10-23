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
/// Solid colors (colors/ffffff) get the model's name in front, so two
/// models' color materials never share a file: house_colors_ffffff.
pub fn flat_material(key: &str) -> String {
    if key.is_empty() {
        return "none".into();
    }
    let flat = key.replace('/', "_");
    COLOR_PREFIX.with(|p| {
        let p = p.borrow();
        if key.starts_with("colors/") && !p.is_empty() {
            format!("{}_{}", p, flat)
        } else {
            flat
        }
    })
}

thread_local! {
    /// The model's name, for color materials (set while compiling).
    pub static COLOR_PREFIX: std::cell::RefCell<String> = std::cell::RefCell::new(String::new());
}

/// The color-material prefix for a model path ("props/house" → "house").
pub fn color_prefix_for(model_path: &str) -> String {
    let base = model_path.trim_end_matches(".mdl").rsplit('/').next().unwrap_or("");
    base.chars().map(|c| if c.is_ascii_alphanumeric() || c == '_' || c == '-' { c.to_ascii_lowercase() } else { '_' }).collect()
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

/// The reference (visual) SMD: every visual brush (bodygroups and skins aside).
/// Compiling uses `variant_export`; this is the plain one (tests, bonk-special).
#[allow(dead_code)]
pub fn reference_smd(ed: &Editor) -> (String, usize, usize) {
    smd_of(ed, ed.doc.brushes.iter().filter(|b| b.entity.is_none() && !ed.doc.is_collision(b)), &|_, m| m.to_string())
}

/// A visual SMD from these brushes; `name` gives each face's material name
/// (the brush, the material as painted).
fn smd_of<'a>(ed: &Editor, brushes: impl Iterator<Item = &'a Brush>, name: &dyn Fn(&Brush, &str) -> String) -> (String, usize, usize) {
    let mut o = String::new();
    header(&mut o);
    let origin = ed.doc.origin;
    let mut tris = 0;
    let mut untextured = 0;
    for b in brushes {
        for f in b.faces.iter().filter(|f| f.material != geom::NODRAW) {
            if f.material.is_empty() {
                untextured += 1;
            }
            let (tw, th) = texstore::size(&f.material).map(|(w, h)| (w as f32, h as f32)).unwrap_or((512.0, 512.0));
            let tm = f.tex.or_default_for(f.normal);
            let mat = flat_material(&name(b, &f.material));
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

/// The material that hides faces in a skin (a VMT with $no_draw).
pub const HIDDEN_MATERIAL: &str = "bonk_hidden";

/// Everything the model's variants add to a compile.
#[derive(Debug, Default)]
pub struct VariantExport {
    /// The base body (reference.smd): visual brushes in no bodygroup option
    pub reference: String,
    pub tris: usize,
    pub untextured: usize,
    /// Extra SMDs: bodygroup options and LODs (file name, text)
    pub smds: Vec<(String, String)>,
    /// QC lines in place of the plain "$body": bodygroups, $texturegroup, $lod
    pub qc: String,
    /// The SMD's sequence source (reference.smd, or a bodygroup's first option when the base body is empty)
    pub sequence: String,
    /// Materials that are copies of another (for brushes a skin hides): (copy, original)
    pub aliases: Vec<(String, String)>,
    /// The skins use the invisible material
    pub hidden: bool,
}

/// A name usable in a file name.
fn slug(s: &str) -> String {
    let o: String = s.chars().map(|c| if c.is_ascii_alphanumeric() { c.to_ascii_lowercase() } else { '_' }).collect();
    let o = o.trim_matches('_').to_string();
    if o.is_empty() { "part".into() } else { o }
}

fn quote(s: &str) -> String {
    s.replace('"', "'")
}

/// The variant-aware export: reference.smd, bodygroup and LOD SMDs, and the QC for them.
pub fn variant_export(ed: &Editor) -> VariantExport {
    let doc = &ed.doc;
    let v = &doc.variants;
    let visual: Vec<&Brush> = doc.brushes.iter().filter(|b| b.entity.is_none() && !doc.is_collision(b)).collect();
    // which skins hide each brush (bit k-1 = skin k)
    let mask = |b: &Brush| -> u32 {
        v.skins.iter().enumerate().filter(|(_, s)| s.hidden.contains(&b.id)).fold(0u32, |m, (i, _)| m | (1 << i.min(31)))
    };
    let name = |b: &Brush, m: &str| -> String {
        let k = mask(b);
        if k == 0 { m.to_string() } else { format!("{m}__h{k:x}") }
    };
    let mut out = VariantExport::default();
    // bodygroups
    let bgs = doc.bodygroups();
    let in_option = |b: &Brush| doc.option_group_of(b);
    let base: Vec<&Brush> = visual.iter().copied().filter(|b| in_option(b).is_none()).collect();
    let (r, tris, un) = smd_of(ed, base.iter().copied(), &name);
    out.reference = r;
    out.tris = tris;
    out.untextured = un;
    let mut bodies: Vec<(String, Vec<&Brush>)> = Vec::new(); // smd file → its brushes (for LODs)
    let mut qc = String::new();
    if !base.is_empty() || bgs.is_empty() {
        qc.push_str("$body \"body\" \"reference.smd\"\n");
        bodies.push(("reference.smd".into(), base.clone()));
        out.sequence = "reference.smd".into();
    }
    for bg in &bgs {
        let _ = writeln!(qc, "$bodygroup \"{}\"\n{{", quote(&bg.name));
        for (gid, opt) in &bg.options {
            let file = format!("bg_{}_{}_{}.smd", slug(&bg.name), slug(opt), gid);
            let members: Vec<&Brush> = visual.iter().copied().filter(|b| in_option(b) == Some(*gid)).collect();
            let (text, t, u) = smd_of(ed, members.iter().copied(), &name);
            out.tris += t;
            out.untextured += u;
            let _ = writeln!(qc, "\tstudio \"{file}\"");
            if out.sequence.is_empty() {
                out.sequence = file.clone();
            }
            out.smds.push((file.clone(), text));
            bodies.push((file, members));
        }
        if bg.blank {
            qc.push_str("\tblank\n");
        }
        qc.push_str("}\n");
    }
    // skins: a column for each material that differs in some skin
    if !v.skins.is_empty() {
        let mut cols: Vec<String> = Vec::new(); // material names as in the SMD (aliases included)
        for b in &visual {
            for f in b.faces.iter().filter(|f| f.material != geom::NODRAW) {
                let n = name(b, &f.material);
                if !cols.contains(&n) {
                    cols.push(n);
                }
            }
        }
        let real = |n: &str| -> (String, u32) {
            match n.rsplit_once("__h") {
                Some((m, k)) if u32::from_str_radix(k, 16).is_ok() => (m.to_string(), u32::from_str_radix(k, 16).unwrap()),
                _ => (n.to_string(), 0),
            }
        };
        let cell = |n: &str, skin: usize| -> String {
            if skin == 0 {
                return flat_material(n);
            }
            let (m, k) = real(n);
            if k & (1 << (skin - 1).min(31)) != 0 {
                return HIDDEN_MATERIAL.into();
            }
            let to = v.skins[skin - 1].swap(&m);
            if to == geom::NODRAW {
                HIDDEN_MATERIAL.into()
            } else if to == m {
                flat_material(n) // unchanged in this skin
            } else {
                flat_material(to)
            }
        };
        let varying: Vec<&String> = cols.iter().filter(|c| (1..=v.skins.len()).any(|s| cell(c, s) != cell(c, 0))).collect();
        if !varying.is_empty() {
            qc.push_str("$texturegroup \"skinfamilies\"\n{\n");
            for s in 0..=v.skins.len() {
                let row: Vec<String> = varying.iter().map(|c| format!("\"{}\"", cell(c, s))).collect();
                let _ = writeln!(qc, "\t{{ {} }}", row.join(" "));
                if s > 0 && row.iter().any(|r| r.contains(HIDDEN_MATERIAL)) {
                    out.hidden = true;
                }
            }
            qc.push_str("}\n");
        }
        for c in &cols {
            let (m, k) = real(c);
            if k != 0 {
                out.aliases.push((c.clone(), m));
            }
        }
    }
    // LODs: each body without the small brushes
    let mut lods = v.lods.clone();
    lods.sort_by(|a, b| a.distance.total_cmp(&b.distance));
    for (i, lod) in lods.iter().enumerate() {
        let _ = writeln!(qc, "$lod {}\n{{", lod.distance.max(1.0).round() as i64);
        for (file, members) in &bodies {
            let kept: Vec<&Brush> = members.iter().copied().filter(|b| crate::doc::Document::lod_keeps(b, lod.detail)).collect();
            if kept.is_empty() {
                let _ = writeln!(qc, "\treplacemodel \"{file}\" blank");
                continue;
            }
            let lf = format!("{}_lod{}.smd", file.trim_end_matches(".smd"), i + 1);
            let (text, _, _) = smd_of(ed, kept.iter().copied(), &name);
            let _ = writeln!(qc, "\treplacemodel \"{file}\" \"{lf}\"");
            out.smds.push((lf, text));
        }
        qc.push_str("}\n");
    }
    out.qc = qc;
    out
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
    let use_brush = |b: &&Brush| {
        b.entity.is_none()
            && if from_visual {
                !ed.doc.is_collision(b) && b.faces.iter().any(|f| f.material != geom::NODRAW)
            } else {
                ed.doc.layer(b.layer).is_some_and(|l| l.kind == LayerKind::Collision)
            }
    };
    physics_smd_from(ed, ed.doc.brushes.iter().filter(use_brush).collect())
}

/// The physics SMD from these brushes (one convex piece each).
pub fn physics_smd_from(ed: &Editor, brushes: Vec<&Brush>) -> (String, usize, usize) {
    let mut o = String::new();
    header(&mut o);
    let mut pieces = 0;
    let mut thin = 0;
    let model_origin = ed.doc.origin;
    for b in brushes {
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
    // what skins swap in (once each)
    for sk in &ed.doc.variants.skins {
        for (_, to) in &sk.swaps {
            if to != geom::NODRAW && !out.iter().any(|m| &m.0 == to) {
                out.push((to.clone(), 0));
            }
        }
    }
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
        COLOR_PREFIX.with(|p| *p.borrow_mut() = color_prefix_for("props/My House.mdl"));
        assert_eq!(flat_material("colors/ffffff"), "my_house_colors_ffffff");
        assert_eq!(flat_material("walls/brick"), "walls_brick");
        COLOR_PREFIX.with(|p| p.borrow_mut().clear());
        assert_eq!(flat_material("colors/ffffff"), "colors_ffffff");
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

    #[test]
    fn variants_make_bodygroups_skins_and_lods() {
        let mut e = Editor::new();
        let house = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(256.0, 256.0, 128.0));
        let sign = e.doc.add_brush(v3(0.0, -8.0, 96.0), v3(16.0, 0.0, 112.0)); // small: LODs drop it
        let closed = e.doc.add_brush(v3(100.0, -4.0, 0.0), v3(156.0, 0.0, 100.0));
        let open = e.doc.add_brush(v3(96.0, -60.0, 0.0), v3(100.0, 0.0, 100.0));
        for (id, m) in [(house, "walls/brick"), (sign, "signs/shop"), (closed, "doors/wood"), (open, "doors/wood")] {
            for f in &mut e.doc.brush_mut(id).unwrap().faces {
                f.material = m.into();
            }
        }
        e.doc.selection = vec![closed];
        let g1 = e.doc.group_selection().unwrap();
        e.doc.rename_group(g1, "Closed");
        e.doc.selection = vec![open];
        let g2 = e.doc.group_selection().unwrap();
        e.doc.rename_group(g2, "Open");
        e.doc.set_bodygroup(g1, "Door");
        e.doc.set_bodygroup(g2, "Door");
        e.doc.set_bodygroup_blank("Door", true);
        // skin 1: plaster walls; skin 2: no sign
        e.doc.add_skin(0);
        e.doc.skin_swap("walls/brick", "walls/plaster");
        e.doc.add_skin(0);
        e.doc.skin_hide(&[sign], true);
        e.doc.variants.lods.push(crate::variants::Lod { distance: 40.0, detail: 32.0 });
        let v = variant_export(&e);
        // the base body has the house and the sign, not the door options
        assert!(v.reference.contains("walls_brick") && v.reference.contains("signs_shop__h2"));
        assert!(!v.reference.contains("doors_wood"));
        assert_eq!(v.sequence, "reference.smd");
        assert!(v.qc.contains("$body \"body\" \"reference.smd\""));
        assert!(v.qc.contains(&format!("$bodygroup \"Door\"\n{{\n\tstudio \"bg_door_closed_{g1}.smd\"\n\tstudio \"bg_door_open_{g2}.smd\"\n\tblank\n}}")), "{}", v.qc);
        assert!(v.smds.iter().any(|(n, t)| n == &format!("bg_door_open_{g2}.smd") && t.contains("doors_wood")));
        // skins: brick swaps in skin 1; the sign's copy hides in skin 2
        assert!(v.qc.contains("$texturegroup \"skinfamilies\""), "{}", v.qc);
        assert!(v.qc.contains("{ \"walls_brick\" \"signs_shop__h2\" }"), "{}", v.qc);
        assert!(v.qc.contains("{ \"walls_plaster\" \"signs_shop__h2\" }"), "{}", v.qc);
        assert!(v.qc.contains("{ \"walls_brick\" \"bonk_hidden\" }"), "{}", v.qc);
        assert!(v.hidden);
        assert_eq!(v.aliases, vec![("signs/shop__h2".to_string(), "signs/shop".to_string())]);
        // LOD: the sign drops out of the base body, the door options stay
        assert!(v.qc.contains("$lod 40\n{\n\treplacemodel \"reference.smd\" \"reference_lod1.smd\""), "{}", v.qc);
        let lod = &v.smds.iter().find(|(n, _)| n == "reference_lod1.smd").unwrap().1;
        assert!(lod.contains("walls_brick") && !lod.contains("signs_shop"));
        // swap targets are compiled too
        assert!(used_materials(&e).iter().any(|m| m.0 == "walls/plaster"));
        // no variants: the plain body, nothing else
        let plain = variant_export(&Editor::new());
        assert_eq!(plain.qc, "$body \"body\" \"reference.smd\"\n");
        assert!(plain.smds.is_empty() && plain.aliases.is_empty());
    }
}
