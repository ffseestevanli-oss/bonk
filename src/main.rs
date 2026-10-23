// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Steventechno and the Bonk! contributors
//! Bonk! Model Prop-inator! A Hammer-style brush modeler for Source engine models.
//!
//! All editor logic and rendering live in Rust. The Qt6 window, menus and input
//! plumbing live in `cpp/host.cpp`, which calls the `bonk_*` functions below.

mod collision;
mod doc;
mod variants;
mod editor;
mod export;
mod geom;
mod gl;
mod io;
mod math;
mod overlap;
mod render;
mod shapes;
mod tex;
mod texstore;
mod vtf;
mod vtfread;
mod vmf;

use math::{v3, V3};
use editor::{Editor, ViewKind};
use render::{Renderer, Text};
use std::cell::RefCell;
use std::ffi::{c_char, c_int, c_void, CString};

type TextCb = extern "C" fn(*mut c_void, f32, f32, c_int, *const c_char, u32);

thread_local! {
    static EDITOR: RefCell<Editor> = RefCell::new(Editor::new());
    static RENDERERS: RefCell<[Option<Renderer>; 4]> = const { RefCell::new([None, None, None, None]) };
    static STATUS: RefCell<CString> = RefCell::new(CString::default());
    static LAST_ERROR: RefCell<CString> = RefCell::new(CString::default());
    static STR_BUF: RefCell<CString> = RefCell::new(CString::default());
}

fn with_ed<R>(f: impl FnOnce(&mut Editor) -> R) -> R {
    EDITOR.with(|e| f(&mut e.borrow_mut()))
}

fn vid(view: c_int) -> usize {
    view.clamp(0, 3) as usize
}

#[no_mangle]
pub extern "C" fn bonk_view_init(view: c_int, getproc: gl::GetProc) {
    let r = unsafe { Renderer::new(getproc) };
    match r {
        Ok(r) => RENDERERS.with(|rs| rs.borrow_mut()[vid(view)] = Some(r)),
        Err(e) => eprintln!("bonk: OpenGL init failed for {} view: {}", ViewKind::from_id(view).name(), e),
    }
}

#[no_mangle]
pub extern "C" fn bonk_view_paint(view: c_int, w: f32, h: f32, _dpr: f32, text: TextCb, ctx: *mut c_void) {
    let id = vid(view);
    let mut texts: Vec<Text> = Vec::new();
    with_ed(|ed| {
        ed.set_view_size(id, w, h);
        RENDERERS.with(|rs| {
            if let Some(r) = rs.borrow()[id].as_ref() {
                unsafe {
                    if id == 0 {
                        r.render_3d(ed);
                    } else {
                        r.render_2d(ed, id, &mut texts);
                    }
                }
            }
        });
    });
    for t in texts {
        if let Ok(s) = CString::new(t.s) {
            text(ctx, t.x, t.y, t.align, s.as_ptr(), t.rgba);
        }
    }
}

#[no_mangle]
pub extern "C" fn bonk_view_resize(view: c_int, w: f32, h: f32) {
    with_ed(|ed| ed.set_view_size(vid(view), w, h));
}

/// kind: 0 press, 1 release, 2 move
#[no_mangle]
pub extern "C" fn bonk_mouse(view: c_int, kind: c_int, x: f32, y: f32, button: c_int, mods: c_int) -> u32 {
    let id = vid(view);
    with_ed(|ed| match kind {
        0 => ed.mouse_press(id, x, y, button, mods),
        1 => ed.mouse_release(id, x, y, button),
        _ => ed.mouse_move(id, x, y, mods),
    })
}

#[no_mangle]
pub extern "C" fn bonk_mouse_delta(dx: f32, dy: f32) -> u32 {
    with_ed(|ed| ed.mouse_delta(dx, dy))
}

#[no_mangle]
pub extern "C" fn bonk_wheel(view: c_int, x: f32, y: f32, delta: f32) -> u32 {
    with_ed(|ed| ed.wheel(vid(view), x, y, delta))
}

#[no_mangle]
pub extern "C" fn bonk_key(view: c_int, key: c_int, pressed: c_int, mods: c_int, repeat: c_int) -> u32 {
    with_ed(|ed| ed.key(vid(view), key, pressed != 0, mods, repeat != 0))
}

#[no_mangle]
pub extern "C" fn bonk_focus_lost(_view: c_int) -> u32 {
    with_ed(|ed| ed.focus_lost())
}

#[no_mangle]
pub extern "C" fn bonk_tick(dt: f32) -> u32 {
    with_ed(|ed| ed.tick(dt.clamp(0.0, 0.1)))
}

#[no_mangle]
pub extern "C" fn bonk_command(cmd: c_int) -> u32 {
    with_ed(|ed| ed.command(cmd))
}

#[no_mangle]
pub extern "C" fn bonk_tool() -> c_int {
    with_ed(|ed| ed.tool as c_int)
}

/// 1 when the Bisect tool is in Split & Delete mode.
#[no_mangle]
pub extern "C" fn bonk_bisect_delete() -> c_int {
    with_ed(|ed| if !ed.bisect_delete { 0 } else if ed.bisect_larger { 2 } else { 1 })
}

#[no_mangle]
pub extern "C" fn bonk_is_captured() -> c_int {
    with_ed(|ed| ed.captured as c_int)
}

/// Returned pointer stays valid until the next call.
#[no_mangle]
pub extern "C" fn bonk_status() -> *const c_char {
    let s = with_ed(|ed| ed.status());
    STATUS.with(|st| {
        *st.borrow_mut() = CString::new(s).unwrap_or_default();
        st.borrow().as_ptr()
    })
}

fn set_error(e: &str) {
    LAST_ERROR.with(|l| *l.borrow_mut() = CString::new(e.replace('\0', "")).unwrap_or_default());
}

fn path_arg(path: *const c_char) -> Option<String> {
    if path.is_null() {
        return None;
    }
    unsafe { std::ffi::CStr::from_ptr(path) }.to_str().ok().map(str::to_string)
}

/// Save to a .bpmdl file. Returns 0 on success; see bonk_last_error otherwise.
#[no_mangle]
pub extern "C" fn bonk_save(path: *const c_char) -> c_int {
    let Some(p) = path_arg(path) else { return 1 };
    match with_ed(|ed| io::save_file(ed, &p)) {
        Ok(()) => 0,
        Err(e) => {
            set_error(&e);
            1
        }
    }
}

/// Load a .bpmdl file. Returns 0 on success; see bonk_last_error otherwise.
#[no_mangle]
pub extern "C" fn bonk_load(path: *const c_char) -> c_int {
    let Some(p) = path_arg(path) else { return 1 };
    match with_ed(|ed| io::load_file(ed, &p)) {
        Ok((repaired, legacy)) => {
            let mut notes = Vec::new();
            if repaired > 0 {
                notes.push(format!("{} damaged brush(es) were rebuilt from their vertices", repaired));
            }
            if legacy > 0 {
                notes.push(format!("converted {} old untextured face{} to nodraw", legacy, if legacy == 1 { "" } else { "s" }));
            }
            if !notes.is_empty() {
                with_ed(|ed| ed.message = Some(format!("Loaded; {}", notes.join("; "))));
            }
            0
        }
        Err(e) => {
            set_error(&e);
            1
        }
    }
}

#[no_mangle]
pub extern "C" fn bonk_last_error() -> *const c_char {
    LAST_ERROR.with(|l| l.borrow().as_ptr())
}

#[no_mangle]
pub extern "C" fn bonk_is_dirty() -> c_int {
    with_ed(|ed| ed.doc.dirty as c_int)
}

/// Apply preferences from the Settings dialog.
#[no_mangle]
pub extern "C" fn bonk_set_config(nudge: c_int, swap_alt: c_int, grid_intensity: f32, r: f32, g: f32, b: f32, rot_snap: f32, hammer_keys: c_int) {
    with_ed(|ed| {
        ed.cfg = editor::Config {
            nudge: nudge != 0,
            swap_alt: swap_alt != 0,
            grid_intensity: grid_intensity.clamp(0.0, 2.0),
            bg: [r.clamp(0.0, 1.0), g.clamp(0.0, 1.0), b.clamp(0.0, 1.0)],
            rot_snap: rot_snap.clamp(1.0, 90.0),
            hammer_keys: hammer_keys != 0,
        };
    });
}

/// View (1-3) a hold-key action is steering, or -1.
#[no_mangle]
pub extern "C" fn bonk_hold_view() -> c_int {
    with_ed(|ed| ed.hold_view().map(|v| v as c_int).unwrap_or(-1))
}

/// The hold-key hint: returns its text (empty when nothing is held);
/// out = [view 0-3, kind] (kinds: see Editor::hold_hint).
#[no_mangle]
pub extern "C" fn bonk_hold_hint(out: *mut c_int) -> *const c_char {
    let h = with_ed(|ed| ed.hold_hint());
    let (v, k, t) = h.map(|(v, k, t)| (v as c_int, k, t)).unwrap_or((-1, 0, String::new()));
    if !out.is_null() {
        unsafe { std::ptr::copy_nonoverlapping([v, k].as_ptr(), out, 2) };
    }
    ret_str(t)
}

/// out = [ignore groups, box touches, axis lock, solid collision]
#[no_mangle]
pub extern "C" fn bonk_selection_options(out: *mut c_int) {
    let v = with_ed(|ed| [ed.ignore_groups as c_int, ed.sel_touch as c_int, ed.axis_lock as c_int, ed.collision_solid as c_int]);
    if !out.is_null() {
        unsafe { std::ptr::copy_nonoverlapping(v.as_ptr(), out, 4) };
    }
}

#[no_mangle]
pub extern "C" fn bonk_set_axis_lock(on: c_int) {
    with_ed(|ed| ed.axis_lock = on != 0);
}

#[no_mangle]
pub extern "C" fn bonk_set_collision_solid(on: c_int) {
    with_ed(|ed| ed.collision_solid = on != 0);
}

/// The selection box waits for Enter (1, the old way) or selects on release (0).
#[no_mangle]
pub extern "C" fn bonk_set_sel_box_wait(on: c_int) {
    with_ed(|ed| ed.sel_box_wait = on != 0);
}

