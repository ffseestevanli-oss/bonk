// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Steventechno and the Bonk! contributors
//! .bpmdl files: a readable KeyValues-style text format (the same family of
//! syntax as Valve's VMT/VMF files).
//!
//! ```text
//! // Bonk! Model Prop-inator! model
//! "BonkModel"
//! {
//!     "format"    "2"
//!     "editor"  { "grid" "16"  "camera" "-320 -320 256 45 -25" ... }
//!     "origin"  "0 0 0"                         // the model origin
//!     "active_layer" "1"
//!     "layer"   { "id" "1" "name" "Visual 1" "kind" "visual" "visible" "1" "locked" "0" }
//!     "brush"
//!     {
//!         "id"        "1"
//!         "layer"     "1"
//!         "entity"    "mannequin"     // only for entities
//!         "color"     "0.55 0.64 0.88"
//!         "rotation"  "0 0 15"
//!         "v"         "0 0 0"        // one per vertex
//!         "f"         "0 1 3 2"      // one per face: vertex indices, CCW from outside
//!         "mat"       "tools/toolsnodraw"   // optional: material of the face above
//!         "t"         "ux uy uz shiftu vx vy vz shiftv scaleu scalev rot"  // optional: texture mapping
//!     }
//! }
//! ```
//!
//! Format 1 files (no layers, origin or materials) still load.

use crate::doc::{Brush, Entity, Group, Layer, LayerKind};
use crate::editor::Editor;
use crate::geom::{self, Face};
use crate::tex::TexMap;
use crate::math::*;

pub const FORMAT_VERSION: u32 = 3;

// ------------------------------------------------------------------ writing

fn num(v: f32) -> String {
    if v == 0.0 {
        return "0".into(); // also turns -0 into 0
    }
    format!("{}", v) // shortest representation that round-trips
}

fn vec3(v: V3) -> String {
    format!("{} {} {}", num(v.x), num(v.y), num(v.z))
}

fn kv(out: &mut String, indent: usize, k: &str, v: &str) {
    out.push_str(&"\t".repeat(indent));
    // The format has no escapes: keep values on one line and free of quotes.
    let v: String = v.chars().map(|c| match c { '"' => '\'', '\n' | '\r' => ' ', c => c }).collect();
    out.push_str(&format!("\"{}\"\t\"{}\"\n", k, v));
}

pub fn write(ed: &Editor) -> String {
    let mut o = String::new();
    o.push_str("// Bonk! Model Prop-inator! model\n\"BonkModel\"\n{\n");
    kv(&mut o, 1, "format", &FORMAT_VERSION.to_string());
    o.push_str("\t\"editor\"\n\t{\n");
    kv(&mut o, 2, "grid", &num(ed.grid));
    let c = &ed.cam;
    kv(&mut o, 2, "camera", &format!("{} {} {}", vec3(c.pos), num(c.yaw), num(c.pitch)));
    for (i, v) in ed.views.iter().enumerate() {
        kv(&mut o, 2, &format!("view{}", i + 1), &format!("{} {} {}", num(v.cx), num(v.cy), num(v.zoom)));
    }
    kv(&mut o, 2, "show_mannequins", if ed.show_entities { "1" } else { "0" });
    o.push_str("\t}\n");
    kv(&mut o, 1, "origin", &vec3(ed.doc.origin));
    if !ed.doc.model_path.is_empty() {
        kv(&mut o, 1, "model_path", &ed.doc.model_path);
    }
    for (m, props) in &ed.doc.mat_overrides {
        kv(&mut o, 1, "material_override", &format!("{}|{}", m, props));
    }
    kv(&mut o, 1, "active_layer", &ed.doc.active_layer.to_string());
    for l in &ed.doc.layers {
        o.push_str("\t\"layer\"\n\t{\n");
        kv(&mut o, 2, "id", &l.id.to_string());
        kv(&mut o, 2, "name", &l.name);
        kv(&mut o, 2, "kind", if l.kind == LayerKind::Collision { "collision" } else { "visual" });
        kv(&mut o, 2, "visible", if l.visible { "1" } else { "0" });
        kv(&mut o, 2, "locked", if l.locked { "1" } else { "0" });
        if let Some(org) = l.origin {
            kv(&mut o, 2, "origin", &vec3(org));
        }
        o.push_str("\t}\n");
    }
    for g in &ed.doc.groups {
        write_group(&mut o, g);
    }
    for b in &ed.doc.brushes {
        write_brush(&mut o, b);
    }
    o.push_str("}\n");
    o
}

