// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Steventechno and the Bonk! contributors
//! Hammer VMF import: brushes with their texture alignment, Hammer groups
//! (as Bonk groups) and visgroups (as layers). Tool textures map onto Bonk's
//! ideas: nodraw stays nodraw, clip brushes become the collision layer, and
//! map-only tools (skybox, hint, triggers…) are left out. Displacements and
//! point entities are skipped and counted.

use crate::doc::{Brush, Document, Group, Layer, LayerKind};
use crate::geom::{self, TexMap};
use crate::math::*;

// ------------------------------------------------------------------ KeyValues

#[derive(Debug, Default, Clone)]
pub struct Node {
    pub name: String,
    pub kv: Vec<(String, String)>,
    pub children: Vec<Node>,
}

impl Node {
    pub fn get(&self, k: &str) -> Option<&str> {
        self.kv.iter().find(|p| p.0.eq_ignore_ascii_case(k)).map(|p| p.1.as_str())
    }
    pub fn kids<'a>(&'a self, name: &'a str) -> impl Iterator<Item = &'a Node> + 'a {
        self.children.iter().filter(move |c| c.name.eq_ignore_ascii_case(name))
    }
    pub fn kid(&self, name: &str) -> Option<&Node> {
        self.children.iter().find(|c| c.name.eq_ignore_ascii_case(name))
    }
}

fn tokens(text: &str) -> Vec<String> {
    let mut out = Vec::new();
    let b = text.as_bytes();
    let mut i = 0;
    while i < b.len() {
        let c = b[i];
        if c.is_ascii_whitespace() {
            i += 1;
        } else if c == b'/' && b.get(i + 1) == Some(&b'/') {
            while i < b.len() && b[i] != b'\n' {
                i += 1;
            }
        } else if c == b'{' || c == b'}' {
            out.push((c as char).to_string());
            i += 1;
        } else if c == b'"' {
            let s = i + 1;
            i += 1;
            while i < b.len() && b[i] != b'"' {
                i += 1;
            }
            out.push(String::from_utf8_lossy(&b[s..i.min(b.len())]).into_owned());
            i += 1;
        } else {
            let s = i;
            while i < b.len() && !b[i].is_ascii_whitespace() && b[i] != b'{' && b[i] != b'}' && b[i] != b'"' {
                i += 1;
            }
            out.push(String::from_utf8_lossy(&b[s..i]).into_owned());
        }
    }
    out
}

/// Parse KeyValues text (VMF, VMT) into a root node holding the top-level blocks.
pub fn parse(text: &str) -> Node {
    let t = tokens(text);
    let mut stack = vec![Node::default()];
    let mut i = 0;
    while i < t.len() {
        if t[i] == "}" {
            if stack.len() > 1 {
                let n = stack.pop().unwrap();
                stack.last_mut().unwrap().children.push(n);
            }
            i += 1;
        } else if t.get(i + 1).is_some_and(|x| x == "{") {
            stack.push(Node { name: t[i].clone(), ..Default::default() });
            i += 2;
        } else if t[i] == "{" {
            stack.push(Node::default());
            i += 1;
        } else if let Some(v) = t.get(i + 1) {
            stack.last_mut().unwrap().kv.push((t[i].clone(), v.clone()));
            i += 2;
        } else {
            i += 1;
        }
    }
    while stack.len() > 1 {
        let n = stack.pop().unwrap();
        stack.last_mut().unwrap().children.push(n);
    }
    stack.pop().unwrap()
}

// ------------------------------------------------------------------ geometry

type D3 = [f64; 3];

fn nums(s: &str) -> Vec<f64> {
    s.split(|c: char| c == '(' || c == ')' || c == '[' || c == ']' || c.is_whitespace())
        .filter(|x| !x.is_empty())
        .filter_map(|x| x.parse().ok())
        .collect()
}

fn sub(a: D3, b: D3) -> D3 {
    [a[0] - b[0], a[1] - b[1], a[2] - b[2]]
}
fn dot(a: D3, b: D3) -> f64 {
    a[0] * b[0] + a[1] * b[1] + a[2] * b[2]
}
fn cross(a: D3, b: D3) -> D3 {
    [a[1] * b[2] - a[2] * b[1], a[2] * b[0] - a[0] * b[2], a[0] * b[1] - a[1] * b[0]]
}

/// A side's plane: outward normal and distance, from its three points.
fn plane(s: &str) -> Option<(D3, f64)> {
    let v = nums(s);
    if v.len() < 9 {
        return None;
    }
    let (p0, p1, p2) = ([v[0], v[1], v[2]], [v[3], v[4], v[5]], [v[6], v[7], v[8]]);
    let n = cross(sub(p0, p1), sub(p2, p1));
    let l = dot(n, n).sqrt();
    if l < 1e-9 {
        return None;
    }
    let n = [n[0] / l, n[1] / l, n[2] / l];
    Some((n, dot(n, p0)))
}