/// 3D field of view in degrees (10-170; the host keeps it at 120 unless unlocked).
#[no_mangle]
pub extern "C" fn bonk_set_fov(deg: f32) {
    with_ed(|ed| ed.fov = deg.clamp(10.0, 170.0));
}

/// The "dropped" ripple is animating (the host keeps repainting while it is).
#[no_mangle]
pub extern "C" fn bonk_animating() -> c_int {
    with_ed(|ed| ed.ripple.is_some() as c_int)
}

/// The Vertex tool's vertex select (↖) is on.
#[no_mangle]
pub extern "C" fn bonk_vertex_pick() -> c_int {
    with_ed(|ed| ed.vertex_pick as c_int)
}

/// The selection as clipboard text (empty if nothing is selected). `source`:
/// this project's textures folder, so another project can fetch them.
/// With `cut`, the selection is deleted afterwards (one undo step).
#[no_mangle]
pub extern "C" fn bonk_copy(source: *const c_char, cut: c_int) -> *const c_char {
    let src = path_arg(source).unwrap_or_default();
    let t = with_ed(|ed| {
        if ed.doc.selection.is_empty() {
            return String::new();
        }
        let t = io::write_clip(ed, &src);
        let n = ed.doc.selection.len();
        if cut != 0 {
            ed.doc.checkpoint();
            ed.doc.delete_selected();
            ed.vsel.clear();
            ed.message = Some(format!("Cut {} brush{}", n, if n == 1 { "" } else { "es" }));
        } else {
            ed.message = Some(format!("Copied {} brush{}", n, if n == 1 { "" } else { "es" }));
        }
        t
    });
    ret_str(t)
}

/// Paste clipboard text (at_mouse: under the mouse in a 2D view).
#[no_mangle]
pub extern "C" fn bonk_paste(text: *const c_char, at_mouse: c_int) -> u32 {
    let t = path_arg(text).unwrap_or_default();
    with_ed(|ed| {
        if let Err(e) = ed.paste(&t, at_mouse != 0) {
            ed.message = Some(format!("Can't paste: {e}"));
        }
        editor::flags::ALL | editor::flags::TOOL
    })
}

/// The selection as prefab text (the clipboard format, textures from `source`), "" if nothing is selected.
#[no_mangle]
pub extern "C" fn bonk_prefab_text(source: *const c_char) -> *const c_char {
    let src = path_arg(source).unwrap_or_default();
    let t = with_ed(|ed| if ed.doc.selection.is_empty() { String::new() } else { io::write_clip(ed, &src) });
    ret_str(t)
}

/// How high the selection sits above what's under it.
#[no_mangle]
pub extern "C" fn bonk_selection_lift() -> f32 {
    with_ed(|ed| ed.selection_lift())
}

/// Place prefab text in `view` at (x, y) (view -1: under the last mouse position).
/// `keep` keeps it `lift` units above the surface it lands on.
#[no_mangle]
pub extern "C" fn bonk_place_prefab(text: *const c_char, view: c_int, x: f32, y: f32, keep: c_int, lift: f32) -> u32 {
    let t = path_arg(text).unwrap_or_default();
    with_ed(|ed| {
        let at = (view >= 0).then_some((view as usize, x, y));
        if let Err(e) = ed.place_prefab(&t, at, (keep != 0).then_some(lift)) {
            ed.message = Some(format!("Can't place it: {e}"));
        }
        editor::flags::ALL | editor::flags::TOOL
    })
}

/// Tools → Find Overlapping Brushes: one line per pair, "a b volume duplicate".
#[no_mangle]
pub extern "C" fn bonk_overlap_scan() -> *const c_char {
    let t = with_ed(|ed| {
        let list: Vec<&doc::Brush> = ed.doc.brushes.iter().filter(|b| b.entity.is_none() && ed.pickable(b)).collect();
        let r = overlap::scan(&list, &|b| ed.doc.is_collision(b) as u8, 1000);
        r.iter().map(|o| format!("{} {} {:.1} {}\n", o.a, o.b, o.volume, o.duplicate as u8)).collect::<String>()
    });
    ret_str(t)
}

/// Select these brush ids (space-separated) and center the views on them.
#[no_mangle]
pub extern "C" fn bonk_select_ids(ids: *const c_char) -> u32 {
    let t = path_arg(ids).unwrap_or_default();
    with_ed(|ed| {
        let ids: Vec<u32> = t.split_whitespace().filter_map(|x| x.parse().ok()).filter(|&id| ed.doc.brush(id).is_some()).collect();
        if ids.is_empty() {
            ed.message = Some("Those brushes are gone (deleted or undone)".into());
            return editor::flags::ALL;
        }
        if ed.tool != editor::Tool::Select {
            ed.command(editor::cmd::TOOL_SELECT);
        }
        ed.doc.selection = ids;
        ed.command(editor::cmd::CENTER_2D) | editor::flags::ALL | editor::flags::TOOL
    })
}

/// What the Entity tool places: 0 mannequin, 1 car.
#[no_mangle]
pub extern "C" fn bonk_place_kind() -> c_int {
    with_ed(|ed| (ed.place == doc::Entity::Car) as c_int)
}

/// The group the selection is exactly (0 if none), for the Layers pane to
/// highlight. Cached until the selection or the groups change.
#[no_mangle]
pub extern "C" fn bonk_selected_group() -> c_int {
    thread_local! {
        static CACHE: RefCell<(u64, Vec<u32>, c_int)> = const { RefCell::new((u64::MAX, Vec::new(), 0)) };
    }
    with_ed(|ed| {
        CACHE.with(|c| {
            let mut c = c.borrow_mut();
            if c.0 != ed.doc.rev || c.1 != ed.doc.selection {
                *c = (ed.doc.rev, ed.doc.selection.clone(), ed.doc.selected_group().map(|g| g as c_int).unwrap_or(0));
            }
            c.2
        })
    })
}

/// The Select tool's mode: 0 point and click, 1 box select (instant), 2 box select (Enter).
#[no_mangle]
pub extern "C" fn bonk_sel_box_mode() -> c_int {
    with_ed(|ed| if !ed.sel_box_only { 0 } else if ed.sel_box_wait { 2 } else { 1 })
}

/// Select every face using `material` (switches to Face Select & Paint). Returns the count.
#[no_mangle]
pub extern "C" fn bonk_select_faces_with(material: *const c_char) -> c_int {
    let m = path_arg(material).unwrap_or_default();
    with_ed(|ed| ed.select_faces_with(&m) as c_int)
}

/// Replace `find` with `with` (selected_only: just the selected faces). Returns faces changed.
#[no_mangle]
pub extern "C" fn bonk_replace_material(find: *const c_char, with: *const c_char, selected_only: c_int) -> c_int {
    let (f, w) = (path_arg(find).unwrap_or_default(), path_arg(with).unwrap_or_default());
    with_ed(|ed| ed.replace_material(&f, &w, selected_only != 0) as c_int)
}

/// The materials on the selected faces, one per line.
#[no_mangle]
pub extern "C" fn bonk_selected_face_materials() -> *const c_char {
    ret_str(with_ed(|ed| ed.selected_face_materials().join("\n")))
}

/// A new collision layer: mode 0 complex (a copy of the visual model), 1 simplified
/// (detail size in units). Returns the pieces made.
#[no_mangle]
pub extern "C" fn bonk_new_collision(mode: c_int, detail: f32) -> c_int {
    with_ed(|ed| {
        let (_, n) = if mode == 1 { ed.doc.add_collision_simplified(detail) } else { ed.doc.add_collision_from_visual() };
        ed.message = Some(if mode == 1 {
            format!("New collision layer: simplified into {n} piece{}", if n == 1 { "" } else { "s" })
        } else {
            format!("New collision layer: a copy of the visual model ({n} brush{}). Simplify it into the physics hull", if n == 1 { "" } else { "es" })
        });
        n as c_int
    })
}

/// How many pieces a simplified collision model would have (out[1]: the complex count).
#[no_mangle]
pub extern "C" fn bonk_simplify_count(detail: f32, out: *mut c_int) -> c_int {
    let (s, c) = with_ed(|ed| {
        let v = ed.doc.visual_brushes();
        (collision::simplify(&v, detail).len(), v.len())
    });
    if !out.is_null() {
        unsafe { *out.add(1) = c as c_int };
    }
    s as c_int
}

/// The detail size simplified collision uses when compiling without a collision layer.
#[no_mangle]
pub extern "C" fn bonk_set_simplify_detail(d: f32) {
    with_ed(|ed| ed.simplify_detail = d.clamp(1.0, 512.0));
}

/// J (Join Brushes) joins exactly only (1) or wraps a hull when it must (0).
#[no_mangle]
pub extern "C" fn bonk_set_join_exact(on: c_int) {
    with_ed(|ed| ed.join_exact_only = on != 0);
}

thread_local! {
    /// Models loaded for spawn icons: path → (modified time, the model)
    static THUMB_MODELS: RefCell<std::collections::HashMap<String, (std::time::SystemTime, Box<Editor>)>> = RefCell::new(Default::default());
}

/// Run `f` on the model saved at `path` (loaded once, again when the file changes).
fn with_thumb_model<R>(path: &str, f: impl FnOnce(&Editor) -> R) -> Option<R> {
    let mtime = std::fs::metadata(path).and_then(|m| m.modified()).ok()?;
    THUMB_MODELS.with(|m| {
        let mut m = m.borrow_mut();
        let fresh = m.get(path).is_some_and(|(t, _)| *t == mtime);
        if !fresh {
            let mut ed = Box::new(Editor::new());
            io::load_file(&mut ed, path).ok()?;
            m.insert(path.to_string(), (mtime, ed));
        }
        Some(f(&m.get(path)?.1))
    })
}

/// The visible part of a saved model (faces that aren't nodraw, visual layers):
/// out = [min x, y, z, max x, y, z]. Returns 0, or -1 if it can't be read or is empty.
#[no_mangle]
pub extern "C" fn bonk_model_bounds(path: *const c_char, out: *mut f32) -> c_int {
    let Some(p) = path_arg(path) else { return -1 };
    let b = with_thumb_model(&p, |ed| {
        let mut bb: Option<(V3, V3)> = None;
        for br in ed.doc.brushes.iter().filter(|b| b.entity.is_none() && !ed.doc.is_collision(b)) {
            for f in br.faces.iter().filter(|f| f.material != geom::NODRAW) {
                for &i in &f.idx {
                    let v = br.verts[i];
                    bb = Some(match bb {
                        Some((l, h)) => (l.min(v), h.max(v)),
                        None => (v, v),
                    });
                }
            }
        }
        bb
    })
    .flatten();
    let Some((l, h)) = b else { return -1 };
    if !out.is_null() {
        unsafe { std::ptr::copy_nonoverlapping([l.x, l.y, l.z, h.x, h.y, h.z].as_ptr(), out, 6) };
    }
    0
}