fn write_brush(o: &mut String, b: &Brush) {
    o.push_str("\t\"brush\"\n\t{\n");
    kv(o, 2, "id", &b.id.to_string());
    kv(o, 2, "layer", &b.layer.to_string());
    if let Some(g) = b.group {
        kv(o, 2, "group", &g.to_string());
    }
    if let Some(e) = b.entity {
        kv(o, 2, "entity", e.key());
    }
    kv(o, 2, "color", &format!("{} {} {}", num(b.color[0]), num(b.color[1]), num(b.color[2])));
    kv(o, 2, "rotation", &format!("{} {} {}", num(b.rot[0]), num(b.rot[1]), num(b.rot[2])));
    for &p in &b.verts {
        kv(o, 2, "v", &vec3(p));
    }
    for f in &b.faces {
        let idx: Vec<String> = f.idx.iter().map(|i| i.to_string()).collect();
        kv(o, 2, "f", &idx.join(" "));
        if !f.material.is_empty() {
            kv(o, 2, "mat", &f.material);
        }
        if f.tex != TexMap::world(f.normal) {
            let t = &f.tex;
            kv(o, 2, "t", &format!("{} {} {} {} {} {} {}", vec3(t.u), num(t.shift[0]), vec3(t.v), num(t.shift[1]), num(t.scale[0]), num(t.scale[1]), num(t.rot)));
        }
    }
    o.push_str("\t}\n");
}

fn write_group(o: &mut String, g: &Group) {
    o.push_str("\t\"group\"\n\t{\n");
    kv(o, 2, "id", &g.id.to_string());
    kv(o, 2, "name", &g.name);
    if let Some(p) = g.parent {
        kv(o, 2, "parent", &p.to_string());
    }
    o.push_str("\t}\n");
}

/// The selection as clipboard text: a small model with just those brushes,
/// the groups they're in, and the material settings they use. `source` is
/// the textures folder they came from (so another project can fetch them).
pub fn write_clip(ed: &Editor, source: &str) -> String {
    let mut o = String::new();
    o.push_str("// Bonk! Model Prop-inator! clipboard\n\"BonkModel\"\n{\n");
    kv(&mut o, 1, "format", &FORMAT_VERSION.to_string());
    kv(&mut o, 1, "clipboard", "1");
    if !source.is_empty() {
        kv(&mut o, 1, "source_textures", source);
    }
    let sel: Vec<&Brush> = ed.doc.brushes.iter().filter(|b| ed.doc.is_selected(b.id)).collect();
    let mut mats: Vec<&str> = sel.iter().flat_map(|b| b.faces.iter().map(|f| f.material.as_str())).collect();
    mats.sort();
    mats.dedup();
    for (m, props) in ed.doc.mat_overrides.iter().filter(|(m, _)| mats.contains(&m.as_str())) {
        kv(&mut o, 1, "material_override", &format!("{}|{}", m, props));
    }
    // groups the brushes are in, and their parents
    let mut gids: Vec<u32> = Vec::new();
    for b in &sel {
        let mut g = b.group;
        while let Some(id) = g {
            if gids.contains(&id) {
                break;
            }
            gids.push(id);
            g = ed.doc.group(id).and_then(|x| x.parent);
        }
    }
    for g in ed.doc.groups.iter().filter(|g| gids.contains(&g.id)) {
        write_group(&mut o, g);
    }
    for b in sel {
        write_brush(&mut o, b);
    }
    o.push_str("}\n");
    o
}

// ------------------------------------------------------------------ parsing

#[derive(Debug)]
enum Val {
    Str(String),
    Block(Vec<(String, Val)>),
}

struct Lexer<'a> {
    s: &'a [u8],
    i: usize,
    line: usize,
}

#[derive(Debug, PartialEq)]
enum Tok {
    Str(String),
    Open,
    Close,
    End,
}