/// Corners of the convex solid bounded by `planes` (n·p <= d).
fn corners(planes: &[(D3, f64)]) -> Vec<V3> {
    let mut pts: Vec<D3> = Vec::new();
    let n = planes.len();
    for i in 0..n {
        for j in i + 1..n {
            for k in j + 1..n {
                let (a, b, c) = (planes[i], planes[j], planes[k]);
                let bc = cross(b.0, c.0);
                let det = dot(a.0, bc);
                if det.abs() < 1e-9 {
                    continue;
                }
                let ca = cross(c.0, a.0);
                let ab = cross(a.0, b.0);
                let p = [
                    (a.1 * bc[0] + b.1 * ca[0] + c.1 * ab[0]) / det,
                    (a.1 * bc[1] + b.1 * ca[1] + c.1 * ab[1]) / det,
                    (a.1 * bc[2] + b.1 * ca[2] + c.1 * ab[2]) / det,
                ];
                if planes.iter().all(|q| dot(q.0, p) - q.1 <= 1e-3) && !pts.iter().any(|r| dot(sub(*r, p), sub(*r, p)) < 1e-6) {
                    pts.push(p);
                }
            }
        }
    }
    // Hammer works on a grid: snap what's within a hair of a whole unit
    pts.iter()
        .map(|p| {
            let s = |x: f64| if (x - x.round()).abs() < 1e-3 { x.round() } else { x };
            v3(s(p[0]) as f32, s(p[1]) as f32, s(p[2]) as f32)
        })
        .collect()
}

/// "[ux uy uz shift] scale"
fn axis(s: &str) -> Option<(V3, f32, f32)> {
    let v = nums(s);
    if v.len() < 5 {
        return None;
    }
    Some((v3(v[0] as f32, v[1] as f32, v[2] as f32), v[3] as f32, v[4] as f32))
}

// ------------------------------------------------------------------ tool textures

/// What a material means when it's on a brush in a map.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
enum Tool {
    /// An ordinary material
    None,
    /// Never drawn, but part of the brush (nodraw, invisible…)
    Nodraw,
    /// Collision only (clips)
    Clip,
    /// Map-only (skybox, hint, triggers…): the brush means nothing on a prop
    Skip,
}

fn tool_of(m: &str) -> Tool {
    let m = m.to_ascii_lowercase();
    let Some(t) = m.strip_prefix("tools/") else { return Tool::None };
    match t {
        "toolsnodraw" | "toolsinvisible" | "toolsblocklight" | "toolsblock_los" | "toolsblockbullets" => Tool::Nodraw,
        "toolsclip" | "toolsplayerclip" | "toolsnpcclip" | "toolsgrenadeclip" | "toolsinvisibleladder" | "toolsclip_vphysics" => Tool::Clip,
        "toolsskybox" | "toolsskybox2d" | "toolshint" | "toolsskip" | "toolstrigger" | "toolsareaportal" | "toolsoccluder" | "toolsfog"
        | "toolsskyfog" | "toolsdotted" | "toolsblack_noportal" => Tool::Skip,
        _ => Tool::None,
    }
}

/// Brush entities that are map logic, not shapes.
fn skipped_entity(class: &str) -> bool {
    let c = class.to_ascii_lowercase();
    c.starts_with("trigger_")
        || matches!(
            c.as_str(),
            "func_areaportal" | "func_areaportalwindow" | "func_occluder" | "func_viscluster" | "func_vehicleclip" | "func_ladder"
                | "func_nobuild" | "func_precipitation" | "func_dustcloud" | "func_dustmotes" | "func_smokevolume" | "func_bomb_target"
                | "func_buyzone" | "func_hostage_rescue" | "func_respawnroom" | "func_regenerate" | "func_nogrenades" | "func_no_build"
        )
}

// ------------------------------------------------------------------ scan + import

/// One solid as found in the file, before building.
struct Solid<'a> {
    node: &'a Node,
    entity: Option<&'a Node>,
    visgroup: Option<u32>,
    group: Option<u32>,
}

fn editor_u32(n: &Node, key: &str) -> Option<u32> {
    n.kid("editor").and_then(|e| e.get(key)).and_then(|v| v.trim().parse().ok())
}