thread_local! {
    /// Thumbnails get a floor grid under the model (the Workshop thumbnail snapper).
    static THUMB_GRID: std::cell::Cell<bool> = const { std::cell::Cell::new(false) };
}

/// Draw a floor grid under the model in thumbnails from now on (1) or not (0).
#[no_mangle]
pub extern "C" fn bonk_thumb_grid(on: c_int) {
    THUMB_GRID.with(|g| g.set(on != 0));
}

/// "name=option;…" (option "" = None) → bodygroup choices.
fn parse_choices(t: &str) -> Vec<(String, String)> {
    t.split(';').filter_map(|p| p.split_once('=')).map(|(a, b)| (a.to_string(), b.to_string())).collect()
}

/// The model as it compiles with these choices (see variants::compiled_view).
fn preview_doc(skin: c_int, choices: *const c_char, lod: c_int) -> doc::Document {
    let c = parse_choices(&path_arg(choices).unwrap_or_default());
    with_ed(|ed| variants::compiled_view(&ed.doc, skin.max(0) as usize, &c, lod.max(0) as usize))
}

/// View → Model Preview: draw the model as compiled (skin, bodygroup choices, LOD)
/// into the bound framebuffer, like a spawn icon. Returns triangles drawn, or -1.
#[no_mangle]
pub extern "C" fn bonk_render_preview(view: c_int, skin: c_int, choices: *const c_char, lod: c_int, yaw: f32, pitch: f32, dist: f32, tx: f32, ty: f32, tz: f32, fov: f32, w: c_int, h: c_int, grid: c_int) -> c_int {
    let d = preview_doc(skin, choices, lod);
    let target = v3(tx, ty, tz);
    let (sy, cy) = yaw.to_radians().sin_cos();
    let (sp, cp) = pitch.to_radians().sin_cos();
    let eye = target + v3(cy * cp, sy * cp, sp) * dist.max(1.0);
    let tris: usize = d.brushes.iter().flat_map(|b| b.faces.iter()).filter(|f| f.material != geom::NODRAW).map(|f| f.idx.len().saturating_sub(2)).sum();
    let ok = RENDERERS.with(|rs| {
        if let Some(r) = rs.borrow()[vid(view)].as_ref() {
            unsafe { r.render_thumb(&d, eye, target, fov.clamp(5.0, 120.0), w as f32, h as f32, grid != 0) };
            true
        } else {
            false
        }
    });
    if ok { tris as c_int } else { -1 }
}

/// Frame the previewed model: out = [target x, y, z, distance]. Returns 0, or -1 (nothing to show).
#[no_mangle]
pub extern "C" fn bonk_preview_fit(skin: c_int, choices: *const c_char, lod: c_int, yaw: f32, pitch: f32, fov: f32, aspect: f32, out: *mut f32) -> c_int {
    let d = preview_doc(skin, choices, lod);
    let pts: Vec<V3> = d.brushes.iter().flat_map(|b| b.faces.iter().filter(|f| f.material != geom::NODRAW).flat_map(move |f| f.idx.iter().map(move |&i| b.verts[i]))).collect();
    let Some((t, dist)) = thumb_fit(&pts, yaw, pitch, fov, aspect) else { return -1 };
    if !out.is_null() {
        unsafe { std::ptr::copy_nonoverlapping([t.x, t.y, t.z, dist].as_ptr(), out, 4) };
    }
    0
}

/// Bonk Pixel, the low-resolution interface font (a TrueType file), built into the binary.
#[no_mangle]
pub extern "C" fn bonk_pixel_font(len: *mut c_int) -> *const u8 {
    static FONT: &[u8] = include_bytes!("../assets/fonts/BonkPixel.ttf");
    if !len.is_null() {
        unsafe { *len = FONT.len() as c_int };
    }
    FONT.as_ptr()
}

/// Frame the saved model at `path` for a spawn icon seen from yaw / pitch with
/// `fov` (vertical, degrees) in a view `aspect` wide per unit tall: out =
/// [target x, y, z, distance], fitting its visible vertices (not just its box)
/// snugly. Returns 0, or -1.
#[no_mangle]
pub extern "C" fn bonk_model_fit(path: *const c_char, yaw: f32, pitch: f32, fov: f32, aspect: f32, out: *mut f32) -> c_int {
    let Some(p) = path_arg(path) else { return -1 };
    let r = with_thumb_model(&p, |ed| {
        let pts: Vec<V3> = ed
            .doc
            .brushes
            .iter()
            .filter(|b| b.entity.is_none() && !ed.doc.is_collision(b))
            .flat_map(|b| b.faces.iter().filter(|f| f.material != geom::NODRAW).flat_map(move |f| f.idx.iter().map(move |&i| b.verts[i])))
            .collect();
        thumb_fit(&pts, yaw, pitch, fov, aspect)
    })
    .flatten();
    let Some((t, d)) = r else { return -1 };
    if !out.is_null() {
        unsafe { std::ptr::copy_nonoverlapping([t.x, t.y, t.z, d].as_ptr(), out, 4) };
    }
    0
}

/// Target and distance that fit `pts` in a view `aspect` wide (per unit tall) from yaw / pitch:
/// the target is centered on the points' extent across the view, and the
/// distance is the least that keeps every point inside (with a small margin).
fn thumb_fit(pts: &[V3], yaw: f32, pitch: f32, fov: f32, aspect: f32) -> Option<(V3, f32)> {
    if pts.is_empty() {
        return None;
    }
    let (sy, cy) = yaw.to_radians().sin_cos();
    let (sp, cp) = pitch.to_radians().sin_cos();
    let e = v3(cy * cp, sy * cp, sp); // toward the eye
    let r = v3(-sy, cy, 0.0);
    let u = v3(-cy * sp, -sy * sp, cp);
    let t = (fov.clamp(5.0, 120.0).to_radians() * 0.5).tan() / 1.06;
    let tw = t * aspect.clamp(0.1, 10.0);
    // center on the extent across the view plane, then back off; a second pass
    // re-centers on the perspective extent
    let (mut lo, mut hi) = (pts[0], pts[0]);
    for p in pts {
        lo = lo.min(*p);
        hi = hi.max(*p);
    }
    let mut c = (lo + hi) * 0.5;
    let need = |c: V3| pts.iter().fold(8.0f32, |d, p| {
        let q = *p - c;
        d.max((q.dot(r).abs() / tw).max(q.dot(u).abs() / t) + q.dot(e))
    });
    let reach = (hi - lo).len() + 1.0;
    for _ in 0..8 {
        let d = need(c);
        // for this distance, slide across the view so the points' screen extent is
        // centered: (max + min) of (h - s) / depth falls as s grows, so bisect s
        let shift = |axis: V3| {
            let mid = |s: f32| {
                let (mut a, mut b) = (f32::MAX, f32::MIN);
                for p in pts {
                    let q = *p - c;
                    let v = (q.dot(axis) - s) / (d - q.dot(e)).max(1e-3);
                    a = a.min(v);
                    b = b.max(v);
                }
                a + b
            };
            let (mut s0, mut s1) = (-reach, reach);
            for _ in 0..40 {
                let m = (s0 + s1) * 0.5;
                if mid(m) > 0.0 { s0 = m } else { s1 = m }
            }
            (s0 + s1) * 0.5
        };
        let (sx, sy) = (shift(r), shift(u));
        c = c + r * sx + u * sy;
    }
    Some((c, need(c)))
}

/// Draw a spawn icon of the saved model at `path` into the bound framebuffer,
/// with `view`'s GL context current: orbiting `target` at yaw / pitch (degrees)
/// and distance, fov in degrees. Returns 0, or -1.
#[no_mangle]
pub extern "C" fn bonk_render_thumb(view: c_int, path: *const c_char, yaw: f32, pitch: f32, dist: f32, tx: f32, ty: f32, tz: f32, fov: f32, w: c_int, h: c_int) -> c_int {
    let Some(p) = path_arg(path) else { return -1 };
    let target = v3(tx, ty, tz);
    let (sy, cy) = yaw.to_radians().sin_cos();
    let (sp, cp) = pitch.to_radians().sin_cos();
    let eye = target + v3(cy * cp, sy * cp, sp) * dist.max(1.0);
    let ok = with_thumb_model(&p, |ed| {
        RENDERERS.with(|rs| {
            if let Some(r) = rs.borrow()[vid(view)].as_ref() {
                unsafe { r.render_thumb(&ed.doc, eye, target, fov.clamp(5.0, 120.0), w as f32, h as f32, THUMB_GRID.with(|g| g.get())) };
                true
            } else {
                false
            }
        })
    });
    if ok == Some(true) { 0 } else { -1 }
}

/// Block tool shape (0 block, 1 cylinder, 2 wedge, 3 spike, 4 orb) and faces count.
#[no_mangle]
pub extern "C" fn bonk_set_block_shape(shape: c_int, faces: c_int) -> u32 {
    with_ed(|ed| {
        ed.block_shape = shapes::Shape::from_index(shape);
        ed.block_faces = faces.clamp(2, 32) as u32;
        editor::flags::ALL
    })
}

/// Arch shape: the ring's thickness in units.
#[no_mangle]
pub extern "C" fn bonk_set_arch_thickness(t: f32) -> u32 {
    with_ed(|ed| {
        ed.arch_thickness = t.clamp(1.0, 4096.0);
        editor::flags::ALL
    })
}

/// out = [shape, faces as set, faces the shape really uses]
#[no_mangle]
pub extern "C" fn bonk_block_shape(out: *mut c_int) {
    let v = with_ed(|ed| [ed.block_shape.index(), ed.block_faces as c_int, ed.block_shape.clamp_faces(ed.block_faces) as c_int]);
    if !out.is_null() {
        unsafe { std::ptr::copy_nonoverlapping(v.as_ptr(), out, 3) };
    }
}