impl<'a> Lexer<'a> {
    fn next(&mut self) -> Result<Tok, String> {
        loop {
            while self.i < self.s.len() && self.s[self.i].is_ascii_whitespace() {
                if self.s[self.i] == b'\n' {
                    self.line += 1;
                }
                self.i += 1;
            }
            if self.s[self.i..].starts_with(b"//") {
                while self.i < self.s.len() && self.s[self.i] != b'\n' {
                    self.i += 1;
                }
                continue;
            }
            break;
        }
        if self.i >= self.s.len() {
            return Ok(Tok::End);
        }
        match self.s[self.i] {
            b'{' => {
                self.i += 1;
                Ok(Tok::Open)
            }
            b'}' => {
                self.i += 1;
                Ok(Tok::Close)
            }
            b'"' => {
                self.i += 1;
                let start = self.i;
                while self.i < self.s.len() && self.s[self.i] != b'"' {
                    if self.s[self.i] == b'\n' {
                        return Err(format!("line {}: unterminated string", self.line));
                    }
                    self.i += 1;
                }
                if self.i >= self.s.len() {
                    return Err(format!("line {}: unterminated string", self.line));
                }
                let t = String::from_utf8_lossy(&self.s[start..self.i]).into_owned();
                self.i += 1;
                Ok(Tok::Str(t))
            }
            _ => {
                // bare word
                let start = self.i;
                while self.i < self.s.len() && !self.s[self.i].is_ascii_whitespace() && !b"{}\"".contains(&self.s[self.i]) {
                    self.i += 1;
                }
                Ok(Tok::Str(String::from_utf8_lossy(&self.s[start..self.i]).into_owned()))
            }
        }
    }
}

fn parse_block(lx: &mut Lexer, top: bool) -> Result<Vec<(String, Val)>, String> {
    let mut items = Vec::new();
    loop {
        match lx.next()? {
            Tok::End if top => return Ok(items),
            Tok::End => return Err(format!("line {}: missing '}}'", lx.line)),
            Tok::Close if !top => return Ok(items),
            Tok::Close => return Err(format!("line {}: unexpected '}}'", lx.line)),
            Tok::Open => return Err(format!("line {}: unexpected '{{'", lx.line)),
            Tok::Str(k) => match lx.next()? {
                Tok::Str(v) => items.push((k, Val::Str(v))),
                Tok::Open => items.push((k, Val::Block(parse_block(lx, false)?))),
                _ => return Err(format!("line {}: expected a value for \"{}\"", lx.line, k)),
            },
        }
    }
}

fn floats(s: &str, n: usize, what: &str) -> Result<Vec<f32>, String> {
    let v: Result<Vec<f32>, _> = s.split_whitespace().map(|t| t.parse::<f32>()).collect();
    match v {
        Ok(v) if v.len() == n && v.iter().all(|x| x.is_finite()) => Ok(v),
        _ => Err(format!("bad {} value \"{}\"", what, s)),
    }
}

/// Parsed contents of a .bpmdl file.
#[derive(Debug)]
pub struct Loaded {
    pub brushes: Vec<Brush>,
    pub grid: Option<f32>,
    pub camera: Option<(V3, f32, f32)>,
    pub views: [Option<(f32, f32, f32)>; 3],
    pub show_entities: Option<bool>,
    pub layers: Vec<Layer>,
    pub active_layer: u32,
    pub origin: V3,
    pub model_path: String,
    pub mat_overrides: Vec<(String, String)>,
    pub groups: Vec<Group>,
    /// Brushes whose stored faces didn't match their vertices and were rebuilt
    pub repaired: usize,
}

fn flag(s: &str) -> bool {
    matches!(s.trim(), "1" | "true" | "yes")
}

