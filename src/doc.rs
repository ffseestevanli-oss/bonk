// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Steventechno and the Bonk! contributors
//! The document: convex brushes, entities, layers, the model origin, the
//! selection, and snapshot-based undo/redo.

use crate::tex::TexMap;
use crate::geom::{self, Face, Hull};
use crate::math::*;
use std::collections::{HashMap, HashSet};

/// Non-solid things placed in the scene. They are stored as box-shaped brushes
/// so selection, moving, layers, undo and saving all work the same way, but
/// they are drawn as their own shape and never exported.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum Entity {
    /// Scale reference: Source player hull, 32 × 32 × 72, origin at bottom center.
    Mannequin,
}

impl Entity {
    pub fn size(self) -> V3 {
        match self {
            Entity::Mannequin => v3(32.0, 32.0, 72.0),
        }
    }
    pub fn key(self) -> &'static str {
        match self {
            Entity::Mannequin => "mannequin",
        }
    }
    pub fn from_key(k: &str) -> Option<Entity> {
        match k {
            "mannequin" => Some(Entity::Mannequin),
            _ => None,
        }
    }
}

/// A convex solid: vertices plus planar faces (CCW from outside).
#[derive(Clone, Debug, PartialEq)]
pub struct Brush {
    pub id: u32,
    pub verts: Vec<V3>,
    pub faces: Vec<Face>,
    pub color: [f32; 3],
    /// Accumulated rotation in degrees about the X, Y and Z axes (from the
    /// rotate tools). Lets Alt+Q straighten a brush to the nearest 45°.
    /// For entities, rot[2] is the facing (yaw).
    pub rot: [f32; 3],
    /// Layer the brush lives on
    pub layer: u32,
    /// Set for entities (scale mannequin, …)
    pub entity: Option<Entity>,
    /// Innermost group this brush belongs to
    pub group: Option<u32>,
}

impl Brush {
    pub fn new_box(id: u32, lo: V3, hi: V3, color: [f32; 3]) -> Brush {
        let h = box_hull(lo, hi);
        Brush { id, verts: h.verts, faces: h.faces, color, rot: [0.0; 3], layer: 1, entity: None, group: None }
    }

    /// An entity standing with its bottom center at `base`.
    pub fn new_entity(id: u32, kind: Entity, base: V3, yaw: f32) -> Brush {
        let mut b = Brush::new_box(id, V3::default(), v3(1.0, 1.0, 1.0), ENTITY_COLOR);
        b.entity = Some(kind);
        b.rot = [0.0, 0.0, yaw];
        b.place_entity(base);
        b
    }

    /// Bottom center of the brush's bounds (an entity's origin).
    pub fn base(&self) -> V3 {
        let (lo, hi) = self.bounds();
        v3((lo.x + hi.x) * 0.5, (lo.y + hi.y) * 0.5, lo.z)
    }

    /// Re-place an entity's fixed-size box with its bottom center at `base`.
    pub fn place_entity(&mut self, base: V3) {
        let Some(kind) = self.entity else { return };
        let s = kind.size();
        let lo = v3(base.x - s.x * 0.5, base.y - s.y * 0.5, base.z);
        let h = box_hull(lo, lo + s);
        self.verts = h.verts;
        self.faces = h.faces;
    }

    pub fn bounds(&self) -> (V3, V3) {
        let mut lo = self.verts[0];
        let mut hi = self.verts[0];
        for &p in &self.verts {
            lo = lo.min(p);
            hi = hi.max(p);
        }
        (lo, hi)
    }

    /// Unique edges as vertex index pairs.
    pub fn edges(&self) -> Vec<(usize, usize)> {
        let mut e: Vec<(usize, usize)> = Vec::new();
        for f in &self.faces {
            for i in 0..f.idx.len() {
                let a = f.idx[i];
                let b = f.idx[(i + 1) % f.idx.len()];
                let k = (a.min(b), a.max(b));
                if !e.contains(&k) {
                    e.push(k);
                }
            }
        }
        e
    }

    /// Recompute face planes after the vertices were moved without changing
    /// the topology (translate / rotate / positive scale).
    pub fn recompute_planes(&mut self) {
        for f in &mut self.faces {
            let (n, d) = geom::newell(&self.verts, &f.idx);
            f.normal = n;
            f.dist = d;
        }
    }

    /// Replace the geometry with a freshly built hull, carrying each face's
    /// material over from the face it came from.
    pub fn set_hull(&mut self, h: Hull) {
        let mut faces = h.faces;
        carry_materials(&self.faces, &mut faces);
        self.verts = h.verts;
        self.faces = faces;
    }