// ------------------------------------------------------------------ groups

#[no_mangle]
pub extern "C" fn bonk_group_count() -> c_int {
    with_ed(|ed| ed.doc.groups.len() as c_int)
}

/// out = [id, parent (0 = none), layer of its first brush, brush count]; returns the name.
#[no_mangle]
pub extern "C" fn bonk_group_info(i: c_int, out: *mut c_int) -> *const c_char {
    thread_local! {
        // the layers pane asks for every group in a row: work membership out once per change
        static IX: RefCell<(u64, usize, Option<doc::GroupIndex>)> = const { RefCell::new((0, 0, None)) };
    }
    let (vals, name) = with_ed(|ed| {
        let Some(g) = ed.doc.groups.get(i.max(0) as usize) else { return ([0; 4], String::new()) };
        let key = (ed.doc.rev, ed.doc.brushes.len());
        let members = IX.with(|c| {
            let mut c = c.borrow_mut();
            if c.2.is_none() || (c.0, c.1) != key {
                *c = (key.0, key.1, Some(ed.doc.group_index()));
            }
            c.2.as_ref().unwrap().members.get(&g.id).cloned().unwrap_or_default()
        });
        let layer = members.first().and_then(|&m| ed.doc.brush(m)).map(|b| b.layer).unwrap_or(0);
        ([g.id as c_int, g.parent.unwrap_or(0) as c_int, layer as c_int, members.len() as c_int], g.name.clone())
    });
    if !out.is_null() {
        unsafe { std::ptr::copy_nonoverlapping(vals.as_ptr(), out, 4) };
    }
    ret_str(name)
}

/// op: 0 select it, 1 duplicate it (the copy is selected), 2 ungroup it, 3 delete it
#[no_mangle]
pub extern "C" fn bonk_group_op(op: c_int, id: c_int) -> u32 {
    let g = id.max(0) as u32;
    with_ed(|ed| {
        if ed.doc.group(g).is_none() {
            return 0;
        }
        let members: Vec<u32> = ed.doc.group_members(g).into_iter().filter(|&m| ed.doc.brush(m).is_some_and(|b| ed.pickable(b))).collect();
        match op {
            0 => {
                if members.is_empty() {
                    ed.message = Some("Can't select that group: its brushes are hidden, ghosted or on a locked layer".into());
                }
                ed.doc.selection = members;
            }
            1 => {
                ed.doc.checkpoint();
                let all = ed.doc.group_members(g);
                ed.doc.selection = ed.doc.duplicate(&all);
                ed.message = Some("Duplicated in place: hold G (or Alt+drag) to move the copy".into());
            }
            2 => {
                ed.doc.ungroup(g);
            }
            3 => {
                ed.doc.selection = ed.doc.group_members(g);
                ed.doc.delete_selected();
            }
            _ => return 0,
        }
        editor::flags::ALL | editor::flags::TOOL
    })
}

#[no_mangle]
pub extern "C" fn bonk_group_rename(id: c_int, name: *const c_char) -> u32 {
    let n = path_arg(name).unwrap_or_default();
    with_ed(|ed| if ed.doc.rename_group(id.max(0) as u32, &n) { editor::flags::ALL } else { 0 })
}

/// Ghosted visual brushes' opacity while editing collision (0..1).
#[no_mangle]
pub extern "C" fn bonk_set_ghost_opacity(a: f32) {
    with_ed(|ed| ed.ghost_opacity = a.clamp(0.0, 1.0));
}

/// 1 when the active layer is a collision layer
#[no_mangle]
pub extern "C" fn bonk_editing_collision() -> c_int {
    with_ed(|ed| ed.editing_collision() as c_int)
}

/// 1 when "Ignore groups" (Ctrl+W) is on; 2 bit: nodraw shown in 2D
#[no_mangle]
pub extern "C" fn bonk_view_flags() -> c_int {
    with_ed(|ed| ed.ignore_groups as c_int | (ed.nodraw_2d as c_int) << 1)
}

/// Nodraw look: style 0 solid / 1 RGB cycle / 2 rainbow flow; color; intensity 0..1; stripe width px; speed
#[no_mangle]
pub extern "C" fn bonk_set_nodraw_look(style: c_int, r: f32, g: f32, b: f32, intensity: f32, width: f32, speed: f32, show_2d: c_int) {
    with_ed(|ed| {
        ed.nd_style = style.clamp(0, 2);
        ed.nd_col = [r, g, b];
        ed.nd_intensity = intensity.clamp(0.0, 1.0);
        ed.nd_width = width.clamp(2.0, 48.0);
        ed.nd_speed = speed.clamp(0.0, 4.0);
        ed.nodraw_2d = show_2d != 0;
    });
}

/// Rainbow flow: how many pixels one whole rainbow stretches over.
#[no_mangle]
pub extern "C" fn bonk_set_nodraw_flow(px: f32) {
    with_ed(|ed| ed.nd_flow = px.clamp(12.0, 600.0));
}

/// The selection box takes brushes it touches (1) or only ones fully inside (0).
#[no_mangle]
pub extern "C" fn bonk_set_sel_touch(on: c_int) {
    with_ed(|ed| ed.sel_touch = on != 0);
}

/// Budget: out = [triangles, peak triangles of one material, vertices, materials, collision pieces]
#[no_mangle]
pub extern "C" fn bonk_budget(out: *mut c_int) {
    let b = with_ed(|ed| ed.budget());
    if !out.is_null() {
        let v = [b.0 as c_int, b.1 as c_int, b.2 as c_int, b.3 as c_int, b.4 as c_int];
        unsafe { std::ptr::copy_nonoverlapping(v.as_ptr(), out, 5) };
    }
}

/// The costliest brushes for a meter (0 triangles, 1 vertices, 2 materials, 3 collision):
/// selects them when `select` is set; returns a short description of the top few.
#[no_mangle]
pub extern "C" fn bonk_budget_offenders(kind: c_int, select: c_int) -> *const c_char {
    let s = with_ed(|ed| {
        let list = ed.budget_offenders(kind);
        if select != 0 && !list.is_empty() {
            ed.doc.selection = list.iter().map(|x| x.0).collect();
            ed.message = Some(format!("Selected {} brush{} for that meter", list.len(), if list.len() == 1 { "" } else { "es" }));
        }
        let unit = match kind {
            0 => "triangles",
            1 => "vertices",
            2 => "",
            _ => "",
        };
        list.iter()
            .take(3)
            .map(|(id, c)| {
                let b = ed.doc.brush(*id);
                let layer = b.and_then(|b| ed.doc.layer(b.layer)).map(|l| l.name.clone()).unwrap_or_default();
                let grp = ed.doc.top_group(*id).and_then(|g| ed.doc.group(g)).map(|g| format!(" / {}", g.name)).unwrap_or_default();
                if unit.is_empty() { format!("brush #{} ({}{})", id, layer, grp) } else { format!("brush #{} ({}{}): {} {}", id, layer, grp, c, unit) }
            })
            .collect::<Vec<_>>()
            .join("\n")
    });
    ret_str(s)
}

/// Faces in this model using a material.
#[no_mangle]
pub extern "C" fn bonk_material_usage(name: *const c_char) -> c_int {
    let n = path_arg(name).unwrap_or_default();
    with_ed(|ed| ed.material_usage(&n) as c_int)
}

/// Point faces at a texture's new name (moved / renamed). Returns faces changed.
#[no_mangle]
pub extern "C" fn bonk_rename_material(old: *const c_char, new: *const c_char) -> c_int {
    let (Some(o), Some(n)) = (path_arg(old), path_arg(new)) else { return 0 };
    with_ed(|ed| ed.rename_material(&o, &n) as c_int)
}

/// Write the model's SMDs for studiomdl. `phys` may be "" (no physics file).
/// phys_mode: 0 collision layers, 1 use the visual brushes as collision, 2 a simplified
/// hull of the visual brushes (detail: bonk_set_simplify_detail).
/// out = [triangles, collision pieces, too-thin collision pieces left out]. Returns 0 or -1.
#[no_mangle]
pub extern "C" fn bonk_export_smd(reference: *const c_char, phys: *const c_char, phys_mode: c_int, out: *mut c_int) -> c_int {
    let Some(rp) = path_arg(reference) else { return -1 };
    let pp = path_arg(phys).unwrap_or_default();
    let (r, p) = with_ed(|ed| {
        let phys = if pp.is_empty() {
            (String::new(), 0, 0)
        } else if phys_mode == 2 {
            let pieces = collision::simplify(&ed.doc.visual_brushes(), ed.simplify_detail);
            export::physics_smd_from(ed, pieces.iter().collect())
        } else {
            export::physics_smd(ed, phys_mode == 1)
        };
        let v = export::variant_export(ed);
        ((v.reference.clone(), v.tris, v.untextured, v.smds), phys)
    });
    if let Err(e) = std::fs::write(&rp, &r.0) {
        set_error(&format!("couldn't write {}: {}", rp, e));
        return -1;
    }
    // bodygroup options and LODs go beside it
    let dir = std::path::Path::new(&rp).parent().map(|d| d.to_path_buf()).unwrap_or_default();
    for (name, text) in &r.3 {
        if let Err(e) = std::fs::write(dir.join(name), text) {
            set_error(&format!("couldn't write {}: {}", name, e));
            return -1;
        }
    }
    if !pp.is_empty() && p.1 > 0 {
        if let Err(e) = std::fs::write(&pp, p.0) {
            set_error(&format!("couldn't write {}: {}", pp, e));
            return -1;
        }
    }
    if !out.is_null() {
        let v = [r.1 as c_int, p.1 as c_int, p.2 as c_int];
        unsafe { std::ptr::copy_nonoverlapping(v.as_ptr(), out, 3) };
    }
    0
}

/// Collision pieces the collision layers would give (0 = no collision model).
#[no_mangle]
pub extern "C" fn bonk_collision_pieces() -> c_int {
    with_ed(|ed| export::physics_smd(ed, false).1 as c_int)
}

/// Materials the model uses, most-used first, one per line ("" lines = no material).
#[no_mangle]
pub extern "C" fn bonk_used_materials() -> *const c_char {
    let s = with_ed(|ed| export::used_materials(ed).into_iter().map(|m| m.0).collect::<Vec<_>>().join("\n"));
    ret_str(s)
}