pub fn read(text: &str) -> Result<Loaded, String> {
    let mut lx = Lexer { s: text.as_bytes(), i: 0, line: 1 };
    let top = parse_block(&mut lx, true)?;
    let body = top
        .into_iter()
        .find_map(|(k, v)| match v {
            Val::Block(b) if k.eq_ignore_ascii_case("BonkModel") => Some(b),
            _ => None,
        })
        .ok_or("not a Bonk! model (no \"BonkModel\" block)")?;

    let mut out = Loaded {
        brushes: Vec::new(),
        grid: None,
        camera: None,
        views: [None; 3],
        show_entities: None,
        layers: Vec::new(),
        active_layer: 0,
        origin: V3::default(),
        model_path: String::new(),
        mat_overrides: Vec::new(),
        groups: Vec::new(),
        repaired: 0,
    };
    for (k, v) in body {
        match (k.as_str(), v) {
            ("format", Val::Str(s)) => {
                let ver: u32 = s.trim().parse().map_err(|_| format!("bad format version \"{}\"", s))?;
                if ver > FORMAT_VERSION {
                    return Err(format!("this file was saved by a newer Bonk! (format {}); please update", ver));
                }
            }
            ("editor", Val::Block(items)) => {
                for (k, v) in items {
                    let Val::Str(s) = v else { continue };
                    match k.as_str() {
                        "grid" => out.grid = floats(&s, 1, "grid").ok().map(|v| v[0]),
                        "camera" => out.camera = floats(&s, 5, "camera").ok().map(|v| (v3(v[0], v[1], v[2]), v[3], v[4])),
                        "view1" | "view2" | "view3" => {
                            let i = (k.as_bytes()[4] - b'1') as usize;
                            out.views[i] = floats(&s, 3, &k).ok().map(|v| (v[0], v[1], v[2]));
                        }
                        "show_mannequins" => out.show_entities = Some(flag(&s)),
                        _ => {}
                    }
                }
            }
            ("origin", Val::Str(s)) => {
                let o = floats(&s, 3, "origin")?;
                out.origin = v3(o[0], o[1], o[2]);
            }
            ("model_path", Val::Str(s)) => out.model_path = s.trim().to_string(),
            ("material_override", Val::Str(s)) => {
                if let Some((m, p)) = s.split_once('|') {
                    out.mat_overrides.push((m.trim().to_string(), p.to_string()));
                }
            }
            ("active_layer", Val::Str(s)) => out.active_layer = s.trim().parse().unwrap_or(0),
            ("layer", Val::Block(items)) => {
                let mut l = Layer { id: 0, name: String::new(), kind: LayerKind::Visual, visible: true, locked: false, origin: None };
                for (k, v) in items {
                    let Val::Str(s) = v else { continue };
                    match k.as_str() {
                        "id" => l.id = s.trim().parse().map_err(|_| format!("bad layer id \"{}\"", s))?,
                        "name" => l.name = s,
                        "kind" => l.kind = if s.trim() == "collision" { LayerKind::Collision } else { LayerKind::Visual },
                        "visible" => l.visible = flag(&s),
                        "locked" => l.locked = flag(&s),
                        "origin" => l.origin = floats(&s, 3, "layer origin").ok().map(|o| v3(o[0], o[1], o[2])),
                        _ => {}
                    }
                }
                if l.id != 0 && !out.layers.iter().any(|x| x.id == l.id) {
                    if l.name.trim().is_empty() {
                        l.name = format!("Layer {}", l.id);
                    }
                    out.layers.push(l);
                }
            }
            ("group", Val::Block(items)) => {
                let mut g = Group { id: 0, name: String::new(), parent: None };
                for (k, v) in items {
                    let Val::Str(s) = v else { continue };
                    match k.as_str() {
                        "id" => g.id = s.trim().parse().unwrap_or(0),
                        "name" => g.name = s.trim().to_string(),
                        "parent" => g.parent = s.trim().parse().ok(),
                        _ => {}
                    }
                }
                if g.id != 0 && !out.groups.iter().any(|x| x.id == g.id) {
                    if g.name.is_empty() {
                        g.name = format!("Group {}", g.id);
                    }
                    out.groups.push(g);
                }
            }
            ("brush", Val::Block(items)) => {
                let n = out.brushes.len();
                let (b, repaired) = read_brush(items).map_err(|e| format!("brush #{}: {}", n + 1, e))?;
                out.repaired += repaired as usize;
                out.brushes.push(b);
            }
            _ => {} // unknown keys are ignored, for forward compatibility
        }
    }
    // Ids must be unique; renumber duplicates.
    let mut seen = Vec::new();
    let mut next = out.brushes.iter().map(|b| b.id).max().unwrap_or(0) + 1;
    for b in &mut out.brushes {
        if b.id == 0 || seen.contains(&b.id) {
            b.id = next;
            next += 1;
        }
        seen.push(b.id);
    }
    Ok(out)
}

