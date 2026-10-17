// SPDX-License-Identifier: GPL-3.0-or-later
//! Variants of one model: skins (material swaps, optionally hiding brushes),
//! bodygroups (groups marked as options players switch between) and LODs
//! (simpler versions for a distance).
//!
//! Skin 0 is the model as painted; `skins` are the extra ones. Source skins
//! only swap materials, so a skin that hides brushes is exported by giving
//! those brushes' faces their own copies of their materials, which that skin
//! swaps for an invisible one.

use crate::doc::{Brush, Document};

#[derive(Clone, Debug, PartialEq, Default)]
pub struct Skin {
    pub name: String,
    /// (material as painted, material in this skin)
    pub swaps: Vec<(String, String)>,
    /// Brushes this skin hides
    pub hidden: Vec<u32>,
}

impl Skin {
    pub fn swap<'a>(&'a self, m: &'a str) -> &'a str {
        self.swaps.iter().find(|(a, _)| a == m).map(|(_, b)| b.as_str()).unwrap_or(m)
    }
}

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Lod {
    /// studiomdl's $lod switch point (bigger = farther away)
    pub distance: f32,
    /// Brushes whose largest side is smaller than this are left out
    pub detail: f32,
}

#[derive(Clone, Debug, PartialEq, Default)]
pub struct Variants {
    pub skins: Vec<Skin>,
    pub lods: Vec<Lod>,
    /// Bodygroups that have a "None" option (nothing shown)
    pub blank: Vec<String>,
    /// View state: the skin the views show and paint (0 = as painted)
    pub skin: usize,
    /// View state: bodygroup → the option the views show ("" = None)
    pub shown: Vec<(String, String)>,
}

/// One bodygroup: its options (group id, option name) in order, and whether it has "None".
#[derive(Clone, Debug, PartialEq)]
pub struct Bodygroup {
    pub name: String,
    pub options: Vec<(u32, String)>,
    pub blank: bool,
}

impl Document {
    /// The skin the views show, if it isn't skin 0.
    pub fn active_skin(&self) -> Option<&Skin> {
        self.variants.skin.checked_sub(1).and_then(|i| self.variants.skins.get(i))
    }

    /// A material as the views show it in the active skin.
    pub fn skin_material<'a>(&'a self, m: &'a str) -> &'a str {
        match self.active_skin() {
            Some(s) => s.swap(m),
            None => m,
        }
    }

    /// The bodygroup option group a brush belongs to: its innermost group
    /// (or an outer one) that's marked as an option.
    pub fn option_group_of(&self, b: &Brush) -> Option<u32> {
        let mut g = b.group;
        let mut guard = 0;
        while let Some(id) = g {
            let grp = self.groups.iter().find(|x| x.id == id)?;
            if grp.bodygroup.is_some() {
                return Some(id);
            }
            g = grp.parent;
            guard += 1;
            if guard > 64 {
                return None;
            }
        }
        None
    }

    pub fn bodygroup_of(&self, b: &Brush) -> Option<(&str, u32)> {
        let g = self.option_group_of(b)?;
        let grp = self.groups.iter().find(|x| x.id == g)?;
        grp.bodygroup.as_deref().map(|n| (n, g))
    }

    /// Every bodygroup, in the order they first appear.
    pub fn bodygroups(&self) -> Vec<Bodygroup> {
        let mut out: Vec<Bodygroup> = Vec::new();
        for g in &self.groups {
            let Some(name) = &g.bodygroup else { continue };
            match out.iter_mut().find(|b| &b.name == name) {
                Some(b) => b.options.push((g.id, g.name.clone())),
                None => out.push(Bodygroup { name: name.clone(), options: vec![(g.id, g.name.clone())], blank: false }),
            }
        }
        for b in &mut out {
            b.blank = self.variants.blank.contains(&b.name);
        }
        out
    }

    /// The option a bodygroup shows in the views: its group id, or None for "None".
    pub fn shown_option(&self, bg: &Bodygroup) -> Option<u32> {
        match self.variants.shown.iter().find(|(n, _)| *n == bg.name) {
            Some((_, o)) if o.is_empty() && bg.blank => None,
            Some((_, o)) => bg.options.iter().find(|(_, n)| n == o).or(bg.options.first()).map(|x| x.0),
            None => bg.options.first().map(|x| x.0),
        }
    }