/// The QC and materials the model's variants need, as lines:
/// "sequence\t<smd>", "alias\t<copy>\t<original>", "hidden" (the skins use the
/// invisible material), then "qc" and the QC lines after it.
#[no_mangle]
pub extern "C" fn bonk_variant_export_info() -> *const c_char {
    let t = with_ed(|ed| {
        let v = export::variant_export(ed);
        let mut o = format!("sequence\t{}\n", v.sequence);
        for (a, b) in &v.aliases {
            o += &format!("alias\t{a}\t{b}\n");
        }
        if v.hidden {
            o += "hidden\n";
        }
        o += "qc\n";
        o += &v.qc;
        o
    });
    ret_str(t)
}

// ------------------------------------------------------------------ variants

/// The model's variants as lines (tab-separated):
/// "skin\t<index>\t<name>\t<swaps>\t<hidden>" (index 1…), "active\t<index>",
/// "swap\t<skin>\t<from>\t<to>", "bg\t<name>\t<blank 0/1>\t<shown option>",
/// "opt\t<bodygroup>\t<group id>\t<option name>", "lod\t<distance>\t<detail>",
/// "group\t<id>\t<name>\t<bodygroup or empty>" for the selected group.
#[no_mangle]
pub extern "C" fn bonk_variants_info() -> *const c_char {
    let t = with_ed(|ed| {
        let d = &ed.doc;
        let v = &d.variants;
        let mut o = String::new();
        for (i, s) in v.skins.iter().enumerate() {
            o += &format!("skin\t{}\t{}\t{}\t{}\n", i + 1, s.name, s.swaps.len(), s.hidden.len());
            for (a, b) in &s.swaps {
                o += &format!("swap\t{}\t{a}\t{b}\n", i + 1);
            }
        }
        o += &format!("active\t{}\n", v.skin);
        for bg in d.bodygroups() {
            let shown = match d.shown_option(&bg) {
                Some(g) => bg.options.iter().find(|x| x.0 == g).map(|x| x.1.clone()).unwrap_or_default(),
                None => String::new(),
            };
            o += &format!("bg\t{}\t{}\t{}\n", bg.name, bg.blank as u8, shown);
            for (g, n) in &bg.options {
                o += &format!("opt\t{}\t{g}\t{n}\n", bg.name);
            }
        }
        for l in &v.lods {
            o += &format!("lod\t{}\t{}\n", l.distance, l.detail);
        }
        if let Some(g) = d.selected_group().and_then(|g| d.groups.iter().find(|x| x.id == g)) {
            o += &format!("group\t{}\t{}\t{}\n", g.id, g.name, g.bodygroup.clone().unwrap_or_default());
        }
        o
    });
    ret_str(t)
}

/// Skins: op 0 add (copy of `index`), 1 remove `index`, 2 make `index` active,
/// 3 rename `index` to `text`, 4 hide the selection in the active skin, 5 show it,
/// 6 drop the swap of material `text` from skin `index`.
#[no_mangle]
pub extern "C" fn bonk_skin_op(op: c_int, index: c_int, text: *const c_char) -> u32 {
    let t = path_arg(text).unwrap_or_default();
    with_ed(|ed| {
        let i = index.max(0) as usize;
        match op {
            0 => {
                let n = ed.doc.add_skin(i);
                ed.message = Some(format!("Skin {n} added: paint faces to swap their materials in it"));
            }
            1 => ed.doc.remove_skin(i),
            2 => {
                ed.doc.variants.skin = i.min(ed.doc.variants.skins.len());
                ed.doc.rev += 1;
            }
            3 => {
                if let Some(s) = i.checked_sub(1).and_then(|k| ed.doc.variants.skins.get(k)).cloned() {
                    if !t.trim().is_empty() && s.name != t.trim() {
                        ed.doc.checkpoint();
                        ed.doc.variants.skins[i - 1].name = t.trim().to_string();
                    }
                }
            }
            4 | 5 => {
                let ids = ed.doc.selection.clone();
                if ids.is_empty() {
                    ed.message = Some("Select the brushes to hide in this skin first".into());
                } else if ed.doc.skin_hide(&ids, op == 4) {
                    if op == 4 {
                        ed.doc.selection.clear();
                    }
                    ed.message = Some(format!("{} brush{} {} in this skin", ids.len(), if ids.len() == 1 { "" } else { "es" }, if op == 4 { "hidden" } else { "shown" }));
                } else {
                    ed.message = Some("Pick a skin other than skin 0 to hide brushes in".into());
                }
            }
            6 => {
                if let Some(k) = i.checked_sub(1).filter(|k| *k < ed.doc.variants.skins.len()) {
                    ed.doc.checkpoint();
                    ed.doc.variants.skins[k].swaps.retain(|(a, _)| *a != t);
                }
            }
            7 => {
                // show every brush the active skin hides
                if let Some(k) = ed.doc.variants.skin.checked_sub(1).filter(|k| *k < ed.doc.variants.skins.len()) {
                    ed.doc.checkpoint();
                    ed.doc.variants.skins[k].hidden.clear();
                }
            }
            _ => {}
        }
        editor::flags::ALL
    })
}

/// Bodygroups: op 0 mark group `id` as an option of bodygroup `text` ("" unmarks),
/// 1 add the None option to bodygroup `text`, 2 remove it, 3 show option `text2` of bodygroup `text`.
#[no_mangle]
pub extern "C" fn bonk_bodygroup_op(op: c_int, id: c_int, text: *const c_char, text2: *const c_char) -> u32 {
    let a = path_arg(text).unwrap_or_default();
    let b = path_arg(text2).unwrap_or_default();
    with_ed(|ed| {
        match op {
            0 => {
                ed.doc.set_bodygroup(id.max(0) as u32, &a);
                ed.message = Some(if a.trim().is_empty() { "No longer a bodygroup option".into() } else { format!("Now an option of the bodygroup {}", a.trim()) });
            }
            1 | 2 => ed.doc.set_bodygroup_blank(&a, op == 1),
            3 => ed.doc.show_option(&a, &b),
            _ => {}
        }
        editor::flags::ALL
    })
}

/// LODs as "distance detail" pairs separated by ';' (replaces them all).
#[no_mangle]
pub extern "C" fn bonk_set_lods(text: *const c_char) -> u32 {
    let t = path_arg(text).unwrap_or_default();
    with_ed(|ed| {
        let lods: Vec<variants::Lod> = t
            .split(';')
            .filter_map(|p| {
                let mut it = p.split_whitespace().filter_map(|x| x.parse::<f32>().ok());
                Some(variants::Lod { distance: it.next()?, detail: it.next()? })
            })
            .collect();
        if lods != ed.doc.variants.lods {
            ed.doc.checkpoint();
            ed.doc.variants.lods = lods;
        }
        editor::flags::ALL
    })
}

/// Triangles LOD `i` (1…) keeps of the visual model, and of the whole: out = [kept, all].
#[no_mangle]
pub extern "C" fn bonk_lod_tris(detail: f32, out: *mut c_int) {
    let v = with_ed(|ed| {
        let mut all = 0usize;
        let mut kept = 0usize;
        for b in ed.doc.brushes.iter().filter(|b| b.entity.is_none() && !ed.doc.is_collision(b)) {
            let t: usize = b.faces.iter().filter(|f| f.material != geom::NODRAW).map(|f| f.idx.len().saturating_sub(2)).sum();
            all += t;
            if doc::Document::lod_keeps(b, detail) {
                kept += t;
            }
        }
        [kept as c_int, all as c_int]
    });
    if !out.is_null() {
        unsafe { std::ptr::copy_nonoverlapping(v.as_ptr(), out, 2) };
    }
}

/// Current grid size in units.
#[no_mangle]
pub extern "C" fn bonk_grid() -> f32 {
    with_ed(|ed| ed.grid)
}

/// 1 when brush outlines show in the 3D view.
#[no_mangle]
pub extern "C" fn bonk_show_edges() -> c_int {
    with_ed(|ed| ed.show_edges_3d as c_int)
}

#[no_mangle]
pub extern "C" fn bonk_set_show_edges(on: c_int) {
    with_ed(|ed| ed.show_edges_3d = on != 0);
}

/// Test helper: the "+ Collision" button.
#[cfg(test)]
pub fn bonk_test_add_collision(ed: &mut editor::Editor) -> u32 {
    ed.doc.add_collision_from_visual().0
}

/// Hold Shift and move to clone the selection (2D views).
#[no_mangle]
pub extern "C" fn bonk_set_shift_clone(on: c_int) {
    with_ed(|ed| ed.shift_clone = on != 0);
}

/// Texture Lock and "scale texture with brush".
#[no_mangle]
pub extern "C" fn bonk_set_texlock(lock: c_int, scale: c_int) {
    with_ed(|ed| {
        ed.tex_lock = lock != 0;
        ed.tex_scale = scale != 0;
    });
}

// ------------------------------------------------------------------ layers

/// Hands a string to C++; valid until the next string-returning call.
fn ret_str(s: String) -> *const c_char {
    STR_BUF.with(|b| {
        *b.borrow_mut() = CString::new(s.replace('\0', "")).unwrap_or_default();
        b.borrow().as_ptr()
    })
}

/// Changes whenever the layers pane should refresh.
#[no_mangle]
pub extern "C" fn bonk_layers_rev() -> u64 {
    with_ed(|ed| ed.doc.rev)
}

#[no_mangle]
pub extern "C" fn bonk_layer_count() -> c_int {
    with_ed(|ed| ed.doc.layers.len() as c_int)
}

/// Info for layer `i`: id, kind (0 visual / 1 collision), visible, locked,
/// active, brush count. Returns the name.
#[no_mangle]
pub extern "C" fn bonk_layer_info(i: c_int, out: *mut c_int) -> *const c_char {
    let (vals, name) = with_ed(|ed| {
        let Some(l) = ed.doc.layers.get(i.max(0) as usize) else { return ([0; 6], String::new()) };
        let count = ed.doc.brushes.iter().filter(|b| b.layer == l.id).count() as c_int;
        (
            [l.id as c_int, (l.kind == doc::LayerKind::Collision) as c_int, l.visible as c_int, l.locked as c_int, (ed.doc.active_layer == l.id) as c_int, count],
            l.name.clone(),
        )
    });
    if !out.is_null() {
        unsafe { std::ptr::copy_nonoverlapping(vals.as_ptr(), out, 6) };
    }
    ret_str(name)
}