fn read_brush(items: Vec<(String, Val)>) -> Result<(Brush, bool), String> {
    let mut id = 0u32;
    let mut color = [0.8f32, 0.8, 0.8];
    let mut rot = [0.0f32; 3];
    let mut layer = 0u32;
    let mut entity = None;
    let mut group: Option<u32> = None;
    let mut verts = Vec::new();
    let mut faces: Vec<Vec<usize>> = Vec::new();
    let mut mats: Vec<String> = Vec::new();
    let mut texs: Vec<Option<TexMap>> = Vec::new();
    for (k, v) in items {
        let Val::Str(s) = v else { continue };
        match k.as_str() {
            "id" => id = s.trim().parse().map_err(|_| format!("bad id \"{}\"", s))?,
            "layer" => layer = s.trim().parse().unwrap_or(0),
            "group" => group = s.trim().parse().ok(),
            "entity" => entity = Some(Entity::from_key(s.trim()).ok_or_else(|| format!("unknown entity \"{}\"", s))?),
            "color" => {
                let c = floats(&s, 3, "color")?;
                color = [c[0].clamp(0.0, 1.0), c[1].clamp(0.0, 1.0), c[2].clamp(0.0, 1.0)];
            }
            "rotation" => {
                let r = floats(&s, 3, "rotation")?;
                rot = [r[0], r[1], r[2]];
            }
            "v" => {
                let p = floats(&s, 3, "vertex")?;
                verts.push(v3(p[0], p[1], p[2]));
            }
            "f" => {
                let idx: Result<Vec<usize>, _> = s.split_whitespace().map(|t| t.parse::<usize>()).collect();
                faces.push(idx.map_err(|_| format!("bad face \"{}\"", s))?);
                mats.push(String::new());
                texs.push(None);
            }
            "t" => {
                let t = floats(&s, 11, "texture mapping")?;
                if let Some(x) = texs.last_mut() {
                    *x = Some(TexMap { u: v3(t[0], t[1], t[2]), shift: [t[3], t[7]], v: v3(t[4], t[5], t[6]), scale: [t[8], t[9]], rot: t[10] });
                }
            }
            "mat" => {
                if let Some(m) = mats.last_mut() {
                    *m = s.trim().to_string();
                }
            }
            _ => {}
        }
    }
    if verts.len() < 4 {
        return Err("needs at least 4 vertices".into());
    }
    if let Some(kind) = entity {
        // Entities are fixed-size boxes: rebuild from the stored base.
        let lo = verts.iter().fold(verts[0], |a, &p| a.min(p));
        let hi = verts.iter().fold(verts[0], |a, &p| a.max(p));
        let base = v3((lo.x + hi.x) * 0.5, (lo.y + hi.y) * 0.5, lo.z);
        let mut b = Brush::new_entity(id, kind, base, rot[2]);
        b.layer = layer;
        return Ok((b, false));
    }
    // Use the stored faces when they describe a valid convex solid; otherwise
    // rebuild the brush from its vertices.
    let stored_ok = faces.len() >= 4 && faces.iter().all(|f| f.len() >= 3 && f.iter().all(|&i| i < verts.len()));
    if stored_ok {
        let faces: Vec<Face> = faces
            .into_iter()
            .zip(mats.iter().cloned().zip(texs.iter().cloned()))
            .map(|(idx, (material, tex))| {
                let (normal, dist) = geom::newell(&verts, &idx);
                let tex = tex.map(|t| t.or_default_for(normal)).unwrap_or_else(|| TexMap::world(normal));
                Face { idx, normal, dist, material, tex }
            })
            .collect();
        let convex = faces.iter().all(|f| verts.iter().all(|&p| f.normal.dot(p) - f.dist < 0.01));
        if convex {
            return Ok((Brush { id, verts, faces, color, rot, layer, entity: None, group }, false));
        }
        // Damaged faces: rebuild, but try to keep the materials.
        let h = geom::hull(&verts).ok_or("vertices don't form a solid")?;
        let mut b = Brush { id, verts: Vec::new(), faces, color, rot, layer, entity: None, group };
        b.set_hull(h);
        return Ok((b, true));
    }
    let h = geom::hull(&verts).ok_or("vertices don't form a solid")?;
    Ok((Brush { id, verts: h.verts, faces: h.faces, color, rot, layer, entity: None, group }, true))
}