fn solids(root: &Node) -> Vec<Solid<'_>> {
    let mut out = Vec::new();
    fn walk<'a>(n: &'a Node, ent: Option<&'a Node>, out: &mut Vec<Solid<'a>>) {
        for c in &n.children {
            let lname = c.name.to_ascii_lowercase();
            if lname == "solid" {
                // an entity's own grouping applies to its solids
                let owner = ent.unwrap_or(c);
                out.push(Solid {
                    node: c,
                    entity: ent,
                    visgroup: editor_u32(c, "visgroupid").or_else(|| ent.and_then(|e| editor_u32(e, "visgroupid"))),
                    group: editor_u32(c, "groupid").or_else(|| editor_u32(owner, "groupid")),
                });
            } else if lname == "hidden" {
                walk(c, ent, out);
            }
        }
    }
    for top in &root.children {
        match top.name.to_ascii_lowercase().as_str() {
            "world" => walk(top, None, &mut out),
            "entity" => walk(top, Some(top), &mut out),
            "hidden" => {
                for e in top.kids("entity") {
                    walk(e, Some(e), &mut out);
                }
                walk(top, None, &mut out);
            }
            _ => {}
        }
    }
    out
}

#[derive(Debug, Default, Clone, PartialEq)]
pub struct Visgroup {
    pub id: u32,
    pub name: String,
    pub solids: usize,
}

/// What's in a VMF, for the import dialog.
#[derive(Debug, Default, Clone)]
pub struct Scan {
    pub solids: usize,
    pub visgroups: Vec<Visgroup>,
    /// Materials on the brushes that would come in (lowercase), most-used first
    pub materials: Vec<(String, usize)>,
    pub point_entities: usize,
}

fn visgroup_list(root: &Node) -> Vec<Visgroup> {
    fn walk(n: &Node, prefix: &str, out: &mut Vec<Visgroup>) {
        for v in n.kids("visgroup") {
            let name = v.get("name").unwrap_or("Visgroup").to_string();
            let full = if prefix.is_empty() { name.clone() } else { format!("{prefix} / {name}") };
            if let Some(id) = v.get("visgroupid").and_then(|x| x.trim().parse().ok()) {
                out.push(Visgroup { id, name: full.clone(), solids: 0 });
            }
            walk(v, &full, out);
        }
    }
    let mut out = Vec::new();
    for vg in root.kids("visgroups") {
        walk(vg, "", &mut out);
    }
    out
}

pub fn scan(text: &str) -> Scan {
    let root = parse(text);
    let mut s = Scan { visgroups: visgroup_list(&root), ..Default::default() };
    for e in root.kids("entity") {
        if !e.children.iter().any(|c| c.name.eq_ignore_ascii_case("solid")) {
            s.point_entities += 1;
        }
    }
    for so in solids(&root) {
        s.solids += 1;
        if let Some(v) = so.visgroup.and_then(|id| s.visgroups.iter_mut().find(|g| g.id == id)) {
            v.solids += 1;
        }
        if so.entity.is_some_and(|e| skipped_entity(e.get("classname").unwrap_or(""))) {
            continue;
        }
        for side in so.node.kids("side") {
            let m = side.get("material").unwrap_or("").to_ascii_lowercase();
            if m.is_empty() || tool_of(&m) != Tool::None {
                continue;
            }
            match s.materials.iter_mut().find(|x| x.0 == m) {
                Some(x) => x.1 += 1,
                None => s.materials.push((m, 1)),
            }
        }
    }
    s.materials.sort_by(|a, b| b.1.cmp(&a.1).then(a.0.cmp(&b.0)));
    s
}

#[derive(Debug, Default, Clone, PartialEq)]
pub struct Report {
    pub brushes: usize,
    pub collision: usize,
    pub skipped_tools: usize,
    pub skipped_entities: usize,
    pub displacements: usize,
    pub point_entities: usize,
    pub broken: usize,
    pub excluded: usize,
    pub groups: usize,
}