/// op: 0 add visual, 1 add collision, 2 delete, 3 set active, 4 move selection
/// here, 5 copy selection here, 6 select contents, 7 toggle visible, 8 toggle locked.
#[no_mangle]
pub extern "C" fn bonk_layer_op(op: c_int, id: c_int) -> u32 {
    use editor::flags;
    let id = id.max(0) as u32;
    with_ed(|ed| {
        let d = &mut ed.doc;
        match op {
            0 => {
                d.add_layer(doc::LayerKind::Visual);
            }
            1 => {
                let (_, n) = d.add_collision_from_visual();
                ed.message = Some(if n > 0 {
                    format!("New collision layer: started as a copy of the visual model ({n} brush{}). Simplify it into the physics hull", if n == 1 { "" } else { "es" })
                } else {
                    "New collision layer".into()
                });
            }
            2 => {
                if !d.delete_layer(id) {
                    ed.message = Some("The last visual layer can't be deleted".into());
                }
            }
            3 => {
                if d.layer(id).is_some() {
                    d.active_layer = id;
                    d.rev += 1;
                }
            }
            4 => {
                let n = d.move_selection_to_layer(id);
                ed.message = Some(format!("Moved {n} item(s) to the layer"));
            }
            5 => {
                let n = d.copy_selection_to_layer(id);
                ed.message = Some(format!("Copied {n} item(s) to the layer"));
            }
            6 => d.select_layer(id),
            9 => {
                // own origin on / off
                let model = d.origin;
                if let Some(l) = d.layers.iter().find(|l| l.id == id).cloned() {
                    d.checkpoint();
                    if let Some(x) = d.layers.iter_mut().find(|x| x.id == id) {
                        x.origin = if l.origin.is_some() { None } else { Some(model) };
                    }
                    ed.message = Some(if l.origin.is_some() { format!("{} uses the model origin again", l.name) } else { format!("{} has its own origin: make it active and use the Origin tool (5) to place it", l.name) });
                }
            }
            7 | 8 => {
                if let Some(l) = d.layer(id).cloned() {
                    let (vis, lock) = if op == 7 { (!l.visible, l.locked) } else { (l.visible, !l.locked) };
                    d.set_layer_flags(id, vis, lock);
                }
            }
            _ => return 0,
        }
        flags::ALL
    })
}

#[no_mangle]
pub extern "C" fn bonk_layer_rename(id: c_int, name: *const c_char) -> u32 {
    let Some(n) = path_arg(name) else { return 0 };
    with_ed(|ed| ed.doc.rename_layer(id.max(0) as u32, &n));
    editor::flags::ALL
}

// ------------------------------------------------------------------ materials

/// Apply a material to the texture utility's targets ("" clears).
#[no_mangle]
pub extern "C" fn bonk_apply_material(name: *const c_char) -> c_int {
    let m = path_arg(name).unwrap_or_default();
    with_ed(|ed| ed.apply_material(&m) as c_int)
}

/// What the texture utility would apply to, as a sentence.
#[no_mangle]
pub extern "C" fn bonk_material_target() -> *const c_char {
    let s = with_ed(|ed| ed.material_target_desc());
    ret_str(s)
}

/// The material the picker has selected (Face tool right-click applies it).
#[no_mangle]
pub extern "C" fn bonk_set_current_material(name: *const c_char) {
    let m = path_arg(name).unwrap_or_default();
    with_ed(|ed| ed.current_material = if m.is_empty() { crate::geom::NODRAW.to_string() } else { m });
}

#[no_mangle]
pub extern "C" fn bonk_current_material() -> *const c_char {
    let s = with_ed(|ed| ed.current_material.clone());
    ret_str(s)
}

/// Convert an RGBA8 image (rows top to bottom) to a VTF file.
/// flags: 1 = normal map, 2 = alpha test. Returns 0, or -1 (see bonk_last_error).
#[no_mangle]
pub extern "C" fn bonk_write_vtf(path: *const c_char, w: c_int, h: c_int, rgba: *const u8, flags: c_int) -> c_int {
    let Some(p) = path_arg(path) else { return -1 };
    if rgba.is_null() || w <= 0 || h <= 0 {
        set_error("bad image");
        return -1;
    }
    let px = unsafe { std::slice::from_raw_parts(rgba, w as usize * h as usize * 4) }.to_vec();
    let img = vtf::Image { w: w as usize, h: h as usize, px };
    match vtf::write(&p, &img, vtf::VtfOptions { normal: flags & 1 != 0, alpha_test: flags & 2 != 0 }) {
        Ok(()) => 0,
        Err(e) => {
            set_error(&e);
            -1
        }
    }
}

/// Face Select & Paint: values for the picker's Face section.
/// out = [count, shift u, shift v, scale u, scale v, rotation, image w, image h]; returns count (0 = none).
#[no_mangle]
pub extern "C" fn bonk_face_info(out: *mut f32) -> c_int {
    let info = with_ed(|ed| ed.face_info());
    let Some((n, t, (w, h))) = info else { return 0 };
    if !out.is_null() {
        let o = unsafe { std::slice::from_raw_parts_mut(out, 8) };
        o.copy_from_slice(&[n as f32, t.shift[0], t.shift[1], t.scale[0], t.scale[1], t.rot, w, h]);
    }
    n as c_int
}

/// field: 0/1 shift u/v, 2/3 scale u/v, 4 rotation
#[no_mangle]
pub extern "C" fn bonk_face_set(field: c_int, value: f32) -> u32 {
    with_ed(|ed| ed.face_set(field.max(0) as u32, value))
}

/// 0 left, 1 right, 2 top, 3 bottom, 4 center, 5 fit, 6 reset
#[no_mangle]
pub extern "C" fn bonk_face_justify(mode: c_int) -> u32 {
    with_ed(|ed| ed.face_justify(mode.max(0) as u32))
}

/// "Treat as one" for face texture edits
#[no_mangle]
pub extern "C" fn bonk_set_tex_group(on: c_int) {
    with_ed(|ed| ed.tex_group = on != 0);
}

/// Hand a decoded image (RGBA8, rows top to bottom) to the renderer.
#[no_mangle]
pub extern "C" fn bonk_texture_set(name: *const c_char, w: c_int, h: c_int, rgba: *const u8) {
    let Some(n) = path_arg(name) else { return };
    if rgba.is_null() || w <= 0 || h <= 0 {
        return;
    }
    let len = w as usize * h as usize * 4;
    let data = unsafe { std::slice::from_raw_parts(rgba, len) }.to_vec();
    texstore::set(&n, w as u32, h as u32, data);
}

#[no_mangle]
pub extern "C" fn bonk_texture_remove(name: *const c_char) {
    if let Some(n) = path_arg(name) {
        texstore::remove(&n);
    }
}

#[no_mangle]
pub extern "C" fn bonk_texture_clear() {
    texstore::clear();
}

/// This model's override of a material's properties ("" = none).
#[no_mangle]
pub extern "C" fn bonk_mat_override(name: *const c_char) -> *const c_char {
    let n = path_arg(name).unwrap_or_default();
    let s = with_ed(|ed| ed.doc.mat_overrides.iter().find(|o| o.0 == n).map(|o| o.1.clone()).unwrap_or_default());
    ret_str(s)
}

/// Set (or with "" remove) this model's override of a material's properties.
#[no_mangle]
pub extern "C" fn bonk_set_mat_override(name: *const c_char, props: *const c_char) {
    let n = path_arg(name).unwrap_or_default();
    let p = path_arg(props).unwrap_or_default().replace(['"', '\n', '\r'], " ");
    with_ed(|ed| {
        let o = &mut ed.doc.mat_overrides;
        let old = o.clone();
        o.retain(|x| x.0 != n);
        if !p.is_empty() {
            o.push((n, p));
        }
        if *o != old {
            ed.doc.dirty = true;
        }
    });
}

#[no_mangle]
pub extern "C" fn bonk_model_path() -> *const c_char {
    let s = with_ed(|ed| ed.doc.model_path.clone());
    ret_str(s)
}

#[no_mangle]
pub extern "C" fn bonk_mass() -> f32 {
    with_ed(|ed| ed.doc.mass)
}

/// The model's mass in kg (0 = automatic).
#[no_mangle]
pub extern "C" fn bonk_set_mass(kg: f32) {
    with_ed(|ed| {
        let m = if kg > 0.0 { kg.min(50000.0) } else { 0.0 };
        if ed.doc.mass != m {
            ed.doc.mass = m;
            ed.doc.dirty = true;
        }
    });
}

#[no_mangle]
pub extern "C" fn bonk_set_model_path(path: *const c_char) {
    let p = path_arg(path).unwrap_or_default();
    with_ed(|ed| {
        if ed.doc.model_path != p {
            ed.doc.model_path = p;
            ed.doc.dirty = true;
        }
    });
}

#[no_mangle]
pub extern "C" fn bonk_show_mannequins() -> c_int {
    with_ed(|ed| ed.show_entities as c_int)
}

/// How the 3D view draws a texture's alpha: 0 ignore, 1 cut-out below `reference`, 2 blend.
#[no_mangle]
pub extern "C" fn bonk_texture_alpha(name: *const c_char, mode: c_int, reference: f32) {
    let n = path_arg(name).unwrap_or_default();
    texstore::set_alpha(&n, mode.clamp(0, 2) as u8, reference);
}

/// How a material moves in the 3D view: frames stacked in its image (and their
/// fps), TextureScroll (rate in texture widths per second, angle in degrees),
/// and Sine pulsing (mode 0 off, 1 brightness, 2 alpha; min, max, period).
#[no_mangle]
pub extern "C" fn bonk_texture_anim(name: *const c_char, frames: c_int, fps: f32, scroll_rate: f32, scroll_angle: f32, pulse_mode: c_int, pmin: f32, pmax: f32, period: f32) {
    let n = path_arg(name).unwrap_or_default();
    let (sn, cs) = scroll_angle.to_radians().sin_cos();
    texstore::set_anim(
        &n,
        texstore::Anim {
            frames: frames.max(1) as u32,
            fps,
            scroll: [scroll_rate * cs, -scroll_rate * sn],
            pulse: [pmin, pmax, if pulse_mode == 0 { 0.0 } else { period.max(0.05) }],
            pulse_alpha: pulse_mode == 2,
        },
    );
}