    /// Rebuild from a new point set; returns false (and leaves the brush
    /// untouched) if the points don't form a valid convex solid.
    pub fn set_points(&mut self, pts: &[V3]) -> bool {
        match geom::hull(pts) {
            Some(h) if geom::all_points_are_corners(pts, &h) => {
                self.set_hull(h);
                true
            }
            _ => false,
        }
    }

    pub fn has_vertex(&self, p: V3) -> bool {
        self.verts.iter().any(|&q| (q - p).len() < 1e-3)
    }
}

pub const ENTITY_COLOR: [f32; 3] = [0.35, 0.95, 0.45];

fn box_hull(lo: V3, hi: V3) -> Hull {
    let (lo, hi) = (lo.min(hi), lo.max(hi));
    let corners: Vec<V3> = (0..8)
        .map(|i| v3(if i & 1 != 0 { hi.x } else { lo.x }, if i & 2 != 0 { hi.y } else { lo.y }, if i & 4 != 0 { hi.z } else { lo.z }))
        .collect();
    geom::hull(&corners).expect("box must have volume")
}

/// Give every new face the material of the old face it most resembles
/// (same direction, nearest plane). Faces with no counterpart get the most
/// common material of the old faces.
pub fn carry_materials(old: &[Face], new: &mut [Face]) {
    if old.iter().all(|f| (f.material.is_empty() || f.material == geom::NODRAW) && f.tex == TexMap::world(f.normal)) {
        return;
    }
    let fallback = {
        let mut counts: Vec<(&str, usize)> = Vec::new();
        for f in old {
            match counts.iter_mut().find(|c| c.0 == f.material) {
                Some(c) => c.1 += 1,
                None => counts.push((&f.material, 1)),
            }
        }
        counts.into_iter().max_by_key(|c| c.1).map(|c| c.0.to_string()).unwrap_or_default()
    };
    for nf in new.iter_mut() {
        let best = old
            .iter()
            .map(|of| (of.normal.dot(nf.normal) - (of.dist - nf.dist).abs() * 1e-3, of))
            .max_by(|a, b| a.0.partial_cmp(&b.0).unwrap());
        match best {
            Some((score, of)) if score > 0.7 => {
                nf.material = of.material.clone();
                nf.tex = of.tex.or_default_for(nf.normal);
            }
            _ => nf.material = fallback.clone(),
        }
    }
}

// ------------------------------------------------------------------ layers

#[derive(Clone, Copy, Debug, PartialEq, Eq)]
pub enum LayerKind {
    Visual,
    Collision,
}

#[derive(Clone, Debug, PartialEq)]
pub struct Layer {
    pub id: u32,
    pub name: String,
    pub kind: LayerKind,
    pub visible: bool,
    pub locked: bool,
    /// Its own origin (e.g. a collision layer offset from the model), or
    /// None to use the model origin
    pub origin: Option<V3>,
}

/// Group membership worked out for the whole document at once.
#[derive(Default)]
pub struct GroupIndex {
    /// brush → its outermost group
    pub top: HashMap<u32, u32>,
    /// group → every brush in it (nested groups included)
    pub members: HashMap<u32, Vec<u32>>,
}

/// A named group of brushes (Ctrl+G). Groups nest through `parent`.
#[derive(Clone, Debug, PartialEq)]
pub struct Group {
    pub id: u32,
    pub name: String,
    pub parent: Option<u32>,
}

pub fn default_layers() -> Vec<Layer> {
    vec![Layer { id: 1, name: "Visual 1".into(), kind: LayerKind::Visual, visible: true, locked: false, origin: None }]
}

// ------------------------------------------------------------------ document

#[derive(Clone)]
struct Snapshot {
    brushes: Vec<Brush>,
    selection: Vec<u32>,
    next_id: u32,
    layers: Vec<Layer>,
    active_layer: u32,
    origin: V3,
    groups: Vec<Group>,
}