/// Build a document from VMF text. `skip_visgroups`: visgroup ids left out.
pub fn import(text: &str, skip_visgroups: &[u32]) -> Result<(Document, Report), String> {
    let root = parse(text);
    if root.kid("world").is_none() {
        return Err("this isn't a Hammer map (no world block)".into());
    }
    let visgroups = visgroup_list(&root);
    let mut rep = Report { point_entities: scan_points(&root), ..Default::default() };
    let mut doc = Document::new();
    // layers: one per visgroup that ends up with brushes, else "Visual 1"
    let mut layers: Vec<Layer> = vec![Layer { id: 1, name: "Visual 1".into(), kind: LayerKind::Visual, visible: true, locked: false, origin: None }];
    let col_layer = 2u32;
    let mut vis_layer: Vec<(u32, u32)> = Vec::new(); // visgroup id → layer id
    let mut built: Vec<(Brush, Option<u32>)> = Vec::new(); // brush, vmf group id
    let mut entity_groups: Vec<(usize, Vec<usize>)> = Vec::new(); // entity node address, indices into built
    let mut has_collision = false;
    for so in solids(&root) {
        if so.visgroup.is_some_and(|v| skip_visgroups.contains(&v)) {
            rep.excluded += 1;
            continue;
        }
        let class = so.entity.and_then(|e| e.get("classname")).unwrap_or("worldspawn").to_ascii_lowercase();
        if skipped_entity(&class) {
            rep.skipped_entities += 1;
            continue;
        }
        let sides: Vec<&Node> = so.node.kids("side").collect();
        if sides.iter().any(|s| s.kid("dispinfo").is_some()) {
            rep.displacements += 1;
            continue;
        }
        let tools: Vec<Tool> = sides.iter().map(|s| tool_of(s.get("material").unwrap_or(""))).collect();
        let all_tools = tools.iter().all(|t| *t != Tool::None);
        if all_tools && tools.contains(&Tool::Skip) && !tools.contains(&Tool::Clip) {
            rep.skipped_tools += 1;
            continue;
        }
        let collision = class == "func_clip_vphysics" || (all_tools && tools.contains(&Tool::Clip));
        let mut planes = Vec::new();
        for s in &sides {
            match s.get("plane").and_then(plane) {
                Some(p) => planes.push(p),
                None => {}
            }
        }
        let pts = corners(&planes);
        let Some(h) = geom::hull(&pts) else {
            rep.broken += 1;
            continue;
        };
        let mut b = Brush { id: 0, verts: h.verts, faces: h.faces, color: [1.0; 3], rot: [0.0; 3], layer: 1, entity: None, group: None };
        for f in &mut b.faces {
            // the side whose plane this face lies on
            let fnn = [f.normal.x as f64, f.normal.y as f64, f.normal.z as f64];
            let best = sides
                .iter()
                .zip(&planes)
                .zip(&tools)
                .map(|((s, p), t)| (dot(p.0, fnn) - (p.1 - f.dist as f64).abs() * 1e-2, s, t))
                .max_by(|a, b| a.0.partial_cmp(&b.0).unwrap());
            let Some((score, side, tool)) = best else { continue };
            if score < 0.9 {
                continue;
            }
            f.material = match tool {
                Tool::None => side.get("material").unwrap_or("").to_ascii_lowercase(),
                _ => geom::NODRAW.to_string(),
            };
            let rot = side.get("rotation").and_then(|r| r.trim().parse::<f32>().ok()).unwrap_or(0.0);
            if let (Some(u), Some(v)) = (side.get("uaxis").and_then(axis), side.get("vaxis").and_then(axis)) {
                let t = TexMap { u: u.0, v: v.0, shift: [u.1, v.1], scale: [u.2, v.2], rot };
                f.tex = t.or_default_for(f.normal);
            }
        }
        if collision {
            b.layer = col_layer;
            has_collision = true;
            rep.collision += 1;
        } else if let Some(vg) = so.visgroup.and_then(|id| visgroups.iter().find(|g| g.id == id)) {
            let lid = match vis_layer.iter().find(|x| x.0 == vg.id) {
                Some(x) => x.1,
                None => {
                    let lid = 3 + vis_layer.len() as u32;
                    vis_layer.push((vg.id, lid));
                    layers.push(Layer { id: lid, name: vg.name.clone(), kind: LayerKind::Visual, visible: true, locked: false, origin: None });
                    lid
                }
            };
            b.layer = lid;
        }
        rep.brushes += 1;
        if let Some(e) = so.entity {
            let addr = e as *const Node as usize;
            if class != "func_detail" {
                match entity_groups.iter_mut().find(|x| x.0 == addr) {
                    Some(x) => x.1.push(built.len()),
                    None => entity_groups.push((addr, vec![built.len()])),
                }
            }
        }
        built.push((b, so.group));
    }
    if built.is_empty() {
        return Err("nothing to import: no usable brushes in this map".into());
    }
    // Move it so its bottom center sits on the model origin.
    let (mut lo, mut hi) = built[0].0.bounds();
    for (b, _) in &built {
        let (l, h) = b.bounds();
        lo = lo.min(l);
        hi = hi.max(h);
    }
    let off = v3(((lo.x + hi.x) * 0.5).round(), ((lo.y + hi.y) * 0.5).round(), lo.z.round());
    for (b, _) in built.iter_mut() {
        for p in b.verts.iter_mut() {
            *p = *p - off;
        }
        for f in b.faces.iter_mut() {
            f.tex = f.tex.translated(off * -1.0);
        }
        b.recompute_planes();
    }
    if has_collision {
        // hidden at first: clip hulls usually wrap the whole thing
        layers.push(Layer { id: col_layer, name: "Collision".into(), kind: LayerKind::Collision, visible: false, locked: false, origin: None });
    }
    // Layer 1 only if something is on it
    if !built.iter().any(|(b, _)| b.layer == 1) && layers.len() > 1 {
        layers.retain(|l| l.id != 1);
    }
    doc.replace_all(Vec::new(), layers, 1, V3::default());
    // Hammer groups (nested through their editor groupid) → Bonk groups
    let mut gmap: Vec<(u32, u32)> = Vec::new(); // vmf id → bonk id
    let world = root.kid("world").unwrap();
    let vmf_groups: Vec<(u32, Option<u32>)> = world
        .kids("group")
        .filter_map(|g| Some((g.get("id")?.trim().parse().ok()?, editor_u32(g, "groupid"))))
        .collect();
    let mut next_gid = 1u32;
    for (vid, _) in &vmf_groups {
        gmap.push((*vid, next_gid));
        next_gid += 1;
    }
    let mut groups: Vec<Group> = Vec::new();
    for (i, (vid, parent)) in vmf_groups.iter().enumerate() {
        let id = gmap.iter().find(|x| x.0 == *vid).unwrap().1;
        let parent = parent.and_then(|p| gmap.iter().find(|x| x.0 == p).map(|x| x.1));
        groups.push(Group { id, name: format!("Group {}", i + 1), parent, bodygroup: None });
    }
    let mut ids = Vec::with_capacity(built.len());
    let mut entity_of: Vec<Option<u32>> = vec![None; built.len()];
    for (k, (_, members)) in entity_groups.iter().enumerate() {
        if members.len() < 2 {
            continue;
        }
        let gid = next_gid;
        next_gid += 1;
        // the entity's own group sits inside the Hammer group its first solid was in
        let parent = built[members[0]].1.and_then(|g| gmap.iter().find(|x| x.0 == g).map(|x| x.1));
        groups.push(Group { id: gid, name: format!("Entity {}", k + 1), parent, bodygroup: None });
        for &m in members {
            entity_of[m] = Some(gid);
        }
    }
    for (i, (b, g)) in built.into_iter().enumerate() {
        let layer = b.layer;
        let id = doc.add_piece(b);
        let bb = doc.brush_mut(id).unwrap();
        bb.layer = layer;
        bb.group = entity_of[i].or_else(|| g.and_then(|g| gmap.iter().find(|x| x.0 == g).map(|x| x.1)));
        ids.push(id);
    }
    doc.groups = groups;
    doc.prune_groups();
    collapse_wrapper_groups(&mut doc);
    rep.groups = doc.groups.len();
    doc.dirty = true;
    Ok((doc, rep))
}