    /// Hidden by the variant the views show: another option of its bodygroup,
    /// or hidden in the active skin.
    pub fn variant_hides(&self, b: &Brush) -> bool {
        if self.active_skin().is_some_and(|s| s.hidden.contains(&b.id)) {
            return true;
        }
        if !self.groups.iter().any(|g| g.bodygroup.is_some()) {
            return false;
        }
        let Some((name, gid)) = self.bodygroup_of(b) else { return false };
        let bgs = self.bodygroups();
        let Some(bg) = bgs.iter().find(|x| x.name == name) else { return false };
        self.shown_option(bg) != Some(gid)
    }

    /// Mark (or unmark, `name` empty) a group as an option of bodygroup `name`.
    pub fn set_bodygroup(&mut self, group: u32, name: &str) {
        self.checkpoint();
        if let Some(g) = self.groups.iter_mut().find(|g| g.id == group) {
            g.bodygroup = if name.trim().is_empty() { None } else { Some(name.trim().to_string()) };
        }
        let live: Vec<String> = self.groups.iter().filter_map(|g| g.bodygroup.clone()).collect();
        self.variants.blank.retain(|b| live.contains(b));
        self.variants.shown.retain(|(b, _)| live.contains(b));
    }

    pub fn set_bodygroup_blank(&mut self, name: &str, on: bool) {
        self.checkpoint();
        self.variants.blank.retain(|b| b != name);
        if on {
            self.variants.blank.push(name.to_string());
        } else {
            self.variants.shown.retain(|(b, o)| !(b == name && o.is_empty()));
        }
    }

    /// Show this option of a bodygroup in the views ("" = None).
    pub fn show_option(&mut self, bodygroup: &str, option: &str) {
        self.variants.shown.retain(|(b, _)| b != bodygroup);
        self.variants.shown.push((bodygroup.to_string(), option.to_string()));
        self.rev += 1;
    }

    // ---- skins

    pub fn add_skin(&mut self, copy_of: usize) -> usize {
        self.checkpoint();
        let mut s = copy_of.checked_sub(1).and_then(|i| self.variants.skins.get(i)).cloned().unwrap_or_default();
        let n = (1..).find(|n| !self.variants.skins.iter().any(|s| s.name == format!("Skin {n}"))).unwrap();
        s.name = format!("Skin {n}");
        self.variants.skins.push(s);
        self.variants.skin = self.variants.skins.len();
        self.variants.skin
    }

    pub fn remove_skin(&mut self, index: usize) {
        if index == 0 || index > self.variants.skins.len() {
            return;
        }
        self.checkpoint();
        self.variants.skins.remove(index - 1);
        self.variants.skin = self.variants.skin.min(self.variants.skins.len());
        if self.variants.skin >= index && self.variants.skin > 0 {
            self.variants.skin -= 1;
        }
    }

    /// In the active skin, `from` (as painted) shows as `to`.
    pub fn skin_swap(&mut self, from: &str, to: &str) {
        let Some(i) = self.variants.skin.checked_sub(1) else { return };
        let Some(s) = self.variants.skins.get_mut(i) else { return };
        s.swaps.retain(|(a, _)| a != from);
        if from != to {
            s.swaps.push((from.to_string(), to.to_string()));
        }
    }

    /// Hide (or show) brushes in the active skin.
    pub fn skin_hide(&mut self, ids: &[u32], hide: bool) -> bool {
        let Some(i) = self.variants.skin.checked_sub(1) else { return false };
        if i >= self.variants.skins.len() {
            return false;
        }
        self.checkpoint();
        let s = &mut self.variants.skins[i];
        s.hidden.retain(|id| !ids.contains(id));
        if hide {
            s.hidden.extend_from_slice(ids);
        }
        true
    }