pub struct Document {
    pub brushes: Vec<Brush>,
    pub selection: Vec<u32>,
    pub layers: Vec<Layer>,
    pub groups: Vec<Group>,
    /// New brushes go here
    pub active_layer: u32,
    /// The model's origin, honoured on export
    pub origin: V3,
    /// Where the model goes in the game, e.g. "housepack/onestory/house_1"
    /// (models/<path>.mdl). Stored in the .bpmdl so it's set once.
    pub model_path: String,
    /// Material properties this model overrides: (material, serialized
    /// properties). The texture's own .bonkmat applies otherwise.
    pub mat_overrides: Vec<(String, String)>,
    /// Unsaved changes since the last save/load
    pub dirty: bool,
    /// Bumped whenever layers or the brush → layer mapping may have changed
    /// (the layers pane uses it to know when to refresh)
    pub rev: u64,
    pub(crate) next_id: u32,
    undo: Vec<Snapshot>,
    redo: Vec<Snapshot>,
}

const UNDO_LIMIT: usize = 256;

impl Default for Document {
    fn default() -> Self {
        Self::new()
    }
}

impl Document {
    pub fn new() -> Self {
        Document {
            brushes: Vec::new(),
            selection: Vec::new(),
            layers: default_layers(),
            groups: Vec::new(),
            active_layer: 1,
            origin: V3::default(),
            model_path: String::new(),
            mat_overrides: Vec::new(),
            dirty: false,
            rev: 1,
            next_id: 1,
            undo: Vec::new(),
            redo: Vec::new(),
        }
    }

    fn snapshot(&self) -> Snapshot {
        Snapshot {
            brushes: self.brushes.clone(),
            selection: self.selection.clone(),
            next_id: self.next_id,
            layers: self.layers.clone(),
            active_layer: self.active_layer,
            origin: self.origin,
            groups: self.groups.clone(),
        }
    }

    fn restore(&mut self, s: Snapshot) {
        // Visibility and locks are view state: keep the current ones.
        let mut layers = s.layers;
        for l in &mut layers {
            if let Some(cur) = self.layers.iter().find(|c| c.id == l.id) {
                l.visible = cur.visible;
                l.locked = cur.locked;
            }
        }
        self.brushes = s.brushes;
        self.selection = s.selection;
        self.next_id = s.next_id;
        self.layers = layers;
        self.active_layer = s.active_layer;
        self.origin = s.origin;
        self.groups = s.groups;
        self.rev += 1;
    }

    /// Record the current state so the next change can be undone.
    pub fn checkpoint(&mut self) {
        self.dirty = true;
        self.rev += 1;
        self.undo.push(self.snapshot());
        if self.undo.len() > UNDO_LIMIT {
            self.undo.remove(0);
        }
        self.redo.clear();
    }

    pub fn undo(&mut self) -> bool {
        match self.undo.pop() {
            Some(s) => {
                self.redo.push(self.snapshot());
                self.restore(s);
                self.dirty = true;
                true
            }
            None => false,
        }
    }

    pub fn redo(&mut self) -> bool {
        match self.redo.pop() {
            Some(s) => {
                self.undo.push(self.snapshot());
                self.restore(s);
                self.dirty = true;
                true
            }
            None => false,
        }
    }

    /// Throw away everything since the last checkpoint (no redo entry).
    pub fn revert_to_checkpoint(&mut self) {
        if let Some(s) = self.undo.pop() {
            self.restore(s);
        }
    }

    fn take_id(&mut self) -> u32 {
        let id = self.next_id;
        self.next_id += 1;
        id
    }

    /// Copy the given brushes; returns the new ids (same order).
    pub fn duplicate(&mut self, ids: &[u32]) -> Vec<u32> {
        self.duplicate_to(ids, None)
    }

    /// Copy brushes, optionally onto another layer.
    pub fn duplicate_to(&mut self, ids: &[u32], layer: Option<u32>) -> Vec<u32> {
        self.duplicate_ext(ids, layer, false)
    }

    /// Copy brushes; `keep_names` gives copied groups the same names (used
    /// when a collision layer starts as a copy of the visual model).
    pub fn duplicate_ext(&mut self, ids: &[u32], layer: Option<u32>, keep_names: bool) -> Vec<u32> {
        // Groups whose members are all being copied get copied too, named
        // "Window (1)", "Window (2)"…; the rest keep their members' group.
        let mut gmap: Vec<(u32, u32)> = Vec::new();
        let mut covered: Vec<u32> = Vec::new();
        let ix = self.group_index();
        let idset: HashSet<u32> = ids.iter().copied().collect();
        for (&g, members) in &ix.members {
            if members.iter().any(|m| idset.contains(m)) && members.iter().all(|m| idset.contains(m)) {
                covered.push(g);
            }
        }
        covered.sort();
        // parents before children
        covered.sort_by_key(|&g| self.group_depth(g));
        for g in covered {
            let Some(src) = self.group(g).cloned() else { continue };
            let nid = self.next_group_id();
            let parent = src.parent.map(|p| gmap.iter().find(|m| m.0 == p).map(|m| m.1).unwrap_or(p));
            let name = if keep_names { src.name.clone() } else { self.numbered_name(&src.name) };
            self.groups.push(Group { id: nid, name, parent });
            gmap.push((g, nid));
        }
        let mut out = Vec::new();
        for &id in ids {
            if let Some(b) = self.brush(id).cloned() {
                let nid = self.take_id();
                let color = if b.entity.is_some() { b.color } else { palette(nid) };
                let group = b.group.map(|g| gmap.iter().find(|m| m.0 == g).map(|m| m.1).unwrap_or(g));
                self.brushes.push(Brush { id: nid, color, layer: layer.unwrap_or(b.layer), group, ..b });
                out.push(nid);
            }
        }
        if !gmap.is_empty() {
            self.rev += 1;
        }
        out
    }