/// A multi-frame VTF from frames stacked top to bottom in one image (each w × frame_h).
#[no_mangle]
pub extern "C" fn bonk_write_vtf_frames(path: *const c_char, w: c_int, frame_h: c_int, frames: c_int, rgba: *const u8, flags: c_int) -> c_int {
    let Some(p) = path_arg(path) else { return -1 };
    if rgba.is_null() || w <= 0 || frame_h <= 0 || frames <= 0 {
        set_error("bad image");
        return -1;
    }
    let one = w as usize * frame_h as usize * 4;
    let all = unsafe { std::slice::from_raw_parts(rgba, one * frames as usize) };
    let imgs: Vec<vtf::Image> = all.chunks(one).map(|c| vtf::Image { w: w as usize, h: frame_h as usize, px: c.to_vec() }).collect();
    match vtf::write_frames(&p, &imgs, vtf::VtfOptions { normal: flags & 1 != 0, alpha_test: flags & 2 != 0 }) {
        Ok(()) => 0,
        Err(e) => {
            set_error(&e);
            -1
        }
    }
}

// ------------------------------------------------------------------ VMF / VTF / VMT import

thread_local! {
    static VMF_REPORT: RefCell<String> = RefCell::new(String::new());
    static PIXELS: RefCell<Vec<u8>> = RefCell::new(Vec::new());
}

/// What's in a VMF file, one fact per line:
/// "solids N", "points N", "vg <id>\t<solids>\t<name>", "mat <name>\t<count>".
#[no_mangle]
pub extern "C" fn bonk_vmf_scan(path: *const c_char) -> *const c_char {
    let Some(p) = path_arg(path) else { return ret_str(String::new()) };
    let text = match std::fs::read(&p) {
        Ok(b) => String::from_utf8_lossy(&b).into_owned(),
        Err(e) => {
            set_error(&format!("couldn't open {}: {}", p, e));
            return ret_str(String::new());
        }
    };
    let s = vmf::scan(&text);
    let mut o = format!("solids {}\npoints {}\n", s.solids, s.point_entities);
    for v in &s.visgroups {
        o += &format!("vg {}\t{}\t{}\n", v.id, v.solids, v.name);
    }
    for (m, n) in &s.materials {
        o += &format!("mat {}\t{}\n", m, n);
    }
    ret_str(o)
}

/// Replace the open model with a VMF's brushes. `skip`: comma-separated
/// visgroup ids to leave out. 0 = done (see bonk_vmf_report), else an error.
#[no_mangle]
pub extern "C" fn bonk_vmf_import(path: *const c_char, skip: *const c_char) -> c_int {
    let Some(p) = path_arg(path) else { return 1 };
    let skip: Vec<u32> = path_arg(skip).unwrap_or_default().split(',').filter_map(|x| x.trim().parse().ok()).collect();
    let text = match std::fs::read(&p) {
        Ok(b) => String::from_utf8_lossy(&b).into_owned(),
        Err(e) => {
            set_error(&format!("couldn't open {}: {}", p, e));
            return 1;
        }
    };
    match vmf::import(&text, &skip) {
        Ok((doc, r)) => {
            with_ed(|ed| {
                ed.set_document(doc);
                // frame it: 2D views centered and zoomed to fit, camera backed off
                let (mut lo, mut hi) = (V3::default(), V3::default());
                for (i, b) in ed.doc.brushes.iter().enumerate() {
                    let (l, h) = b.bounds();
                    if i == 0 { lo = l; hi = h; } else { lo = lo.min(l); hi = hi.max(h); }
                }
                let c = (lo + hi) * 0.5;
                let size = (hi - lo).len().max(64.0);
                for v in ed.views.iter_mut() {
                    let (ha, va, _) = v.kind.axes();
                    v.cx = c.get(ha);
                    v.cy = c.get(va);
                    v.zoom = (v.w.min(v.h) * 0.8 / size).clamp(0.02, 64.0);
                }
                let k = size / 450.0;
                ed.cam.pos = v3(c.x - 320.0 * k.max(0.3), c.y - 320.0 * k.max(0.3), c.z + 256.0 * k.max(0.3));
                ed.cam.yaw = 45.0;
                ed.cam.pitch = -25.0;
                ed.message = Some(format!("Imported {} brush{} from {}", r.brushes, if r.brushes == 1 { "" } else { "es" },
                    std::path::Path::new(&p).file_name().map(|f| f.to_string_lossy().into_owned()).unwrap_or_default()));
            });
            VMF_REPORT.with(|v| {
                *v.borrow_mut() = format!(
                    "brushes {}\ncollision {}\ngroups {}\nskipped_tools {}\nskipped_entities {}\ndisplacements {}\npoint_entities {}\nbroken {}\nexcluded {}\n",
                    r.brushes, r.collision, r.groups, r.skipped_tools, r.skipped_entities, r.displacements, r.point_entities, r.broken, r.excluded
                )
            });
            0
        }
        Err(e) => {
            set_error(&e);
            1
        }
    }
}

/// Edit → Tidy Nested Groups: dissolve groups that only wrap one other group.
/// Undoable. Returns how many went.
#[no_mangle]
pub extern "C" fn bonk_tidy_groups() -> c_int {
    with_ed(|ed| {
        if vmf::wrapper_group(&ed.doc).is_none() {
            return 0;
        }
        let before = ed.doc.groups.len();
        ed.doc.checkpoint();
        vmf::collapse_wrapper_groups(&mut ed.doc);
        ed.doc.rev += 1;
        (before - ed.doc.groups.len()) as c_int
    })
}

#[no_mangle]
pub extern "C" fn bonk_vmf_report() -> *const c_char {
    ret_str(VMF_REPORT.with(|v| v.borrow().clone()))
}

/// A VTF's size without decoding it: out = [width, height, format]. 0 = ok.
#[no_mangle]
pub extern "C" fn bonk_vtf_info(data: *const u8, len: usize, out: *mut c_int) -> c_int {
    if data.is_null() || out.is_null() {
        return 1;
    }
    let d = unsafe { std::slice::from_raw_parts(data, len) };
    match vtfread::info(d) {
        Ok(i) => {
            unsafe {
                *out = i.w as c_int;
                *out.add(1) = i.h as c_int;
                *out.add(2) = i.format as c_int;
            }
            0
        }
        Err(e) => {
            set_error(&e);
            1
        }
    }
}

/// Decode a VTF's top image to RGBA8; the pixels stay valid until the next call.
#[no_mangle]
pub extern "C" fn bonk_vtf_decode(data: *const u8, len: usize, w: *mut c_int, h: *mut c_int) -> *const u8 {
    if data.is_null() || w.is_null() || h.is_null() {
        return std::ptr::null();
    }
    let d = unsafe { std::slice::from_raw_parts(data, len) };
    match vtfread::decode(d) {
        Ok(img) => {
            unsafe {
                *w = img.w as c_int;
                *h = img.h as c_int;
            }
            PIXELS.with(|p| {
                *p.borrow_mut() = img.px;
                p.borrow().as_ptr()
            })
        }
        Err(e) => {
            set_error(&e);
            std::ptr::null()
        }
    }
}

/// A VMT as "shader" on the first line, then "key\tvalue" lines (lowercase keys).
#[no_mangle]
pub extern "C" fn bonk_parse_vmt(text: *const c_char) -> *const c_char {
    let t = path_arg(text).unwrap_or_default();
    let Some(v) = vmf::parse_vmt(&t) else { return ret_str(String::new()) };
    let mut o = v.shader.clone() + "\n";
    for (k, val) in &v.params {
        o += &format!("{}\t{}\n", k, val.replace(['\n', '\t'], " "));
    }
    ret_str(o)
}

/// A texture path as VMTs write it, normalized ("Brick\\Wall.vtf" → "brick/wall").
#[no_mangle]
pub extern "C" fn bonk_clean_texture_path(p: *const c_char) -> *const c_char {
    ret_str(vmf::clean_path(&path_arg(p).unwrap_or_default()))
}

extern "C" {
    fn bonk_qt_run(argc: *mut c_int, argv: *mut *mut c_char) -> c_int;
}

// ------------------------------------------------------------------ crash log

/// ~/.local/share/bonk/crash.log (the Flatpak's own data folder there).
fn crash_log_path() -> std::path::PathBuf {
    let base = std::env::var_os("XDG_DATA_HOME")
        .map(std::path::PathBuf::from)
        .unwrap_or_else(|| std::path::PathBuf::from(std::env::var_os("HOME").unwrap_or_default()).join(".local/share"));
    base.join("bonk").join("crash.log")
}

fn write_crash(what: &str) {
    use std::io::Write;
    let p = crash_log_path();
    let _ = std::fs::create_dir_all(p.parent().unwrap());
    if let Ok(mut f) = std::fs::OpenOptions::new().create(true).append(true).open(&p) {
        let when = std::time::SystemTime::now().duration_since(std::time::UNIX_EPOCH).map(|d| d.as_secs()).unwrap_or(0);
        let model = with_ed_try(|ed| format!("{} brushes, {} groups, {} layers", ed.doc.brushes.len(), ed.doc.groups.len(), ed.doc.layers.len()));
        let _ = writeln!(f, "==== Bonk! {} crash (unix time {when})\n{what}\nmodel: {}\n{}\n",
            env!("CARGO_PKG_VERSION"), model.unwrap_or_else(|| "(busy)".into()), std::backtrace::Backtrace::force_capture());
    }
}

/// Like with_ed, but gives up instead of panicking if the editor is in use
/// (the crash may have happened inside it).
fn with_ed_try<R>(f: impl FnOnce(&Editor) -> R) -> Option<R> {
    EDITOR.with(|e| e.try_borrow().ok().map(|ed| f(&ed)))
}

/// Called from the Qt side's signal handler: record where it crashed.
#[no_mangle]
pub extern "C" fn bonk_crash_report(signal: c_int) {
    let name = match signal {
        11 => "SIGSEGV (bad memory access)",
        6 => "SIGABRT (aborted)",
        7 => "SIGBUS",
        8 => "SIGFPE",
        4 => "SIGILL",
        _ => "signal",
    };
    write_crash(&format!("crashed with {name}"));
}