/// Apply a loaded file to the editor (replaces the document). Returns how
/// many faces still had the old "none" material and became nodraw.
pub fn apply(ed: &mut Editor, l: Loaded) -> usize {
    let mut brushes = l.brushes;
    let mut legacy = 0;
    for b in brushes.iter_mut().filter(|b| b.entity.is_none()) {
        for f in b.faces.iter_mut().filter(|f| f.material.is_empty()) {
            f.material = geom::NODRAW.to_string();
            legacy += 1;
        }
    }
    ed.doc.replace_all(brushes, l.layers, l.active_layer, l.origin);
    ed.doc.model_path = l.model_path;
    ed.doc.mat_overrides = l.mat_overrides;
    ed.doc.groups = l.groups;
    ed.doc.prune_groups();
    if let Some(v) = l.show_entities {
        ed.show_entities = v;
    }
    if let Some(g) = l.grid {
        if (1.0..=512.0).contains(&g) {
            ed.grid = g;
        }
    }
    if let Some((pos, yaw, pitch)) = l.camera {
        ed.cam.pos = pos;
        ed.cam.yaw = yaw;
        ed.cam.pitch = pitch.clamp(-89.0, 89.0);
    }
    for (v, s) in ed.views.iter_mut().zip(l.views) {
        if let Some((cx, cy, zoom)) = s {
            v.cx = cx;
            v.cy = cy;
            v.zoom = zoom.clamp(0.02, 64.0);
        }
    }
    ed.block_box = None;
    ed.vsel.clear();
    legacy
}

pub fn save_file(ed: &mut Editor, path: &str) -> Result<(), String> {
    let text = write(ed);
    // Write to a temporary file first so a failed save never truncates the old file.
    let tmp = format!("{}.tmp", path);
    std::fs::write(&tmp, text).map_err(|e| format!("couldn't write {}: {}", tmp, e))?;
    std::fs::rename(&tmp, path).map_err(|e| {
        let _ = std::fs::remove_file(&tmp);
        format!("couldn't save {}: {}", path, e)
    })?;
    ed.doc.dirty = false;
    Ok(())
}