    // ---------------------------------------------------------------- groups

    pub fn group(&self, id: u32) -> Option<&Group> {
        self.groups.iter().find(|g| g.id == id)
    }

    fn next_group_id(&self) -> u32 {
        self.groups.iter().map(|g| g.id).max().unwrap_or(0) + 1
    }

    /// "Window" → "Window (1)", "Window (3)" → the next free number.
    fn numbered_name(&self, name: &str) -> String {
        let base = match name.rfind(" (") {
            Some(i) if name.ends_with(')') && name[i + 2..name.len() - 1].parse::<u32>().is_ok() => &name[..i],
            _ => name,
        };
        let mut n = 1;
        while self.groups.iter().any(|g| g.name == format!("{base} ({n})")) {
            n += 1;
        }
        format!("{base} ({n})")
    }

    /// The groups a brush is in, innermost first.
    pub fn group_chain(&self, bid: u32) -> Vec<u32> {
        let mut out = Vec::new();
        let mut g = self.brush(bid).and_then(|b| b.group);
        while let Some(id) = g {
            if out.contains(&id) {
                break;
            }
            out.push(id);
            g = self.group(id).and_then(|x| x.parent);
        }
        out
    }

    fn group_depth(&self, g: u32) -> usize {
        let mut d = 0;
        let mut cur = self.group(g).and_then(|x| x.parent);
        while let Some(p) = cur {
            d += 1;
            if d > 64 {
                break;
            }
            cur = self.group(p).and_then(|x| x.parent);
        }
        d
    }

    /// Outermost group of a brush.
    pub fn top_group(&self, bid: u32) -> Option<u32> {
        self.group_chain(bid).last().copied()
    }

    /// Every brush inside a group (nested groups included).
    pub fn group_members(&self, g: u32) -> Vec<u32> {
        self.group_index().members.remove(&g).unwrap_or_default()
    }

    /// Who's in which group, worked out in one pass (imported maps can have
    /// thousands of brushes and hundreds of groups).
    pub fn group_index(&self) -> GroupIndex {
        let parents: HashMap<u32, Option<u32>> = self.groups.iter().map(|g| (g.id, g.parent)).collect();
        let mut ix = GroupIndex::default();
        for b in &self.brushes {
            let mut g = b.group;
            let mut depth = 0;
            let mut top = None;
            while let Some(id) = g {
                if depth > 64 || !parents.contains_key(&id) {
                    break;
                }
                ix.members.entry(id).or_default().push(b.id);
                top = Some(id);
                g = parents[&id];
                depth += 1;
            }
            if let Some(t) = top {
                ix.top.insert(b.id, t);
            }
        }
        ix
    }

    /// Grow a set of brushes to whole (outermost) groups.
    pub fn expand_to_groups(&self, ids: &[u32]) -> Vec<u32> {
        let ix = self.group_index();
        let mut seen: HashSet<u32> = HashSet::new();
        let mut out: Vec<u32> = Vec::new();
        for &id in ids {
            match ix.top.get(&id) {
                Some(g) => {
                    for &m in ix.members.get(g).map(|v| v.as_slice()).unwrap_or(&[]) {
                        if seen.insert(m) {
                            out.push(m);
                        }
                    }
                }
                None => {
                    if seen.insert(id) {
                        out.push(id);
                    }
                }
            }
        }
        out
    }