/// Hammer maps often have groups wrapped in groups wrapped in groups (pressing
/// Ctrl+G on something already grouped). A group holding nothing but one other
/// group adds nothing: dissolve it, then number what's left "Group 1", "Group 2"…
/// A group that holds no brushes of its own and exactly one sub-group.
pub fn wrapper_group(doc: &Document) -> Option<u32> {
    use std::collections::{HashMap, HashSet};
    let direct: HashSet<u32> = doc.brushes.iter().filter_map(|b| b.group).collect();
    let mut kids: HashMap<u32, usize> = HashMap::new();
    for g in &doc.groups {
        if let Some(p) = g.parent {
            *kids.entry(p).or_default() += 1;
        }
    }
    doc.groups.iter().map(|g| g.id).find(|g| !direct.contains(g) && kids.get(g) == Some(&1))
}

pub fn collapse_wrapper_groups(doc: &mut Document) {
    loop {
        let wrapper = wrapper_group(doc);
        match wrapper {
            Some(g) => doc.dissolve_group(g),
            None => break,
        }
    }
    let mut n = 0;
    for g in doc.groups.iter_mut() {
        if g.name.starts_with("Group ") {
            n += 1;
            g.name = format!("Group {n}");
        }
    }
}

fn scan_points(root: &Node) -> usize {
    root.kids("entity").filter(|e| !e.children.iter().any(|c| c.name.eq_ignore_ascii_case("solid"))).count()
}

// ------------------------------------------------------------------ VMT

/// The parts of a VMT that matter for importing: shader and parameters
/// (lowercase keys), proxies left out.
#[derive(Debug, Default, Clone, PartialEq)]
pub struct Vmt {
    pub shader: String,
    pub params: Vec<(String, String)>,
}

impl Vmt {
    #[allow(dead_code)]
    pub fn get(&self, k: &str) -> Option<&str> {
        self.params.iter().find(|p| p.0 == k).map(|p| p.1.as_str())
    }
}