    /// Brushes no longer in the model drop out of skins.
    pub fn prune_variants(&mut self) {
        let ids: std::collections::HashSet<u32> = self.brushes.iter().map(|b| b.id).collect();
        for s in &mut self.variants.skins {
            s.hidden.retain(|id| ids.contains(id));
        }
        self.variants.skin = self.variants.skin.min(self.variants.skins.len());
    }

    /// What a LOD keeps: brushes at least `detail` across.
    pub fn lod_keeps(b: &Brush, detail: f32) -> bool {
        let (lo, hi) = b.bounds();
        let s = hi - lo;
        s.x.max(s.y).max(s.z) >= detail
    }
}

/// The model as compiled with these choices: `skin` (0 = as painted),
/// bodygroup → option ("" = None), and a LOD (0 = full detail). Visual brushes only.
pub fn compiled_view(doc: &Document, skin: usize, choices: &[(String, String)], lod: usize) -> Document {
    let mut out = Document::new();
    out.origin = doc.origin;
    let bgs = doc.bodygroups();
    let sk = skin.checked_sub(1).and_then(|i| doc.variants.skins.get(i));
    let detail = lod.checked_sub(1).and_then(|i| {
        let mut l = doc.variants.lods.clone();
        l.sort_by(|a, b| a.distance.total_cmp(&b.distance));
        l.get(i).map(|x| x.detail)
    });
    for b in doc.brushes.iter().filter(|b| b.entity.is_none() && !doc.is_collision(b)) {
        if let Some((name, gid)) = doc.bodygroup_of(b) {
            let Some(bg) = bgs.iter().find(|x| x.name == name) else { continue };
            let want = choices.iter().find(|(n, _)| n == name).map(|(_, o)| o.as_str());
            let chosen = match want {
                Some("") if bg.blank => None,
                Some(o) => bg.options.iter().find(|(_, n)| n == o).or(bg.options.first()).map(|x| x.0),
                None => bg.options.first().map(|x| x.0),
            };
            if chosen != Some(gid) {
                continue;
            }
        }
        if sk.is_some_and(|s| s.hidden.contains(&b.id)) {
            continue;
        }
        if detail.is_some_and(|d| !Document::lod_keeps(b, d)) {
            continue;
        }
        let mut nb = b.clone();
        nb.layer = out.layers[0].id;
        nb.group = None;
        if let Some(s) = sk {
            for f in &mut nb.faces {
                let to = s.swap(&f.material).to_string();
                f.material = to;
            }
        }
        out.brushes.push(nb);
    }
    out
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::math::v3;

    #[test]
    fn skins_swap_and_hide_bodygroups_show_one_option() {
        let mut d = Document::new();
        let wall = d.add_brush(v3(0.0, 0.0, 0.0), v3(64.0, 8.0, 64.0));
        let closed = d.add_brush(v3(0.0, 0.0, 0.0), v3(8.0, 64.0, 64.0));
        let open = d.add_brush(v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 8.0));
        for f in &mut d.brush_mut(wall).unwrap().faces {
            f.material = "walls/brick".into();
        }
        // skins
        assert_eq!(d.skin_material("walls/brick"), "walls/brick");
        let s = d.add_skin(0);
        assert_eq!((s, d.variants.skin), (1, 1));
        d.skin_swap("walls/brick", "walls/plaster");
        assert_eq!(d.skin_material("walls/brick"), "walls/plaster");
        d.skin_hide(&[wall], true);
        assert!(d.variant_hides(d.brush(wall).unwrap()));
        d.variants.skin = 0;
        assert_eq!(d.skin_material("walls/brick"), "walls/brick");
        assert!(!d.variant_hides(d.brush(wall).unwrap()));
        // a copy of skin 1
        d.add_skin(1);
        assert_eq!(d.variants.skins[1].swaps, d.variants.skins[0].swaps);
        assert_eq!(d.variants.skins[1].name, "Skin 2");
        d.remove_skin(1);
        assert_eq!(d.variants.skins.len(), 1);
        assert_eq!(d.variants.skins[0].name, "Skin 2");
        // bodygroups: the first option shows, the others hide
        d.selection = vec![closed];
        let g1 = d.group_selection().unwrap();
        d.rename_group(g1, "Closed");
        d.selection = vec![open];
        let g2 = d.group_selection().unwrap();
        d.rename_group(g2, "Open");
        d.set_bodygroup(g1, "Door");
        d.set_bodygroup(g2, "Door");
        let bgs = d.bodygroups();
        assert_eq!(bgs.len(), 1);
        assert_eq!(bgs[0].options.iter().map(|o| o.1.as_str()).collect::<Vec<_>>(), ["Closed", "Open"]);
        assert!(!d.variant_hides(d.brush(closed).unwrap()));
        assert!(d.variant_hides(d.brush(open).unwrap()));
        d.show_option("Door", "Open");
        assert!(d.variant_hides(d.brush(closed).unwrap()));
        assert!(!d.variant_hides(d.brush(open).unwrap()));
        // "None" only once it's added
        d.show_option("Door", "");
        assert!(!d.variant_hides(d.brush(closed).unwrap()), "no None option yet: the first shows");
        d.set_bodygroup_blank("Door", true);
        assert!(d.variant_hides(d.brush(closed).unwrap()) && d.variant_hides(d.brush(open).unwrap()));
        assert!(d.bodygroups()[0].blank);
        // unmarking
        d.set_bodygroup(g2, "");
        assert_eq!(d.bodygroups()[0].options.len(), 1);
        assert!(!d.variant_hides(d.brush(open).unwrap()));
        // undo brings the mark back
        d.undo();
        assert_eq!(d.bodygroups()[0].options.len(), 2);
    }

    #[test]
    fn variants_save_and_load() {
        let mut e = crate::editor::Editor::new();
        let a = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(32.0, 32.0, 32.0));
        e.doc.selection = vec![a];
        let g = e.doc.group_selection().unwrap();
        e.doc.set_bodygroup(g, "Door");
        e.doc.set_bodygroup_blank("Door", true);
        e.doc.add_skin(0);
        e.doc.skin_swap("walls/brick", "walls/plaster");
        e.doc.skin_hide(&[a], true);
        e.doc.variants.lods.push(Lod { distance: 30.0, detail: 24.0 });
        e.doc.show_option("Door", "");
        let text = crate::io::write(&e);
        let mut f = crate::editor::Editor::new();
        crate::io::apply(&mut f, crate::io::read(&text).unwrap());
        assert_eq!(f.doc.variants, e.doc.variants);
        assert_eq!(f.doc.groups[0].bodygroup.as_deref(), Some("Door"));
    }

    #[test]
    fn the_compiled_view_follows_the_choices() {
        let mut d = Document::new();
        let wall = d.add_brush(v3(0.0, 0.0, 0.0), v3(128.0, 8.0, 64.0));
        let sign = d.add_brush(v3(0.0, -4.0, 40.0), v3(12.0, 0.0, 52.0));
        let door = d.add_brush(v3(40.0, -4.0, 0.0), v3(80.0, 0.0, 60.0));
        for f in &mut d.brush_mut(wall).unwrap().faces {
            f.material = "walls/brick".into();
        }
        d.selection = vec![door];
        let g = d.group_selection().unwrap();
        d.rename_group(g, "Shut");
        d.set_bodygroup(g, "Door");
        d.set_bodygroup_blank("Door", true);
        d.add_skin(0);
        d.skin_swap("walls/brick", "walls/plaster");
        d.skin_hide(&[sign], true);
        d.variants.lods.push(Lod { distance: 30.0, detail: 32.0 });
        d.variants.skin = 0;
        let ids = |x: &Document| x.brushes.iter().map(|b| b.verts[0]).collect::<Vec<_>>().len();
        assert_eq!(ids(&compiled_view(&d, 0, &[], 0)), 3);
        assert_eq!(ids(&compiled_view(&d, 0, &[("Door".into(), String::new())], 0)), 2, "None");
        let s1 = compiled_view(&d, 1, &[], 0);
        assert_eq!(s1.brushes.len(), 2, "skin 1 hides the sign");
        assert!(s1.brushes.iter().any(|b| b.faces.iter().all(|f| f.material == "walls/plaster")));
        assert_eq!(compiled_view(&d, 0, &[], 1).brushes.len(), 2, "the LOD drops the small sign");
    }
}