    /// Ctrl+G: group the selection. Groups already in it become sub-groups.
    pub fn group_selection(&mut self) -> Option<u32> {
        let sel: Vec<u32> = self.selection.iter().copied().filter(|&id| self.brush(id).is_some_and(|b| b.entity.is_none())).collect();
        if sel.is_empty() {
            return None;
        }
        self.checkpoint();
        let gid = self.next_group_id();
        let n = (1..).find(|n| !self.groups.iter().any(|g| g.name == format!("Group {n}"))).unwrap();
        self.groups.push(Group { id: gid, name: format!("Group {n}"), parent: None });
        for id in sel {
            match self.top_group(id) {
                Some(t) if t != gid => {
                    if let Some(g) = self.groups.iter_mut().find(|g| g.id == t) {
                        g.parent = Some(gid);
                    }
                }
                Some(_) => {}
                None => {
                    if let Some(b) = self.brush_mut(id) {
                        b.group = Some(gid);
                    }
                }
            }
        }
        self.rev += 1;
        Some(gid)
    }

    /// Ctrl+U: take apart the outermost groups of the selection (one level).
    pub fn ungroup_selection(&mut self) -> usize {
        let mut tops: Vec<u32> = self.selection.iter().filter_map(|&id| self.top_group(id)).collect();
        tops.sort();
        tops.dedup();
        if tops.is_empty() {
            return 0;
        }
        self.checkpoint();
        for &t in &tops {
            self.dissolve_group(t);
        }
        self.rev += 1;
        tops.len()
    }

    pub(crate) fn dissolve_group(&mut self, t: u32) {
        let parent = self.group(t).and_then(|g| g.parent);
        for g in self.groups.iter_mut().filter(|g| g.parent == Some(t)) {
            g.parent = parent;
        }
        for b in self.brushes.iter_mut().filter(|b| b.group == Some(t)) {
            b.group = parent;
        }
        self.groups.retain(|g| g.id != t);
    }

    /// Take a specific group apart (from the layers pane).
    pub fn ungroup(&mut self, g: u32) -> bool {
        if self.group(g).is_none() {
            return false;
        }
        self.checkpoint();
        self.dissolve_group(g);
        self.rev += 1;
        true
    }

    pub fn rename_group(&mut self, g: u32, name: &str) -> bool {
        let name = name.trim();
        if name.is_empty() || self.group(g).is_none_or(|x| x.name == name) {
            return false;
        }
        self.checkpoint();
        if let Some(x) = self.groups.iter_mut().find(|x| x.id == g) {
            x.name = name.to_string();
        }
        true
    }

    /// Drop groups with no brushes left in them.
    pub fn prune_groups(&mut self) {
        loop {
            let ix = self.group_index();
            let empty: Vec<u32> = self.groups.iter().map(|g| g.id).filter(|g| !ix.members.contains_key(g)).collect();
            if empty.is_empty() {
                break;
            }
            for g in empty {
                self.dissolve_group(g);
            }
            self.rev += 1;
        }
        // parents that no longer exist
        let ids: Vec<u32> = self.groups.iter().map(|g| g.id).collect();
        for g in &mut self.groups {
            if g.parent.is_some_and(|p| !ids.contains(&p)) {
                g.parent = None;
            }
        }
        for b in &mut self.brushes {
            if b.group.is_some_and(|p| !ids.contains(&p)) {
                b.group = None;
            }
        }
    }

    /// Replace everything with a loaded document (clears undo history).
    pub fn replace_with(&mut self, brushes: Vec<Brush>) {
        self.replace_all(brushes, default_layers(), 1, V3::default());
    }

    pub fn replace_all(&mut self, brushes: Vec<Brush>, layers: Vec<Layer>, active: u32, origin: V3) {
        let layers = if layers.iter().any(|l| l.kind == LayerKind::Visual) { layers } else {
            let mut l = layers;
            let id = l.iter().map(|x| x.id).max().unwrap_or(0) + 1;
            l.insert(0, Layer { id, name: "Visual 1".into(), kind: LayerKind::Visual, visible: true, locked: false, origin: None });
            l
        };
        let first_visual = layers.iter().find(|l| l.kind == LayerKind::Visual).unwrap().id;
        let mut brushes = brushes;
        for b in &mut brushes {
            if !layers.iter().any(|l| l.id == b.layer) {
                b.layer = first_visual;
            }
        }
        self.next_id = brushes.iter().map(|b| b.id).max().unwrap_or(0) + 1;
        self.brushes = brushes;
        self.active_layer = if layers.iter().any(|l| l.id == active) { active } else { first_visual };
        self.layers = layers;
        self.origin = origin;
        self.model_path.clear();
        self.mat_overrides.clear();
        self.groups.clear();
        self.selection.clear();
        self.undo.clear();
        self.redo.clear();
        self.dirty = false;
        self.rev += 1;
    }