pub fn parse_vmt(text: &str) -> Option<Vmt> {
    let root = parse(text);
    let top = root.children.first()?;
    let mut v = Vmt { shader: top.name.to_ascii_lowercase(), params: Vec::new() };
    let add = |n: &Node, v: &mut Vmt| {
        for (k, val) in &n.kv {
            let k = k.to_ascii_lowercase();
            if !v.params.iter().any(|p| p.0 == k) {
                v.params.push((k, val.clone()));
            }
        }
    };
    if v.shader == "patch" {
        // Patch materials: "insert"/"replace" blocks override the included material
        for blk in top.children.iter().filter(|c| c.name.eq_ignore_ascii_case("insert") || c.name.eq_ignore_ascii_case("replace")) {
            add(blk, &mut v);
        }
        if let Some(inc) = top.get("include") {
            v.params.push(("include".into(), inc.to_string()));
        }
    } else {
        add(top, &mut v);
        // fallback blocks like "GPU>=2?..." aren't needed; the main body is
    }
    Some(v)
}

/// Normalize a texture path as VMTs write them ("Brick\\Wall.vtf" → "brick/wall").
pub fn clean_path(p: &str) -> String {
    let mut s = p.trim().replace('\\', "/").to_ascii_lowercase();
    while s.contains("//") {
        s = s.replace("//", "/");
    }
    if let Some(x) = s.strip_prefix("materials/") {
        s = x.to_string();
    }
    if let Some(x) = s.strip_suffix(".vtf").or_else(|| s.strip_suffix(".vmt")) {
        s = x.to_string();
    }
    s.trim_matches('/').to_string()
}

#[cfg(test)]
mod tests {
    use super::*;

    const SIDE: &str = r#"
        side
        {
            "id" "{ID}"
            "plane" "{P}"
            "material" "{M}"
            "uaxis" "[1 0 0 0] 0.25"
            "vaxis" "[0 -1 0 0] 0.25"
            "rotation" "0"
            "lightmapscale" "16"
            "smoothing_groups" "0"
        }"#;

    /// A box solid in VMF form, Hammer's plane order, with per-side materials.
    fn box_solid(id: u32, lo: [i32; 3], hi: [i32; 3], mats: [&str; 6], editor: &str) -> String {
        let (x0, y0, z0, x1, y1, z1) = (lo[0], lo[1], lo[2], hi[0], hi[1], hi[2]);
        let planes = [
            format!("({x0} {y1} {z1}) ({x1} {y1} {z1}) ({x1} {y0} {z1})"), // top
            format!("({x0} {y0} {z0}) ({x1} {y0} {z0}) ({x1} {y1} {z0})"), // bottom
            format!("({x0} {y1} {z1}) ({x0} {y0} {z1}) ({x0} {y0} {z0})"), // -x
            format!("({x1} {y1} {z0}) ({x1} {y0} {z0}) ({x1} {y0} {z1})"), // +x
            format!("({x1} {y1} {z1}) ({x0} {y1} {z1}) ({x0} {y1} {z0})"), // +y
            format!("({x1} {y0} {z0}) ({x0} {y0} {z0}) ({x0} {y0} {z1})"), // -y
        ];
        let mut s = format!("solid\n{{\n\"id\" \"{id}\"\n");
        for (i, (p, m)) in planes.iter().zip(mats).enumerate() {
            s += &SIDE.replace("{ID}", &(id * 10 + i as u32).to_string()).replace("{P}", p).replace("{M}", m);
        }
        s += &format!("\neditor\n{{\n\"color\" \"0 180 0\"\n{editor}\n}}\n}}\n");
        s
    }