#[no_mangle]
pub extern "C" fn bonk_crash_log_path() -> *const c_char {
    ret_str(crash_log_path().to_string_lossy().into_owned())
}

fn main() {
    std::panic::set_hook(Box::new(|info| {
        eprintln!("{info}");
        write_crash(&format!("panic: {info}"));
    }));
    let args: Vec<CString> = std::env::args().map(|a| CString::new(a).unwrap_or_default()).collect();
    let mut argv: Vec<*mut c_char> = args.iter().map(|a| a.as_ptr() as *mut c_char).collect();
    argv.push(std::ptr::null_mut());
    let mut argc = args.len() as c_int;
    let rc = unsafe { bonk_qt_run(&mut argc, argv.as_mut_ptr()) };
    drop(args);
    std::process::exit(rc);
}

#[cfg(test)]
mod bench {
    use super::*;
    use std::time::Instant;

    /// cargo test --release bench_big_import -- --ignored --nocapture
    #[test]
    #[ignore]
    fn bench_big_import() {
        let path = std::env::var("BONK_BENCH_VMF").unwrap_or_default();
        let text = std::fs::read_to_string(&path).unwrap();
        let t = Instant::now();
        let (doc, r) = vmf::import(&text, &[]).unwrap();
        println!("import: {:?} ({} brushes, {} groups)", t.elapsed(), r.brushes, r.groups);
        with_ed(|ed| ed.set_document(doc));
        let time = |name: &str, f: &mut dyn FnMut()| {
            let t = Instant::now();
            for _ in 0..10 {
                f();
            }
            println!("{name}: {:?} each", t.elapsed() / 10);
        };
        time("budget", &mut || { let mut o = [0; 5]; bonk_budget(o.as_mut_ptr()); });
        time("offenders0", &mut || { bonk_budget_offenders(0, 0); });
        time("offenders1", &mut || { bonk_budget_offenders(1, 0); });
        time("group infos", &mut || { let mut o = [0; 4]; for i in 0..bonk_group_count() { bonk_group_info(i, o.as_mut_ptr()); } });
        time("layer infos", &mut || { let mut o = [0; 6]; for i in 0..bonk_layer_count() { bonk_layer_info(i, o.as_mut_ptr()); } });
        time("material target", &mut || { bonk_material_target(); });
        time("status", &mut || { bonk_status(); });
        time("checkpoint", &mut || with_ed(|ed| ed.doc.checkpoint()));
        time("expand all to groups", &mut || with_ed(|ed| { let ids: Vec<u32> = ed.doc.brushes.iter().map(|b| b.id).collect(); ed.doc.expand_to_groups(&ids); }));
        time("prune groups", &mut || with_ed(|ed| ed.doc.prune_groups()));
        time("duplicate all", &mut || with_ed(|ed| { let ids: Vec<u32> = ed.doc.brushes.iter().take(300).map(|b| b.id).collect(); let n = ed.doc.duplicate(&ids); ed.doc.brushes.retain(|b| !n.contains(&b.id)); }));
        // a drag: select one brush, move it in the top view
        with_ed(|ed| {
            let id = ed.doc.brushes[5].id;
            ed.doc.selection = vec![id];
        });
        time("tick", &mut || { bonk_tick(0.016); });
        let t = Instant::now();
        with_ed(|ed| {
            for v in ed.views.iter_mut() { v.w = 800.0; v.h = 600.0; }
            let b = ed.doc.brushes[5].bounds();
            let c = (b.0 + b.1) * 0.5;
            let p = ed.views[1].project(c);
            ed.mouse_press(1, p.0, p.1, 1, 0);
            for k in 0..20 { ed.mouse_move(1, p.0 + k as f32 * 3.0, p.1, 0); }
            ed.mouse_release(1, p.0 + 60.0, p.1, 1);
        });
        println!("drag (20 moves): {:?}", t.elapsed());
        if let Ok(out) = std::env::var("BONK_BENCH_SAVE") {
            with_ed(|ed| io::save_file(ed, &out).unwrap());
        }
    }
}

#[cfg(test)]
mod bench2 {
    use super::*;
    use std::time::Instant;

    /// BONK_BENCH_MODEL=file.bpmdl cargo test --release hidden_layer_fuzz -- --ignored --nocapture
    #[test]
    #[ignore]
    fn hidden_layer_fuzz() {
        let path = std::env::var("BONK_BENCH_MODEL").unwrap_or_default();
        with_ed(|ed| io::load_file(ed, &path).unwrap());
        with_ed(|ed| {
            for v in ed.views.iter_mut() { v.w = 800.0; v.h = 600.0; }
            ed.persp_size = (800.0, 600.0);
        });
        for round in 0..2 {
            let t = Instant::now();
            bonk_layer_op(7, 1);
            println!("toggle {round}: {:?}", t.elapsed());
            for tool in [7, 8, 13, 15, 21, 20] {
                bonk_command(tool);
                let t = Instant::now();
                for view in 0..4 {
                    for k in 0..30 {
                        bonk_mouse(view, 2, 20.0 + k as f32 * 25.0, 300.0 + (k % 7) as f32 * 30.0, 0, 0);
                    }
                }
                let mut o = [0; 5];
                bonk_budget(o.as_mut_ptr());
                bonk_budget_offenders(0, 0);
                bonk_budget_offenders(1, 0);
                bonk_status();
                bonk_material_target();
                bonk_tick(0.016);
                println!("  tool {tool}: {:?}", t.elapsed());
            }
        }
    }
}

#[cfg(test)]
mod bench3 {
    use super::*;
    #[test]
    #[ignore]
    fn tidy_user_model() {
        let path = std::env::var("BONK_BENCH_MODEL").unwrap_or_default();
        with_ed(|ed| io::load_file(ed, &path).unwrap());
        let t = std::time::Instant::now();
        let n = bonk_tidy_groups();
        let left = with_ed(|ed| ed.doc.groups.len());
        println!("tidy: removed {n}, {left} left, {:?}", t.elapsed());
        with_ed(|ed| ed.doc.undo());
        println!("after undo: {}", with_ed(|ed| ed.doc.groups.len()));
    }

    #[test]
    #[ignore]
    fn simplify_user_model() {
        let path = std::env::var("BONK_BENCH_MODEL").unwrap_or_default();
        with_ed(|ed| io::load_file(ed, &path).unwrap());
        for d in [8.0f32, 16.0, 32.0] {
            let t = std::time::Instant::now();
            let mut out = [0 as c_int; 2];
            let n = bonk_simplify_count(d, out.as_mut_ptr());
            println!("simplify detail {d}: {n} pieces from {} in {:?}", out[1], t.elapsed());
        }
    }

    #[test]
    #[ignore]
    fn overlap_user_model() {
        let path = std::env::var("BONK_BENCH_MODEL").unwrap_or_default();
        with_ed(|ed| io::load_file(ed, &path).unwrap());
        let t = std::time::Instant::now();
        let r = unsafe { std::ffi::CStr::from_ptr(bonk_overlap_scan()) }.to_string_lossy().into_owned();
        println!("overlap: {} pairs in {:?}\n{}", r.lines().count(), t.elapsed(), r.lines().take(8).collect::<Vec<_>>().join("\n"));
    }
}

#[cfg(test)]
mod thumb_tests {
    use super::*;

    #[test]
    fn thumb_fit_frames_every_point_and_centers_them() {
        // an L-shaped spread of points
        let pts = [v3(0.0, 0.0, 0.0), v3(400.0, 0.0, 0.0), v3(0.0, 40.0, 0.0), v3(0.0, 0.0, 100.0), v3(400.0, 40.0, 10.0)];
        for (yaw, pitch, aspect) in [(-60.0f32, 20.0f32, 1.0f32), (30.0, 45.0, 1.0), (0.0, 0.0, 1.0), (-60.0, 20.0, 2.0), (30.0, 45.0, 0.5)] {
            let (c, d) = thumb_fit(&pts, yaw, pitch, 30.0, aspect).unwrap();
            let (sy, cy) = yaw.to_radians().sin_cos();
            let (sp, cp) = pitch.to_radians().sin_cos();
            let e = v3(cy * cp, sy * cp, sp);
            let r = v3(-sy, cy, 0.0);
            let u = v3(-cy * sp, -sy * sp, cp);
            let t = (15.0f32).to_radians().tan();
            let (mut x0, mut x1, mut y0, mut y1) = (f32::MAX, f32::MIN, f32::MAX, f32::MIN);
            for p in &pts {
                let q = *p - c;
                let depth = d - q.dot(e);
                let (x, y) = (q.dot(r) / depth / (t * aspect), q.dot(u) / depth / t);
                assert!(x.abs() <= 1.0 && y.abs() <= 1.0, "point off screen at {yaw},{pitch}: {x} {y}");
                x0 = x0.min(x);
                x1 = x1.max(x);
                y0 = y0.min(y);
                y1 = y1.max(y);
            }
            // snug: the larger extent nearly fills the view, and it's centered
            assert!((x1 - x0).max(y1 - y0) > 1.8, "too loose at {yaw},{pitch}: {}", (x1 - x0).max(y1 - y0));
            assert!(((x0 + x1) * 0.5).abs() < 0.05 && ((y0 + y1) * 0.5).abs() < 0.05, "off center at {yaw},{pitch}: {x0} {x1} {y0} {y1}");
        }
        assert!(thumb_fit(&[], 0.0, 0.0, 30.0, 1.0).is_none());
    }
}

/// Name color materials after this model while compiling ("" turns it off).
/// Returns the prefix used.
#[no_mangle]
pub extern "C" fn bonk_set_color_prefix(model_path: *const c_char) -> *const c_char {
    let mp = if model_path.is_null() { String::new() } else { unsafe { std::ffi::CStr::from_ptr(model_path) }.to_string_lossy().into_owned() };
    let p = export::color_prefix_for(&mp);
    export::COLOR_PREFIX.with(|c| *c.borrow_mut() = p.clone());
    thread_local! { static OUT: std::cell::RefCell<std::ffi::CString> = std::cell::RefCell::new(std::ffi::CString::default()); }
    OUT.with(|o| {
        *o.borrow_mut() = std::ffi::CString::new(p).unwrap_or_default();
        o.borrow().as_ptr()
    })
}