    /// Add an already-built piece (e.g. from a bisect) as a new brush with its own id and color.
    pub fn add_piece(&mut self, mut b: Brush) -> u32 {
        let id = self.take_id();
        b.id = id;
        b.color = palette(id);
        self.brushes.push(b);
        id
    }

    pub fn add_brush(&mut self, mins: V3, maxs: V3) -> u32 {
        let id = self.take_id();
        let mut b = Brush::new_box(id, mins, maxs, palette(id));
        b.layer = self.active_layer;
        self.brushes.push(b);
        id
    }

    pub fn add_entity(&mut self, kind: Entity, base: V3) -> u32 {
        let id = self.take_id();
        let mut b = Brush::new_entity(id, kind, base, 0.0);
        // Entities aren't collision geometry: keep them on a visual layer.
        b.layer = match self.layer(self.active_layer) {
            Some(l) if l.kind == LayerKind::Visual => l.id,
            _ => self.layers.iter().find(|l| l.kind == LayerKind::Visual).map(|l| l.id).unwrap_or(self.active_layer),
        };
        self.brushes.push(b);
        id
    }

    /// Brushes are kept in id order (ids only grow), so this is a binary
    /// search; the linear scan is only a fallback.
    fn brush_index(&self, id: u32) -> Option<usize> {
        match self.brushes.binary_search_by_key(&id, |b| b.id) {
            Ok(i) => Some(i),
            Err(_) => self.brushes.iter().position(|b| b.id == id),
        }
    }

    pub fn brush(&self, id: u32) -> Option<&Brush> {
        self.brush_index(id).map(|i| &self.brushes[i])
    }

    pub fn brush_mut(&mut self, id: u32) -> Option<&mut Brush> {
        self.brush_index(id).map(move |i| &mut self.brushes[i])
    }

    pub fn is_selected(&self, id: u32) -> bool {
        self.selection.contains(&id)
    }

    pub fn selected_brushes(&self) -> Vec<Brush> {
        self.brushes.iter().filter(|b| self.is_selected(b.id)).cloned().collect()
    }

    pub fn selection_bounds(&self) -> Option<(V3, V3)> {
        let mut it = self.brushes.iter().filter(|b| self.is_selected(b.id));
        let (mut lo, mut hi) = it.next()?.bounds();
        for b in it {
            let (l, h) = b.bounds();
            lo = lo.min(l);
            hi = hi.max(h);
        }
        Some((lo, hi))
    }

    pub fn delete_selected(&mut self) -> bool {
        if self.selection.is_empty() {
            return false;
        }
        self.checkpoint();
        let sel = std::mem::take(&mut self.selection);
        self.brushes.retain(|b| !sel.contains(&b.id));
        self.prune_groups();
        true
    }

    // ---------------------------------------------------------------- layers

    pub fn layer(&self, id: u32) -> Option<&Layer> {
        self.layers.iter().find(|l| l.id == id)
    }

    /// Drawn in the views (layer visible).
    pub fn layer_visible(&self, b: &Brush) -> bool {
        self.layer(b.layer).is_none_or(|l| l.visible)
    }

    /// Can be picked, selected and edited (layer visible and unlocked).
    pub fn layer_editable(&self, b: &Brush) -> bool {
        self.layer(b.layer).is_none_or(|l| l.visible && !l.locked)
    }

    pub fn is_collision(&self, b: &Brush) -> bool {
        self.layer(b.layer).is_some_and(|l| l.kind == LayerKind::Collision)
    }

    pub fn add_layer(&mut self, kind: LayerKind) -> u32 {
        self.checkpoint();
        let id = self.layers.iter().map(|l| l.id).max().unwrap_or(0) + 1;
        let n = self.layers.iter().filter(|l| l.kind == kind).count() + 1;
        let name = match kind {
            LayerKind::Visual => format!("Visual {n}"),
            LayerKind::Collision => format!("Collision {n}"),
        };
        self.layers.push(Layer { id, name, kind, visible: true, locked: false, origin: None });
        self.active_layer = id;
        id
    }