    fn sample() -> String {
        let b = ["BRICK/BRICKWALL001A"; 6];
        let mut t = String::from("versioninfo\n{\n\"editorversion\" \"400\"\n}\nvisgroups\n{\nvisgroup\n{\n\"name\" \"Props\"\n\"visgroupid\" \"7\"\n}\nvisgroup\n{\n\"name\" \"Junk\"\n\"visgroupid\" \"8\"\n}\n}\nworld\n{\n\"id\" \"1\"\n\"classname\" \"worldspawn\"\n");
        // a 64 cube far from the origin, in a Hammer group
        let mut m = b;
        m[1] = "TOOLS/TOOLSNODRAW";
        t += &box_solid(2, [1000, 1000, 64], [1064, 1064, 128], m, "\"groupid\" \"50\"");
        // a second brush in the same group, on visgroup Props
        t += &box_solid(3, [1064, 1000, 64], [1128, 1064, 96], ["CONCRETE/CONCRETEFLOOR001A"; 6], "\"groupid\" \"50\"\n\"visgroupid\" \"7\"");
        // a clip brush: collision
        t += &box_solid(4, [1000, 1000, 64], [1128, 1064, 128], ["TOOLS/TOOLSPLAYERCLIP"; 6], "");
        // a skybox brush: skipped
        t += &box_solid(5, [0, 0, 1000], [64, 64, 1064], ["TOOLS/TOOLSSKYBOX"; 6], "");
        // on visgroup Junk
        t += &box_solid(6, [1000, 1000, 200], [1016, 1016, 216], b, "\"visgroupid\" \"8\"");
        // hidden solid
        t += &format!("hidden\n{{\n{}\n}}\n", box_solid(9, [1000, 1100, 64], [1016, 1116, 80], b, ""));
        // the group
        t += "group\n{\n\"id\" \"50\"\neditor\n{\n\"color\" \"0 100 0\"\n}\n}\n}\n";
        // brush entity with two solids, a trigger, a point entity
        t += &format!("entity\n{{\n\"id\" \"20\"\n\"classname\" \"func_brush\"\n{}{}editor\n{{\n}}\n}}\n",
            box_solid(21, [900, 1000, 64], [932, 1032, 96], b, ""), box_solid(22, [900, 1032, 64], [932, 1064, 96], b, ""));
        t += &format!("entity\n{{\n\"id\" \"30\"\n\"classname\" \"trigger_multiple\"\n{}}}\n", box_solid(31, [0, 0, 0], [64, 64, 64], ["TOOLS/TOOLSTRIGGER"; 6], ""));
        t += "entity\n{\n\"id\" \"40\"\n\"classname\" \"info_player_start\"\n\"origin\" \"0 0 0\"\n}\n";
        // a displacement
        t += &format!("entity\n{{\n\"classname\" \"func_detail\"\n{}}}\n",
            box_solid(60, [2000, 0, 0], [2064, 64, 16], b, "").replacen("\"smoothing_groups\" \"0\"", "\"smoothing_groups\" \"0\"\ndispinfo\n{\n\"power\" \"2\"\n}", 1));
        t
    }

    #[test]
    fn keyvalues_and_scan() {
        let n = parse("a { \"b\" \"c\" // comment\n d { e f } }");
        let a = n.kid("a").unwrap();
        assert_eq!(a.get("b"), Some("c"));
        assert_eq!(a.kid("d").unwrap().get("e"), Some("f"));
        let s = scan(&sample());
        assert_eq!(s.visgroups.iter().map(|v| (v.name.as_str(), v.solids)).collect::<Vec<_>>(), vec![("Props", 1), ("Junk", 1)]);
        assert_eq!(s.point_entities, 1);
        assert_eq!(s.materials[0].0, "brick/brickwall001a");
        assert!(s.materials.iter().any(|m| m.0 == "concrete/concretefloor001a"));
        assert!(!s.materials.iter().any(|m| m.0.starts_with("tools/")));
    }

    #[test]
    fn import_builds_brushes_layers_groups() {
        let (doc, rep) = import(&sample(), &[8]).unwrap();
        // 2 world + clip + hidden + 2 entity solids (Junk excluded, sky/trigger/disp skipped)
        assert_eq!(rep.brushes, 6, "{rep:?}");
        assert_eq!(rep.collision, 1);
        assert_eq!(rep.skipped_tools, 1);
        assert_eq!(rep.skipped_entities, 1);
        assert_eq!(rep.displacements, 1);
        assert_eq!(rep.point_entities, 1);
        assert_eq!(rep.excluded, 1);
        // layers: Visual 1, Props, Collision
        let names: Vec<&str> = doc.layers.iter().map(|l| l.name.as_str()).collect();
        assert!(names.contains(&"Visual 1") && names.contains(&"Props") && names.contains(&"Collision"), "{names:?}");
        let col = doc.layers.iter().find(|l| l.kind == LayerKind::Collision).unwrap().id;
        assert_eq!(doc.brushes.iter().filter(|b| b.layer == col).count(), 1);
        // the cube: 5 brick faces + nodraw bottom, textures aligned as in Hammer
        let cube = doc.brushes.iter().find(|b| b.faces.iter().filter(|f| f.material == "brick/brickwall001a").count() == 5).unwrap();
        let bottom = cube.faces.iter().find(|f| f.normal.z < -0.9).unwrap();
        assert_eq!(bottom.material, geom::NODRAW);
        let top = cube.faces.iter().find(|f| f.normal.z > 0.9).unwrap();
        assert_eq!(top.tex.u, v3(1.0, 0.0, 0.0));
        assert_eq!(top.tex.scale, [0.25, 0.25]);
        // the texture stays where it was on the brush after the move
        let (lo, _) = cube.bounds();
        let p_world = v3(1000.0, 1000.0, 128.0);
        let p_here = v3(lo.x, lo.y, cube.bounds().1.z);
        let t0 = TexMap { u: v3(1.0, 0.0, 0.0), v: v3(0.0, -1.0, 0.0), shift: [0.0, 0.0], scale: [0.25, 0.25], rot: 0.0 };
        let (a, b) = (t0.uv(p_world), top.tex.uv(p_here));
        assert!((a.0 - b.0).abs() < 1e-2 && (a.1 - b.1).abs() < 1e-2);
        // moved: bottom center on the origin
        let (mut lo, mut hi) = doc.brushes[0].bounds();
        for b in &doc.brushes {
            lo = lo.min(b.bounds().0);
            hi = hi.max(b.bounds().1);
        }
        assert_eq!(lo.z, 0.0);
        assert!(((lo.x + hi.x) * 0.5).abs() <= 0.5 && ((lo.y + hi.y) * 0.5).abs() <= 0.5);
        // groups: the Hammer group (2 brushes) and the func_brush entity (2 solids)
        assert_eq!(doc.groups.len(), 2);
        for g in &doc.groups {
            assert_eq!(doc.brushes.iter().filter(|b| b.group == Some(g.id)).count(), 2);
        }
        // nothing has the old blank material
        assert!(doc.brushes.iter().all(|b| b.faces.iter().all(|f| !f.material.is_empty())));
    }