/// Load a model: (damaged brushes rebuilt, old "none" faces turned nodraw).
pub fn load_file(ed: &mut Editor, path: &str) -> Result<(usize, usize), String> {
    let text = std::fs::read_to_string(path).map_err(|e| format!("couldn't open {}: {}", path, e))?;
    let l = read(&text)?;
    let repaired = l.repaired;
    let legacy = apply(ed, l);
    Ok((repaired, legacy))
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::editor::{cmd, Editor};

    #[test]
    fn roundtrip_preserves_everything() {
        let mut ed = Editor::new();
        ed.doc.add_brush(v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0));
        let id = ed.doc.add_brush(v3(-100.5, 3.25, 0.0), v3(-20.0, 90.0, 33.3));
        // make the second one a ramp-ish rotated shape
        let b = ed.doc.brush_mut(id).unwrap();
        let mut pts = b.verts.clone();
        for p in pts.iter_mut() {
            if p.z > 1.0 && p.y > 50.0 {
                p.z = 0.0;
            }
        }
        assert!(b.set_points(&pts));
        b.rot = [0.0, 0.0, 33.5];
        b.faces[0].material = geom::NODRAW.into();
        // layers, an entity and the origin
        let col = ed.doc.add_layer(crate::doc::LayerKind::Collision);
        ed.doc.add_brush(v3(0.0, 0.0, 0.0), v3(32.0, 32.0, 32.0));
        ed.doc.add_entity(Entity::Mannequin, v3(100.0, 0.0, 64.0));
        ed.doc.rename_layer(col, "Hull \"main\"");
        ed.doc.set_layer_flags(1, true, true);
        ed.doc.origin = v3(16.0, -8.0, 0.0);
        ed.doc.model_path = "housepack/onestory/house_1".into();
        ed.doc.mat_overrides = vec![("walls/brick".into(), "shader=UnlitGeneric;selfillum=1".into())];
        if let Some(b) = ed.doc.brushes.first_mut() {
            b.faces[0].tex.shift = [3.5, -7.0];
            b.faces[0].tex.scale = [0.5, 0.125];
        }
        ed.show_entities = false;
        ed.grid = 8.0;
        ed.cam.yaw = 12.5;
        ed.views[1].zoom = 2.0;

        let text = write(&ed);
        let mut ed2 = Editor::new();
        apply(&mut ed2, read(&text).unwrap());
        assert_eq!(ed2.doc.brushes, ed.doc.brushes);
        assert_eq!(ed2.doc.layers.len(), 2);
        assert_eq!(ed2.doc.layer(col).unwrap().name, "Hull 'main'");
        assert!(ed2.doc.layer(1).unwrap().locked);
        assert_eq!(ed2.doc.active_layer, col);
        assert_eq!(ed2.doc.origin, v3(16.0, -8.0, 0.0));
        assert_eq!(ed2.doc.model_path, "housepack/onestory/house_1");
        assert_eq!(ed2.doc.mat_overrides, vec![("walls/brick".to_string(), "shader=UnlitGeneric;selfillum=1".to_string())]);
        assert_eq!(ed2.doc.brushes[0].faces[0].tex.shift, [3.5, -7.0]);
        assert_eq!(ed2.doc.brushes[0].faces[0].tex.scale, [0.5, 0.125]);
        assert!(!ed2.show_entities);
        assert_eq!(ed2.grid, 8.0);
        assert_eq!(ed2.cam.yaw, 12.5);
        assert_eq!(ed2.views[1].zoom, 2.0);
        assert!(!ed2.doc.dirty);
        // new brushes get fresh ids after loading
        let n = ed2.doc.add_brush(v3(0.0, 0.0, 0.0), v3(1.0, 1.0, 1.0));
        assert!(ed2.doc.brushes.iter().filter(|b| b.id == n).count() == 1);
    }

    #[test]
    fn format_1_files_still_load() {
        let text = r#"
            "BonkModel"
            {
                "format" "1"
                "brush"
                {
                    "id" "3"
                    "v" "0 0 0"  "v" "32 0 0"  "v" "0 32 0"  "v" "0 0 32"
                    "f" "0 2 1"  "f" "0 1 3"  "f" "0 3 2"  "f" "1 2 3"
                }
            }"#;
        let mut ed = Editor::new();
        apply(&mut ed, read(text).unwrap());
        assert_eq!(ed.doc.layers.len(), 1);
        assert_eq!(ed.doc.brushes[0].layer, ed.doc.layers[0].id);
        assert_eq!(ed.doc.origin, V3::default());
    }

    #[test]
    fn rejects_garbage_and_newer_formats() {
        assert!(read("hello").is_err());
        assert!(read("\"BonkModel\" { \"format\" \"99\" }").unwrap_err().contains("newer"));
        assert!(read("\"BonkModel\" { \"brush\" { \"v\" \"0 0\" } }").is_err());
        assert!(read("\"BonkModel\" {").is_err());
    }

    #[test]
    fn rebuilds_brushes_with_bad_faces() {
        let text = r#"
            // hand-written: faces missing, should be rebuilt from vertices
            "BonkModel"
            {
                "format" "1"
                "brush"
                {
                    "id" "7"
                    "v" "0 0 0"  "v" "32 0 0"  "v" "0 32 0"  "v" "0 0 32"
                }
            }"#;
        let l = read(text).unwrap();
        assert_eq!(l.repaired, 1);
        assert_eq!(l.brushes[0].id, 7);
        assert_eq!(l.brushes[0].faces.len(), 4);
    }

    #[test]
    fn legacy_none_faces_become_nodraw() {
        let mut ed = Editor::new();
        let id = ed.doc.add_brush(v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0));
        for f in &mut ed.doc.brush_mut(id).unwrap().faces {
            f.material = String::new();
        }
        ed.doc.brush_mut(id).unwrap().faces[0].material = "walls/brick".into();
        let text = write(&ed);
        let mut ed2 = Editor::new();
        assert_eq!(apply(&mut ed2, read(&text).unwrap()), 5);
        let b = &ed2.doc.brushes[0];
        assert_eq!(b.faces.iter().filter(|f| f.material == geom::NODRAW).count(), 5);
        assert_eq!(b.faces.iter().filter(|f| f.material == "walls/brick").count(), 1);
        assert!(!ed2.doc.dirty, "converting doesn't make the file dirty");
    }

    #[test]
    fn save_and_load_files_and_dirty_flag() {
        let dir = std::env::temp_dir().join(format!("bonk_io_test_{}", std::process::id()));
        std::fs::create_dir_all(&dir).unwrap();
        let path = dir.join("crate.bpmdl");
        let path = path.to_str().unwrap();
        let mut ed = Editor::new();
        ed.doc.checkpoint();
        ed.doc.add_brush(v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0));
        assert!(ed.doc.dirty);
        save_file(&mut ed, path).unwrap();
        assert!(!ed.doc.dirty);
        let mut ed2 = Editor::new();
        ed2.command(cmd::NEW);
        load_file(&mut ed2, path).unwrap();
        assert_eq!(ed2.doc.brushes, ed.doc.brushes);
        assert!(load_file(&mut ed2, "/nonexistent/x.bpmdl").is_err());
        std::fs::remove_dir_all(&dir).unwrap();
    }
}