    /// A new collision layer. If there's a visual model already, the layer
    /// starts as a copy of it (to simplify into the physics hull).
    /// Returns (layer id, brushes copied).
    pub fn add_collision_from_visual(&mut self) -> (u32, usize) {
        let src: Vec<u32> = self
            .brushes
            .iter()
            .filter(|b| b.entity.is_none() && self.layer(b.layer).is_some_and(|l| l.kind == LayerKind::Visual && l.visible))
            .map(|b| b.id)
            .collect();
        let id = self.add_layer(LayerKind::Collision); // checkpoints
        let n = if src.is_empty() { 0 } else { self.duplicate_ext(&src, Some(id), true).len() };
        self.selection.clear();
        (id, n)
    }

    /// Delete a layer and the brushes on it. The last visual layer can't go.
    pub fn delete_layer(&mut self, id: u32) -> bool {
        let Some(l) = self.layer(id) else { return false };
        if l.kind == LayerKind::Visual && self.layers.iter().filter(|x| x.kind == LayerKind::Visual).count() <= 1 {
            return false;
        }
        self.checkpoint();
        self.layers.retain(|x| x.id != id);
        self.brushes.retain(|b| b.layer != id);
        self.prune_groups();
        let alive: Vec<u32> = self.brushes.iter().map(|b| b.id).collect();
        self.selection.retain(|s| alive.contains(s));
        if self.active_layer == id {
            self.active_layer = self.layers.iter().find(|x| x.kind == LayerKind::Visual).map(|x| x.id).unwrap_or(1);
        }
        true
    }

    pub fn rename_layer(&mut self, id: u32, name: &str) -> bool {
        let name = name.trim();
        if name.is_empty() || self.layer(id).is_none_or(|l| l.name == name) {
            return false;
        }
        self.checkpoint();
        if let Some(l) = self.layers.iter_mut().find(|l| l.id == id) {
            l.name = name.to_string();
        }
        true
    }

    /// Show/hide or lock/unlock a layer. Hidden or locked layers drop out of the selection.
    pub fn set_layer_flags(&mut self, id: u32, visible: bool, locked: bool) {
        if let Some(l) = self.layers.iter_mut().find(|l| l.id == id) {
            if l.visible != visible || l.locked != locked {
                l.visible = visible;
                l.locked = locked;
                self.dirty = true;
                self.rev += 1;
            }
        }
        if !visible || locked {
            let gone: Vec<u32> = self.brushes.iter().filter(|b| b.layer == id).map(|b| b.id).collect();
            self.selection.retain(|s| !gone.contains(s));
        }
    }

    pub fn move_selection_to_layer(&mut self, id: u32) -> usize {
        if self.layer(id).is_none() || self.selection.is_empty() {
            return 0;
        }
        self.checkpoint();
        let sel = self.selection.clone();
        let mut n = 0;
        for b in self.brushes.iter_mut().filter(|b| sel.contains(&b.id)) {
            b.layer = id;
            n += 1;
        }
        n
    }

    pub fn copy_selection_to_layer(&mut self, id: u32) -> usize {
        if self.layer(id).is_none() || self.selection.is_empty() {
            return 0;
        }
        self.checkpoint();
        let sel = self.selection.clone();
        let new = self.duplicate_to(&sel, Some(id));
        let n = new.len();
        self.selection = new;
        n
    }

    /// Select every editable brush on a layer.
    pub fn select_layer(&mut self, id: u32) {
        let ids: Vec<u32> = self.brushes.iter().filter(|b| b.layer == id && self.layer_editable(b)).map(|b| b.id).collect();
        self.selection = ids;
    }
}

/// Pleasant, distinct pastel colors per brush (golden-ratio hue walk).
fn palette(id: u32) -> [f32; 3] {
    let h = (id as f32 * 0.618_034).fract();
    hsv(h, 0.38, 0.88)
}