    #[test]
    fn wrapper_groups_collapse() {
        let b = ["BRICK/A"; 6];
        let mut t = String::from("world\n{\n");
        // brushes 2,3 in group 13; 13 inside 12 inside 11 inside 10 (wrappers); 12 also holds brush 4
        t += &box_solid(2, [0, 0, 0], [16, 16, 16], b, "\"groupid\" \"13\"");
        t += &box_solid(3, [16, 0, 0], [32, 16, 16], b, "\"groupid\" \"13\"");
        t += &box_solid(4, [32, 0, 0], [48, 16, 16], b, "\"groupid\" \"12\"");
        t += "group\n{\n\"id\" \"10\"\neditor\n{\n}\n}\n";
        t += "group\n{\n\"id\" \"11\"\neditor\n{\n\"groupid\" \"10\"\n}\n}\n";
        t += "group\n{\n\"id\" \"12\"\neditor\n{\n\"groupid\" \"11\"\n}\n}\n";
        t += "group\n{\n\"id\" \"13\"\neditor\n{\n\"groupid\" \"12\"\n}\n}\n}\n";
        let (doc, rep) = import(&t, &[]).unwrap();
        assert_eq!(rep.groups, 2, "10 and 11 only wrapped 12");
        let outer = doc.groups.iter().find(|g| g.parent.is_none()).unwrap();
        let inner = doc.groups.iter().find(|g| g.parent == Some(outer.id)).unwrap();
        assert_eq!(doc.group_members(outer.id).len(), 3);
        assert_eq!(doc.group_members(inner.id).len(), 2);
        assert_eq!((outer.name.as_str(), inner.name.as_str()), ("Group 1", "Group 2"));
    }

    #[test]
    fn rotated_and_odd_planes() {
        // a wedge: 5 sides
        let t = "world\n{\nsolid\n{\n\
            side { \"plane\" \"(0 0 0) (0 64 0) (64 64 0)\" \"material\" \"A\" }\n\
            side { \"plane\" \"(0 0 0) (0 0 64) (0 64 64)\" \"material\" \"B\" }\n\
            side { \"plane\" \"(0 64 0) (0 64 64) (64 64 0)\" \"material\" \"C\" }\n\
            side { \"plane\" \"(64 0 0) (0 0 0) (64 64 0)\" \"material\" \"D\" }\n\
            side { \"plane\" \"(0 0 64) (64 0 0) (64 64 0)\" \"material\" \"E\" }\n\
            }\n}\n";
        // whichever winding these planes have, a bad one builds nothing rather than garbage
        let r = import(t, &[]);
        if let Ok((doc, _)) = r {
            for b in &doc.brushes {
                assert!(b.faces.len() >= 4);
            }
        }
        assert!(import("nothing here", &[]).is_err());
    }

    #[test]
    fn vmt_parsing() {
        let v = parse_vmt("\"LightmappedGeneric\"\n{\n\"$basetexture\" \"Brick\\\\BrickWall001a\"\n\"$surfaceprop\" \"brick\"\n\"Proxies\" { \"AnimatedTexture\" { } }\n}").unwrap();
        assert_eq!(v.shader, "lightmappedgeneric");
        assert_eq!(clean_path(v.get("$basetexture").unwrap()), "brick/brickwall001a");
        assert_eq!(v.get("$surfaceprop"), Some("brick"));
        let p = parse_vmt("patch { include \"materials/brick/a.vmt\" insert { \"$envmap\" \"env_cubemap\" } }").unwrap();
        assert_eq!(p.shader, "patch");
        assert_eq!(p.get("include"), Some("materials/brick/a.vmt"));
        assert_eq!(p.get("$envmap"), Some("env_cubemap"));
    }
}