fn hsv(h: f32, s: f32, v: f32) -> [f32; 3] {
    let i = (h * 6.0).floor();
    let f = h * 6.0 - i;
    let p = v * (1.0 - s);
    let q = v * (1.0 - f * s);
    let t = v * (1.0 - (1.0 - f) * s);
    match (i as i32).rem_euclid(6) {
        0 => [v, t, p],
        1 => [q, v, p],
        2 => [p, v, t],
        3 => [p, q, v],
        4 => [t, p, v],
        _ => [v, p, q],
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn undo_redo_roundtrip() {
        let mut d = Document::new();
        d.checkpoint();
        let id = d.add_brush(v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0));
        d.selection = vec![id];
        assert!(d.delete_selected());
        assert!(d.brushes.is_empty());
        assert!(d.undo());
        assert_eq!(d.brushes.len(), 1);
        assert!(d.undo());
        assert!(d.brushes.is_empty());
        assert!(d.redo());
        assert!(d.redo());
        assert!(d.brushes.is_empty());
    }

    #[test]
    fn add_normalizes_extents() {
        let mut d = Document::new();
        d.add_brush(v3(64.0, 0.0, 32.0), v3(0.0, 64.0, 0.0));
        assert_eq!(d.brushes[0].bounds(), (v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 32.0)));
        assert_eq!(d.brushes[0].verts.len(), 8);
        assert_eq!(d.brushes[0].edges().len(), 12);
    }

    #[test]
    fn invalid_points_leave_brush_untouched() {
        let mut b = Brush::new_box(1, v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0), [1.0; 3]);
        let before = b.clone();
        let mut pts = b.verts.clone();
        let i = pts.iter().position(|p| *p == v3(64.0, 64.0, 64.0)).unwrap();
        pts[i] = v3(32.0, 32.0, 32.0); // dent: would be concave
        assert!(!b.set_points(&pts));
        assert_eq!(b, before);
    }

    #[test]
    fn materials_survive_vertex_edits() {
        let mut b = Brush::new_box(1, v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0), [1.0; 3]);
        for f in &mut b.faces {
            f.material = "walls/plaster".into();
        }
        let top = b.faces.iter().position(|f| f.normal.z > 0.9).unwrap();
        b.faces[top].material = geom::NODRAW.into();
        // pull one top corner up a little: the top face tilts but keeps its material
        let mut pts = b.verts.clone();
        let i = pts.iter().position(|p| *p == v3(64.0, 64.0, 64.0)).unwrap();
        pts[i].z = 80.0;
        assert!(b.set_points(&pts));
        let nodraw: Vec<&Face> = b.faces.iter().filter(|f| f.material == geom::NODRAW).collect();
        assert!(!nodraw.is_empty());
        assert!(nodraw.iter().all(|f| f.normal.z > 0.5));
        assert!(b.faces.iter().filter(|f| f.normal.z < -0.9).all(|f| f.material == "walls/plaster"));
    }

    #[test]
    fn layers_move_copy_hide_lock_delete() {
        let mut d = Document::new();
        let a = d.add_brush(v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0));
        let col = d.add_layer(LayerKind::Collision);
        assert_eq!(d.active_layer, col);
        let b = d.add_brush(v3(100.0, 0.0, 0.0), v3(164.0, 64.0, 64.0));
        assert_eq!(d.brush(b).unwrap().layer, col);
        assert!(d.is_collision(d.brush(b).unwrap()));
        d.selection = vec![a];
        assert_eq!(d.copy_selection_to_layer(col), 1);
        assert_eq!(d.brushes.iter().filter(|x| x.layer == col).count(), 2);
        assert_eq!(d.brush(a).unwrap().layer, 1, "copy leaves the original");
        d.selection = vec![a];
        d.move_selection_to_layer(col);
        assert_eq!(d.brush(a).unwrap().layer, col);
        d.undo();
        assert_eq!(d.brush(a).unwrap().layer, 1);
        // hiding drops the layer's brushes from the selection, and survives undo
        d.select_layer(col);
        assert_eq!(d.selection.len(), 2);
        d.set_layer_flags(col, false, false);
        assert!(d.selection.is_empty());
        assert!(!d.layer_visible(d.brush(b).unwrap()));
        d.undo();
        assert!(!d.layer(col).unwrap().visible, "visibility is view state, not undone");
        d.set_layer_flags(col, true, true);
        assert!(!d.layer_editable(d.brush(b).unwrap()));
        // the last visual layer can't be deleted; others take their brushes with them
        assert!(!d.delete_layer(1));
        assert!(d.delete_layer(col));
        assert!(d.brushes.iter().all(|x| x.layer == 1));
        assert_eq!(d.active_layer, 1);
    }

    #[test]
    fn entity_box_is_fixed_size_at_its_base() {
        let mut d = Document::new();
        let id = d.add_entity(Entity::Mannequin, v3(10.0, 20.0, 30.0));
        let e = d.brush(id).unwrap();
        assert_eq!(e.bounds(), (v3(-6.0, 4.0, 30.0), v3(26.0, 36.0, 102.0)));
        assert_eq!(e.base(), v3(10.0, 20.0, 30.0));
        let mut e = e.clone();
        e.place_entity(v3(0.0, 0.0, 0.0));
        assert_eq!(e.bounds(), (v3(-16.0, -16.0, 0.0), v3(16.0, 16.0, 72.0)));
    }
}
