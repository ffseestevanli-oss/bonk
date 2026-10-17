// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Steventechno and the Bonk! contributors
//! Editor state and input handling: tools, 2D views, 3D camera, mouselook.

use crate::doc::*;
use crate::tex::{self, TexMap};
use crate::texstore;
use crate::geom;
use crate::shapes::{self, Shape};
use crate::math::*;

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum ViewKind {
    Persp,
    Top,
    Front,
    Side,
}

impl ViewKind {
    pub fn from_id(i: i32) -> ViewKind {
        match i {
            1 => ViewKind::Top,
            2 => ViewKind::Front,
            3 => ViewKind::Side,
            _ => ViewKind::Persp,
        }
    }

    /// (horizontal axis, vertical axis, depth axis) for 2D views. Axis 0=X, 1=Y, 2=Z.
    pub fn axes(self) -> (usize, usize, usize) {
        match self {
            ViewKind::Top => (0, 1, 2),
            ViewKind::Front => (1, 2, 0),
            ViewKind::Side => (0, 2, 1),
            ViewKind::Persp => (0, 1, 2),
        }
    }

    pub fn name(self) -> &'static str {
        match self {
            ViewKind::Persp => "camera",
            ViewKind::Top => "top (x/y)",
            ViewKind::Front => "front (y/z)",
            ViewKind::Side => "side (x/z)",
        }
    }
}

pub struct View2D {
    pub kind: ViewKind,
    pub cx: f32,
    pub cy: f32,
    /// pixels per world unit
    pub zoom: f32,
    pub w: f32,
    pub h: f32,
}

impl View2D {
    fn new(kind: ViewKind, cy: f32) -> Self {
        View2D { kind, cx: 0.0, cy, zoom: 0.5, w: 400.0, h: 300.0 }
    }
    pub fn to_world(&self, x: f32, y: f32) -> (f32, f32) {
        (self.cx + (x - self.w * 0.5) / self.zoom, self.cy - (y - self.h * 0.5) / self.zoom)
    }
    pub fn to_screen(&self, h: f32, v: f32) -> (f32, f32) {
        ((h - self.cx) * self.zoom + self.w * 0.5, (self.cy - v) * self.zoom + self.h * 0.5)
    }
    /// Screen position of a 3D point.
    pub fn project(&self, p: V3) -> (f32, f32) {
        let (ha, va, _) = self.kind.axes();
        self.to_screen(p.get(ha), p.get(va))
    }
}

pub struct Camera {
    pub pos: V3,
    /// degrees, 0 = looking down +X, counter-clockwise seen from above
    pub yaw: f32,
    /// degrees, positive = looking up
    pub pitch: f32,
}

impl Camera {
    pub fn forward(&self) -> V3 {
        let (y, p) = (self.yaw.to_radians(), self.pitch.to_radians());
        v3(p.cos() * y.cos(), p.cos() * y.sin(), p.sin())
    }
    pub fn right(&self) -> V3 {
        self.forward().cross(v3(0.0, 0.0, 1.0)).norm()
    }
    pub fn up(&self) -> V3 {
        self.right().cross(self.forward())
    }
}

pub const FOV_DEG: f32 = 75.0;
/// How long the "dropped here" ripple lasts
pub const RIPPLE_SECS: f32 = 0.55;
/// Within this many pixels of where a move started, it sits exactly where it was
pub const MOVE_DEAD_ZONE_PX: f32 = 7.0;
/// How far a locked move goes before it picks its axis
pub const AXIS_COMMIT_PX: f32 = 10.0;
const LOOK_SENS: f32 = 0.18; // degrees per pixel
const FLY_SPEED: f32 = 600.0; // units per second
const TURN_SPEED: f32 = 100.0; // degrees per second (arrow keys)

/// User preferences (Settings → Configure Bonk!). Persisted by the Qt host.
#[derive(Clone, Debug, PartialEq)]
pub struct Config {
    /// Arrow keys nudge the selection in 2D views
    pub nudge: bool,
    /// Swap Alt+drag (free) and Alt+Shift+drag (on grid)
    pub swap_alt: bool,
    /// Grid line strength, 1.0 = default
    pub grid_intensity: f32,
    /// Viewport background
    pub bg: [f32; 3],
    /// Rotation snap step in degrees
    pub rot_snap: f32,
    /// Hammer-style tool keys (Shift+S/B/V/X) are enabled
    pub hammer_keys: bool,
}

impl Default for Config {
    fn default() -> Self {
        Config { nudge: true, swap_alt: false, grid_intensity: 1.0, bg: [0.0, 0.0, 0.0], rot_snap: 15.0, hammer_keys: false }
    }
}

impl Config {
    pub fn light_bg(&self) -> bool {
        0.2126 * self.bg[0] + 0.7152 * self.bg[1] + 0.0722 * self.bg[2] > 0.5
    }
    /// Color that stands out against the background (for grid lines, labels).
    pub fn contrast(&self) -> [f32; 3] {
        if self.light_bg() {
            [0.0, 0.0, 0.0]
        } else {
            [1.0, 1.0, 1.0]
        }
    }
}
pub const HANDLE_PX: f32 = 4.0; // half-size of a resize handle
pub const VERTEX_PX: f32 = 3.0; // half-size of a vertex handle

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Tool {
    Select,
    Block,
    Vertex,
    Bisect,
    /// Place an entity (which one: `Editor::place`)
    Entity,
    /// Place / drag the model origin
    Origin,
    /// Pick individual faces (3D view)
    Face,
}

/// What the handles around a selection do (Hammer: click a selected object to cycle).
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum HandleMode {
    Scale,
    Rotate,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
enum Target {
    Block,
    Selection,
    /// The Select tool's selection box
    SelBox,
}

enum Drag {
    /// `quick`: Quick Draw (right-drag): one grid unit deep, created on release
    NewBox { view: usize, start: (f32, f32), start_px: (f32, f32), ctrl: bool, quick: bool },
    Move { view: usize, target: Target, grab: (f32, f32), orig: (V3, V3), orig_brushes: Vec<Brush>, changed: bool, toggle_on_click: bool, clone: bool },
    /// R held in the Vertex tool with vertices selected: turn them about their center
    VRotate { view: usize, center: V3, start_x: f32, orig_brushes: Vec<Brush>, sel: Vec<(u32, V3)>, changed: bool },
    /// S held in the Vertex tool with vertices selected: scale them about their center
    VScale { view: usize, center: V3, center_px: (f32, f32), start_d: f32, orig_brushes: Vec<Brush>, sel: Vec<(u32, V3)>, changed: bool },
    Resize { view: usize, target: Target, hx: i8, hy: i8, orig: (V3, V3), orig_brushes: Vec<Brush>, changed: bool },
    /// `by_key`: started by holding R (ends when R is released, not on mouse up)
    Rotate { view: usize, center: (f32, f32), start_ang: f32, orig_brushes: Vec<Brush>, changed: bool, by_key: bool },
    Vertex { view: usize, grab: (f32, f32), anchor: V3, orig_brushes: Vec<Brush>, sel: Vec<(u32, V3)>, changed: bool },
    Marquee { view: usize, start: (f32, f32), cur: (f32, f32), ctrl: bool },
    Pan { view: usize, last: (f32, f32) },
    /// Right-drag look in 3D. In Face Select & Paint a right *click* (no
    /// drag) paints instead: `face_click` until it moves.
    Look { last: (f32, f32), start: (f32, f32), face_click: bool },
    /// Face Select & Paint, right button held: paint every face swept over (one undo step)
    PaintFaces { last: Option<(u32, usize)> },
    /// Alt+drag in the 3D view: slide the selection across a horizontal plane
    Move3D { plane_z: f32, start: V3, orig_brushes: Vec<Brush>, changed: bool },
    /// Hold S + move the mouse: resize from the corner nearest where S went down
    /// Select tool: drawing a selection box
    SelNew { view: usize, start: (f32, f32), start_px: (f32, f32) },
    SSize { view: usize, target: Target, orig: (V3, V3), orig_brushes: Vec<Brush>, grab: (f32, f32), hx: i8, hy: i8, changed: bool },
    /// Bisect tool: drawing a new cut line
    BisectNew { view: usize, start: (f32, f32), start_px: (f32, f32), ctrl: bool },
    /// Bisect tool: dragging one end of the cut line
    BisectEnd { view: usize, which: usize },
    /// Middle-drag in 3D: turntable orbit around `pivot`
    Orbit { last: (f32, f32), pivot: V3, dist: f32 },
    /// Shift+middle-drag in 3D: pan, `dist` sets the speed
    Pan3D { last: (f32, f32), dist: f32 },
    /// Shift+Ctrl+drag in 3D: paint-select. `mode` (select / deselect) is set
    /// by the first brush (or face) touched.
    Paint { mode: Option<bool> },
    /// Face tool: dragging on a face adjusts the selected faces' textures:
    /// mode 0 = shift (follows the mouse), 1 = rotate (Ctrl), 2 = scale (Shift).
    /// Until it moves it's a click (`ctrl_click`: toggle that face on release).
    TexDrag { start: (f32, f32), face: (u32, usize), mode: u8, orig: Vec<((u32, usize), TexMap)>, moved: bool, ctrl_click: bool },
    /// Origin tool: dragging the origin in a 2D view
    OriginDrag { view: usize, changed: bool },
}

/// What a hold of the S key in a 2D view has been used for.
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
enum SUse {
    Nothing,
    Arrows,
    Mouse,
}

#[derive(Clone, Copy, Debug)]
struct SHold {
    view: usize,
    start: (f32, f32),
    /// S went down with the cursor in this view and something to size
    /// (it doesn't have to be over the brush: outside grabs the nearest side)
    over: bool,
    used: SUse,
    shift_at_press: bool,
}

/// The pieces a pending bisect cut would produce.
pub struct BisectPreview {
    /// (original brush id, piece on the negative side, piece on the positive side)
    pub pieces: Vec<(u32, Option<Brush>, Option<Brush>)>,
    /// In Split & Delete mode: true if the positive-side pieces will be deleted
    pub delete_pos: bool,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Hit {
    None,
    Inside,
    Handle(i8, i8),
}

/// Bit flags returned to the Qt host after every event.
pub mod flags {
    pub const R3D: u32 = 1;
    pub const R2D: u32 = 2;
    pub const ALL: u32 = R3D | R2D;
    pub const CAP_ON: u32 = 8;
    pub const CAP_OFF: u32 = 16;
    pub const TOOL: u32 = 32;
    pub fn cursor(c: u32) -> u32 {
        c << 8
    }
}

pub mod cursor {
    pub const ARROW: u32 = 1;
    pub const CROSS: u32 = 2;
    pub const SIZE_ALL: u32 = 3;
    pub const SIZE_HOR: u32 = 4;
    pub const SIZE_VER: u32 = 5;
    pub const SIZE_FDIAG: u32 = 6;
    pub const SIZE_BDIAG: u32 = 7;
    pub const HAND: u32 = 8;
    pub const POINT: u32 = 9;
    pub const COPY: u32 = 10;
}

/// Qt key codes we care about.
pub mod key {
    pub const Z: i32 = 0x5a;
    pub const W: i32 = 0x57;
    pub const F: i32 = 0x46;
    pub const G: i32 = 0x47;
    pub const H: i32 = 0x48;
    pub const COMMA: i32 = 0x2C;
    pub const PERIOD: i32 = 0x2E;
    pub const A: i32 = 0x41;
    pub const S: i32 = 0x53;
    pub const D: i32 = 0x44;
    pub const Q: i32 = 0x51;
    pub const E: i32 = 0x45;
    pub const SPACE: i32 = 0x20;
    pub const ESCAPE: i32 = 0x0100_0000;
    pub const RETURN: i32 = 0x0100_0004;
    pub const ENTER: i32 = 0x0100_0005;
    pub const LEFT: i32 = 0x0100_0012;
    pub const UP: i32 = 0x0100_0013;
    pub const RIGHT: i32 = 0x0100_0014;
    pub const DOWN: i32 = 0x0100_0015;
    pub const SHIFT: i32 = 0x0100_0020;
    pub const ALT: i32 = 0x0100_0023;
    pub const R: i32 = 0x52;
}

/// Modifier bits as sent by the host.
#[allow(dead_code)]
pub mod mods {
    pub const SHIFT: i32 = 1;
    pub const CTRL: i32 = 2;
    pub const ALT: i32 = 4;
}

pub mod button {
    pub const LEFT: i32 = 1;
    pub const RIGHT: i32 = 2;
    pub const MIDDLE: i32 = 4;
}

pub mod cmd {
    pub const UNDO: i32 = 1;
    pub const REDO: i32 = 2;
    pub const DELETE: i32 = 3;
    pub const SELECT_ALL: i32 = 4;
    pub const GRID_SMALLER: i32 = 5;
    pub const GRID_LARGER: i32 = 6;
    pub const TOOL_SELECT: i32 = 7;
    pub const TOOL_BLOCK: i32 = 8;
    pub const NEW: i32 = 9;
    pub const CENTER_2D: i32 = 10;
    pub const RELEASE_CAPTURE: i32 = 11;
    pub const CREATE_BLOCK: i32 = 12;
    pub const TOOL_VERTEX: i32 = 13;
    pub const QUANTIZE: i32 = 14;
    /// Switch to Bisect; when already active, toggles Split / Split & Delete
    pub const TOOL_BISECT: i32 = 15;
    pub const BISECT_MODE_SPLIT: i32 = 16;
    pub const BISECT_MODE_DELETE: i32 = 17;
    pub const BISECT_APPLY: i32 = 18;
    /// Entity tool with the scale mannequin
    pub const TOOL_MANNEQUIN: i32 = 19;
    pub const TOOL_ORIGIN: i32 = 20;
    pub const TOOL_FACE: i32 = 21;
    pub const ORIGIN_TO_SEL_BOTTOM: i32 = 22;
    pub const ORIGIN_TO_SEL_CENTER: i32 = 23;
    pub const TOGGLE_MANNEQUINS: i32 = 24;
    pub const GROUP: i32 = 25;
    pub const UNGROUP: i32 = 26;
    pub const IGNORE_GROUPS: i32 = 27;
    pub const TOGGLE_NODRAW_2D: i32 = 28;
    pub const DESELECT: i32 = 29;
    pub const TOGGLE_EDGES_3D: i32 = 30;
    /// Bisect mode: Split & Delete removing the larger side
    pub const BISECT_MODE_DELETE_LARGER: i32 = 31;
    /// H: hide the selection; with nothing selected, show what was hidden
    pub const HIDE_TOGGLE: i32 = 32;
    /// Mirror the selection along the hovered view's horizontal / vertical axis
    pub const MIRROR_H: i32 = 33;
    pub const MIRROR_V: i32 = 34;
    /// Selection options: lock G / Shift moves to one axis
    pub const TOGGLE_AXIS_LOCK: i32 = 35;
    /// Selection options: the selection box takes what it touches / only what's inside
    pub const TOGGLE_BOX_TOUCH: i32 = 36;
    /// View → Collision Look: ghosted ↔ solid
    pub const TOGGLE_COLLISION_SOLID: i32 = 37;
    /// The Vertex tool's ↖ button: vertex select on / off (switches to the Vertex tool)
    pub const TOGGLE_VERTEX_PICK: i32 = 38;
    /// Measurement Buddies: place a car (HL2 jeep sized)
    pub const TOOL_CAR: i32 = 39;
    /// The Select button's right half: box select (again: instant ↔ Enter)
    pub const TOGGLE_SEL_BOX_MODE: i32 = 40;
    /// Ctrl+J: join the selected brushes (hull, or exact only, as Settings says)
    pub const JOIN: i32 = 41;
    /// Edit → Join Brushes (Hull) / (Exact)
    pub const JOIN_HULL: i32 = 42;
    pub const JOIN_EXACT: i32 = 43;
}

#[derive(Default)]
struct Keys {
    w: bool,
    a: bool,
    s: bool,
    d: bool,
    q: bool,
    e: bool,
    up: bool,
    down: bool,
    left: bool,
    right: bool,
    shift: bool,
    space: bool,
}

pub struct Editor {
    pub doc: Document,
    pub tool: Tool,
    pub grid: f32,
    /// 2D views, indexed by view id - 1 (1 = top, 2 = front, 3 = side)
    pub views: [View2D; 3],
    pub cam: Camera,
    pub captured: bool,
    /// Pending box of the block tool (not yet a brush)
    pub block_box: Option<(V3, V3)>,
    /// Vertex tool's vertex select (the ↖ button): clicks and boxes pick vertices on any brush
    pub vertex_pick: bool,
    /// What the Block tool makes, and how many sides round shapes get
    pub block_shape: Shape,
    pub block_faces: u32,
    /// (depth axis, horizontal axis) of the view the block box was drawn in
    pub block_axis: (usize, usize),
    /// Scale or rotate handles around the selection (selection tool)
    pub sel_mode: HandleMode,
    /// Selected vertices in the vertex tool, as (brush id, position)
    pub vsel: Vec<(u32, V3)>,
    /// Alt held without Shift: snapping disabled
    pub free: bool,
    /// Alt held (Alt+drag grabs and moves any brush)
    pub alt: bool,
    /// Shift held (Shift+drag clones)
    pub shift: bool,
    /// Ctrl held (keeps proportions while S-sizing with the mouse)
    pub ctrl: bool,
    s_hold: Option<SHold>,
    /// Bisect mode: false = Split, true = Split & Delete
    pub bisect_delete: bool,
    /// Split & Delete removes the larger side instead of the smaller one
    pub bisect_larger: bool,
    /// F presses on the pending cut: in Split & Delete an odd count flips the
    /// removed side; in Split it picks what stays selected (both / smaller / larger)
    pub bisect_flip: u8,
    /// Pending cut line: (view id, start, end) in that view's plane coordinates
    pub bisect_line: Option<(usize, (f32, f32), (f32, f32))>,
    pub cfg: Config,
    /// Draw (and allow picking) scale mannequins
    pub show_entities: bool,
    /// Entity the Entity tool places
    pub place: Entity,
    /// Selected faces in the Face tool: (brush id, face index)
    pub fsel: Vec<(u32, usize)>,
    /// Material the texture picker has selected (right-click in the Face tool applies it)
    pub current_material: String,
    /// Face whose texture and alignment right-click paints (Face Select & Paint)
    pub src_face: Option<(u32, usize)>,
    /// "Treat as one": face texture edits act on the selection as one surface
    pub tex_group: bool,
    /// Texture Lock: textures travel with brushes that move or rotate
    pub tex_lock: bool,
    /// Hold Shift and move the mouse in a 2D view to clone the selection
    pub shift_clone: bool,
    /// Ctrl+W: clicks pick single brushes instead of whole groups
    pub ignore_groups: bool,
    /// Select tool: the selection box drawn on empty space (Enter selects)
    pub sel_box: Option<(V3, V3)>,
    /// The selection box takes brushes it touches (not just ones fully inside)
    pub sel_touch: bool,
    /// The selection box waits for Enter (the old way) instead of selecting on release
    pub sel_box_wait: bool,
    /// The Select button's right half: every drag draws a selection box (never grabs a brush)
    pub sel_box_only: bool,
    /// What was selected before a Ctrl+box started (live boxes add to it)
    sel_box_base: Vec<u32>,
    /// G / Shift moves stay on one axis (the one you move along most); Alt flips it for a hold
    pub axis_lock: bool,
    axis_flip: bool,
    /// The axis a locked hold committed to (0 = the view's horizontal / world X,
    /// 1 = vertical / world Y); it stays for the whole hold
    axis_commit: Option<u8>,
    /// Brushes hidden with H (this session only, never saved)
    pub hidden: Vec<u32>,
    /// Collision brushes drawn solid, each in its own color, instead of orange glass
    pub collision_solid: bool,
    /// 3D view field of view, degrees
    pub fov: f32,
    /// "Dropped here" ripple: (bounds, start time)
    pub ripple: Option<((V3, V3), f32)>,
    mat_anim_acc: f32,
    /// Detail size for simplified collision made while compiling
    pub simplify_detail: f32,
    /// Ctrl+J only joins when the result is exactly the same shape (Settings → Editing)
    pub join_exact_only: bool,
    /// A hold (G / R) started in the 3D view: nothing to pin the cursor to
    hold_in_3d: bool,
    /// How visible ghosted (non-collision) brushes are while editing collision, 0..1
    pub ghost_opacity: f32,
    /// Nodraw faces get their striped fill in the 2D views too
    pub nodraw_2d: bool,
    /// Brush outlines in the 3D view (selected brushes always keep theirs)
    pub show_edges_3d: bool,
    /// Nodraw look: style 0 solid / 1 RGB cycle / 2 rainbow flow, color,
    /// intensity 0..1, stripe width in px, speed (cycles per second)
    pub nd_style: i32,
    pub nd_col: [f32; 3],
    pub nd_intensity: f32,
    pub nd_width: f32,
    pub nd_speed: f32,
    /// Rainbow flow: px for one whole rainbow
    pub nd_flow: f32,
    /// Seconds since start, for animated nodraw
    pub time: f32,
    anim_acc: f32,
    /// Shift went down on its own over a 2D view: (view, start pixel, cloning yet)
    shift_hold: Option<(usize, (f32, f32), bool)>,
    /// M held over a 2D view: (view, start pixel, moving yet)
    move_hold: Option<(usize, (f32, f32), bool)>,
    /// With Texture Lock, resizing a brush stretches its textures too
    pub tex_scale: bool,
    /// Pivot shown while orbiting the 3D view
    pub orbit_pivot: Option<V3>,
    /// Transient status message (e.g. why a vertex move was refused)
    pub message: Option<String>,
    /// Extents used for the depth axis when drawing a new box in a 2D view
    work: (V3, V3),
    drag: Option<Drag>,
    keys: Keys,
    pub persp_size: (f32, f32),
    hover: Option<(usize, f32, f32)>,
}

impl Default for Editor {
    fn default() -> Self {
        Self::new()
    }
}

impl Editor {
    pub fn new() -> Self {
        Editor {
            doc: Document::new(),
            tool: Tool::Block,
            grid: 16.0,
            views: [View2D::new(ViewKind::Top, 0.0), View2D::new(ViewKind::Front, 64.0), View2D::new(ViewKind::Side, 64.0)],
            cam: Camera { pos: v3(-320.0, -320.0, 256.0), yaw: 45.0, pitch: -25.0 },
            captured: false,
            block_box: None,
            vertex_pick: false,
            block_shape: Shape::Block,
            block_faces: 8,
            block_axis: (2, 0),
            sel_mode: HandleMode::Scale,
            vsel: Vec::new(),
            free: false,
            alt: false,
            shift: false,
            ctrl: false,
            s_hold: None,
            bisect_delete: false,
            bisect_larger: false,
            bisect_flip: 0,
            bisect_line: None,
            cfg: Config::default(),
            show_entities: true,
            place: Entity::Mannequin,
            fsel: Vec::new(),
            current_material: geom::NODRAW.to_string(),
            src_face: None,
            tex_group: false,
            tex_lock: true,
            shift_clone: true,
            ignore_groups: false,
            sel_box: None,
            sel_touch: true,
            sel_box_wait: false,
            sel_box_only: false,
            sel_box_base: Vec::new(),
            axis_lock: true,
            axis_flip: false,
            axis_commit: None,
            hidden: Vec::new(),
            collision_solid: false,
            fov: FOV_DEG,
            ripple: None,
            mat_anim_acc: 0.0,
            simplify_detail: 8.0,
            join_exact_only: false,
            hold_in_3d: false,
            ghost_opacity: 0.25,
            nodraw_2d: false,
            show_edges_3d: true,
            nd_style: 0,
            nd_col: [0.86, 0.74, 0.25],
            nd_intensity: 0.6,
            nd_width: 6.0,
            nd_speed: 0.25,
            nd_flow: 72.0,
            time: 0.0,
            anim_acc: 0.0,
            shift_hold: None,
            move_hold: None,
            tex_scale: false,
            orbit_pivot: None,
            message: None,
            work: (v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0)),
            drag: None,
            keys: Keys::default(),
            persp_size: (400.0, 300.0),
            hover: None,
        }
    }

    pub fn view(&self, id: usize) -> &View2D {
        &self.views[id - 1]
    }

    pub fn set_view_size(&mut self, id: usize, w: f32, h: f32) {
        if id == 0 {
            self.persp_size = (w.max(1.0), h.max(1.0));
        } else if id <= 3 {
            self.views[id - 1].w = w.max(1.0);
            self.views[id - 1].h = h.max(1.0);
        }
    }

    /// Snap to the grid unless Alt is held.
    fn snap(&self, v: f32) -> f32 {
        if self.free {
            (v * 100.0).round() / 100.0
        } else {
            snap(v, self.grid)
        }
    }

    /// Handle style for the current tool.
    pub fn handle_mode(&self) -> HandleMode {
        if self.tool == Tool::Select {
            self.sel_mode
        } else {
            HandleMode::Scale
        }
    }

    /// The box whose handles are shown in 2D views for the current tool.
    pub fn active_box(&self) -> Option<(V3, V3)> {
        match self.tool {
            Tool::Block => self.block_box.or_else(|| self.doc.selection_bounds()),
            Tool::Select => self.sel_box.or_else(|| self.doc.selection_bounds()),
            _ => None,
        }
    }

    /// What the model costs, as studiomdl will see it. Counts only what gets
    /// exported: faces on visual layers that aren't nodraw (mannequins and
    /// collision layers skipped). Returns
    /// (triangles, peak triangles of one material, vertices, materials, collision pieces).
    pub fn budget(&self) -> (usize, usize, usize, usize, usize) {
        let mut tris = 0;
        let mut verts = 0;
        let mut per_mat: Vec<(&str, usize)> = Vec::new();
        let mut pieces = 0;
        for b in self.doc.brushes.iter().filter(|b| b.entity.is_none()) {
            if self.doc.is_collision(b) {
                pieces += 1;
                continue;
            }
            for f in b.faces.iter().filter(|f| f.material != geom::NODRAW) {
                let t = f.idx.len().saturating_sub(2);
                tris += t;
                verts += f.idx.len();
                match per_mat.iter_mut().find(|m| m.0 == f.material) {
                    Some(m) => m.1 += t,
                    None => per_mat.push((&f.material, t)),
                }
            }
        }
        let peak = per_mat.iter().map(|m| m.1).max().unwrap_or(0);
        (tris, peak, verts, per_mat.len(), pieces)
    }

    /// The brushes that cost the most for a budget meter: 0 triangles (the
    /// brushes of the heaviest material, biggest first), 1 vertices, 2
    /// materials (one brush per material), 3 collision pieces.
    pub fn budget_offenders(&self, kind: i32) -> Vec<(u32, usize)> {
        let visual = |b: &&Brush| b.entity.is_none() && !self.doc.is_collision(b);
        let cost = |b: &Brush| b.faces.iter().filter(|f| f.material != geom::NODRAW).map(|f| f.idx.len().saturating_sub(2)).sum::<usize>();
        let mut out: Vec<(u32, usize)> = match kind {
            0 => {
                let mut per_mat: Vec<(String, usize)> = Vec::new();
                for b in self.doc.brushes.iter().filter(visual) {
                    for f in b.faces.iter().filter(|f| f.material != geom::NODRAW) {
                        match per_mat.iter_mut().find(|m| m.0 == f.material) {
                            Some(m) => m.1 += f.idx.len() - 2,
                            None => per_mat.push((f.material.clone(), f.idx.len() - 2)),
                        }
                    }
                }
                let Some(top) = per_mat.into_iter().max_by_key(|m| m.1).map(|m| m.0) else { return Vec::new() };
                self.doc
                    .brushes
                    .iter()
                    .filter(visual)
                    .map(|b| (b.id, b.faces.iter().filter(|f| f.material == top).map(|f| f.idx.len() - 2).sum::<usize>()))
                    .filter(|x| x.1 > 0)
                    .collect()
            }
            1 => self.doc.brushes.iter().filter(visual).map(|b| (b.id, b.faces.iter().filter(|f| f.material != geom::NODRAW).map(|f| f.idx.len()).sum::<usize>())).filter(|x| x.1 > 0).collect(),
            2 => {
                let mut seen: Vec<&str> = Vec::new();
                let mut v = Vec::new();
                for b in self.doc.brushes.iter().filter(visual) {
                    for f in b.faces.iter().filter(|f| f.material != geom::NODRAW) {
                        if !seen.contains(&f.material.as_str()) {
                            seen.push(&f.material);
                            v.push((b.id, cost(b)));
                        }
                    }
                }
                v
            }
            _ => self.doc.brushes.iter().filter(|b| b.entity.is_none() && self.doc.is_collision(b)).map(|b| (b.id, 1)).collect(),
        };
        out.sort_by(|a, b| b.1.cmp(&a.1));
        out
    }

    /// Faces using a material, in this model.
    pub fn material_usage(&self, m: &str) -> usize {
        self.doc.brushes.iter().map(|b| b.faces.iter().filter(|f| f.material == m).count()).sum()
    }

    /// A texture was moved or renamed: point faces (and overrides) at the new
    /// name. One undo step. Returns faces changed.
    /// Face Select & Paint with every face using `material` selected (brushes you
    /// can click; hidden and locked layers are left out). Returns the count.
    pub fn select_faces_with(&mut self, material: &str) -> usize {
        self.tool = Tool::Face;
        self.drag = None;
        let mut fsel = Vec::new();
        for b in self.doc.brushes.iter().filter(|b| b.entity.is_none() && self.pickable(b)) {
            for (i, f) in b.faces.iter().enumerate() {
                if f.material == material {
                    fsel.push((b.id, i));
                }
            }
        }
        self.fsel = fsel;
        self.src_face = self.fsel.first().copied();
        let n = self.fsel.len();
        self.message = Some(format!("{} face{} use {}", n, if n == 1 { "" } else { "s" }, material_display(material)));
        n
    }

    /// Put `with` where `find` is: on every face in the model (`selected_only`
    /// false) or only on the selected faces (an empty `find` then means all of
    /// them). Alignment stays. One undo step; returns how many faces changed.
    pub fn replace_material(&mut self, find: &str, with: &str, selected_only: bool) -> usize {
        let targets: Vec<(u32, usize)> = if selected_only {
            self.tex_targets()
                .into_iter()
                .filter(|k| find.is_empty() || self.doc.brush(k.0).and_then(|b| b.faces.get(k.1)).is_some_and(|f| f.material == find))
                .collect()
        } else {
            self.doc
                .brushes
                .iter()
                .filter(|b| b.entity.is_none())
                .flat_map(|b| b.faces.iter().enumerate().filter(|(_, f)| f.material == find).map(move |(i, _)| (b.id, i)))
                .collect()
        };
        if targets.is_empty() || find == with {
            self.message = Some("Nothing to replace".into());
            return 0;
        }
        self.doc.checkpoint();
        for &(bid, i) in &targets {
            if let Some(f) = self.doc.brush_mut(bid).and_then(|b| b.faces.get_mut(i)) {
                f.material = with.to_string();
            }
        }
        let n = targets.len();
        self.message = Some(format!("Replaced on {} face{}: now {}", n, if n == 1 { "" } else { "s" }, material_display(with)));
        n
    }

    /// The materials on the selected faces, in order, each once.
    pub fn selected_face_materials(&self) -> Vec<String> {
        let mut out: Vec<String> = Vec::new();
        for (bid, i) in self.tex_targets() {
            if let Some(f) = self.doc.brush(bid).and_then(|b| b.faces.get(i)) {
                if !out.contains(&f.material) {
                    out.push(f.material.clone());
                }
            }
        }
        out
    }

    pub fn rename_material(&mut self, old: &str, new: &str) -> usize {
        let n = self.material_usage(old);
        let has_override = self.doc.mat_overrides.iter().any(|o| o.0 == old);
        if n == 0 && !has_override && self.current_material != old {
            return 0;
        }
        if n > 0 || has_override {
            self.doc.checkpoint();
        }
        for b in &mut self.doc.brushes {
            for f in &mut b.faces {
                if f.material == old {
                    f.material = new.to_string();
                }
            }
        }
        for o in &mut self.doc.mat_overrides {
            if o.0 == old {
                o.0 = new.to_string();
            }
        }
        if self.current_material == old {
            self.current_material = new.to_string();
        }
        n
    }

    /// Brushes the selection box would select (whole groups unless ignored).
    pub fn sel_box_catch(&self) -> Vec<u32> {
        let Some((lo, hi)) = self.sel_box else { return Vec::new() };
        let ids: Vec<u32> = self
            .doc
            .brushes
            .iter()
            .filter(|b| self.pickable(b))
            .filter(|b| {
                let (bl, bh) = b.bounds();
                if self.sel_touch {
                    (0..3).all(|a| bh.get(a) >= lo.get(a) && bl.get(a) <= hi.get(a))
                } else {
                    (0..3).all(|a| bl.get(a) >= lo.get(a) - 1e-3 && bh.get(a) <= hi.get(a) + 1e-3)
                }
            })
            .map(|b| b.id)
            .collect();
        if self.ignore_groups || ids.is_empty() {
            return ids;
        }
        // a group counts when the box catches all of it
        let ix = self.doc.group_index();
        let caught: std::collections::HashSet<u32> = ids.iter().copied().collect();
        let mut out = Vec::new();
        for g in self.doc.expand_to_groups(&ids) {
            let whole_in = match ix.top.get(&g).and_then(|t| ix.members.get(t)) {
                Some(ms) => ms.iter().all(|m| caught.contains(m)),
                None => caught.contains(&g),
            };
            if self.sel_touch || whole_in {
                out.push(g);
            }
        }
        out
    }

    /// Enter with a selection box: select what it catches (Ctrl adds).
    /// Instant box: what the box touches is the selection, live, while it's up.
    fn live_sel_box(&mut self) {
        if self.tool != Tool::Select || !self.sel_box_only || self.sel_box_wait {
            return;
        }
        let mut sel = self.sel_box_base.clone();
        for c in self.sel_box_catch() {
            if !sel.contains(&c) {
                sel.push(c);
            }
        }
        if sel != self.doc.selection {
            self.doc.selection = sel;
            self.sel_mode = HandleMode::Scale;
        }
    }

    /// An instant box is up and live.
    fn live_box_up(&self) -> bool {
        self.tool == Tool::Select && self.sel_box_only && !self.sel_box_wait && self.sel_box.is_some()
    }

    fn apply_sel_box(&mut self, add: bool) -> u32 {
        let caught = self.sel_box_catch();
        if add {
            for c in caught {
                if !self.doc.is_selected(c) {
                    self.doc.selection.push(c);
                }
            }
        } else {
            self.doc.selection = caught;
        }
        self.sel_box = None;
        self.sel_mode = HandleMode::Scale;
        let n = self.doc.selection.len();
        self.message = Some(format!("{} brush{} selected", n, if n == 1 { "" } else { "es" }));
        flags::ALL
    }

    /// Drawn in the views: layer visible, and mannequins shown.
    pub fn drawable(&self, b: &Brush) -> bool {
        self.doc.layer_visible(b) && (b.entity.is_none() || self.show_entities) && !self.hidden.contains(&b.id)
    }

    /// Can be clicked / selected / edited.
    pub fn pickable(&self, b: &Brush) -> bool {
        self.doc.layer_editable(b) && (b.entity.is_none() || self.show_entities) && !self.ghosted(b) && !self.hidden.contains(&b.id)
    }

    /// Editing a collision layer: everything that isn't collision is a ghost
    /// (drawn see-through, can't be clicked).
    pub fn ghosted(&self, b: &Brush) -> bool {
        self.editing_collision() && !self.doc.is_collision(b)
    }

    pub fn editing_collision(&self) -> bool {
        self.doc.layer(self.doc.active_layer).is_some_and(|l| l.kind == LayerKind::Collision)
    }

    /// The origin the Origin tool edits: the active layer's own, if it has one.
    pub fn active_origin(&self) -> V3 {
        self.doc.layer(self.doc.active_layer).and_then(|l| l.origin).unwrap_or(self.doc.origin)
    }

    pub fn face_selected(&self, id: u32, face: usize) -> bool {
        self.fsel.contains(&(id, face))
    }

    /// Current marquee rectangle in screen pixels (vertex tool).
    pub fn marquee(&self) -> Option<(usize, (f32, f32), (f32, f32))> {
        match self.drag {
            Some(Drag::Marquee { view, start, cur, .. }) => Some((view, start, cur)),
            _ => None,
        }
    }

    pub fn vertex_selected(&self, id: u32, p: V3) -> bool {
        self.vsel.iter().any(|&(i, q)| i == id && (q - p).len() < 1e-3)
    }

    // ------------------------------------------------------------------ hit testing

    /// Hit-test a box's 2D projection (handles first, then interior) in screen pixels.
    pub fn hit_box(&self, id: usize, b: (V3, V3), x: f32, y: f32, mode: HandleMode) -> Hit {
        let v = self.view(id);
        let (ha, va, _) = v.kind.axes();
        let (x0, y0) = v.to_screen(b.0.get(ha), b.1.get(va));
        let (x1, y1) = v.to_screen(b.1.get(ha), b.0.get(va));
        let small = (x1 - x0) < HANDLE_PX * 6.0 || (y1 - y0) < HANDLE_PX * 6.0;
        let hs = HANDLE_PX + 2.0;
        for (hx, hy) in handle_list(small || mode == HandleMode::Rotate) {
            let px = [x0, (x0 + x1) * 0.5, x1][(hx + 1) as usize];
            // hy = +1 is the max vertical coord = top of screen
            let py = [y1, (y0 + y1) * 0.5, y0][(hy + 1) as usize];
            if (x - px).abs() <= hs && (y - py).abs() <= hs {
                return Hit::Handle(hx, hy);
            }
        }
        if x >= x0 && x <= x1 && y >= y0 && y <= y1 {
            Hit::Inside
        } else {
            Hit::None
        }
    }

    /// Pick the brush under a point in a 2D view (smallest projected area wins).
    fn pick_2d(&self, id: usize, x: f32, y: f32) -> Option<u32> {
        let v = self.view(id);
        let (ha, va, _) = v.kind.axes();
        let p = v.to_world(x, y);
        let tol = 2.0 / v.zoom;
        let mut best: Option<(f32, u32)> = None;
        for b in self.doc.brushes.iter().filter(|b| self.pickable(b)) {
            let poly = geom::hull2d(&b.verts.iter().map(|q| (q.get(ha), q.get(va))).collect::<Vec<_>>());
            if geom::in_convex2d(&poly, p, tol) {
                let a = geom::area2d(&poly);
                if best.map_or(true, |(ba, _)| a < ba) {
                    best = Some((a, b.id));
                }
            }
        }
        best.map(|(_, id)| id)
    }

    /// Ray through a pixel of the 3D view.
    pub fn persp_ray(&self, x: f32, y: f32) -> (V3, V3) {
        let (w, h) = self.persp_size;
        let nx = 2.0 * x / w - 1.0;
        let ny = 1.0 - 2.0 * y / h;
        let t = (self.fov.to_radians() * 0.5).tan();
        let dir = (self.cam.forward() + self.cam.right() * (nx * t * (w / h)) + self.cam.up() * (ny * t)).norm();
        (self.cam.pos, dir)
    }

    /// Where a world point lands in the 3D view, in pixels (the inverse of persp_ray).
    #[allow(dead_code)]
    pub fn persp_project(&self, p: V3) -> Option<(f32, f32)> {
        let (w, h) = self.persp_size;
        let d = p - self.cam.pos;
        let z = d.dot(self.cam.forward());
        if z <= 1e-3 {
            return None;
        }
        let t = (self.fov.to_radians() * 0.5).tan();
        let nx = d.dot(self.cam.right()) / z / (t * (w / h));
        let ny = d.dot(self.cam.up()) / z / t;
        Some(((nx + 1.0) * 0.5 * w, (1.0 - ny) * 0.5 * h))
    }

    #[allow(dead_code)]
    pub fn persp_mvp(&self) -> M4 {
        let (w, h) = self.persp_size;
        let proj = M4::perspective(self.fov.to_radians(), w / h, 1.0, 65536.0);
        let view = M4::look_at(self.cam.pos, self.cam.pos + self.cam.forward(), v3(0.0, 0.0, 1.0));
        proj.mul(&view)
    }

    /// Nearest brush under a pixel of the 3D view, with the hit distance.
    fn pick_3d_t(&self, x: f32, y: f32) -> Option<(u32, f32)> {
        self.pick_3d_face(x, y).map(|(id, t, _)| (id, t))
    }

    /// Nearest brush under a pixel of the 3D view: (id, distance, face entered).
    fn pick_3d_face(&self, x: f32, y: f32) -> Option<(u32, f32, Option<usize>)> {
        let (o, d) = self.persp_ray(x, y);
        self.ray_pick(o, d, false)
    }

    fn ray_pick(&self, o: V3, d: V3, solids_only: bool) -> Option<(u32, f32, Option<usize>)> {
        self.doc
            .brushes
            .iter()
            .filter(|b| self.pickable(b) && !(solids_only && b.entity.is_some()))
            .filter_map(|b| geom::ray_convex_face(o, d, &b.faces).map(|(t, f)| (t, b.id, f)))
            .min_by(|a, b| a.0.partial_cmp(&b.0).unwrap())
            .map(|(t, id, f)| (id, t, f))
    }

    /// Height of the highest solid surface at (x, y) below `from_z` (0 if none).
    fn surface_below(&self, x: f32, y: f32, from_z: f32) -> f32 {
        match self.ray_pick(v3(x, y, from_z), v3(0.0, 0.0, -1.0), true) {
            Some((_, t, _)) if t > 0.0 => from_z - t,
            _ => 0.0,
        }
    }

    /// Group screen-space handle candidates: return every entry that projects to
    /// the same spot as the nearest one (Hammer grabs all stacked vertices).
    fn nearest_group(cands: Vec<(f32, f32, f32, Vec<(u32, V3)>)>) -> Vec<(u32, V3)> {
        let Some(best) = cands.iter().min_by(|a, b| a.0.partial_cmp(&b.0).unwrap()).map(|c| (c.1, c.2)) else {
            return Vec::new();
        };
        let mut out: Vec<(u32, V3)> = Vec::new();
        for (_, sx, sy, items) in cands {
            if (sx - best.0).abs() < 1.5 && (sy - best.1).abs() < 1.5 {
                for it in items {
                    if !out.iter().any(|o| o.0 == it.0 && (o.1 - it.1).len() < 1e-3) {
                        out.push(it);
                    }
                }
            }
        }
        out
    }

    /// Vertices of selected brushes under a screen point.
    pub fn hit_vertices(&self, id: usize, x: f32, y: f32) -> Vec<(u32, V3)> {
        self.hit_vertices_of(id, x, y, false)
    }

    /// `any`: vertices of every brush you can click, not just selected ones (vertex select).
    fn hit_vertices_of(&self, id: usize, x: f32, y: f32, any: bool) -> Vec<(u32, V3)> {
        let v = self.view(id);
        let r = VERTEX_PX + 3.0;
        let mut cands = Vec::new();
        for b in self.doc.brushes.iter().filter(|b| (self.doc.is_selected(b.id) || (any && self.pickable(b))) && b.entity.is_none()) {
            for &p in &b.verts {
                let (sx, sy) = v.project(p);
                let dist = ((sx - x).powi(2) + (sy - y).powi(2)).sqrt();
                if (sx - x).abs() <= r && (sy - y).abs() <= r {
                    cands.push((dist, sx, sy, vec![(b.id, p)]));
                }
            }
        }
        Self::nearest_group(cands)
    }

    /// Edge midpoints of selected brushes under a screen point (returns endpoints).
    pub fn hit_edges(&self, id: usize, x: f32, y: f32) -> Vec<(u32, V3)> {
        let v = self.view(id);
        let r = VERTEX_PX + 2.0;
        let mut cands = Vec::new();
        for b in self.doc.brushes.iter().filter(|b| self.doc.is_selected(b.id) && b.entity.is_none()) {
            for (i, j) in b.edges() {
                let (a, c) = (b.verts[i], b.verts[j]);
                let (sa, sc) = (v.project(a), v.project(c));
                if (sa.0 - sc.0).abs() < 1.0 && (sa.1 - sc.1).abs() < 1.0 {
                    continue; // edge seen end-on: its midpoint sits on a vertex
                }
                let (sx, sy) = v.project((a + c) * 0.5);
                let dist = ((sx - x).powi(2) + (sy - y).powi(2)).sqrt();
                if (sx - x).abs() <= r && (sy - y).abs() <= r {
                    cands.push((dist, sx, sy, vec![(b.id, a), (b.id, c)]));
                }
            }
        }
        Self::nearest_group(cands)
    }

    fn click_select(&mut self, id: Option<u32>, ctrl: bool) {
        let before = self.doc.selection.clone();
        // a click on a grouped brush takes its whole (outermost) group
        let set: Vec<u32> = match id {
            Some(id) if !self.ignore_groups => self.doc.expand_to_groups(&[id]),
            Some(id) => vec![id],
            None => Vec::new(),
        };
        if ctrl {
            if !set.is_empty() {
                if set.iter().all(|s| self.doc.is_selected(*s)) {
                    self.doc.selection.retain(|s| !set.contains(s));
                } else {
                    for s in set {
                        if !self.doc.is_selected(s) {
                            self.doc.selection.push(s);
                        }
                    }
                }
            }
        } else {
            self.doc.selection = set;
        }
        if self.doc.selection != before {
            self.sel_mode = HandleMode::Scale;
            self.prune_vsel();
        }
    }

    fn prune_vsel(&mut self) {
        let doc = &self.doc;
        self.vsel.retain(|&(id, p)| doc.is_selected(id) && doc.brush(id).is_some_and(|b| b.has_vertex(p)));
    }

    fn selection_drag_move(&self, id: usize, grab: (f32, f32), toggle_on_click: bool) -> Option<Drag> {
        let b = self.doc.selection_bounds()?;
        Some(Drag::Move { view: id, target: Target::Selection, grab, orig: b, orig_brushes: self.doc.selected_brushes(), changed: false, toggle_on_click, clone: false })
    }

    // ------------------------------------------------------------------ mouse

    fn set_mods(&mut self, m: i32) {
        self.alt = m & mods::ALT != 0;
        self.shift = m & mods::SHIFT != 0;
        self.ctrl = m & mods::CTRL != 0;
        self.update_free();
    }

    /// Alt frees placement from the grid and Alt+Shift keeps it, unless the
    /// "swap Alt drag behavior" setting reverses the two.
    fn update_free(&mut self) {
        self.free = self.alt && (self.shift == self.cfg.swap_alt) && self.move_hold.is_none() && self.shift_hold.is_none();
    }

    /// A G or Shift hold is carrying the selection right now.
    fn hold_carrying(&self) -> bool {
        matches!(self.move_hold, Some((_, _, true))) || matches!(self.shift_hold, Some((_, _, true)))
    }

    fn axis_locked(&self) -> bool {
        self.axis_lock != self.axis_flip
    }

    /// Shift held on its own in a 2D view and the mouse moved past the dead
    /// zone: copy the selection and drag the copy (relative, no jump).
    fn maybe_start_shift_clone(&mut self, id: usize, x: f32, y: f32) {
        // Hold M: the same, without the copy (smaller dead zone: M is deliberate)
        if let Some((view, start, false)) = self.move_hold {
            if view == id && self.drag.is_none() && ((x - start.0).powi(2) + (y - start.1).powi(2)).sqrt() >= 6.0 {
                // the Vertex tool with vertices selected: G moves the vertices
                if self.tool == Tool::Vertex && !self.vsel.is_empty() {
                    let grab = self.view(id).to_world(start.0, start.1);
                    self.drag = Some(Drag::Vertex { view: id, grab, anchor: self.vsel[0].1, orig_brushes: self.doc.selected_brushes(), sel: self.vsel.clone(), changed: false });
                    self.move_hold = Some((view, start, true));
                    return;
                }
                // the Block tool's unfinished box moves like a brush
                if let (Tool::Block, Some(bb)) = (self.tool, self.block_box) {
                    let grab = self.view(id).to_world(start.0, start.1);
                    self.drag = Some(Drag::Move { view: id, target: Target::Block, grab, orig: bb, orig_brushes: vec![], changed: false, toggle_on_click: false, clone: false });
                    self.move_hold = Some((view, start, true));
                    return;
                }
                let selbox = if self.tool == Tool::Select { self.sel_box } else { None };
                let Some(b) = selbox.or_else(|| self.doc.selection_bounds()) else {
                    self.move_hold = None;
                    return;
                };
                let grab = self.view(id).to_world(start.0, start.1);
                self.drag = Some(Drag::Move {
                    view: id,
                    target: if selbox.is_some() { Target::SelBox } else { Target::Selection },
                    grab,
                    orig: b,
                    orig_brushes: if selbox.is_some() { vec![] } else { self.doc.selected_brushes() },
                    changed: false,
                    toggle_on_click: false,
                    clone: false,
                });
                self.move_hold = Some((view, start, true));
            }
            return;
        }
        let Some((view, start, false)) = self.shift_hold else { return };
        if view != id || self.drag.is_some() || ((x - start.0).powi(2) + (y - start.1).powi(2)).sqrt() < 10.0 {
            return;
        }
        if self.doc.selection.is_empty() {
            self.shift_hold = None;
            return;
        }
        self.doc.checkpoint();
        let sel = self.doc.selection.clone();
        self.doc.selection = self.doc.duplicate(&sel);
        self.vsel.clear();
        self.block_box = None;
        let Some(b) = self.doc.selection_bounds() else { return };
        let grab = self.view(id).to_world(start.0, start.1);
        self.drag = Some(Drag::Move {
            view: id,
            target: Target::Selection,
            grab,
            orig: b,
            orig_brushes: self.doc.selected_brushes(),
            changed: true, // clone + move is one undo step
            toggle_on_click: false,
            clone: true,
        });
        self.shift_hold = Some((view, start, true));
    }

    /// Drop a Shift-held clone (or an M-held move) where it is.
    fn place_shift_clone(&mut self) -> u32 {
        self.axis_flip = false;
        self.axis_commit = None;
        if let Some(Drag::Vertex { .. }) = self.drag {
            self.drag = None;
            return flags::ALL;
        }
        if let Some(Drag::Move3D { .. }) = self.drag {
            self.drag = None;
            self.hold_in_3d = false;
            if let Some(b) = self.doc.selection_bounds() {
                self.ripple = Some((b, self.time));
                self.message = Some("Moved".into());
            }
            return flags::ALL;
        }
        if let Some(Drag::Move { orig, clone: false, target, .. }) = self.drag {
            self.drag = None;
            let b = if target == Target::Block { self.block_box } else { self.doc.selection_bounds() };
            if let Some(b) = b {
                if target != Target::Block {
                    self.work = b;
                }
                self.message = if b != orig { Some("Moved".into()) } else { None };
                if b != orig {
                    self.ripple = Some((b, self.time));
                }
            }
            return flags::ALL;
        }
        if let Some(Drag::Move { orig, .. }) = self.drag.take() {
            match self.doc.selection_bounds() {
                Some(b) if b == orig => {
                    self.doc.revert_to_checkpoint(); // never moved: no stacked copy
                    self.message = None;
                }
                Some(b) => {
                    self.work = b;
                    self.ripple = Some((b, self.time));
                    let n = self.doc.selection.len();
                    self.message = Some(format!("Cloned {} brush{}", n, if n == 1 { "" } else { "es" }));
                }
                None => {}
            }
        }
        flags::ALL
    }

    /// Shift+drag on a brush: duplicate it (or the selection it belongs to) and drag the copy.
    fn clone_grab(&mut self, id: usize, x: f32, y: f32, ctrl: bool) -> bool {
        if self.over_tool_handle(id, x, y) {
            return false;
        }
        let Some(bid) = self.pick_2d(id, x, y) else { return false };
        if !self.doc.is_selected(bid) {
            self.click_select(Some(bid), ctrl);
        }
        self.doc.checkpoint();
        let sel = self.doc.selection.clone();
        self.doc.selection = self.doc.duplicate(&sel);
        self.vsel.clear();
        let Some(b) = self.doc.selection_bounds() else { return false };
        let grab = self.view(id).to_world(x, y);
        self.drag = Some(Drag::Move {
            view: id,
            target: Target::Selection,
            grab,
            orig: b,
            orig_brushes: self.doc.selected_brushes(),
            changed: true, // checkpoint already taken: clone + move is one undo step
            toggle_on_click: false,
            clone: true,
        });
        true
    }

    /// Is the cursor on something the current tool grabs before Alt+drag does
    /// (resize/rotate handles, the pending block box, vertex/edge handles)?
    fn over_tool_handle(&self, id: usize, x: f32, y: f32) -> bool {
        match self.tool {
            Tool::Block => {
                if let Some(b) = self.block_box {
                    return self.hit_box(id, b, x, y, HandleMode::Scale) != Hit::None;
                }
                self.doc.selection_bounds().is_some_and(|b| matches!(self.hit_box(id, b, x, y, HandleMode::Scale), Hit::Handle(..)))
            }
            Tool::Select => self.doc.selection_bounds().is_some_and(|b| matches!(self.hit_box(id, b, x, y, self.sel_mode), Hit::Handle(..))),
            Tool::Vertex => !self.hit_vertices(id, x, y).is_empty() || !self.hit_edges(id, x, y).is_empty(),
            Tool::Bisect => self.bisect_line.is_some_and(|(lv, a, b)| {
                lv == id
                    && [a, b].iter().any(|p| {
                        let (sx, sy) = self.view(id).to_screen(p.0, p.1);
                        (sx - x).abs() <= 8.0 && (sy - y).abs() <= 8.0
                    })
            }),
            Tool::Origin => self.near_origin(id, x, y),
            Tool::Entity | Tool::Face => false,
        }
    }

    fn near_origin(&self, id: usize, x: f32, y: f32) -> bool {
        let (sx, sy) = self.view(id).project(self.doc.origin);
        (sx - x).abs() <= 9.0 && (sy - y).abs() <= 9.0
    }

    /// Tools where Alt / Shift change placement instead of grabbing brushes.
    fn placement_tool(&self) -> bool {
        matches!(self.tool, Tool::Bisect | Tool::Origin | Tool::Entity)
    }

    /// Alt+drag: grab the brush under the cursor (selecting it if needed) and move it.
    fn alt_grab(&mut self, id: usize, x: f32, y: f32, ctrl: bool) -> bool {
        if self.over_tool_handle(id, x, y) {
            return false;
        }
        let Some(bid) = self.pick_2d(id, x, y) else { return false };
        if !self.doc.is_selected(bid) {
            self.click_select(Some(bid), ctrl);
        }
        let grab = self.view(id).to_world(x, y);
        self.drag = self.selection_drag_move(id, grab, false);
        self.drag.is_some()
    }

    pub fn mouse_press(&mut self, id: usize, x: f32, y: f32, btn: i32, m: i32) -> u32 {
        if matches!(self.drag, Some(Drag::Rotate { by_key: true, .. }) | Some(Drag::SSize { .. })) {
            return 0; // R or S is held: the mouse only steers the rotation / sizing
        }
        if let Some((_, _, cloning)) = self.shift_hold.take() {
            if cloning {
                return self.place_shift_clone(); // a click also drops the copy
            }
        }
        if let Some((_, _, moving)) = self.move_hold.take() {
            if moving {
                return self.place_shift_clone();
            }
        }
        self.set_mods(m);
        self.message = None;
        if id == 0 {
            return self.persp_press(x, y, btn, m);
        }
        if btn == button::MIDDLE || (btn == button::LEFT && self.keys.space) {
            self.drag = Some(Drag::Pan { view: id, last: (x, y) });
            return flags::cursor(cursor::HAND);
        }
        if btn == button::RIGHT && self.tool == Tool::Block {
            // Quick Draw: right-drag a block one grid unit deep
            let (wh, wv) = self.view(id).to_world(x, y);
            self.block_box = None;
            self.drag = Some(Drag::NewBox { view: id, start: (wh, wv), start_px: (x, y), ctrl: false, quick: true });
            return flags::ALL;
        }
        if btn != button::LEFT {
            return 0;
        }
        let ctrl = m & mods::CTRL != 0;
        if self.alt && !self.placement_tool() && self.alt_grab(id, x, y, ctrl) {
            return flags::ALL | flags::cursor(cursor::SIZE_ALL);
        }
        if self.shift && !self.alt && !self.placement_tool() && self.clone_grab(id, x, y, ctrl) {
            return flags::ALL | flags::cursor(cursor::COPY);
        }
        let (wh, wv) = self.view(id).to_world(x, y);
        match self.tool {
            Tool::Block => {
                if let Some(b) = self.block_box {
                    match self.hit_box(id, b, x, y, HandleMode::Scale) {
                        Hit::Handle(hx, hy) => {
                            self.drag = Some(Drag::Resize { view: id, target: Target::Block, hx, hy, orig: b, orig_brushes: vec![], changed: false });
                            return 0;
                        }
                        Hit::Inside => {
                            self.drag = Some(Drag::Move { view: id, target: Target::Block, grab: (wh, wv), orig: b, orig_brushes: vec![], changed: false, toggle_on_click: false, clone: false });
                            return 0;
                        }
                        Hit::None => {}
                    }
                } else if let Some(b) = self.doc.selection_bounds() {
                    // Resize handles of the selection also work in the block tool.
                    if let Hit::Handle(hx, hy) = self.hit_box(id, b, x, y, HandleMode::Scale) {
                        self.drag = Some(Drag::Resize {
                            view: id,
                            target: Target::Selection,
                            hx,
                            hy,
                            orig: b,
                            orig_brushes: self.doc.selected_brushes(),
                            changed: false,
                        });
                        return 0;
                    }
                }
                self.block_box = None;
                self.drag = Some(Drag::NewBox { view: id, start: (wh, wv), start_px: (x, y), ctrl, quick: false });
                flags::ALL
            }
            Tool::Select => {
                if let Some(b) = self.sel_box {
                    match self.hit_box(id, b, x, y, HandleMode::Scale) {
                        Hit::Handle(hx, hy) => {
                            self.drag = Some(Drag::Resize { view: id, target: Target::SelBox, hx, hy, orig: b, orig_brushes: vec![], changed: false });
                            return 0;
                        }
                        Hit::Inside => {
                            self.drag = Some(Drag::Move { view: id, target: Target::SelBox, grab: (wh, wv), orig: b, orig_brushes: vec![], changed: false, toggle_on_click: false, clone: false });
                            return 0;
                        }
                        Hit::None => {
                            self.sel_box = None;
                        }
                    }
                }
                if self.sel_box_only {
                    // box select: every drag draws a box, even over brushes
                    self.sel_box_base = if ctrl { self.doc.selection.clone() } else { Vec::new() };
                    self.drag = Some(Drag::SelNew { view: id, start: (wh, wv), start_px: (x, y) });
                    return flags::ALL;
                }
                let mode = self.sel_mode;
                if let Some(b) = self.doc.selection_bounds() {
                    match self.hit_box(id, b, x, y, mode) {
                        Hit::Handle(hx, hy) => {
                            if mode == HandleMode::Rotate {
                                let v = self.view(id);
                                let (ha, va, _) = v.kind.axes();
                                let c = ((b.0.get(ha) + b.1.get(ha)) * 0.5, (b.0.get(va) + b.1.get(va)) * 0.5);
                                self.drag = Some(Drag::Rotate {
                                    view: id,
                                    center: c,
                                    start_ang: (wv - c.1).atan2(wh - c.0),
                                    orig_brushes: self.doc.selected_brushes(),
                                    changed: false,
                                    by_key: false,
                                });
                            } else {
                                self.drag = Some(Drag::Resize {
                                    view: id,
                                    target: Target::Selection,
                                    hx,
                                    hy,
                                    orig: b,
                                    orig_brushes: self.doc.selected_brushes(),
                                    changed: false,
                                });
                            }
                            return 0;
                        }
                        Hit::Inside if !ctrl => {
                            // Clicking a selected brush (without dragging) toggles scale/rotate.
                            let under = self.pick_2d(id, x, y);
                            let toggle = under.is_some_and(|u| self.doc.is_selected(u));
                            if toggle || under.is_none() {
                                self.drag = self.selection_drag_move(id, (wh, wv), toggle);
                                return 0;
                            }
                        }
                        _ => {}
                    }
                }
                let hit = self.pick_2d(id, x, y);
                if hit.is_none() {
                    // point and click: empty space deselects (boxes are the right half's job)
                    if !ctrl {
                        self.click_select(None, false);
                    }
                    return flags::ALL;
                }
                self.click_select(hit, ctrl);
                if hit.is_some() && !ctrl {
                    self.drag = self.selection_drag_move(id, (wh, wv), false);
                }
                flags::ALL
            }
            Tool::Vertex if self.vertex_pick => {
                // vertex select: clicks pick vertices on any brush, drags draw a box;
                // G / R / S / arrows do the moving
                let h = self.hit_vertices_of(id, x, y, true);
                if !h.is_empty() {
                    if !ctrl {
                        self.vsel.clear();
                    }
                    for (bid, p) in h {
                        if let Some(i) = self.vsel.iter().position(|&(i, q)| i == bid && (q - p).len() < 1e-3) {
                            if ctrl {
                                self.vsel.remove(i);
                            }
                        } else {
                            self.vsel.push((bid, p));
                        }
                        if !self.doc.is_selected(bid) {
                            self.doc.selection.push(bid);
                        }
                    }
                    return flags::ALL;
                }
                self.drag = Some(Drag::Marquee { view: id, start: (x, y), cur: (x, y), ctrl });
                flags::ALL
            }
            Tool::Vertex => {
                let mut h = self.hit_vertices(id, x, y);
                if h.is_empty() {
                    h = self.hit_edges(id, x, y);
                }
                if !h.is_empty() {
                    if ctrl {
                        for (bid, p) in h {
                            if let Some(i) = self.vsel.iter().position(|&(i, q)| i == bid && (q - p).len() < 1e-3) {
                                self.vsel.remove(i);
                            } else {
                                self.vsel.push((bid, p));
                            }
                        }
                        return flags::ALL;
                    }
                    if !h.iter().all(|&(bid, p)| self.vertex_selected(bid, p)) {
                        self.vsel = h.clone();
                    }
                    self.drag = Some(Drag::Vertex {
                        view: id,
                        grab: (wh, wv),
                        anchor: h[0].1,
                        orig_brushes: self.doc.selected_brushes(),
                        sel: self.vsel.clone(),
                        changed: false,
                    });
                    return flags::ALL;
                }
                if let Some(bid) = self.pick_2d(id, x, y) {
                    if !self.doc.is_selected(bid) {
                        self.click_select(Some(bid), ctrl);
                        return flags::ALL;
                    }
                }
                self.drag = Some(Drag::Marquee { view: id, start: (x, y), cur: (x, y), ctrl });
                flags::ALL
            }
            Tool::Bisect => {
                // Grab an end of the existing cut line to adjust it…
                if let Some((lv, a, b)) = self.bisect_line {
                    if lv == id {
                        let v = self.view(id);
                        for (which, p) in [a, b].into_iter().enumerate() {
                            let (sx, sy) = v.to_screen(p.0, p.1);
                            if (sx - x).abs() <= 8.0 && (sy - y).abs() <= 8.0 {
                                self.drag = Some(Drag::BisectEnd { view: id, which });
                                return flags::ALL;
                            }
                        }
                    }
                }
                // …or start drawing a new one (a plain click selects instead).
                let start = self.bisect_snap(id, x, y);
                self.drag = Some(Drag::BisectNew { view: id, start, start_px: (x, y), ctrl });
                flags::ALL
            }
            Tool::Entity => {
                let v = self.view(id);
                let (ha, va, _) = v.kind.axes();
                let mut base = V3::default();
                base.set(ha, self.snap(wh));
                base.set(va, self.snap(wv));
                if v.kind == ViewKind::Top {
                    // Stand on the highest surface under the click.
                    base.z = self.surface_below(base.x, base.y, 1.0e5);
                }
                self.place_entity(base)
            }
            Tool::Origin => {
                if !self.near_origin(id, x, y) {
                    self.set_origin_2d(id, wh, wv, &mut false);
                    self.drag = Some(Drag::OriginDrag { view: id, changed: true });
                } else {
                    self.drag = Some(Drag::OriginDrag { view: id, changed: false });
                }
                flags::ALL
            }
            Tool::Face => {
                // Faces are picked in the 3D view; in 2D a click picks the brush.
                let hit = self.pick_2d(id, x, y);
                self.click_select(hit, ctrl);
                flags::ALL
            }
        }
    }

    // ------------------------------------------------------------------ entities / origin / faces

    fn place_entity(&mut self, base: V3) -> u32 {
        self.doc.checkpoint();
        let layer_hidden = self.doc.layer(self.doc.active_layer).is_some_and(|l| !l.visible);
        if layer_hidden {
            let a = self.doc.active_layer;
            self.doc.set_layer_flags(a, true, false);
        }
        let id = self.doc.add_entity(self.place, base);
        self.doc.selection = vec![id];
        self.show_entities = true;
        self.tool = Tool::Select;
        self.sel_mode = HandleMode::Scale;
        let what = if self.place == Entity::Car { "car" } else { "scale mannequin" };
        self.message = Some(format!("Placed a {what} at {}, {}, {} (never compiled)", fmt_num(base.x), fmt_num(base.y), fmt_num(base.z)));
        flags::ALL | flags::TOOL
    }

    /// Where a click in the 3D view puts an entity's base: standing on the
    /// face under the cursor (or the surface below it), else on the floor.
    fn place_base_3d(&self, x: f32, y: f32) -> V3 {
        let (o, d) = self.persp_ray(x, y);
        let p = match self.ray_pick(o, d, true) {
            Some((bid, t, f)) => {
                let p = o + d * t;
                let n = f.and_then(|f| self.doc.brush(bid).map(|b| b.faces[f].normal)).unwrap_or(v3(0.0, 0.0, 1.0));
                if n.z > 0.5 {
                    p
                } else {
                    // Clicked a wall: step out from it and stand on whatever is below.
                    let q = p + v3(n.x, n.y, 0.0).norm() * 16.0;
                    v3(q.x, q.y, self.surface_below(q.x, q.y, p.z + 0.01))
                }
            }
            None if d.z < -1e-3 => o + d * (-o.z / d.z),
            None => {
                let q = o + d * 256.0;
                v3(q.x, q.y, self.surface_below(q.x, q.y, q.z))
            }
        };
        v3(self.snap(p.x), self.snap(p.y), p.z)
    }

    fn set_origin_2d(&mut self, id: usize, wh: f32, wv: f32, changed: &mut bool) {
        let (ha, va, _) = self.view(id).kind.axes();
        let mut o = self.active_origin();
        o.set(ha, self.snap(wh));
        o.set(va, self.snap(wv));
        self.set_origin(o, changed);
    }

    fn set_origin(&mut self, o: V3, changed: &mut bool) {
        if o == self.active_origin() {
            return;
        }
        if !*changed {
            self.doc.checkpoint();
            *changed = true;
        }
        let active = self.doc.active_layer;
        if let Some(l) = self.doc.layers.iter_mut().find(|l| l.id == active && l.origin.is_some()) {
            l.origin = Some(o);
            let name = l.name.clone();
            self.message = Some(format!("{} origin: {}, {}, {}", name, fmt_num(o.x), fmt_num(o.y), fmt_num(o.z)));
            return;
        }
        self.doc.origin = o;
        self.message = Some(format!("Model origin: {}, {}, {}", fmt_num(o.x), fmt_num(o.y), fmt_num(o.z)));
    }

    /// Faces the texture utility would act on: the face selection in the Face
    /// tool, otherwise every face of the selected brushes (entities excluded).
    pub fn material_targets(&self) -> Vec<(u32, usize)> {
        if self.tool == Tool::Face && !self.fsel.is_empty() {
            return self.fsel.clone();
        }
        self.doc
            .brushes
            .iter()
            .filter(|b| self.doc.is_selected(b.id) && b.entity.is_none())
            .flat_map(|b| (0..b.faces.len()).map(move |i| (b.id, i)))
            .collect()
    }

    pub fn material_target_desc(&self) -> String {
        let t = self.material_targets();
        if t.is_empty() {
            return "Nothing selected: pick faces with Face Select & Paint (6), or select brushes".into();
        }
        if self.tool == Tool::Face && !self.fsel.is_empty() {
            return format!("{} selected face{}", t.len(), if t.len() == 1 { "" } else { "s" });
        }
        let n = self.doc.selection.iter().filter(|&&id| self.doc.brush(id).is_some_and(|b| b.entity.is_none())).count();
        format!("all {} faces of {} selected brush{}", t.len(), n, if n == 1 { "" } else { "es" })
    }

    /// Apply a material ("" clears it) to the current targets.
    /// Swap in a whole new document (an import), dropping anything in progress.
    pub fn set_document(&mut self, doc: crate::doc::Document) {
        self.drag = None;
        self.block_box = None;
        self.sel_box = None;
        self.bisect_line = None;
        self.vsel.clear();
        self.fsel.clear();
        let rev = self.doc.rev;
        self.hidden.clear();
        self.doc = doc;
        self.doc.rev = rev + 1; // panels watching the revision must refresh
        self.doc.selection.clear();
    }

    pub fn apply_material(&mut self, material: &str) -> usize {
        // there's no "no material": blank means nodraw
        let material = if material.is_empty() { geom::NODRAW } else { material };
        self.current_material = material.to_string();
        let t = self.material_targets();
        if t.is_empty() {
            self.message = Some("Nothing to apply the material to".into());
            return 0;
        }
        self.apply_material_to(&t)
    }

    /// Apply the current material to specific faces.
    fn apply_material_to(&mut self, t: &[(u32, usize)]) -> usize {
        let material = self.current_material.clone();
        let material = material.as_str();
        self.doc.checkpoint();
        for &(id, f) in t {
            if let Some(b) = self.doc.brush_mut(id) {
                if let Some(face) = b.faces.get_mut(f) {
                    face.material = material.to_string();
                }
            }
        }
        let name = if material.is_empty() { "No material" } else { material_display(material) };
        self.message = Some(format!("{} applied to {} face{}", name, t.len(), if t.len() == 1 { "" } else { "s" }));
        t.len()
    }

    // ------------------------------------------------------------------ face textures

    fn face_tex(&self, k: (u32, usize)) -> Option<TexMap> {
        self.doc.brush(k.0).and_then(|b| b.faces.get(k.1)).map(|f| f.tex.or_default_for(f.normal))
    }

    fn face_points(&self, k: (u32, usize)) -> Vec<V3> {
        self.doc.brush(k.0).and_then(|b| b.faces.get(k.1).map(|f| f.idx.iter().map(|&i| b.verts[i]).collect())).unwrap_or_default()
    }

    fn face_plane(&self, k: (u32, usize)) -> Option<(V3, f32)> {
        self.doc.brush(k.0).and_then(|b| b.faces.get(k.1)).map(|f| (f.normal, f.dist))
    }

    fn set_face_tex(&mut self, k: (u32, usize), t: TexMap) {
        if let Some(f) = self.doc.brush_mut(k.0).and_then(|b| b.faces.get_mut(k.1)) {
            f.tex = t.or_default_for(f.normal);
        }
    }

    /// Make a face the paint source: its texture becomes the current one.
    fn make_source(&mut self, k: (u32, usize)) {
        if let Some(f) = self.doc.brush(k.0).and_then(|b| b.faces.get(k.1)) {
            self.current_material = f.material.clone();
            self.src_face = Some(k);
        }
    }

    /// The face whose values the face controls show and edits start from.
    pub fn tex_source(&self) -> Option<(u32, usize)> {
        let valid = |k: &(u32, usize)| self.doc.brush(k.0).is_some_and(|b| k.1 < b.faces.len());
        self.src_face.filter(|k| valid(k) && self.fsel.contains(k)).or_else(|| self.fsel.last().copied().filter(valid))
    }

    /// Image size of a face's material in texels (512 if it isn't loaded).
    fn tex_size_of(&self, k: (u32, usize)) -> (f32, f32) {
        let m = self.doc.brush(k.0).and_then(|b| b.faces.get(k.1)).map(|f| f.material.clone()).unwrap_or_default();
        texstore::size(&m).map(|(w, h)| (w as f32, h as f32)).unwrap_or((512.0, 512.0))
    }

    /// Continue the texture of face `src` onto face `dst` as one sheet.
    fn wrap_onto(&self, src: (u32, usize), dst: (u32, usize)) -> Option<TexMap> {
        let st = self.face_tex(src)?;
        let (ns, ds) = self.face_plane(src)?;
        let (nd, dd) = self.face_plane(dst)?;
        if src == dst || (ns.dot(nd) > 0.9999 && (ds - dd).abs() < 0.01) {
            return Some(st);
        }
        let (a, b) = (self.face_points(src), self.face_points(dst));
        let common: Vec<V3> = a.iter().copied().filter(|p| b.iter().any(|q| (*q - *p).len() < 0.01)).collect();
        let edge = if common.len() >= 2 { Some((common[0], common[1])) } else { None };
        Some(st.wrapped(ns, nd, edge))
    }

    /// Paint face `k`: the current material plus the source face's alignment
    /// (Shift: every face of that brush; Alt: wrap from the source instead,
    /// and the painted face becomes the next source so you can walk around).
    /// Right-drag painting: the face under (x, y), if it's a new one.
    fn paint_drag_at(&mut self, x: f32, y: f32) -> u32 {
        let Some((bid, _, Some(f))) = self.pick_3d_face(x, y) else { return 0 };
        if !self.doc.brush(bid).is_some_and(|b| b.entity.is_none()) {
            return 0;
        }
        let last = match self.drag {
            Some(Drag::PaintFaces { last }) => last,
            _ => return 0,
        };
        // Shift paints whole brushes: one paint per brush is enough
        let same = last.is_some_and(|l| l == (bid, f) || (self.shift && l.0 == bid));
        if same {
            return 0;
        }
        self.drag = Some(Drag::PaintFaces { last: Some((bid, f)) });
        self.paint_face_ext((bid, f), self.shift, self.alt, false)
    }

    fn paint_face(&mut self, k: (u32, usize), whole: bool, wrap: bool) -> u32 {
        self.paint_face_ext(k, whole, wrap, true)
    }

    fn paint_face_ext(&mut self, k: (u32, usize), whole: bool, wrap: bool, checkpoint: bool) -> u32 {
        let src = self.src_face.filter(|s| {
            self.doc.brush(s.0).and_then(|b| b.faces.get(s.1)).is_some_and(|f| f.material == self.current_material)
        });
        let targets: Vec<(u32, usize)> = if whole {
            let n = self.doc.brush(k.0).map(|b| b.faces.len()).unwrap_or(0);
            (0..n).map(|i| (k.0, i)).collect()
        } else {
            vec![k]
        };
        if checkpoint {
            self.doc.checkpoint();
        }
        let material = self.current_material.clone();
        for &t in &targets {
            let tex = match src {
                Some(s) if wrap => self.wrap_onto(s, t),
                Some(s) if s != t => self.face_tex(s).map(|st| {
                    let (n, _) = self.face_plane(t).unwrap();
                    st.or_default_for(n)
                }),
                _ => None,
            };
            if let Some(f) = self.doc.brush_mut(t.0).and_then(|b| b.faces.get_mut(t.1)) {
                f.material = material.clone();
                if let Some(tm) = tex {
                    f.tex = tm.or_default_for(f.normal);
                }
            }
        }
        let name = if material.is_empty() { "No material" } else { material_display(&material) };
        if wrap && src.is_some() && !whole {
            self.src_face = Some(k);
            self.fsel = vec![k];
            self.message = Some(format!("{} wrapped on (this face is the next source)", name));
            return flags::ALL | flags::TOOL;
        }
        self.message = Some(format!("{} painted on {} face{}", name, targets.len(), if targets.len() == 1 { "" } else { "s" }));
        flags::ALL
    }

    /// Faces the face controls work on: the face selection.
    fn tex_targets(&self) -> Vec<(u32, usize)> {
        let doc = &self.doc;
        self.fsel.iter().copied().filter(|k| doc.brush(k.0).is_some_and(|b| k.1 < b.faces.len() && b.entity.is_none())).collect()
    }

    /// Treat as one: lay the selection out as one sheet continuing from the
    /// source face (folded across shared edges, projected for the rest).
    fn unfold_group(&mut self, targets: &[(u32, usize)]) {
        let Some(src) = self.tex_source() else { return };
        let mut done = vec![src];
        let mut todo: Vec<(u32, usize)> = targets.iter().copied().filter(|&k| k != src).collect();
        while !todo.is_empty() {
            // a face that shares an edge with one already laid out, else the next one
            let mut pick = None;
            'outer: for (i, &t) in todo.iter().enumerate() {
                let tp = self.face_points(t);
                for &d in &done {
                    let dp = self.face_points(d);
                    if dp.iter().filter(|p| tp.iter().any(|q| (*q - **p).len() < 0.01)).count() >= 2 {
                        pick = Some((i, d));
                        break 'outer;
                    }
                }
            }
            let (i, from) = pick.unwrap_or((0, src));
            let t = todo.remove(i);
            if let Some(tm) = self.wrap_onto(from, t) {
                self.set_face_tex(t, tm);
            }
            done.push(t);
        }
    }

    /// Texel bounds of faces, each measured with its own mapping.
    fn tex_bounds(&self, targets: &[(u32, usize)]) -> ([f32; 2], [f32; 2]) {
        let mut lo = [f32::MAX; 2];
        let mut hi = [f32::MIN; 2];
        for &k in targets {
            if let Some(t) = self.face_tex(k) {
                let (l, h) = t.bounds(&self.face_points(k));
                lo = [lo[0].min(l[0]), lo[1].min(l[1])];
                hi = [hi[0].max(h[0]), hi[1].max(h[1])];
            }
        }
        (lo, hi)
    }

    /// Values for the face controls: (count, shift, scale, rotation, image size).
    pub fn face_info(&self) -> Option<(usize, TexMap, (f32, f32))> {
        let src = self.tex_source()?;
        Some((self.tex_targets().len(), self.face_tex(src)?, self.tex_size_of(src)))
    }

    /// Face control edits. field: 0/1 shift u/v, 2/3 scale u/v, 4 rotation.
    /// One by one (Hammer): every selected face gets the value. Treat as one:
    /// the change from the source's value is applied to the whole sheet.
    pub fn face_set(&mut self, field: u32, value: f32) -> u32 {
        let targets = self.tex_targets();
        let Some(src) = self.tex_source() else { return 0 };
        let Some(st) = self.face_tex(src) else { return 0 };
        self.doc.checkpoint();
        if self.tex_group && targets.len() > 1 {
            self.unfold_group(&targets);
            let st = self.face_tex(src).unwrap_or(st);
            let (lo, hi) = self.tex_bounds(&targets);
            let c = [(lo[0] + hi[0]) * 0.5, (lo[1] + hi[1]) * 0.5];
            for &k in &targets {
                let Some(t) = self.face_tex(k) else { continue };
                let nt = match field {
                    0 => t.affine([[1.0, 0.0], [0.0, 1.0]], [value - st.shift[0], 0.0], 0.0),
                    1 => t.affine([[1.0, 0.0], [0.0, 1.0]], [0.0, value - st.shift[1]], 0.0),
                    2 => t.scaled_about([value / nzf(st.scale[0]), 1.0], c),
                    3 => t.scaled_about([1.0, value / nzf(st.scale[1])], c),
                    _ => t.rotated_about(value - st.rot, c),
                };
                self.set_face_tex(k, nt);
            }
        } else {
            for &k in &targets {
                let Some(mut t) = self.face_tex(k) else { continue };
                match field {
                    0 => t.shift[0] = value,
                    1 => t.shift[1] = value,
                    2 => t.scale[0] = if value.abs() < 1e-4 { 1e-4 } else { value },
                    3 => t.scale[1] = if value.abs() < 1e-4 { 1e-4 } else { value },
                    _ => {
                        let pts = self.face_points(k);
                        let (l, h) = t.bounds(&pts);
                        t = t.rotated_about(value - t.rot, [(l[0] + h[0]) * 0.5, (l[1] + h[1]) * 0.5]);
                    }
                }
                self.set_face_tex(k, t);
            }
        }
        flags::ALL | flags::TOOL
    }

    /// Justify: 0 left, 1 right, 2 top, 3 bottom, 4 center, 5 fit (stretch
    /// one copy of the image over the face, or the whole sheet), 6 reset to
    /// the world default.
    pub fn face_justify(&mut self, mode: u32) -> u32 {
        let targets = self.tex_targets();
        if targets.is_empty() {
            self.message = Some("Select faces first".into());
            return flags::ALL;
        }
        self.doc.checkpoint();
        if mode == 6 {
            for &k in &targets {
                if let Some((n, _)) = self.face_plane(k) {
                    self.set_face_tex(k, TexMap::world(n));
                }
            }
            self.message = Some("Texture alignment reset".into());
            return flags::ALL | flags::TOOL;
        }
        let group = self.tex_group && targets.len() > 1;
        if group {
            self.unfold_group(&targets);
        }
        let sets: Vec<Vec<(u32, usize)>> = if group { vec![targets.clone()] } else { targets.iter().map(|&k| vec![k]).collect() };
        for set in sets {
            let (w, h) = self.tex_size_of(set[0]);
            let (mut lo, mut hi) = self.tex_bounds(&set);
            if mode == 5 {
                let k = [(hi[0] - lo[0]) / w, (hi[1] - lo[1]) / h];
                for &f in &set {
                    if let Some(t) = self.face_tex(f) {
                        self.set_face_tex(f, t.scaled_about(k, lo));
                    }
                }
                let b = self.tex_bounds(&set);
                lo = b.0;
                hi = b.1;
            }
            let du = match mode {
                0 | 5 => -lo[0],
                1 => w - hi[0],
                4 => w * 0.5 - (lo[0] + hi[0]) * 0.5,
                _ => 0.0,
            };
            let dv = match mode {
                2 | 5 => -lo[1],
                3 => h - hi[1],
                4 => h * 0.5 - (lo[1] + hi[1]) * 0.5,
                _ => 0.0,
            };
            for &f in &set {
                if let Some(t) = self.face_tex(f) {
                    self.set_face_tex(f, t.affine([[1.0, 0.0], [0.0, 1.0]], [du, dv], 0.0));
                }
            }
        }
        self.message = Some(
            match mode {
                0 => "Justified left",
                1 => "Justified right",
                2 => "Justified top",
                3 => "Justified bottom",
                4 => "Centered",
                _ => "Stretched to fit",
            }
            .into(),
        );
        flags::ALL | flags::TOOL
    }

    /// Mouse drag on a face: shift / rotate / scale the textures of `orig`.
    fn tex_drag_update(&mut self, start: (f32, f32), now: (f32, f32), face: (u32, usize), mode: u8, orig: &[((u32, usize), TexMap)]) {
        let Some((n, d)) = self.face_plane(face) else { return };
        let Some(src) = orig.iter().find(|o| o.0 == face).map(|o| o.1).or(orig.first().map(|o| o.1)) else { return };
        let group = self.tex_group && orig.len() > 1;
        let free = self.alt;
        match mode {
            0 => {
                // the texture follows the mouse across the face's plane
                let hit = |x: f32, y: f32| {
                    let (o, dir) = self.persp_ray(x, y);
                    let den = dir.dot(n);
                    if den.abs() < 1e-6 {
                        return None;
                    }
                    Some(o + dir * ((d - o.dot(n)) / den))
                };
                let (Some(a), Some(b)) = (hit(start.0, start.1), hit(now.0, now.1)) else { return };
                let (ua, va) = src.uv(a);
                let (ub, vb) = src.uv(b);
                let (mut du, mut dv) = (ua - ub, va - vb);
                if !free {
                    du = du.round();
                    dv = dv.round();
                }
                for &(k, t) in orig {
                    self.set_face_tex(k, t.affine([[1.0, 0.0], [0.0, 1.0]], [-du, -dv], 0.0));
                }
                self.message = Some(format!("Shift {} {}", fmt_num(src.shift[0] - du), fmt_num(src.shift[1] - dv)));
            }
            1 => {
                let mut ang = (now.0 - start.0) * 0.5;
                if !free {
                    ang = (ang / 15.0).round() * 15.0;
                }
                let c = self.drag_pivot(orig, group);
                for &(k, t) in orig {
                    let ck = if group { c } else { self.drag_pivot(&[(k, t)], false) };
                    self.set_face_tex(k, t.rotated_about(ang, ck));
                }
                self.message = Some(format!("Rotate {}°", fmt_num(tex::wrap_deg(src.rot + ang))));
            }
            _ => {
                let mut f = 2f32.powf((now.0 - start.0) / 120.0);
                let mut s = src.scale[0].abs() * f;
                if !free {
                    s = ((s * 20.0).round() / 20.0).max(0.05);
                }
                f = s / src.scale[0].abs().max(1e-6);
                let c = self.drag_pivot(orig, group);
                for &(k, t) in orig {
                    let ck = if group { c } else { self.drag_pivot(&[(k, t)], false) };
                    self.set_face_tex(k, t.scaled_about([f, f], ck));
                }
                self.message = Some(format!("Scale {}", fmt_num((s * 1000.0).round() / 1000.0)));
            }
        }
    }

    fn drag_pivot(&self, faces: &[((u32, usize), TexMap)], _group: bool) -> [f32; 2] {
        let mut lo = [f32::MAX; 2];
        let mut hi = [f32::MIN; 2];
        for &(k, t) in faces {
            let (l, h) = t.bounds(&self.face_points(k));
            lo = [lo[0].min(l[0]), lo[1].min(l[1])];
            hi = [hi[0].max(h[0]), hi[1].max(h[1])];
        }
        [(lo[0] + hi[0]) * 0.5, (lo[1] + hi[1]) * 0.5]
    }

    fn prune_fsel(&mut self) {
        let doc = &self.doc;
        self.fsel.retain(|&(id, f)| doc.brush(id).is_some_and(|b| f < b.faces.len() && b.entity.is_none()));
    }

    /// Shift+Ctrl paint-select at a pixel of the 3D view.
    fn paint_at(&mut self, x: f32, y: f32) -> u32 {
        let Some(Drag::Paint { mode }) = self.drag else { return 0 };
        let Some((bid, _, face)) = self.pick_3d_face(x, y) else { return 0 };
        if self.tool == Tool::Face {
            let Some(f) = face else { return 0 };
            if self.doc.brush(bid).is_some_and(|b| b.entity.is_some()) {
                return 0;
            }
            let sel = self.face_selected(bid, f);
            let mode = mode.unwrap_or(!sel);
            self.drag = Some(Drag::Paint { mode: Some(mode) });
            if mode && !sel {
                self.fsel.push((bid, f));
            } else if !mode && sel {
                self.fsel.retain(|&s| s != (bid, f));
            } else {
                return 0;
            }
        } else {
            let sel = self.doc.is_selected(bid);
            let mode = mode.unwrap_or(!sel);
            self.drag = Some(Drag::Paint { mode: Some(mode) });
            if mode && !sel {
                self.doc.selection.push(bid);
            } else if !mode && sel {
                self.doc.selection.retain(|&s| s != bid);
                self.prune_vsel();
            } else {
                return 0;
            }
        }
        flags::ALL
    }

    /// Point the 3D view orbits around: what's under the screen center,
    /// else where the center ray meets the floor, else a point ahead.
    fn orbit_pivot_point(&self) -> V3 {
        let (w, h) = self.persp_size;
        let (o, d) = self.persp_ray(w * 0.5, h * 0.5);
        if let Some((_, t, _)) = self.ray_pick(o, d, false) {
            if t > 1.0 {
                return o + d * t;
            }
        }
        if d.z < -1e-3 {
            let t = -o.z / d.z;
            if t < 16384.0 {
                return o + d * t;
            }
        }
        o + d * 512.0
    }

    // ------------------------------------------------------------------ bisect

    /// Snap a cut-line end: edge midpoints of selected brushes win when close,
    /// otherwise the grid (Alt: free).
    fn bisect_snap(&self, id: usize, x: f32, y: f32) -> (f32, f32) {
        let v = self.view(id);
        let (ha, va, _) = v.kind.axes();
        let mut best: Option<(f32, (f32, f32))> = None;
        for b in self.doc.brushes.iter().filter(|b| self.doc.is_selected(b.id)) {
            for (i, j) in b.edges() {
                let m = (b.verts[i] + b.verts[j]) * 0.5;
                let (sx, sy) = v.project(m);
                let d = ((sx - x).powi(2) + (sy - y).powi(2)).sqrt();
                if d <= 8.0 && best.map_or(true, |(bd, _)| d < bd) {
                    best = Some((d, (m.get(ha), m.get(va))));
                }
            }
        }
        if let Some((_, p)) = best {
            return p;
        }
        let (wh, wv) = v.to_world(x, y);
        (self.snap(wh), self.snap(wv))
    }

    /// The cut plane (n·p = d) of the pending bisect line.
    fn bisect_plane(&self) -> Option<(V3, f32)> {
        let (id, a, b) = self.bisect_line?;
        let (ha, va, _) = self.view(id).kind.axes();
        let (dh, dv) = (b.0 - a.0, b.1 - a.1);
        if dh.abs() < 1e-4 && dv.abs() < 1e-4 {
            return None;
        }
        let mut n = V3::default();
        n.set(ha, -dv);
        n.set(va, dh);
        let n = n.norm();
        let mut p = V3::default();
        p.set(ha, a.0);
        p.set(va, a.1);
        Some((n, n.dot(p)))
    }

    /// What the pending cut would do to the selected brushes.
    pub fn bisect_preview(&self) -> Option<BisectPreview> {
        let (n, d) = self.bisect_plane()?;
        let mut pieces = Vec::new();
        let (mut vneg, mut vpos) = (0.0f32, 0.0f32);
        // New cut faces are always nodraw: paint the ones that end up visible.
        for b in self.doc.brushes.iter().filter(|b| self.doc.is_selected(b.id) && b.entity.is_none()) {
            let Some((neg, pos)) = geom::split_points(&b.verts, &b.edges(), n, d) else { continue };
            let cut_mat = geom::NODRAW.to_string();
            let mk = |pts: &[V3]| {
                geom::hull(pts).map(|h| {
                    let mut piece = b.clone();
                    piece.set_hull(h);
                    for f in &mut piece.faces {
                        let k = f.normal.dot(n);
                        if k.abs() > 0.999 && (f.dist - k.signum() * d).abs() < 0.05 {
                            f.material = cut_mat.clone();
                        }
                    }
                    piece
                })
            };
            let (pn, pp) = (mk(&neg), mk(&pos));
            vneg += pn.as_ref().map_or(0.0, |p| geom::volume(&p.verts, &p.faces));
            vpos += pp.as_ref().map_or(0.0, |p| geom::volume(&p.verts, &p.faces));
            pieces.push((b.id, pn, pp));
        }
        if pieces.is_empty() {
            return None;
        }
        // Split & Delete removes the smaller side (or the larger, in that mode);
        // F flips it for this cut, and so does holding Shift.
        let delete_pos = (vpos < vneg) ^ (self.bisect_delete && self.bisect_larger) ^ (self.bisect_delete && self.bisect_flip % 2 == 1) ^ self.shift;
        let _ = (vneg, vpos);
        Some(BisectPreview { pieces, delete_pos })
    }

    fn apply_bisect(&mut self) -> u32 {
        if self.bisect_line.is_none() {
            return 0;
        }
        let Some(pv) = self.bisect_preview() else {
            self.message = Some("The cut line doesn't pass through any selected brush".into());
            return flags::ALL;
        };
        self.doc.checkpoint();
        let mut sel = Vec::new();
        let n = pv.pieces.len();
        // Split: F picks what stays selected (0 both, 1 the smaller side, 2 the larger)
        let small_pos = {
            let (mut vn, mut vp) = (0.0f32, 0.0f32);
            for (_, a, b) in &pv.pieces {
                vn += a.as_ref().map_or(0.0, |p| geom::volume(&p.verts, &p.faces));
                vp += b.as_ref().map_or(0.0, |p| geom::volume(&p.verts, &p.faces));
            }
            vp < vn
        };
        let pick = if self.bisect_delete { 0 } else { self.bisect_flip % 3 };
        let keep_sel = |is_pos: bool| pick == 0 || (pick == 1) == (is_pos == small_pos);
        for (id, neg, pos) in pv.pieces {
            let (keep, other) = if self.bisect_delete && !pv.delete_pos { (pos, neg) } else { (neg, pos) };
            match keep {
                Some(k) => {
                    if let Some(b) = self.doc.brush_mut(id) {
                        *b = k;
                    }
                    if self.bisect_delete || keep_sel(false) {
                        sel.push(id);
                    }
                }
                None => self.doc.brushes.retain(|b| b.id != id),
            }
            if !self.bisect_delete {
                if let Some(o) = other {
                    let nid = self.doc.add_piece(o);
                    if keep_sel(true) {
                        sel.push(nid);
                    }
                }
            }
        }
        self.doc.selection = sel;
        self.bisect_line = None;
        self.bisect_flip = 0;
        self.message = Some(format!(
            "{} {} brush{}",
            if self.bisect_delete { "Cut and trimmed" } else { "Split" },
            n,
            if n == 1 { "" } else { "es" }
        ));
        flags::ALL
    }

    fn persp_press(&mut self, x: f32, y: f32, btn: i32, m: i32) -> u32 {
        if btn == button::RIGHT && self.tool == Tool::Face {
            // paint: under the cursor, or at the crosshair while flying; keep painting while held
            let (px, py) = if self.captured { (self.persp_size.0 * 0.5, self.persp_size.1 * 0.5) } else { (x, y) };
            self.doc.checkpoint();
            self.drag = Some(Drag::PaintFaces { last: None });
            return self.paint_drag_at(px, py) | flags::ALL;
        }
        if btn == button::RIGHT && !self.captured {
            self.drag = Some(Drag::Look { last: (x, y), start: (x, y), face_click: self.tool == Tool::Face });
            return 0;
        }
        if btn == button::MIDDLE && !self.captured {
            let pivot = self.orbit_pivot_point();
            let dist = (pivot - self.cam.pos).len().max(16.0);
            if self.shift {
                self.drag = Some(Drag::Pan3D { last: (x, y), dist });
                return flags::cursor(cursor::HAND);
            }
            self.orbit_pivot = Some(pivot);
            self.drag = Some(Drag::Orbit { last: (x, y), pivot, dist });
            return flags::ALL | flags::cursor(cursor::HAND);
        }
        if btn == button::LEFT {
            let (px, py) = if self.captured { (self.persp_size.0 * 0.5, self.persp_size.1 * 0.5) } else { (x, y) };
            let ctrl = m & mods::CTRL != 0;
            if self.shift && ctrl && !self.alt {
                // Shift+Ctrl+drag: paint-select across brushes (or faces).
                self.drag = Some(Drag::Paint { mode: None });
                return self.paint_at(px, py) | flags::ALL;
            }
            match self.tool {
                Tool::Entity => {
                    let base = self.place_base_3d(px, py);
                    return self.place_entity(base);
                }
                Tool::Origin => {
                    let (o, d) = self.persp_ray(px, py);
                    if let Some((_, t, _)) = self.ray_pick(o, d, true) {
                        let p = o + d * t;
                        let p = v3(self.snap(p.x), self.snap(p.y), self.snap(p.z));
                        self.set_origin(p, &mut false);
                    }
                    return flags::ALL;
                }
                Tool::Face => {
                    let hit = self.pick_3d_face(px, py);
                    let face = hit.and_then(|(bid, _, f)| f.map(|f| (bid, f))).filter(|&(bid, _)| self.doc.brush(bid).is_some_and(|b| b.entity.is_none()));
                    if self.alt {
                        // Alt+click: pick up the face's texture without changing the selection.
                        if let Some(fk) = face {
                            self.make_source(fk);
                            let m = self.current_material.clone();
                            self.message = Some(format!("Picked up {}", if m.is_empty() { "no material" } else { material_display(&m) }));
                            return flags::ALL | flags::TOOL;
                        }
                        return 0;
                    }
                    let Some(fk) = face else {
                        if !ctrl {
                            self.fsel.clear();
                            self.src_face = None;
                        }
                        self.message = Some("Click a face to select it (Ctrl adds), right-click to paint, Alt+right-click to wrap".into());
                        return flags::ALL | flags::TOOL;
                    };
                    // Click selects (and makes it the paint source); dragging
                    // on a face adjusts the texture: plain = shift, Ctrl =
                    // rotate, Shift = scale.
                    if !ctrl && !self.fsel.contains(&fk) {
                        self.fsel = vec![fk];
                    }
                    if !ctrl {
                        self.make_source(fk);
                    }
                    let mode = if ctrl { 1 } else if self.shift { 2 } else { 0 };
                    let targets = if self.fsel.contains(&fk) { self.fsel.clone() } else { vec![fk] };
                    let orig = targets.iter().filter_map(|&k| self.face_tex(k).map(|t| (k, t))).collect();
                    self.drag = Some(Drag::TexDrag { start: (px, py), face: fk, mode, orig, moved: false, ctrl_click: ctrl });
                    let m = self.doc.brush(fk.0).map(|b| b.faces[fk.1].material.clone()).unwrap_or_default();
                    self.message = Some(format!("Face: {}  (right-click paints it on other faces)", if m.is_empty() { "none" } else { material_display(&m) }));
                    return flags::ALL | flags::TOOL;
                }
                _ => {}
            }
            let hit = self.pick_3d_t(px, py);
            if self.alt && !self.captured {
                // Alt+drag: grab the brush and slide it across the floor plane.
                if let Some((bid, t)) = hit {
                    if !self.doc.is_selected(bid) {
                        self.click_select(Some(bid), ctrl);
                    }
                    let (o, d) = self.persp_ray(px, py);
                    let start = o + d * t;
                    self.drag = Some(Drag::Move3D { plane_z: start.z, start, orig_brushes: self.doc.selected_brushes(), changed: false });
                    return flags::ALL;
                }
            }
            self.click_select(hit.map(|h| h.0), ctrl);
            return flags::ALL;
        }
        0
    }

    /// Translate brushes from their drag-start state.
    fn translate_brushes(&mut self, orig: &[Brush], delta: V3, changed: &mut bool) {
        if !*changed {
            self.doc.checkpoint();
            *changed = true;
        }
        let lock = self.tex_lock;
        for ob in orig {
            if let Some(b) = self.doc.brush_mut(ob.id) {
                for (dst, src) in b.verts.iter_mut().zip(&ob.verts) {
                    *dst = v3(tidy(src.x + delta.x), tidy(src.y + delta.y), tidy(src.z + delta.z));
                }
                b.recompute_planes();
                for (f, of) in b.faces.iter_mut().zip(&ob.faces) {
                    f.tex = if lock { of.tex.translated(delta) } else { of.tex };
                }
            }
        }
    }

    pub fn mouse_move(&mut self, id: usize, x: f32, y: f32, m: i32) -> u32 {
        self.set_mods(m);
        self.hover = Some((id, x, y));
        if id >= 1 {
            self.maybe_start_s_mouse(id, x, y);
            self.maybe_start_shift_clone(id, x, y);
        }
        let Some(drag) = self.drag.take() else {
            return self.hover_cursor(id, x, y);
        };
        let mut out = flags::ALL;
        let drag = match drag {
            Drag::Pan { view, last } => {
                let v = &mut self.views[view - 1];
                v.cx -= (x - last.0) / v.zoom;
                v.cy += (y - last.1) / v.zoom;
                out = flags::R2D;
                Drag::Pan { view, last: (x, y) }
            }
            Drag::TexDrag { start, face, mode, orig, mut moved, ctrl_click } => {
                if !moved && ((x - start.0).abs() > 3.0 || (y - start.1).abs() > 3.0) {
                    moved = true;
                    self.doc.checkpoint();
                }
                if moved {
                    self.tex_drag_update(start, (x, y), face, mode, &orig);
                }
                Drag::TexDrag { start, face, mode, orig, moved, ctrl_click }
            }
            Drag::PaintFaces { last } => {
                self.drag = Some(Drag::PaintFaces { last });
                return self.paint_drag_at(x, y) | flags::ALL;
            }
            Drag::Look { last, start, mut face_click } => {
                if face_click && ((x - start.0).abs() > 4.0 || (y - start.1).abs() > 4.0) {
                    face_click = false;
                }
                if !face_click {
                    self.look(x - last.0, y - last.1);
                }
                Drag::Look { last: (x, y), start, face_click }
            }
            Drag::Orbit { last, pivot, dist } => {
                self.look(x - last.0, y - last.1);
                self.cam.pos = pivot - self.cam.forward() * dist;
                Drag::Orbit { last: (x, y), pivot, dist }
            }
            Drag::Pan3D { last, dist } => {
                let (_, h) = self.persp_size;
                let k = 2.0 * dist * (self.fov.to_radians() * 0.5).tan() / h;
                let mv = self.cam.right() * (-(x - last.0) * k) + self.cam.up() * ((y - last.1) * k);
                self.cam.pos = self.cam.pos + mv;
                Drag::Pan3D { last: (x, y), dist }
            }
            d @ Drag::Paint { .. } => {
                self.drag = Some(d);
                if id == 0 && !self.captured {
                    return self.paint_at(x, y);
                }
                return 0;
            }
            Drag::OriginDrag { view, mut changed } => {
                let (wh, wv) = self.view(view).to_world(x, y);
                self.set_origin_2d(view, wh, wv, &mut changed);
                Drag::OriginDrag { view, changed }
            }
            Drag::BisectNew { view, start, start_px, ctrl } => {
                if (x - start_px.0).abs() > 3.0 || (y - start_px.1).abs() > 3.0 {
                    let end = self.bisect_snap(view, x, y);
                    if self.bisect_line.map_or(true, |l| l.1 != start) {
                        self.bisect_flip = 0; // a new cut starts unflipped
                    }
                    self.bisect_line = if end != start { Some((view, start, end)) } else { None };
                }
                Drag::BisectNew { view, start, start_px, ctrl }
            }
            Drag::BisectEnd { view, which } => {
                let p = self.bisect_snap(view, x, y);
                if let Some(line) = self.bisect_line.as_mut() {
                    if which == 0 { line.1 = p } else { line.2 = p }
                }
                Drag::BisectEnd { view, which }
            }
            Drag::SSize { view, target, orig, orig_brushes, grab, hx, hy, mut changed } => {
                let v = self.view(view);
                let (ha, va, _) = v.kind.axes();
                let (wh, wv) = v.to_world(x, y);
                let min = if self.free { 1.0 } else { self.grid };
                let mut nb = orig;
                for (axis, side, d) in [(ha, hx, wh - grab.0), (va, hy, wv - grab.1)] {
                    let (lo, hi) = (orig.0.get(axis), orig.1.get(axis));
                    let m = min.min(hi - lo);
                    if side == 0 {
                        continue;
                    }
                    if side > 0 {
                        nb.1.set(axis, self.snap(hi + d).max(lo + m));
                    } else {
                        nb.0.set(axis, self.snap(lo + d).min(hi - m));
                    }
                }
                if self.ctrl && hx != 0 && hy != 0 {
                    // Keep proportions: follow whichever axis changed more.
                    let (ow, oh) = (orig.1.get(ha) - orig.0.get(ha), orig.1.get(va) - orig.0.get(va));
                    let (nw, nh) = (nb.1.get(ha) - nb.0.get(ha), nb.1.get(va) - nb.0.get(va));
                    let (sx, sy) = (nw / ow.max(1e-6), nh / oh.max(1e-6));
                    let k = if (sx - 1.0).abs() >= (sy - 1.0).abs() { sx } else { sy };
                    let k = k.max(min / ow.max(1e-6)).max(min / oh.max(1e-6));
                    let (w2, h2) = (ow * k, oh * k);
                    if hx > 0 { nb.1.set(ha, tidy(orig.0.get(ha) + w2)) } else { nb.0.set(ha, tidy(orig.1.get(ha) - w2)) }
                    if hy > 0 { nb.1.set(va, tidy(orig.0.get(va) + h2)) } else { nb.0.set(va, tidy(orig.1.get(va) - h2)) }
                }
                if changed || nb != orig {
                    self.apply_box(target, orig, nb, &orig_brushes, &mut changed);
                    if target != Target::Selection {
                        changed = true;
                    }
                } else {
                    out = 0;
                }
                Drag::SSize { view, target, orig, orig_brushes, grab, hx, hy, changed }
            }
            Drag::Move3D { plane_z, start, orig_brushes, mut changed } => {
                let (o, d) = self.persp_ray(x, y);
                let mut moved = false;
                if d.z.abs() > 1e-4 {
                    let t = (plane_z - o.z) / d.z;
                    if t > 0.0 {
                        let p = o + d * t;
                        let mut delta = v3(p.x - start.x, p.y - start.y, 0.0);
                        let near = delta.len() < self.grid * 0.5 && self.hold_carrying();
                        if self.hold_carrying() && self.axis_locked() {
                            if self.axis_commit.is_none() && delta.len() >= self.grid.max(4.0) {
                                self.axis_commit = Some(if delta.x.abs() >= delta.y.abs() { 0 } else { 1 });
                            }
                            match self.axis_commit {
                                Some(0) => delta.y = 0.0,
                                Some(_) => delta.x = 0.0,
                                None => delta = V3::default(),
                            }
                        }
                        delta.x = self.snap(delta.x);
                        delta.y = self.snap(delta.y);
                        if near {
                            delta = V3::default(); // back where it started
                        }
                        if delta.len() < 1e5 && (changed || delta != V3::default()) {
                            self.translate_brushes(&orig_brushes, delta, &mut changed);
                            moved = true;
                        }
                    }
                }
                if !moved {
                    out = 0;
                }
                Drag::Move3D { plane_z, start, orig_brushes, changed }
            }
            Drag::Marquee { view, start, ctrl, .. } => Drag::Marquee { view, start, cur: (x, y), ctrl },
            Drag::SelNew { view, start, start_px } => {
                let v = self.view(view);
                let (ha, va, da) = v.kind.axes();
                let (wh, wv) = v.to_world(x, y);
                let (h0, h1) = (self.snap(start.0), self.snap(wh));
                let (v0, v1) = (self.snap(start.1), self.snap(wv));
                let moved = (x - start_px.0).abs() > 3.0 || (y - start_px.1).abs() > 3.0;
                if moved && h0 != h1 && v0 != v1 {
                    // depth: everything in the scene, so the box catches all of a column
                    let (mut dlo, mut dhi) = (-512.0f32, 512.0f32);
                    for b in &self.doc.brushes {
                        let (l, h) = b.bounds();
                        dlo = dlo.min(self.snap(l.get(da)) - self.grid);
                        dhi = dhi.max(self.snap(h.get(da)) + self.grid);
                    }
                    let mut lo = V3::default();
                    let mut hi = V3::default();
                    lo.set(ha, h0.min(h1));
                    hi.set(ha, h0.max(h1));
                    lo.set(va, v0.min(v1));
                    hi.set(va, v0.max(v1));
                    lo.set(da, dlo);
                    hi.set(da, dhi);
                    self.sel_box = Some((lo, hi));
                } else {
                    self.sel_box = None;
                }
                self.live_sel_box();
                Drag::SelNew { view, start, start_px }
            }
            Drag::NewBox { view, start, start_px, ctrl, quick } => {
                let v = self.view(view);
                let (ha, va, da) = v.kind.axes();
                let (wh, wv) = v.to_world(x, y);
                let (h0, h1) = (self.snap(start.0), self.snap(wh));
                let (v0, v1) = (self.snap(start.1), self.snap(wv));
                let moved = (x - start_px.0).abs() > 3.0 || (y - start_px.1).abs() > 3.0;
                if moved && h0 != h1 && v0 != v1 {
                    let mut lo = V3::default();
                    let mut hi = V3::default();
                    lo.set(ha, h0.min(h1));
                    hi.set(ha, h0.max(h1));
                    lo.set(va, v0.min(v1));
                    hi.set(va, v0.max(v1));
                    if quick {
                        // one grid unit deep: on the floor or on top of the selection (top view),
                        // where the Block tool would start it otherwise
                        let base = if da == 2 {
                            self.doc.selection_bounds().map(|b| b.1.z).unwrap_or(0.0)
                        } else {
                            self.work.0.get(da)
                        };
                        lo.set(da, base);
                        hi.set(da, base + self.grid);
                    } else {
                        lo.set(da, self.work.0.get(da));
                        hi.set(da, self.work.1.get(da));
                    }
                    self.block_box = Some((lo, hi));
                    self.block_axis = (da, ha);
                } else {
                    self.block_box = None;
                }
                Drag::NewBox { view, start, start_px, ctrl, quick }
            }
            Drag::Move { view, target, grab, orig, orig_brushes, mut changed, toggle_on_click, clone } => {
                let v = self.view(view);
                let (ha, va, _) = v.kind.axes();
                let (wh, wv) = v.to_world(x, y);
                let mut nb = orig;
                let mut dh = self.snap(orig.0.get(ha) + wh - grab.0) - orig.0.get(ha);
                let mut dv = self.snap(orig.0.get(va) + wv - grab.1) - orig.0.get(va);
                // back near where it started: exactly where it was
                let px = ((wh - grab.0).powi(2) + (wv - grab.1).powi(2)).sqrt() * v.zoom;
                if px < MOVE_DEAD_ZONE_PX && self.hold_carrying() {
                    dh = 0.0;
                    dv = 0.0;
                }
                // a G / Shift hold with the axis lock on: the first clear direction wins
                // and stays for the whole hold (Alt frees it)
                if self.hold_carrying() && self.axis_locked() {
                    if self.axis_commit.is_none() && px >= AXIS_COMMIT_PX {
                        self.axis_commit = Some(if (wh - grab.0).abs() >= (wv - grab.1).abs() { 0 } else { 1 });
                    }
                    match self.axis_commit {
                        Some(0) => dv = 0.0,
                        Some(_) => dh = 0.0,
                        None => {
                            dh = 0.0;
                            dv = 0.0;
                        }
                    }
                }
                for (axis, d) in [(ha, dh), (va, dv)] {
                    nb.0.set(axis, orig.0.get(axis) + d);
                    nb.1.set(axis, orig.1.get(axis) + d);
                }
                if changed || nb != orig {
                    self.apply_box(target, orig, nb, &orig_brushes, &mut changed);
                } else {
                    out = 0;
                }
                if clone {
                    let sg = |v: f32| if v >= 0.0 { format!("+{}", fmt_num(v)) } else { fmt_num(v) };
                    self.message = Some(format!("Cloning: {} {}  (release Shift to place, Esc cancels)", sg(dh), sg(dv)));
                }
                Drag::Move { view, target, grab, orig, orig_brushes, changed, toggle_on_click, clone }
            }
            Drag::Resize { view, target, hx, hy, orig, orig_brushes, mut changed } => {
                let v = self.view(view);
                let (ha, va, _) = v.kind.axes();
                let (wh, wv) = v.to_world(x, y);
                let mut nb = orig;
                for (axis, side, w) in [(ha, hx, wh), (va, hy, wv)] {
                    let s = self.snap(w);
                    let (lo, hi) = (orig.0.get(axis), orig.1.get(axis));
                    let min_size = if self.free { 1.0 } else { self.grid.min(hi - lo).max(1.0) };
                    if side > 0 {
                        nb.1.set(axis, if s < lo + min_size { lo + min_size } else { s });
                    } else if side < 0 {
                        nb.0.set(axis, if s > hi - min_size { hi - min_size } else { s });
                    }
                }
                if changed || nb != orig {
                    self.apply_box(target, orig, nb, &orig_brushes, &mut changed);
                } else {
                    out = 0;
                }
                Drag::Resize { view, target, hx, hy, orig, orig_brushes, changed }
            }
            Drag::Rotate { view, center, start_ang, orig_brushes, mut changed, by_key } => {
                let v = self.view(view);
                let (ha, va, da) = v.kind.axes();
                let (wh, wv) = v.to_world(x, y);
                // R held: left / right turns it (½° per pixel, right = clockwise)
                let mut ang = if by_key { -(x - start_ang) * 0.5f32.to_radians() } else { (wv - center.1).atan2(wh - center.0) - start_ang };
                if !self.free {
                    let s = self.cfg.rot_snap.max(1.0).to_radians();
                    ang = (ang / s).round() * s;
                }
                if changed || ang != 0.0 {
                    if !changed {
                        self.doc.checkpoint();
                        changed = true;
                    }
                    for ob in &orig_brushes {
                        if let Some(b) = self.doc.brush_mut(ob.id) {
                            if b.entity.is_some() {
                                // Entities stay upright: the base orbits, and a
                                // top-view turn changes which way it faces.
                                let base = rotate_points(&[ob.base()], ha, va, center, ang)[0];
                                if da == 2 {
                                    b.rot[2] = wrap_deg(ob.rot[2] + ang.to_degrees());
                                }
                                b.place_entity(base);
                                continue;
                            }
                            b.verts = rotate_points(&ob.verts, ha, va, center, ang);
                            b.rot = ob.rot;
                            b.rot[da] = wrap_deg(ob.rot[da] + ang.to_degrees());
                            b.recompute_planes();
                            // texture lock: turn the textures with the brush
                            let (mut eh, mut ev, mut c3) = (V3::default(), V3::default(), V3::default());
                            eh.set(ha, 1.0);
                            ev.set(va, 1.0);
                            c3.set(ha, center.0);
                            c3.set(va, center.1);
                            let k = eh.cross(ev);
                            for (f, of) in b.faces.iter_mut().zip(&ob.faces) {
                                f.tex = if self.tex_lock { of.tex.rotated_with(k, c3, ang.to_degrees()) } else { of.tex }.or_default_for(f.normal);
                            }
                        }
                    }
                    let deg = ang.to_degrees();
                    self.message = Some(format!("Rotate {}°", fmt_num((deg * 10.0).round() / 10.0)));
                } else {
                    out = 0;
                }
                Drag::Rotate { view, center, start_ang, orig_brushes, changed, by_key }
            }
            Drag::VRotate { view, center, start_x, orig_brushes, sel, mut changed } => {
                let v = self.view(view);
                let (ha, va, _) = v.kind.axes();
                let mut ang = -(x - start_x) * 0.5f32.to_radians();
                if !self.free {
                    let s = self.cfg.rot_snap.max(1.0).to_radians();
                    ang = (ang / s).round() * s;
                }
                if changed || ang != 0.0 {
                    let c = (center.get(ha), center.get(va));
                    self.apply_vertex_map(&orig_brushes, &sel, &|p| rotate_points(&[p], ha, va, c, ang)[0], &mut changed);
                } else {
                    out = 0;
                }
                Drag::VRotate { view, center, start_x, orig_brushes, sel, changed }
            }
            Drag::VScale { view, center, center_px, start_d, orig_brushes, sel, mut changed } => {
                let v = self.view(view);
                let (ha, va, _) = v.kind.axes();
                let d = ((x - center_px.0).powi(2) + (y - center_px.1).powi(2)).sqrt();
                let mut k = d / start_d.max(4.0);
                if !self.free {
                    k = (k * 20.0).round() / 20.0; // 5% steps (Alt: smooth)
                }
                let k = k.max(0.05);
                if changed || (k - 1.0).abs() > 1e-4 {
                    let f = |p: V3| {
                        let mut q = p;
                        q.set(ha, tidy(center.get(ha) + (p.get(ha) - center.get(ha)) * k));
                        q.set(va, tidy(center.get(va) + (p.get(va) - center.get(va)) * k));
                        q
                    };
                    self.apply_vertex_map(&orig_brushes, &sel, &f, &mut changed);
                    self.message = Some(format!("Scaling vertices: {:.0}%", k * 100.0));
                } else {
                    out = 0;
                }
                Drag::VScale { view, center, center_px, start_d, orig_brushes, sel, changed }
            }
            Drag::Vertex { view, grab, anchor, orig_brushes, sel, mut changed } => {
                let v = self.view(view);
                let (ha, va, _) = v.kind.axes();
                let (wh, wv) = v.to_world(x, y);
                let mut delta = V3::default();
                delta.set(ha, self.snap(anchor.get(ha) + wh - grab.0) - anchor.get(ha));
                delta.set(va, self.snap(anchor.get(va) + wv - grab.1) - anchor.get(va));
                if self.hold_carrying() {
                    // G with vertices: same dead zone and axis lock as moving brushes
                    let px = ((wh - grab.0).powi(2) + (wv - grab.1).powi(2)).sqrt() * v.zoom;
                    if px < MOVE_DEAD_ZONE_PX {
                        delta = V3::default();
                    }
                    if self.axis_locked() {
                        if self.axis_commit.is_none() && px >= AXIS_COMMIT_PX {
                            self.axis_commit = Some(if (wh - grab.0).abs() >= (wv - grab.1).abs() { 0 } else { 1 });
                        }
                        match self.axis_commit {
                            Some(0) => delta.set(va, 0.0),
                            Some(_) => delta.set(ha, 0.0),
                            None => delta = V3::default(),
                        }
                    }
                }
                if changed || delta != V3::default() {
                    self.apply_vertex_move(&orig_brushes, &sel, delta, &mut changed);
                } else {
                    out = 0;
                }
                Drag::Vertex { view, grab, anchor, orig_brushes, sel, changed }
            }
        };
        self.drag = Some(drag);
        out
    }

    /// Move the selected vertices by `delta` (from the drag-start state). Every
    /// affected brush must stay a valid convex solid, or nothing changes.
    fn apply_vertex_move(&mut self, orig: &[Brush], sel: &[(u32, V3)], delta: V3, changed: &mut bool) {
        self.apply_vertex_map(orig, sel, &|p| p + delta, changed);
    }

    /// Send the selected vertices through `f`. A brush that would turn concave
    /// or flat keeps its old shape (the others still change).
    fn apply_vertex_map(&mut self, orig: &[Brush], sel: &[(u32, V3)], f: &dyn Fn(V3) -> V3, changed: &mut bool) {
        let mut rebuilt: Vec<Brush> = Vec::new();
        let mut refused: Vec<u32> = Vec::new();
        for ob in orig {
            let mut any = false;
            let pts: Vec<V3> = ob
                .verts
                .iter()
                .map(|&p| {
                    if sel.iter().any(|&(i, q)| i == ob.id && (q - p).len() < 1e-3) {
                        any = true;
                        f(p)
                    } else {
                        p
                    }
                })
                .collect();
            if !any {
                continue;
            }
            let mut nb = ob.clone();
            if !nb.set_points(&pts) {
                refused.push(ob.id);
                nb = ob.clone(); // snaps back to how it was
            }
            rebuilt.push(nb);
        }
        if refused.len() == rebuilt.len() && !rebuilt.is_empty() && !*changed {
            self.message = Some("Can't move there: the brush would become concave or flat".into());
            return;
        }
        if !*changed {
            self.doc.checkpoint();
            *changed = true;
        }
        for nb in rebuilt {
            if let Some(b) = self.doc.brush_mut(nb.id) {
                *b = nb;
            }
        }
        // Keep moved vertices selected (merged ones collapse into one).
        let mut nsel: Vec<(u32, V3)> = Vec::new();
        for &(id, p) in sel {
            let q = if refused.contains(&id) { p } else { f(p) };
            if self.doc.brush(id).is_some_and(|b| b.has_vertex(q)) && !nsel.iter().any(|&(i, r)| i == id && (r - q).len() < 1e-3) {
                nsel.push((id, q));
            }
        }
        self.vsel = nsel;
        self.message = match refused.len() {
            0 => None,
            1 => Some("1 brush would have become concave or flat: it stays as it was".into()),
            n => Some(format!("{n} brushes would have become concave or flat: they stay as they were")),
        };
    }

    /// Paste clipboard text from `write_clip` (this model or another one).
    /// `at_mouse`: centered under the mouse in the 2D view it's over, else in
    /// the same spot it was copied from. Returns how many brushes came in.
    pub fn paste(&mut self, text: &str, at_mouse: bool) -> Result<usize, String> {
        let l = crate::io::read(text)?;
        if l.brushes.is_empty() {
            return Err("Nothing to paste".into());
        }
        let active = self.doc.active_layer;
        if self.doc.layer(active).is_some_and(|l| l.locked) {
            return Err("The active layer is locked: unlock it or pick another layer".into());
        }
        self.doc.checkpoint();
        for (m, props) in l.mat_overrides {
            if !self.doc.mat_overrides.iter().any(|(k, _)| *k == m) {
                self.doc.mat_overrides.push((m, props));
            }
        }
        let ids = self.doc.paste(l.brushes, l.groups, active);
        self.doc.selection = ids.clone();
        self.vsel.clear();
        if at_mouse {
            if let (Some((id, x, y)), Some((lo, hi))) = (self.hover, self.doc.selection_bounds()) {
                if (1..=3).contains(&id) {
                    let v = self.view(id);
                    let (ha, va, _) = v.kind.axes();
                    let (wh, wv) = v.to_world(x, y);
                    let mut d = V3::default();
                    d.set(ha, self.snap(wh - (hi.get(ha) - lo.get(ha)) * 0.5) - lo.get(ha));
                    d.set(va, self.snap(wv - (hi.get(va) - lo.get(va)) * 0.5) - lo.get(va));
                    let orig = self.doc.selected_brushes();
                    let mut changed = true;
                    self.translate_brushes(&orig, d, &mut changed);
                }
            }
        }
        if let Some(b) = self.doc.selection_bounds() {
            self.work = b;
            self.ripple = Some((b, self.time));
        }
        if self.tool != Tool::Select && self.tool != Tool::Block {
            self.tool = Tool::Select;
        }
        let n = ids.len();
        self.message = Some(format!("Pasted {} brush{}: G moves them", n, if n == 1 { "" } else { "es" }));
        Ok(n)
    }

    /// How high the selection sits above whatever is under it (the floor, a
    /// table…): saved with a prefab so it can land at the same height again.
    pub fn selection_lift(&self) -> f32 {
        let Some((lo, hi)) = self.doc.selection_bounds() else { return 0.0 };
        let c = (lo + hi) * 0.5;
        let floor = self.surface_below(c.x, c.y, lo.z - 0.01);
        (lo.z - floor).max(0.0)
    }

    /// Where a prefab dropped at (x, y) in view `id` goes: its bottom center.
    /// In the 3D view and the top view it stands on the surface under the
    /// mouse (`lift` higher); in the front and side views it's where the mouse
    /// is, its depth unchanged (`None` there for that axis).
    fn prefab_target(&self, id: usize, x: f32, y: f32, lift: f32) -> (V3, [bool; 3]) {
        if id == 0 {
            let mut b = self.place_base_3d(x, y);
            b.z += lift;
            return (b, [true, true, true]);
        }
        let v = self.view(id);
        let (ha, va, _) = v.kind.axes();
        let (wh, wv) = v.to_world(x, y);
        let mut b = V3::default();
        b.set(ha, wh);
        b.set(va, wv);
        let mut use_axis = [false; 3];
        use_axis[ha] = true;
        use_axis[va] = true;
        if v.kind == ViewKind::Top {
            b.z = self.surface_below(b.x, b.y, 1.0e5) + lift;
            use_axis[2] = true;
        } else {
            b.set(va, self.snap(wv));
        }
        (b, use_axis)
    }

    /// Place a prefab (clip text): in view `at` (id, x, y) on what's under the
    /// mouse, else under the last mouse position, else where it was saved.
    /// `lift`: keep its height above the floor (Some) or sit on the surface.
    pub fn place_prefab(&mut self, text: &str, at: Option<(usize, f32, f32)>, lift: Option<f32>) -> Result<usize, String> {
        let at = at.or(self.hover);
        // where it goes, worked out before it's there to get in the way
        let target = at.filter(|(id, _, _)| *id <= 3).map(|(id, x, y)| self.prefab_target(id, x, y, lift.unwrap_or(0.0)));
        let n = self.paste(text, false)?;
        if let (Some((base, use_axis)), Some((lo, hi))) = (target, self.doc.selection_bounds()) {
            let mut d = V3::default();
            for a in 0..3 {
                if !use_axis[a] {
                    continue;
                }
                let want = if a == 2 { base.z } else { self.snap(base.get(a) - (hi.get(a) - lo.get(a)) * 0.5) };
                d.set(a, want - lo.get(a));
            }
            let orig = self.doc.selected_brushes();
            let mut changed = true;
            self.translate_brushes(&orig, d, &mut changed);
            if let Some(b) = self.doc.selection_bounds() {
                self.work = b;
                self.ripple = Some((b, self.time));
            }
        }
        self.message = Some(format!("Placed {} brush{}: G moves them", n, if n == 1 { "" } else { "es" }));
        Ok(n)
    }

    /// Join the selected brushes into one convex brush: exactly their shape when
    /// they make one together, else (unless `exact_only`) a hull around them.
    /// Faces keep the materials of the faces they came from.
    fn join_selection(&mut self, exact_only: bool) -> u32 {
        let sel: Vec<Brush> = self.doc.selected_brushes().into_iter().filter(|b| b.entity.is_none()).collect();
        if sel.len() < 2 {
            self.message = Some("Select two or more brushes to join".into());
            return flags::ALL;
        }
        let refs: Vec<&Brush> = sel.iter().collect();
        let exact = crate::collision::union_is_convex(&refs);
        let was_exact = exact.is_some();
        let joined = match exact {
            Some(b) => b,
            None if exact_only => {
                self.message = Some("These don't make one convex shape: Edit → Join Brushes (Hull) wraps them in one".into());
                return flags::ALL;
            }
            None => {
                let pts: Vec<V3> = sel.iter().flat_map(|b| b.verts.iter().copied()).collect();
                match crate::collision::hull_brush(&pts) {
                    Some(b) => b,
                    None => return flags::ALL,
                }
            }
        };
        let first = &sel[0];
        let mut nb = joined;
        nb.id = first.id;
        nb.color = first.color;
        nb.layer = first.layer;
        nb.group = first.group;
        nb.entity = None;
        nb.rot = [0.0; 3];
        let old: Vec<crate::geom::Face> = sel.iter().flat_map(|b| b.faces.iter().cloned()).collect();
        for f in nb.faces.iter_mut() {
            f.material = geom::NODRAW.to_string();
        }
        carry_materials(&old, &mut nb.faces);
        self.doc.checkpoint();
        let gone: Vec<u32> = sel[1..].iter().map(|b| b.id).collect();
        self.doc.brushes.retain(|b| !gone.contains(&b.id));
        if let Some(b) = self.doc.brush_mut(first.id) {
            *b = nb;
        }
        self.doc.selection = vec![first.id];
        self.doc.prune_groups();
        self.vsel.clear();
        self.fsel.clear();
        let n = sel.len();
        self.message = Some(format!("Joined {n} brushes {}", if was_exact { "(same shape)" } else { "into one hull around them" }));
        flags::ALL
    }

    /// Center of the selected vertices.
    fn vsel_center(&self) -> Option<V3> {
        if self.vsel.is_empty() {
            return None;
        }
        let (mut lo, mut hi) = (self.vsel[0].1, self.vsel[0].1);
        for &(_, p) in &self.vsel {
            lo = lo.min(p);
            hi = hi.max(p);
        }
        Some((lo + hi) * 0.5)
    }

    /// Map the target from box `orig` to box `nb`.
    fn apply_box(&mut self, target: Target, orig: (V3, V3), nb: (V3, V3), orig_brushes: &[Brush], changed: &mut bool) {
        match target {
            Target::Block => {
                self.block_box = Some(nb);
            }
            Target::SelBox => {
                self.sel_box = Some(nb);
                self.live_sel_box();
            }
            Target::Selection => {
                if !*changed {
                    self.doc.checkpoint();
                    *changed = true;
                }
                let map = |p: V3| {
                    let mut r = p;
                    for a in 0..3 {
                        let (o0, o1) = (orig.0.get(a), orig.1.get(a));
                        let (n0, n1) = (nb.0.get(a), nb.1.get(a));
                        let t = if o1 - o0 > 1e-6 { (p.get(a) - o0) / (o1 - o0) } else { 0.0 };
                        r.set(a, tidy(n0 + t * (n1 - n0)));
                    }
                    r
                };
                // Texture lock: a box that only moved carries its textures
                // along; resizing leaves them anchored (or stretches them with
                // "scale texture with brush").
                let (mut k, mut shift) = (v3(1.0, 1.0, 1.0), V3::default());
                for a in 0..3 {
                    let (ow, nw) = (orig.1.get(a) - orig.0.get(a), nb.1.get(a) - nb.0.get(a));
                    if ow > 1e-6 {
                        k.set(a, nw / ow);
                    }
                    if (nw - ow).abs() < 1e-4 {
                        shift.set(a, nb.0.get(a) - orig.0.get(a));
                    }
                }
                let (lock, stretch) = (self.tex_lock, self.tex_scale);
                for ob in orig_brushes {
                    if let Some(b) = self.doc.brush_mut(ob.id) {
                        if b.entity.is_some() {
                            // Entities keep their size: only their base point follows.
                            b.place_entity(map(ob.base()));
                            continue;
                        }
                        for (dst, src) in b.verts.iter_mut().zip(&ob.verts) {
                            *dst = map(*src);
                        }
                        b.recompute_planes();
                        for (f, of) in b.faces.iter_mut().zip(&ob.faces) {
                            f.tex = if !lock {
                                of.tex
                            } else if stretch {
                                of.tex.scaled_with(k, orig.0).translated(nb.0 - orig.0)
                            } else {
                                of.tex.translated(shift)
                            }
                            .or_default_for(f.normal);
                        }
                    }
                }
            }
        }
    }

    pub fn mouse_release(&mut self, id: usize, x: f32, y: f32, _btn: i32) -> u32 {
        let had = self.drag.take();
        let mut out = 0;
        match had {
            Some(Drag::SelNew { .. }) => {
                if self.sel_box.is_none() {
                    if !self.ctrl {
                        self.click_select(None, false); // plain click on empty space: deselect
                    }
                } else if !self.sel_box_wait {
                    // instant: the box stays up, its brushes selected; its handles adjust it
                    let n = self.doc.selection.len();
                    self.message = Some(format!("{} brush{} selected: drag the box's edges to adjust; Delete, G, S, R work on them", n, if n == 1 { "" } else { "es" }));
                } else {
                    let n = self.sel_box_catch().len();
                    self.message = Some(format!("Selection box: {} brush{} inside. Enter selects (Ctrl+Enter adds), S / G adjust, Esc cancels", n, if n == 1 { "" } else { "es" }));
                }
                out |= flags::ALL;
            }
            Some(Drag::NewBox { quick: true, .. }) => {
                // Quick Draw: made as soon as you let go
                if self.block_box.is_some() {
                    out |= self.command(cmd::CREATE_BLOCK);
                    if let Some(b) = self.doc.selection_bounds() {
                        self.ripple = Some((b, self.time));
                    }
                }
                out |= flags::ALL;
            }
            Some(Drag::NewBox { view, start_px, ctrl, .. }) => {
                // A plain click (no box drawn) on a brush selects it, showing its resize handles.
                if self.block_box.is_none() {
                    let hit = self.pick_2d(view, start_px.0, start_px.1);
                    if hit.is_some() || !ctrl {
                        self.click_select(hit, ctrl);
                    }
                }
                out |= flags::ALL;
            }
            Some(Drag::Move { target: Target::Selection, clone: true, orig, .. }) => {
                // Shift+click without dragging: don't leave a copy stacked on the original.
                if self.doc.selection_bounds() == Some(orig) {
                    self.doc.revert_to_checkpoint();
                }
                out |= flags::ALL;
            }
            Some(Drag::Move { target: Target::Selection, changed, toggle_on_click, .. }) => {
                if !changed && toggle_on_click {
                    self.sel_mode = if self.sel_mode == HandleMode::Scale { HandleMode::Rotate } else { HandleMode::Scale };
                    out |= flags::ALL;
                }
                if let Some(b) = self.doc.selection_bounds() {
                    self.work = b;
                }
            }
            Some(Drag::Resize { target: Target::Selection, .. }) => {
                if let Some(b) = self.doc.selection_bounds() {
                    self.work = b;
                }
            }
            Some(d @ Drag::Rotate { by_key: true, .. }) => {
                self.drag = Some(d); // keeps going until R is released
            }
            Some(d @ Drag::SSize { .. }) => {
                self.drag = Some(d); // keeps going until S is released
            }
            Some(Drag::BisectNew { view, start_px, ctrl, .. }) => {
                let drew = self.bisect_line.is_some_and(|(lv, _, _)| lv == view) && ((x - start_px.0).abs() > 3.0 || (y - start_px.1).abs() > 3.0);
                if !drew {
                    let hit = self.pick_2d(view, start_px.0, start_px.1);
                    if hit.is_some() || !ctrl {
                        self.click_select(hit, ctrl);
                    }
                }
                out |= flags::ALL;
            }
            Some(Drag::Rotate { .. }) => {
                self.message = None;
                out |= flags::ALL;
            }
            Some(Drag::Orbit { .. }) => {
                self.orbit_pivot = None;
                out |= flags::ALL;
            }
            Some(Drag::PaintFaces { last: None }) => {
                self.doc.revert_to_checkpoint(); // painted nothing: no empty undo step
            }
            Some(Drag::Look { start, face_click: true, .. }) => {
                // Face Select & Paint: right-click paints that face (Shift: the
                // whole brush, Alt: continue the source face's texture onto it).
                if let Some((bid, _, Some(f))) = self.pick_3d_face(start.0, start.1) {
                    if self.doc.brush(bid).is_some_and(|b| b.entity.is_none()) {
                        out |= self.paint_face((bid, f), self.shift, self.alt);
                    }
                }
                out |= flags::ALL;
            }
            Some(Drag::TexDrag { face, moved: false, ctrl_click, .. }) => {
                if ctrl_click {
                    if self.fsel.contains(&face) {
                        self.fsel.retain(|&s| s != face);
                        if self.src_face == Some(face) {
                            self.src_face = self.fsel.last().copied();
                        }
                    } else {
                        self.fsel.push(face);
                        self.make_source(face);
                    }
                }
                out |= flags::ALL | flags::TOOL;
            }
            Some(Drag::TexDrag { moved: true, .. }) => {
                out |= flags::ALL | flags::TOOL;
            }
            Some(Drag::Marquee { view, start, cur, ctrl }) => {
                let (x0, x1) = (start.0.min(cur.0), start.0.max(cur.0));
                let (y0, y1) = (start.1.min(cur.1), start.1.max(cur.1));
                if !ctrl {
                    self.vsel.clear();
                }
                if x1 - x0 > 2.0 || y1 - y0 > 2.0 {
                    let v = self.view(view);
                    let mut add = Vec::new();
                    let any = self.vertex_pick;
                    for b in self.doc.brushes.iter().filter(|b| (self.doc.is_selected(b.id) || (any && self.pickable(b))) && b.entity.is_none()) {
                        for &p in &b.verts {
                            let (sx, sy) = v.project(p);
                            if sx >= x0 && sx <= x1 && sy >= y0 && sy <= y1 {
                                add.push((b.id, p));
                            }
                        }
                    }
                    for (bid, p) in add {
                        if !self.vertex_selected(bid, p) {
                            self.vsel.push((bid, p));
                        }
                        if !self.doc.is_selected(bid) {
                            self.doc.selection.push(bid);
                        }
                    }
                }
                out |= flags::ALL;
            }
            _ => {}
        }
        out | self.hover_cursor(id, x, y)
    }

    fn hover_cursor(&self, id: usize, x: f32, y: f32) -> u32 {
        if id == 0 {
            return flags::cursor(cursor::ARROW);
        }
        if self.keys.space {
            return flags::cursor(cursor::HAND);
        }
        if self.alt && !self.over_tool_handle(id, x, y) && self.pick_2d(id, x, y).is_some() {
            return flags::cursor(cursor::SIZE_ALL);
        }
        match self.tool {
            Tool::Entity => return flags::cursor(cursor::CROSS),
            Tool::Origin => return flags::cursor(if self.near_origin(id, x, y) { cursor::SIZE_ALL } else { cursor::CROSS }),
            Tool::Face => return flags::cursor(cursor::ARROW),
            _ => {}
        }
        if self.tool == Tool::Bisect {
            if let Some((lv, a, b)) = self.bisect_line {
                if lv == id {
                    let v = self.view(id);
                    for p in [a, b] {
                        let (sx, sy) = v.to_screen(p.0, p.1);
                        if (sx - x).abs() <= 8.0 && (sy - y).abs() <= 8.0 {
                            return flags::cursor(cursor::SIZE_ALL);
                        }
                    }
                }
            }
            return flags::cursor(cursor::CROSS);
        }
        if self.tool == Tool::Vertex {
            let over = !self.hit_vertices(id, x, y).is_empty() || !self.hit_edges(id, x, y).is_empty();
            return flags::cursor(if over { cursor::SIZE_ALL } else { cursor::ARROW });
        }
        let mode = if self.tool == Tool::Block && self.block_box.is_some() { HandleMode::Scale } else { self.handle_mode() };
        let hit = self.active_box().map(|b| self.hit_box(id, b, x, y, mode)).unwrap_or(Hit::None);
        let c = match hit {
            Hit::Handle(_, _) if mode == HandleMode::Rotate => cursor::POINT,
            Hit::Handle(hx, hy) => handle_cursor(hx, hy),
            Hit::Inside if self.tool == Tool::Select || self.block_box.is_some() => cursor::SIZE_ALL,
            _ => {
                if self.tool == Tool::Block {
                    cursor::CROSS
                } else {
                    cursor::ARROW
                }
            }
        };
        flags::cursor(c)
    }

    pub fn wheel(&mut self, id: usize, x: f32, y: f32, delta: f32) -> u32 {
        if matches!(self.shift_hold, Some((_, _, false))) {
            self.shift_hold = None; // Shift+wheel isn't a clone
        }
        if id == 0 {
            let f = self.cam.forward();
            self.cam.pos = self.cam.pos + f * (delta * 64.0);
            return flags::ALL;
        }
        let v = &mut self.views[id - 1];
        let (bh, bv) = v.to_world(x, y);
        v.zoom = (v.zoom * 1.2f32.powf(delta)).clamp(0.02, 64.0);
        let (ah, av) = v.to_world(x, y);
        v.cx += bh - ah;
        v.cy += bv - av;
        flags::R2D
    }

    /// Relative mouse motion while mouselook is captured.
    pub fn mouse_delta(&mut self, dx: f32, dy: f32) -> u32 {
        if !self.captured {
            return 0;
        }
        self.look(dx, dy);
        if matches!(self.drag, Some(Drag::Paint { .. })) {
            let (w, h) = self.persp_size;
            self.paint_at(w * 0.5, h * 0.5);
        }
        if matches!(self.drag, Some(Drag::PaintFaces { .. })) {
            let (w, h) = self.persp_size;
            self.paint_drag_at(w * 0.5, h * 0.5);
        }
        flags::ALL
    }

    fn look(&mut self, dx: f32, dy: f32) {
        self.cam.yaw = (self.cam.yaw - dx * LOOK_SENS).rem_euclid(360.0);
        self.cam.pitch = (self.cam.pitch - dy * LOOK_SENS).clamp(-89.0, 89.0);
    }

    // ------------------------------------------------------------------ keyboard

    pub fn key(&mut self, id: usize, k: i32, pressed: bool, m: i32, repeat: bool) -> u32 {
        let k3d = id == 0 || self.captured;
        // an instant box's brushes get worked on: the box steps aside (they stay selected)
        if pressed && !repeat && self.live_box_up()
            && matches!(k, key::G | key::S | key::R | key::H | key::COMMA | key::PERIOD | key::UP | key::DOWN | key::LEFT | key::RIGHT)
            && m & mods::CTRL == 0
        {
            self.sel_box = None;
        }
        // Hold G (grab) and move the mouse: move the selection (release G to place).
        if k == key::G && !self.captured && m & (mods::CTRL | mods::ALT) == 0 {
            if repeat {
                return 0;
            }
            if pressed {
                let ok_tool = matches!(self.tool, Tool::Select | Tool::Block | Tool::Entity | Tool::Face | Tool::Vertex);
                let has = !self.doc.selection.is_empty()
                    || (self.tool == Tool::Select && self.sel_box.is_some())
                    || (self.tool == Tool::Block && self.block_box.is_some());
                self.axis_flip = false;
        self.axis_commit = None;
                self.move_hold = match self.hover {
                    Some((0, x, y)) if id == 0 && ok_tool && self.drag.is_none() && !self.doc.selection.is_empty() => {
                        // 3D view: slide the selection across the floor plane under it
                        let b = self.doc.selection_bounds().unwrap();
                        let (o, d) = self.persp_ray(x, y);
                        let t = if d.z.abs() > 1e-4 { (b.0.z - o.z) / d.z } else { -1.0 };
                        if t <= 0.0 {
                            self.message = Some("Look toward the selection's floor to move it with G".into());
                            return flags::ALL;
                        }
                        let start = o + d * t;
                        self.drag = Some(Drag::Move3D { plane_z: b.0.z, start, orig_brushes: self.doc.selected_brushes(), changed: false });
                        self.hold_in_3d = true;
                        self.message = Some("Moving: move the mouse, release G to place, Esc cancels".into());
                        Some((0, (x, y), true))
                    }
                    Some((hid, x, y)) if hid == id && (1..=3).contains(&id) && ok_tool && self.s_hold.is_none() && self.drag.is_none()
                        && has => {
                        self.message = Some("Moving: move the mouse, release G to place, Esc cancels".into());
                        Some((id, (x, y), false))
                    }
                    _ => None,
                };
                self.update_free();
                return flags::ALL;
            }
            if let Some((_, _, moving)) = self.move_hold.take() {
                if moving {
                    return self.place_shift_clone();
                }
                self.message = None;
                return flags::ALL;
            }
            return 0;
        }
        // Hold Shift (on its own) and move the mouse: clone the selection.
        if k == key::SHIFT && !repeat {
            if pressed {
                let ok_tool = matches!(self.tool, Tool::Select | Tool::Block);
                self.axis_flip = false;
                self.axis_commit = None;
                self.shift_hold = match self.hover {
                    Some((hid, x, y)) if self.shift_clone && !k3d && hid == id && (1..=3).contains(&id) && ok_tool
                        && m & (mods::CTRL | mods::ALT) == 0 && self.s_hold.is_none() && self.drag.is_none()
                        && !self.doc.selection.is_empty() => Some((id, (x, y), false)),
                    _ => None,
                };
            } else if let Some((_, _, cloning)) = self.shift_hold.take() {
                self.shift = false;
                self.keys.shift = false;
                self.update_free();
                if cloning {
                    return self.place_shift_clone();
                }
            }
        } else if pressed && !repeat && k != key::ALT {
            if matches!(self.shift_hold, Some((_, _, false))) {
                self.shift_hold = None; // Shift+S, Shift+arrow…: not a clone
            }
        }
        // Vertex tool with vertices selected: S scales them about their center
        if k == key::S && !k3d && (1..=3).contains(&id) && self.tool == Tool::Vertex && !self.vsel.is_empty() && m & mods::CTRL == 0 {
            if repeat {
                return 0;
            }
            if pressed {
                if self.drag.is_none() {
                    if let (Some(center), Some((hid, x, y))) = (self.vsel_center(), self.hover) {
                        if hid == id {
                            let v = self.view(id);
                            let (ha, va, _) = v.kind.axes();
                            let cp = v.to_screen(center.get(ha), center.get(va));
                            let start_d = ((x - cp.0).powi(2) + (y - cp.1).powi(2)).sqrt();
                            self.drag = Some(Drag::VScale { view: id, center, center_px: cp, start_d, orig_brushes: self.doc.selected_brushes(), sel: self.vsel.clone(), changed: false });
                            self.message = Some("Scaling vertices: move away from the center to grow, toward it to shrink".into());
                        }
                    }
                }
            } else if matches!(self.drag, Some(Drag::VScale { .. })) {
                self.drag = None;
                self.message = None;
            }
            return flags::ALL;
        }
        // Hold S in a 2D view: arrows (or the mouse) resize instead of nudging.
        if !k3d && (1..=3).contains(&id) {
            if k == key::S && m & mods::CTRL == 0 {
                if pressed {
                    if !repeat && self.s_hold.is_none() {
                        return self.start_s_hold(id);
                    }
                    return 0;
                }
                return self.end_s_hold();
            }
            if pressed && self.s_hold.is_some() {
                if let Some((axis_h, dir)) = match k {
                    key::LEFT => Some((true, -1.0)),
                    key::RIGHT => Some((true, 1.0)),
                    key::UP => Some((false, 1.0)),
                    key::DOWN => Some((false, -1.0)),
                    _ => None,
                } {
                    let hold = self.s_hold.as_mut().unwrap();
                    if hold.used == SUse::Mouse {
                        return 0;
                    }
                    hold.used = SUse::Arrows;
                    return self.s_size_key(id, axis_h, dir, m & mods::SHIFT != 0, m & mods::ALT != 0, repeat);
                }
            }
        }
        // Arrow keys in the 3D view (not flying) nudge along the floor axis nearest
        // to where the camera points.
        if pressed && id == 0 && !self.captured && self.cfg.nudge {
            let dir = match k {
                key::LEFT => Some((-1, 0)),
                key::RIGHT => Some((1, 0)),
                key::UP => Some((0, 1)),
                key::DOWN => Some((0, -1)),
                _ => None,
            };
            if let Some(dir) = dir {
                return self.nudge_3d(dir, m & mods::ALT != 0, repeat);
            }
        }
        // Arrow keys in a 2D view nudge (holding repeats; one undo step per hold).
        if pressed && !k3d && self.cfg.nudge && (1..=3).contains(&id) {
            let dir = match k {
                key::LEFT => Some((-1, 0)),
                key::RIGHT => Some((1, 0)),
                key::UP => Some((0, 1)),
                key::DOWN => Some((0, -1)),
                _ => None,
            };
            if let Some(dir) = dir {
                return self.nudge(id, dir, m & mods::ALT != 0, repeat);
            }
        }
        if repeat {
            return 0;
        }
        match k {
            key::SHIFT => {
                self.keys.shift = pressed;
                self.shift = pressed;
                self.update_free();
                if self.s_hold.is_some() || (self.tool == Tool::Bisect && self.bisect_line.is_some()) {
                    return flags::ALL; // the deleted side flips while Shift is held
                }
            }
            key::ALT => {
                self.alt = pressed;
                if pressed && (self.move_hold.is_some() || self.shift_hold.is_some()) {
                    self.axis_flip = !self.axis_flip;
                    self.axis_commit = None; // locking again picks the axis afresh
                    self.message = Some(if self.axis_locked() { "Axis lock on: moves stay on one axis" } else { "Axis lock off: move freely" }.into());
                    self.update_free();
                    return flags::ALL;
                }
                self.update_free();
                return 0;
            }
            key::R if !self.captured && m & (mods::CTRL | mods::ALT) == 0 => {
                return if pressed { self.start_key_rotate() } else { self.end_key_rotate() };
            }
            key::H if pressed && !self.captured && m & (mods::CTRL | mods::ALT | mods::SHIFT) == 0 => {
                return self.command(cmd::HIDE_TOGGLE);
            }
            key::COMMA if pressed && !self.captured && m & (mods::CTRL | mods::ALT) == 0 => {
                return self.command(cmd::MIRROR_H);
            }
            key::PERIOD if pressed && !self.captured && m & (mods::CTRL | mods::ALT) == 0 => {
                return self.command(cmd::MIRROR_V);
            }
            key::SPACE => {
                self.keys.space = pressed;
                return 0;
            }
            key::F if pressed && self.tool == Tool::Bisect && self.bisect_line.is_some() && m & (mods::CTRL | mods::ALT) == 0 => {
                self.bisect_flip = (self.bisect_flip + 1) % 6;
                return flags::ALL;
            }
            key::D if !k3d && pressed && m & (mods::CTRL | mods::ALT | mods::SHIFT) == 0 && self.s_hold.is_none() => {
                return self.command(cmd::DESELECT); // D in a 2D view; Ctrl+D works everywhere (Edit menu)
            }
            // WASD / QE fly the 3D camera whenever the 3D view has the keys (mouselook or not)
            key::W if k3d => self.keys.w = pressed,
            key::A if k3d => self.keys.a = pressed,
            key::S if k3d => self.keys.s = pressed,
            key::D if k3d => self.keys.d = pressed,
            key::Q if k3d => self.keys.q = pressed,
            key::E if k3d => self.keys.e = pressed,
            key::UP if self.captured => self.keys.up = pressed,
            key::DOWN if self.captured => self.keys.down = pressed,
            key::LEFT if self.captured => self.keys.left = pressed,
            key::RIGHT if self.captured => self.keys.right = pressed,
            key::Z if pressed && m & (mods::CTRL | mods::ALT) == 0 => return self.toggle_capture(),
            key::ESCAPE if pressed => return self.escape(),
            key::RETURN | key::ENTER if pressed => {
                if self.tool == Tool::Select && self.sel_box.is_some() {
                    return self.apply_sel_box(m & mods::CTRL != 0);
                }
                return self.command(if self.tool == Tool::Bisect { cmd::BISECT_APPLY } else { cmd::CREATE_BLOCK });
            }
            _ => {}
        }
        0
    }

    /// Move the current target one grid step (or 1 unit with Alt) in a view's plane.
    fn nudge(&mut self, id: usize, dir: (i32, i32), fine: bool, repeat: bool) -> u32 {
        let step = if fine { 1.0 } else { self.grid };
        let (ha, va, _) = self.view(id).kind.axes();
        let mut d = V3::default();
        d.set(ha, dir.0 as f32 * step);
        d.set(va, dir.1 as f32 * step);
        self.message = None;
        if self.tool == Tool::Origin {
            let o = self.active_origin() + d;
            let mut changed = repeat;
            self.set_origin(o, &mut changed);
            return flags::ALL;
        }
        if self.tool == Tool::Block {
            if let Some((lo, hi)) = self.block_box {
                self.block_box = Some((lo + d, hi + d));
                return flags::ALL;
            }
        }
        if self.tool == Tool::Vertex && !self.vsel.is_empty() {
            let orig = self.doc.selected_brushes();
            let sel = self.vsel.clone();
            let mut changed = repeat;
            self.apply_vertex_move(&orig, &sel, d, &mut changed);
            return flags::ALL;
        }
        if self.doc.selection.is_empty() {
            return 0;
        }
        let orig = self.doc.selected_brushes();
        let mut changed = repeat; // auto-repeat joins the first press's undo step
        self.translate_brushes(&orig, d, &mut changed);
        flags::ALL
    }


    /// Arrow keys in the 3D view: right / up follow the camera, snapped to the
    /// nearest floor axis, so "up" moves away from you.
    fn nudge_3d(&mut self, dir: (i32, i32), fine: bool, repeat: bool) -> u32 {
        let step = if fine { 1.0 } else { self.grid };
        let axis = |v: V3| if v.x.abs() >= v.y.abs() { v3(v.x.signum(), 0.0, 0.0) } else { v3(0.0, v.y.signum(), 0.0) };
        let (r, f) = (axis(self.cam.right()), axis(self.cam.forward()));
        let d = (r * dir.0 as f32 + f * dir.1 as f32) * step;
        if self.tool == Tool::Origin {
            let o = self.active_origin() + d;
            let mut changed = repeat;
            self.set_origin(o, &mut changed);
            return flags::ALL;
        }
        if self.doc.selection.is_empty() {
            return 0;
        }
        self.message = None;
        let orig = self.doc.selected_brushes();
        let mut changed = repeat;
        self.translate_brushes(&orig, d, &mut changed);
        flags::ALL
    }

    /// Mirror the selection about its center along a world axis.
    fn mirror_selection(&mut self, axis: usize) -> u32 {
        let Some((lo, hi)) = self.doc.selection_bounds() else {
            self.message = Some("Select something to mirror".into());
            return flags::ALL;
        };
        let c = (lo.get(axis) + hi.get(axis)) * 0.5;
        self.doc.checkpoint();
        let ids = self.doc.selection.clone();
        for id in ids {
            if let Some(b) = self.doc.brush_mut(id) {
                b.mirror(axis, c);
            }
        }
        self.vsel.clear();
        self.message = Some(format!("Mirrored along {}", ["X", "Y", "Z"][axis]));
        flags::ALL
    }

    /// Snap vertices to the grid: selected vertices in the vertex tool,
    /// otherwise every vertex of the selected brushes.
    fn quantize(&mut self) {
        let g = self.grid;
        let q = |p: V3| v3(snap(p.x, g), snap(p.y, g), snap(p.z, g));
        if self.doc.selection.is_empty() {
            self.message = Some("Select brushes to quantize".into());
            return;
        }
        let only_vsel = self.tool == Tool::Vertex && !self.vsel.is_empty();
        let mut rebuilt = Vec::new();
        let mut skipped = 0;
        let mut straightened = 0;
        for b in self.doc.selected_brushes() {
            if b.entity.is_some() {
                if only_vsel {
                    continue;
                }
                // Entities: snap the base to the grid, straighten the facing.
                let mut nb = b.clone();
                let base = b.base();
                nb.rot[2] = wrap_deg((b.rot[2] / 45.0).round() * 45.0);
                nb.place_entity(v3(snap(base.x, g), snap(base.y, g), snap(base.z, g)));
                if nb != b {
                    if nb.rot != b.rot {
                        straightened += 1;
                    }
                    rebuilt.push(nb);
                }
                continue;
            }
            // First straighten off-angle rotations to the nearest 45° (whole-brush mode only).
            let mut verts = b.verts.clone();
            let mut rot = b.rot;
            if !only_vsel {
                let (lo, hi) = b.bounds();
                let c = (lo + hi) * 0.5;
                let mut any = false;
                for (axis, (ha, va)) in [(2usize, (0usize, 1usize)), (0, (1, 2)), (1, (0, 2))] {
                    let target = (rot[axis] / 45.0).round() * 45.0;
                    if (rot[axis] - target).abs() > 0.01 {
                        verts = rotate_points(&verts, ha, va, (c.get(ha), c.get(va)), (target - rot[axis]).to_radians());
                        rot[axis] = wrap_deg(target);
                        any = true;
                    }
                }
                if any {
                    straightened += 1;
                }
            }
            let pts: Vec<V3> = verts.iter().map(|&p| if !only_vsel || self.vertex_selected(b.id, p) { q(p) } else { p }).collect();
            if pts == b.verts && rot == b.rot {
                continue;
            }
            match geom::hull(&pts) {
                Some(h) => {
                    let mut nb = b.clone();
                    nb.set_hull(h);
                    nb.rot = rot;
                    rebuilt.push(nb);
                }
                None => skipped += 1,
            }
        }
        let n = rebuilt.len();
        if n > 0 {
            self.doc.checkpoint();
            for nb in rebuilt {
                if let Some(b) = self.doc.brush_mut(nb.id) {
                    *b = nb;
                }
            }
            self.prune_fsel();
        }
        let doc = &self.doc;
        let mut nsel: Vec<(u32, V3)> = Vec::new();
        for &(id, p) in &self.vsel {
            let p = q(p);
            if doc.brush(id).is_some_and(|b| b.has_vertex(p)) && !nsel.iter().any(|&(i, r)| i == id && (r - p).len() < 1e-3) {
                nsel.push((id, p));
            }
        }
        self.vsel = nsel;
        let mut msg = if n == 0 && skipped == 0 {
            format!("Already on grid {}", fmt_num(g))
        } else {
            format!("Quantized {} brush{} to grid {}", n, if n == 1 { "" } else { "es" }, fmt_num(g))
        };
        if straightened > 0 && n > 0 {
            msg = format!("Straightened to 45° and q{}", &msg[1..]);
        }
        if skipped > 0 {
            msg += &format!(" ({} skipped: would flatten, try a smaller grid)", skipped);
        }
        self.message = Some(msg);
    }

    /// Hold R over a selected brush in a 2D view to rotate the selection with the mouse.
    fn start_key_rotate(&mut self) -> u32 {
        if self.drag.is_some() {
            return 0;
        }
        let Some((id, x, y)) = self.hover else { return 0 };
        if self.tool == Tool::Vertex && (1..=3).contains(&id) {
            let Some(center) = self.vsel_center() else { return 0 };
            self.drag = Some(Drag::VRotate { view: id, center, start_x: x, orig_brushes: self.doc.selected_brushes(), sel: self.vsel.clone(), changed: false });
            return flags::ALL;
        }
        if id == 0 {
            // 3D view: turn about the vertical axis, steered like the top view
            let Some(b) = self.doc.selection_bounds() else { return 0 };
            let c = ((b.0.x + b.1.x) * 0.5, (b.0.y + b.1.y) * 0.5);
            let top = self.views.iter().position(|v| v.kind == ViewKind::Top).map(|i| i + 1).unwrap_or(1);
            self.hold_in_3d = true;
            self.drag = Some(Drag::Rotate { view: top, center: c, start_ang: x, orig_brushes: self.doc.selected_brushes(), changed: false, by_key: true });
            self.message = Some("Rotating: move the mouse left / right, release R to finish (Esc cancels)".into());
            return flags::ALL;
        }
        // Anywhere in the view: the selection turns about its center as the
        // mouse moves left / right (start_ang holds the starting pixel x).
        let Some(b) = self.doc.selection_bounds() else { return 0 };
        let v = self.view(id);
        let (ha, va, _) = v.kind.axes();
        let c = ((b.0.get(ha) + b.1.get(ha)) * 0.5, (b.0.get(va) + b.1.get(va)) * 0.5);
        let _ = y;
        self.drag = Some(Drag::Rotate { view: id, center: c, start_ang: x, orig_brushes: self.doc.selected_brushes(), changed: false, by_key: true });
        self.message = Some("Rotating: move the mouse left / right, release R to finish (Esc cancels)".into());
        flags::ALL | flags::cursor(cursor::POINT)
    }

    fn end_key_rotate(&mut self) -> u32 {
        if matches!(self.drag, Some(Drag::VRotate { .. })) {
            self.drag = None;
            return flags::ALL;
        }
        if matches!(self.drag, Some(Drag::Rotate { by_key: true, .. })) {
            self.drag = None;
            self.hold_in_3d = false;
            self.message = None;
            return flags::ALL;
        }
        0
    }

    fn drag_changed_doc(&self) -> bool {
        matches!(
            self.drag,
            Some(Drag::Move { target: Target::Selection, changed: true, .. })
                | Some(Drag::Resize { target: Target::Selection, changed: true, .. })
                | Some(Drag::Rotate { changed: true, .. })
                | Some(Drag::Vertex { changed: true, .. })
                | Some(Drag::VRotate { changed: true, .. })
                | Some(Drag::VScale { changed: true, .. })
                | Some(Drag::Move3D { changed: true, .. })
                | Some(Drag::SSize { target: Target::Selection, changed: true, .. })
                | Some(Drag::TexDrag { moved: true, .. })
        )
    }

    // ------------------------------------------------------------------ S-sizing

    fn start_s_hold(&mut self, id: usize) -> u32 {
        let (start, over) = match self.hover {
            Some((hid, x, y)) if hid == id => ((x, y), self.size_target().is_some()),
            _ => ((0.0, 0.0), false),
        };
        self.s_hold = Some(SHold { view: id, start, over, used: SUse::Nothing, shift_at_press: self.shift });
        if self.size_target().is_some() {
            self.message = Some("Sizing: arrows move the top/left edges (Shift: bottom/right), or move the mouse".into());
            return flags::ALL;
        }
        0
    }

    fn end_s_hold(&mut self) -> u32 {
        let Some(hold) = self.s_hold.take() else { return 0 };
        if matches!(self.drag, Some(Drag::SSize { .. })) {
            self.drag = None;
        }
        self.message = None;
        // Hammer-style keys: a plain Shift+S tap (no sizing) switches to the Selection tool.
        if self.cfg.hammer_keys && hold.shift_at_press && hold.used == SUse::Nothing {
            return self.command(cmd::TOOL_SELECT);
        }
        flags::ALL
    }

    /// What S-sizing acts on: the pending block box, else the selection.
    fn size_target(&self) -> Option<(Target, (V3, V3))> {
        if self.tool == Tool::Select {
            if let Some(b) = self.sel_box {
                return Some((Target::SelBox, b));
            }
        }
        if self.tool == Tool::Block {
            if let Some(b) = self.block_box {
                return Some((Target::Block, b));
            }
        }
        self.doc.selection_bounds().map(|b| (Target::Selection, b))
    }

    /// S+arrow: move one edge by a grid step (Alt: 1 unit). Without Shift the
    /// top/left edges move, with Shift the bottom/right ones; the edge always
    /// travels in the arrow's direction.
    fn s_size_key(&mut self, id: usize, horizontal: bool, dir: f32, shift: bool, fine: bool, repeat: bool) -> u32 {
        let Some((target, orig)) = self.size_target() else { return 0 };
        let (ha, va, _) = self.view(id).kind.axes();
        let axis = if horizontal { ha } else { va };
        // left edge = min of h, top edge = max of v; Shift picks the opposite ones
        let move_max = if horizontal { shift } else { !shift };
        let step = if fine { 1.0 } else { self.grid };
        let min_size = step;
        let (lo, hi) = (orig.0.get(axis), orig.1.get(axis));
        let mut nb = orig;
        if move_max {
            let new = (hi + dir * step).max(lo + min_size.min(hi - lo));
            nb.1.set(axis, tidy(new));
        } else {
            let new = (lo + dir * step).min(hi - min_size.min(hi - lo));
            nb.0.set(axis, tidy(new));
        }
        if nb == orig {
            self.message = Some("Already at the minimum size".into());
            return flags::ALL;
        }
        self.message = None;
        let orig_brushes = self.doc.selected_brushes();
        let mut changed = repeat; // a held arrow is one undo step
        self.apply_box(target, orig, nb, &orig_brushes, &mut changed);
        if target == Target::Selection {
            self.work = nb;
        }
        flags::ALL
    }

    /// Mouse moved while S is held: start sizing from the nearest corner once
    /// the cursor has moved past a small dead zone.
    fn maybe_start_s_mouse(&mut self, id: usize, x: f32, y: f32) {
        let Some(hold) = self.s_hold else { return };
        if hold.view != id || !hold.over || hold.used != SUse::Nothing || self.drag.is_some() {
            return;
        }
        if ((x - hold.start.0).powi(2) + (y - hold.start.1).powi(2)).sqrt() < 6.0 {
            return;
        }
        let Some((target, orig)) = self.size_target() else { return };
        let v = self.view(id);
        let (ha, va, _) = v.kind.axes();
        let grab = v.to_world(hold.start.0, hold.start.1);
        let (hx, hy) = size_zone(grab, orig, ha, va);
        let orig_brushes = if target == Target::Selection { self.doc.selected_brushes() } else { vec![] };
        self.drag = Some(Drag::SSize { view: id, target, orig, orig_brushes, grab, hx, hy, changed: false });
        if let Some(h) = self.s_hold.as_mut() {
            h.used = SUse::Mouse;
        }
    }

    /// Edges S-sizing will move, for highlighting: (view, box, edges) where
    /// each edge is (horizontal axis?, max side?) of that view.
    pub fn s_edges(&self) -> Option<(usize, (V3, V3), Vec<(bool, bool)>)> {
        let hold = self.s_hold?;
        let (_, b) = self.size_target()?;
        let zone = |hx: i8, hy: i8| {
            let mut e = Vec::new();
            if hx != 0 {
                e.push((true, hx > 0));
            }
            if hy != 0 {
                e.push((false, hy > 0));
            }
            e
        };
        let edges = if let Some(Drag::SSize { hx, hy, .. }) = self.drag {
            zone(hx, hy)
        } else if hold.over && hold.used == SUse::Nothing {
            let v = self.view(hold.view);
            let (ha, va, _) = v.kind.axes();
            let (hx, hy) = size_zone(v.to_world(hold.start.0, hold.start.1), b, ha, va);
            zone(hx, hy)
        } else if self.shift {
            vec![(true, true), (false, false)] // right + bottom
        } else {
            vec![(true, false), (false, true)] // left + top
        };
        Some((hold.view, b, edges))
    }

    pub fn toggle_capture(&mut self) -> u32 {
        self.captured = !self.captured;
        self.drag = None;
        if self.captured {
            flags::CAP_ON | flags::ALL
        } else {
            self.release_keys();
            flags::CAP_OFF | flags::ALL
        }
    }

    fn escape(&mut self) -> u32 {
        self.shift_hold = None;
        self.move_hold = None;
        self.hold_in_3d = false;
        self.axis_flip = false;
        self.axis_commit = None;
        if self.captured {
            return self.toggle_capture();
        }
        if self.drag.is_some() {
            // Esc mid-drag cancels it, putting everything back.
            if let Some(Drag::SSize { target: Target::Block, orig, .. }) = self.drag {
                self.block_box = Some(orig);
            }
            if let Some(Drag::SSize { target: Target::SelBox, orig, .. }) | Some(Drag::Move { target: Target::SelBox, orig, .. }) | Some(Drag::Resize { target: Target::SelBox, orig, .. }) = self.drag {
                self.sel_box = Some(orig);
            }
            if let Some(h) = self.s_hold.as_mut() {
                h.used = SUse::Arrows; // don't restart mouse sizing in this hold
            }
            let vsel_before = match &self.drag {
                Some(Drag::Vertex { sel, .. }) | Some(Drag::VRotate { sel, .. }) | Some(Drag::VScale { sel, .. }) => Some(sel.clone()),
                _ => None,
            };
            if self.drag_changed_doc() {
                self.doc.revert_to_checkpoint();
                if let Some(sel) = vsel_before {
                    self.vsel = sel;
                }
                self.prune_vsel();
            }
            self.drag = None;
            self.message = None;
            return flags::ALL;
        }
        if self.sel_box.take().is_some() {
            return flags::ALL;
        }
        if self.block_box.take().is_some() {
            return flags::ALL;
        }
        if self.bisect_line.take().is_some() {
            return flags::ALL;
        }
        if self.tool == Tool::Vertex && !self.vsel.is_empty() {
            self.vsel.clear();
            return flags::ALL;
        }
        if self.tool == Tool::Face && !self.fsel.is_empty() {
            self.fsel.clear();
            self.src_face = None;
            return flags::ALL | flags::TOOL;
        }
        if !self.doc.selection.is_empty() {
            self.doc.selection.clear();
            self.vsel.clear();
            return flags::ALL;
        }
        0
    }

    pub fn release_keys(&mut self) {
        self.keys = Keys::default();
        self.free = false;
        self.alt = false;
    }

    pub fn focus_lost(&mut self) -> u32 {
        self.release_keys();
        // Holds end as they stand (placed, not canceled).
        if self.s_hold.take().is_some() && matches!(self.drag, Some(Drag::SSize { .. })) {
            self.drag = None;
        }
        if let Some((_, _, cloning)) = self.shift_hold.take() {
            if cloning {
                self.place_shift_clone();
            }
        }
        if let Some((_, _, moving)) = self.move_hold.take() {
            if moving {
                self.place_shift_clone();
            }
        }
        if matches!(self.drag, Some(Drag::Rotate { by_key: true, .. })) {
            self.drag = None;
        }
        self.hold_in_3d = false;
        flags::ALL
    }

    /// The 2D view a hold-key action (S sizing, Shift clone, R rotate) is
    /// steering right now, so the host can pin the cursor there.
    /// The pending block as the shape it will become (None for a plain block).
    pub fn block_preview(&self) -> Option<geom::Hull> {
        let (lo, hi) = self.block_box?;
        if self.block_shape == Shape::Block || self.tool != Tool::Block {
            return None;
        }
        geom::hull(&shapes::points(self.block_shape, lo, hi, self.block_axis.0, self.block_axis.1, self.block_faces))
    }

    /// What a held key is doing right now, for the hint in the corner of the
    /// view: (view 0-3, kind, text). Kinds: 1 move (G), 2 carry a copy (Shift),
    /// 3 rotate (R), 4 size (S), 5 pan.
    pub fn hold_hint(&self) -> Option<(usize, i32, String)> {
        let lock = if self.axis_locked() { "one axis, Alt frees it" } else { "free, Alt locks an axis" };
        if let Some((v, _, _)) = self.move_hold {
            return Some((v, 1, format!("[G] held: the mouse moves the selection ({lock}). Release to drop")));
        }
        if let Some((v, _, _)) = self.shift_hold {
            return Some((v, 2, format!("[Shift] held: move the mouse to carry a copy ({lock}). Release to drop")));
        }
        if let Some(Drag::VRotate { view, .. }) = self.drag {
            return Some((view, 3, "[R] held: move the mouse left / right to turn the selected vertices. Release to finish".into()));
        }
        if let Some(Drag::VScale { view, .. }) = self.drag {
            return Some((view, 4, "[S] held: away from the center grows, toward it shrinks (Alt: smooth). Release to finish".into()));
        }
        if let Some(Drag::Rotate { view, by_key: true, .. }) = self.drag {
            let v = if self.hold_in_3d { 0 } else { view };
            return Some((v, 3, "[R] held: move the mouse left / right to rotate. Release to finish".into()));
        }
        if let Some(h) = &self.s_hold {
            return Some((h.view, 4, "[S] held: move the mouse to size from the nearest side. Release to finish".into()));
        }
        if let Some(Drag::Pan { view, .. }) = self.drag {
            return Some((view, 5, "Panning: release the middle button to stop".into()));
        }
        None
    }

    pub fn hold_view(&self) -> Option<usize> {
        if self.hold_in_3d {
            return None;
        }
        match self.drag {
            // panning a 2D view with the middle button keeps going past the edge too
            Some(Drag::Pan { view, .. }) => Some(view),
            Some(Drag::VRotate { view, .. }) | Some(Drag::VScale { view, .. }) => Some(view),
            Some(Drag::Vertex { view, .. }) if matches!(self.move_hold, Some((_, _, true))) => Some(view),
            Some(Drag::SSize { view, .. }) => Some(view),
            Some(Drag::Rotate { view, by_key: true, .. }) => Some(view),
            Some(Drag::Move { view, clone: true, .. }) if matches!(self.shift_hold, Some((_, _, true))) => Some(view),
            Some(Drag::Move { view, clone: false, .. }) if matches!(self.move_hold, Some((_, _, true))) => Some(view),
            _ => None,
        }
    }

    /// Per-frame update; flies the camera while movement keys are held.
    pub fn tick(&mut self, dt: f32) -> u32 {
        // Animated nodraw stripes: repaint ~40 times a second while any are drawn.
        self.time += dt;
        let mut anim = 0;
        if let Some((_, t0)) = self.ripple {
            if self.time - t0 > RIPPLE_SECS {
                self.ripple = None;
            }
            anim = flags::ALL;
        }
        if self.nd_style != 0 {
            self.anim_acc += dt;
            if self.anim_acc >= 0.025 {
                self.anim_acc = 0.0;
                if self.doc.brushes.iter().any(|b| b.entity.is_none() && self.drawable(b) && !self.doc.is_collision(b) && b.faces.iter().any(|f| f.material == geom::NODRAW)) {
                    anim = if self.nodraw_2d { flags::ALL } else { flags::R3D };
                }
            }
        }
        // scrolling / pulsing / animated materials play in the 3D view (~30 fps)
        if texstore::any_anim() {
            self.mat_anim_acc += dt;
            if self.mat_anim_acc >= 0.033 {
                self.mat_anim_acc = 0.0;
                if self.doc.brushes.iter().any(|b| self.drawable(b) && b.faces.iter().any(|f| texstore::anim(&f.material).is_some())) {
                    anim |= flags::R3D;
                }
            }
        }
        anim | self.fly(dt)
    }

    fn fly(&mut self, dt: f32) -> u32 {
        let k = &self.keys;
        let ax = |p: bool, n: bool| (p as i32 - n as i32) as f32;
        let fwd = ax(k.w || k.up, k.s || k.down);
        let side = ax(k.d, k.a);
        let vert = ax(k.e, k.q);
        let turn = ax(k.left, k.right);
        if fwd == 0.0 && side == 0.0 && vert == 0.0 && turn == 0.0 {
            return 0;
        }
        let speed = FLY_SPEED * if k.shift { 3.0 } else { 1.0 };
        let mv = self.cam.forward() * fwd + self.cam.right() * side + v3(0.0, 0.0, vert);
        self.cam.pos = self.cam.pos + mv.norm() * (speed * dt);
        self.cam.yaw = (self.cam.yaw + turn * TURN_SPEED * dt).rem_euclid(360.0);
        flags::ALL
    }

    // ------------------------------------------------------------------ commands

    pub fn command(&mut self, c: i32) -> u32 {
        if self.live_box_up() && matches!(c, cmd::DELETE | cmd::MIRROR_H | cmd::MIRROR_V | cmd::HIDE_TOGGLE | cmd::GROUP | cmd::UNGROUP | cmd::QUANTIZE) {
            self.sel_box = None;
        }
        match c {
            cmd::UNDO => {
                self.drag = None;
                self.doc.undo();
                self.prune_vsel();
                self.prune_fsel();
            }
            cmd::REDO => {
                self.drag = None;
                self.doc.redo();
                self.prune_vsel();
                self.prune_fsel();
            }
            cmd::DELETE => {
                self.doc.delete_selected();
                self.vsel.clear();
                self.prune_fsel();
            }
            cmd::SELECT_ALL => {
                if self.tool == Tool::Vertex && !self.doc.selection.is_empty() {
                    self.vsel = self
                        .doc
                        .selected_brushes()
                        .iter()
                        .filter(|b| b.entity.is_none())
                        .flat_map(|b| b.verts.iter().map(move |&p| (b.id, p)))
                        .collect();
                } else if self.tool == Tool::Face {
                    self.fsel = self
                        .doc
                        .brushes
                        .iter()
                        .filter(|b| self.pickable(b) && b.entity.is_none())
                        .flat_map(|b| (0..b.faces.len()).map(move |i| (b.id, i)))
                        .collect();
                } else {
                    self.doc.selection = self.doc.brushes.iter().filter(|b| self.pickable(b)).map(|b| b.id).collect();
                }
            }
            cmd::TOGGLE_MANNEQUINS => {
                self.show_entities = !self.show_entities;
                if !self.show_entities {
                    let doc = &self.doc;
                    let ents: Vec<u32> = doc.brushes.iter().filter(|b| b.entity.is_some()).map(|b| b.id).collect();
                    self.doc.selection.retain(|s| !ents.contains(s));
                }
                return flags::ALL | flags::TOOL;
            }
            cmd::ORIGIN_TO_SEL_BOTTOM | cmd::ORIGIN_TO_SEL_CENTER => {
                let Some((lo, hi)) = self.doc.selection_bounds() else {
                    self.message = Some("Select something to move the origin to".into());
                    return flags::ALL;
                };
                let mid = (lo + hi) * 0.5;
                let o = if c == cmd::ORIGIN_TO_SEL_BOTTOM { v3(mid.x, mid.y, lo.z) } else { mid };
                self.set_origin(o, &mut false);
            }
            cmd::QUANTIZE => self.quantize(),
            cmd::GROUP => {
                match self.doc.group_selection() {
                    Some(g) => {
                        let name = self.doc.group(g).map(|x| x.name.clone()).unwrap_or_default();
                        self.message = Some(format!("Grouped as \"{}\" (rename it in the Layers pane)", name));
                    }
                    None => self.message = Some("Select brushes to group".into()),
                }
                return flags::ALL | flags::TOOL;
            }
            cmd::UNGROUP => {
                let n = self.doc.ungroup_selection();
                self.message = Some(if n == 0 { "Nothing grouped is selected".into() } else { format!("Ungrouped {} group{}", n, if n == 1 { "" } else { "s" }) });
                return flags::ALL | flags::TOOL;
            }
            cmd::IGNORE_GROUPS => {
                self.ignore_groups = !self.ignore_groups;
                self.message = Some(if self.ignore_groups { "Ignoring groups: clicks pick single brushes".into() } else { "Groups on: clicks pick whole groups".into() });
                return flags::ALL | flags::TOOL;
            }
            cmd::DESELECT => {
                // one undoable step, so a slip is a Ctrl+Z away
                let had = !self.doc.selection.is_empty() || !self.fsel.is_empty();
                if had {
                    self.doc.checkpoint();
                }
                self.doc.selection.clear();
                self.fsel.clear();
                self.src_face = None;
                self.vsel.clear();
                self.sel_box = None;
                self.block_box = None;
                self.drag = None;
                self.message = Some(if had { "Deselected (Ctrl+Z brings it back)".into() } else { "Nothing selected".into() });
                return flags::ALL | flags::TOOL;
            }
            cmd::TOGGLE_EDGES_3D => {
                self.show_edges_3d = !self.show_edges_3d;
                return flags::ALL | flags::TOOL;
            }
            cmd::HIDE_TOGGLE => {
                if !self.doc.selection.is_empty() {
                    let n = self.doc.selection.len();
                    let sel = std::mem::take(&mut self.doc.selection);
                    self.hidden.extend(sel);
                    self.vsel.clear();
                    self.fsel.clear();
                    self.message = Some(format!("Hid {} brush{} (H with nothing selected shows everything again)", n, if n == 1 { "" } else { "es" }));
                } else if !self.hidden.is_empty() {
                    let n = self.hidden.len();
                    self.hidden.clear();
                    self.message = Some(format!("Showing {} hidden brush{}", n, if n == 1 { "" } else { "es" }));
                } else {
                    self.message = Some("H hides the selection (H again with nothing selected shows it)".into());
                }
                return flags::ALL;
            }
            cmd::MIRROR_H | cmd::MIRROR_V => {
                let horizontal = c == cmd::MIRROR_H;
                let axis = match self.hover {
                    Some((id, _, _)) if (1..=3).contains(&id) => {
                        let (ha, va, _) = self.view(id).kind.axes();
                        if horizontal { ha } else { va }
                    }
                    // 3D view (or none): horizontal is the floor axis across the screen, vertical is up
                    _ => {
                        if horizontal {
                            let r = self.cam.right();
                            if r.x.abs() >= r.y.abs() { 0 } else { 1 }
                        } else {
                            2
                        }
                    }
                };
                return self.mirror_selection(axis);
            }
            cmd::JOIN | cmd::JOIN_HULL | cmd::JOIN_EXACT => {
                let exact_only = c == cmd::JOIN_EXACT || (c == cmd::JOIN && self.join_exact_only);
                return self.join_selection(exact_only);
            }
            cmd::TOGGLE_SEL_BOX_MODE => {
                if self.tool != Tool::Select || !self.sel_box_only {
                    if self.tool != Tool::Select {
                        self.command(cmd::TOOL_SELECT);
                    }
                    self.sel_box_only = true;
                } else {
                    self.sel_box_wait = !self.sel_box_wait;
                    self.sel_box = None;
                }
                self.message = Some(if self.sel_box_wait {
                    "Box select (Enter): drag a box, adjust it, Enter selects (Ctrl+Enter adds). Click the box half again: instant"
                } else {
                    "Box select (instant): what the box touches is selected as you drag. Click the box half again: Enter mode"
                }.into());
                return flags::ALL | flags::TOOL;
            }
            cmd::TOGGLE_VERTEX_PICK => {
                if self.tool == Tool::Vertex {
                    self.vertex_pick = !self.vertex_pick;
                } else {
                    self.command(cmd::TOOL_VERTEX);
                    self.vertex_pick = true;
                }
                self.message = Some(if self.vertex_pick {
                    "Vertex select: click or box vertices on any brush (Ctrl adds), then G moves, R turns, S scales"
                } else {
                    "Vertex tool: drag handles to move vertices"
                }.into());
                return flags::ALL | flags::TOOL;
            }
            cmd::TOGGLE_AXIS_LOCK => {
                self.axis_lock = !self.axis_lock;
                return flags::ALL | flags::TOOL;
            }
            cmd::TOGGLE_BOX_TOUCH => {
                self.sel_touch = !self.sel_touch;
                return flags::ALL | flags::TOOL;
            }
            cmd::TOGGLE_COLLISION_SOLID => {
                self.collision_solid = !self.collision_solid;
                return flags::ALL | flags::TOOL;
            }
            cmd::TOGGLE_NODRAW_2D => {
                self.nodraw_2d = !self.nodraw_2d;
                return flags::ALL | flags::TOOL;
            }
            cmd::GRID_SMALLER => self.grid = (self.grid * 0.5).max(1.0),
            cmd::GRID_LARGER => self.grid = (self.grid * 2.0).min(512.0),
            cmd::TOOL_BISECT if self.tool == Tool::Bisect => {
                // Already active: Split → Split & Delete (smaller) → Split & Delete (larger) → Split
                let (d, l) = match (self.bisect_delete, self.bisect_larger) {
                    (false, _) => (true, false),
                    (true, false) => (true, true),
                    (true, true) => (false, false),
                };
                self.bisect_delete = d;
                self.bisect_larger = l;
                self.bisect_flip = 0;
                return flags::ALL | flags::TOOL;
            }
            cmd::BISECT_MODE_SPLIT | cmd::BISECT_MODE_DELETE | cmd::BISECT_MODE_DELETE_LARGER => {
                self.bisect_delete = c != cmd::BISECT_MODE_SPLIT;
                self.bisect_larger = c == cmd::BISECT_MODE_DELETE_LARGER;
                self.bisect_flip = 0;
                return flags::ALL | flags::TOOL;
            }
            cmd::BISECT_APPLY => return self.apply_bisect(),
            cmd::TOOL_SELECT | cmd::TOOL_BLOCK | cmd::TOOL_VERTEX | cmd::TOOL_BISECT | cmd::TOOL_MANNEQUIN | cmd::TOOL_CAR | cmd::TOOL_ORIGIN | cmd::TOOL_FACE => {
                self.tool = match c {
                    cmd::TOOL_SELECT => {
                        self.sel_box_only = false; // the left half: point and click
                        self.sel_box = None;
                        Tool::Select
                    }
                    cmd::TOOL_BLOCK => Tool::Block,
                    cmd::TOOL_VERTEX => Tool::Vertex,
                    cmd::TOOL_BISECT => Tool::Bisect,
                    cmd::TOOL_MANNEQUIN => {
                        self.place = Entity::Mannequin;
                        Tool::Entity
                    }
                    cmd::TOOL_CAR => {
                        self.place = Entity::Car;
                        Tool::Entity
                    }
                    cmd::TOOL_ORIGIN => Tool::Origin,
                    _ => Tool::Face,
                };
                self.drag = None;
                self.vsel.clear();
                self.bisect_line = None;
                if self.tool != Tool::Face {
                    self.fsel.clear();
                }
                if self.tool != Tool::Block {
                    self.block_box = None;
                }
                self.sel_box = None;
                return flags::ALL | flags::TOOL;
            }
            cmd::NEW => {
                self.hidden.clear();
                self.drag = None;
                self.block_box = None;
                self.vsel.clear();
                self.fsel.clear();
                self.doc.replace_with(Vec::new());
                self.message = None;
            }
            cmd::CENTER_2D => {
                let (lo, hi) = self.doc.selection_bounds().unwrap_or((V3::default(), V3::default()));
                let c = (lo + hi) * 0.5;
                for v in self.views.iter_mut() {
                    let (ha, va, _) = v.kind.axes();
                    v.cx = c.get(ha);
                    v.cy = c.get(va);
                }
            }
            cmd::RELEASE_CAPTURE => {
                if self.captured {
                    self.captured = false;
                    self.release_keys();
                    return flags::ALL;
                }
                return 0;
            }
            cmd::CREATE_BLOCK => {
                if self.tool != Tool::Block {
                    return 0;
                }
                let Some((lo, hi)) = self.block_box else { return 0 };
                let size = hi - lo;
                if size.x <= 0.0 || size.y <= 0.0 || size.z <= 0.0 {
                    return 0;
                }
                let active = self.doc.active_layer;
                if self.doc.layer(active).is_some_and(|l| l.locked) {
                    self.message = Some("The active layer is locked: unlock it or pick another layer".into());
                    return flags::ALL;
                }
                self.doc.checkpoint();
                if self.doc.layer(active).is_some_and(|l| !l.visible) {
                    self.doc.set_layer_flags(active, true, false);
                }
                let id = if self.block_shape == Shape::Block {
                    self.doc.add_brush(lo, hi)
                } else {
                    let pts = shapes::points(self.block_shape, lo, hi, self.block_axis.0, self.block_axis.1, self.block_faces);
                    match self.doc.add_hull_brush(&pts) {
                        Some(id) => id,
                        None => {
                            self.message = Some("That shape is too thin to make".into());
                            return flags::ALL;
                        }
                    }
                };
                self.doc.selection = vec![id];
                self.sel_mode = HandleMode::Scale;
                self.work = (lo, hi);
                self.block_box = None;
            }
            _ => return 0,
        }
        flags::ALL
    }

    // ------------------------------------------------------------------ status

    pub fn status(&self) -> String {
        let mut parts = Vec::new();
        if self.captured {
            parts.push("MOUSELOOK: WASD fly, Q/E down/up, Shift fast, Z or Esc to release".to_string());
        }
        if let Some(m) = &self.message {
            parts.push(m.clone());
        }
        parts.push(match self.tool {
            Tool::Block => "Block tool".to_string(),
            Tool::Select => format!("Selection tool ({})", if self.sel_mode == HandleMode::Scale { "scale" } else { "rotate" }),
            Tool::Vertex => format!("Vertex tool ({} vertices selected)", self.vsel.len()),
            Tool::Bisect => {
                let mode = match (self.bisect_delete, self.bisect_larger) {
                    (false, _) => "Split",
                    (true, false) => "Split & Delete smaller side",
                    (true, true) => "Split & Delete larger side",
                };
                if self.bisect_line.is_some() {
                    if self.bisect_delete {
                        format!("Bisect tool ({mode}{}): Enter cuts, F flips the removed side, Esc cancels",
                            if self.bisect_flip % 2 == 1 { ", flipped" } else { "" })
                    } else {
                        let what = ["both pieces", "the smaller piece", "the larger piece"][(self.bisect_flip % 3) as usize];
                        format!("Bisect tool ({mode}): Enter cuts and selects {what} (F changes), Esc cancels")
                    }
                } else {
                    format!("Bisect tool ({mode}): draw a cut line across selected brushes")
                }
            }
            Tool::Entity => format!("{}: click a surface in 3D or a spot in a 2D view to place it", if self.place == Entity::Car { "Car" } else { "Mannequin" }),
            Tool::Origin => "Origin tool: click or drag to place the model origin".to_string(),
            Tool::Face => format!("Face Select & Paint ({} face{} selected): click a face to select it, right-click to paint, Alt+right-click to wrap",
                self.fsel.len(), if self.fsel.len() == 1 { "" } else { "s" }),
        });
        parts.push(if self.free { "Grid OFF (Alt)".to_string() } else { format!("Grid {}", self.grid) });
        if let Some((id, x, y)) = self.hover {
            if id >= 1 && !self.captured {
                let v = self.view(id);
                let (h, vv) = v.to_world(x, y);
                parts.push(format!("{}: {}, {}", v.kind.name(), fmt_num(self.snap(h)), fmt_num(self.snap(vv))));
            }
        }
        let n = self.doc.selection.len();
        if let Some((lo, hi)) = self.doc.selection_bounds() {
            let s = hi - lo;
            parts.push(format!("{} selected ({} x {} x {})", n, fmt_num(s.x), fmt_num(s.y), fmt_num(s.z)));
        }
        if let Some((lo, hi)) = self.block_box {
            let s = hi - lo;
            parts.push(format!("New block {} x {} x {} (Enter to create)", fmt_num(s.x), fmt_num(s.y), fmt_num(s.z)));
        }
        let ents = self.doc.brushes.iter().filter(|b| b.entity.is_some()).count();
        let solids = self.doc.brushes.len() - ents;
        parts.push(if ents > 0 {
            format!("{} brush{}, {} measurement buddy{}", solids, if solids == 1 { "" } else { "es" }, ents, if ents == 1 { "" } else { "s" }).replace("buddys", "buddies")
        } else {
            format!("{} brush{}", solids, if solids == 1 { "" } else { "es" })
        });
        parts.join("   |   ")
    }
}

/// Rotate points in the (ha, va) plane around `center` by `ang` radians (CCW).
pub fn rotate_points(pts: &[V3], ha: usize, va: usize, center: (f32, f32), ang: f32) -> Vec<V3> {
    let (sn, cs) = ang.sin_cos();
    pts.iter()
        .map(|&src| {
            let mut dst = src;
            let (dh, dv) = (src.get(ha) - center.0, src.get(va) - center.1);
            dst.set(ha, tidy(center.0 + dh * cs - dv * sn));
            dst.set(va, tidy(center.1 + dh * sn + dv * cs));
            dst
        })
        .collect()
}

/// Which corner or edge of `b` a point grabs, as (h side, v side) with each
/// -1 / 0 / +1. Inside: outer thirds pick that side, dead center the nearest
/// edge. Outside: beside the box (within its span) grabs that one edge,
/// diagonally past a corner grabs both edges of that corner.
pub fn size_zone(p: (f32, f32), b: (V3, V3), ha: usize, va: usize) -> (i8, i8) {
    let t = (p.0 - b.0.get(ha)) / (b.1.get(ha) - b.0.get(ha)).max(1e-6);
    let u = (p.1 - b.0.get(va)) / (b.1.get(va) - b.0.get(va)).max(1e-6);
    let out = |t: f32| -> i8 {
        if t < 0.0 {
            -1
        } else if t > 1.0 {
            1
        } else {
            0
        }
    };
    let (oh, ov) = (out(t), out(u));
    if oh != 0 || ov != 0 {
        return (oh, ov);
    }
    let third = |t: f32| -> i8 {
        if t < 1.0 / 3.0 {
            -1
        } else if t > 2.0 / 3.0 {
            1
        } else {
            0
        }
    };
    let (hx, hy) = (third(t), third(u));
    if hx != 0 || hy != 0 {
        return (hx, hy);
    }
    let d = [(t, (-1, 0)), (1.0 - t, (1, 0)), (u, (0, -1)), (1.0 - u, (0, 1))];
    d.iter().min_by(|a, b| a.0.partial_cmp(&b.0).unwrap()).unwrap().1
}

/// Short name for a material path.
fn nzf(x: f32) -> f32 {
    if x.abs() < 1e-6 {
        1e-6
    } else {
        x
    }
}

pub fn material_display(m: &str) -> &str {
    if m == geom::NODRAW {
        "nodraw"
    } else {
        m.rsplit('/').next().unwrap_or(m)
    }
}

/// Wrap degrees into (-180, 180].
fn wrap_deg(d: f32) -> f32 {
    let mut r = d.rem_euclid(360.0);
    if r > 180.0 {
        r -= 360.0;
    }
    if (r - r.round()).abs() < 1e-3 {
        r = r.round();
    }
    r
}

/// Clean up float noise from rotations/scales (e.g. 63.99998 -> 64).
fn tidy(v: f32) -> f32 {
    let r = (v * 100.0).round() / 100.0;
    if (v - r).abs() < 2e-3 {
        r
    } else {
        v
    }
}

pub fn handle_list(corners_only: bool) -> Vec<(i8, i8)> {
    let mut v = vec![(-1, -1), (1, -1), (-1, 1), (1, 1)];
    if !corners_only {
        v.extend([(0, -1), (0, 1), (-1, 0), (1, 0)]);
    }
    v
}

fn handle_cursor(hx: i8, hy: i8) -> u32 {
    match (hx, hy) {
        (0, _) => cursor::SIZE_VER,
        (_, 0) => cursor::SIZE_HOR,
        // hy=+1 is top of screen: top-right / bottom-left => "/" diagonal
        (1, 1) | (-1, -1) => cursor::SIZE_BDIAG,
        _ => cursor::SIZE_FDIAG,
    }
}

pub fn fmt_num(v: f32) -> String {
    if (v - v.round()).abs() < 1e-3 {
        format!("{}", v.round() as i64)
    } else {
        let s = format!("{:.2}", v);
        s.trim_end_matches('0').trim_end_matches('.').to_string()
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn editor() -> Editor {
        let mut e = Editor::new();
        for i in 1..=3 {
            e.set_view_size(i, 800.0, 600.0);
        }
        e.set_view_size(0, 800.0, 600.0);
        e
    }

    /// Screen position of a world point in view `id`.
    fn sp(e: &Editor, id: usize, h: f32, v: f32) -> (f32, f32) {
        e.view(id).to_screen(h, v)
    }

    fn drag(e: &mut Editor, id: usize, from: (f32, f32), to: (f32, f32), m: i32) {
        e.mouse_press(id, from.0, from.1, button::LEFT, m);
        e.mouse_move(id, to.0, to.1, m);
        e.mouse_release(id, to.0, to.1, button::LEFT);
    }

    fn click(e: &mut Editor, id: usize, at: (f32, f32), m: i32) {
        e.mouse_press(id, at.0, at.1, button::LEFT, m);
        e.mouse_release(id, at.0, at.1, button::LEFT);
    }

    // Evaluate positions (which borrow the editor) before the mutable call.
    macro_rules! drag {
        ($e:ident, $id:expr, $a:expr, $b:expr, $m:expr) => {{
            let (a, b) = ($a, $b);
            drag(&mut $e, $id, a, b, $m);
        }};
    }
    macro_rules! click {
        ($e:ident, $id:expr, $a:expr, $m:expr) => {{
            let a = $a;
            click(&mut $e, $id, a, $m);
        }};
    }

    #[test]
    fn draw_and_create_block_in_top_view() {
        let mut e = editor();
        let a = sp(&e, 1, 0.0, 0.0);
        let b = sp(&e, 1, 128.0, 64.0);
        drag!(e, 1, a, (b.0 + 1.0, b.1 - 1.0), 0);
        let (lo, hi) = e.block_box.expect("box drawn");
        assert_eq!((lo, hi), (v3(0.0, 0.0, 0.0), v3(128.0, 64.0, 64.0)));
        e.key(1, key::RETURN, true, 0, false);
        assert_eq!(e.doc.brushes.len(), 1);
        assert_eq!(e.doc.selection, vec![e.doc.brushes[0].id]);
        assert!(e.block_box.is_none());
        e.command(cmd::UNDO);
        assert!(e.doc.brushes.is_empty());
    }

    #[test]
    fn select_move_and_resize_in_front_view() {
        let mut e = editor();
        e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0));
        e.command(cmd::TOOL_SELECT);
        // Front view: h = Y, v = Z. Click center and drag +32 in Y.
        drag!(e, 2, sp(&e, 2, 32.0, 32.0), sp(&e, 2, 64.0, 32.0), 0);
        assert_eq!(e.doc.brushes[0].bounds(), (v3(0.0, 32.0, 0.0), v3(64.0, 96.0, 64.0)));
        // Drag the top edge handle (max Z) up to 128.
        let t = sp(&e, 2, 64.0, 64.0);
        assert_eq!(e.hit_box(2, e.doc.selection_bounds().unwrap(), t.0, t.1, HandleMode::Scale), Hit::Handle(0, 1));
        drag!(e, 2, t, sp(&e, 2, 64.0, 128.0), 0);
        assert_eq!(e.doc.brushes[0].bounds().1.z, 128.0);
        e.command(cmd::UNDO);
        e.command(cmd::UNDO);
        assert_eq!(e.doc.brushes[0].bounds().0, v3(0.0, 0.0, 0.0));
    }

    #[test]
    fn click_in_block_tool_selects_and_handles_resize_snapped() {
        let mut e = editor();
        e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0));
        assert_eq!(e.tool, Tool::Block);
        click!(e, 1, sp(&e, 1, 32.0, 32.0), 0);
        assert_eq!(e.doc.selection.len(), 1, "plain click selects in block tool");
        assert!(e.block_box.is_none(), "a click must not draw a box");
        // Right-edge handle at x=64; drag to x=101 -> snaps to 96 (grid 16)
        let h = sp(&e, 1, 64.0, 32.0);
        drag!(e, 1, h, sp(&e, 1, 101.0, 32.0), 0);
        assert_eq!(e.doc.brushes[0].bounds().1.x, 96.0);
        // With Alt held: no snapping
        let h = sp(&e, 1, 96.0, 32.0);
        drag!(e, 1, h, sp(&e, 1, 101.0, 32.0), mods::ALT);
        assert!((e.doc.brushes[0].bounds().1.x - 101.0).abs() < 0.05);
    }

    #[test]
    fn clicking_selected_brush_toggles_rotate_and_rotation_snaps() {
        let mut e = editor();
        e.doc.add_brush(v3(-32.0, -32.0, 0.0), v3(32.0, 32.0, 64.0));
        e.command(cmd::TOOL_SELECT);
        let c = sp(&e, 1, 0.0, 0.0);
        click!(e, 1, c, 0);
        assert_eq!(e.sel_mode, HandleMode::Scale);
        click!(e, 1, c, 0);
        assert_eq!(e.sel_mode, HandleMode::Rotate);
        // Drag the top-right corner a little more than 45 degrees around the center.
        let start = sp(&e, 1, 32.0, 32.0);
        let ang = (45.0f32 + 47.0).to_radians(); // a 47 degree drag snaps to 45
        let r = 45.0;
        let end = sp(&e, 1, r * ang.cos(), r * ang.sin());
        drag!(e, 1, start, end, 0);
        // snapped to 45 degrees -> square becomes a diamond with corners on the axes
        let b = &e.doc.brushes[0];
        assert!(b.verts.iter().any(|p| (p.x - 0.0).abs() < 0.01 && (p.y - 45.25).abs() < 0.01), "{:?}", b.verts);
        assert_eq!(b.verts.len(), 8);
        click!(e, 1, c, 0);
        assert_eq!(e.sel_mode, HandleMode::Scale);
    }

    #[test]
    fn vertex_tool_makes_a_ramp_and_refuses_concave() {
        let mut e = editor();
        let id = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0));
        e.doc.selection = vec![id];
        e.command(cmd::TOOL_VERTEX);
        // Front view (h=Y, v=Z): the top-right corner (y=64,z=64) stacks two vertices (x=0 and x=64).
        let tr = sp(&e, 2, 64.0, 64.0);
        assert_eq!(e.hit_vertices(2, tr.0, tr.1).len(), 2);
        // Drag it down to z=0: the top edge collapses onto the bottom -> ramp (wedge).
        drag!(e, 2, tr, sp(&e, 2, 64.0, 0.0), 0);
        let b = e.doc.brush(id).unwrap();
        assert_eq!(b.verts.len(), 6, "{:?}", b.verts);
        assert_eq!(b.faces.len(), 5);
        e.command(cmd::UNDO);
        assert_eq!(e.doc.brush(id).unwrap().verts.len(), 8);
        // Push a corner into the middle of the solid (top view) -> a dent -> refused.
        let before = e.doc.brush(id).unwrap().clone();
        let c = sp(&e, 1, 64.0, 64.0);
        drag!(e, 1, c, sp(&e, 1, 16.0, 16.0), 0);
        assert_eq!(e.doc.brush(id).unwrap(), &before);
        assert!(e.message.is_some());
    }

    #[test]
    fn vertex_marquee_and_edge_drag() {
        let mut e = editor();
        let id = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0));
        e.doc.selection = vec![id];
        e.command(cmd::TOOL_VERTEX);
        // Marquee around the top of the brush in the side view (h=X, v=Z): 4 vertices
        let a = sp(&e, 3, -24.0, 80.0);
        let b = sp(&e, 3, 72.0, 56.0);
        drag!(e, 3, a, b, 0);
        assert_eq!(e.vsel.len(), 4);
        // Raise the whole top face by 32 by dragging one of the selected vertices
        let v = sp(&e, 3, 0.0, 64.0);
        drag!(e, 3, v, sp(&e, 3, 0.0, 96.0), 0);
        assert_eq!(e.doc.brush(id).unwrap().bounds().1.z, 96.0);
        // Edge midpoint drag in the top view moves both endpoints
        e.vsel.clear();
        let mid = sp(&e, 1, 64.0, 32.0); // right edge (x=64), seen from the top
        let h = e.hit_edges(1, mid.0, mid.1);
        assert_eq!(h.len(), 4, "right face's two vertical-stacked edges");
        drag!(e, 1, mid, sp(&e, 1, 96.0, 32.0), 0);
        assert_eq!(e.doc.brush(id).unwrap().bounds().1.x, 96.0);
    }

    #[test]
    fn alt_drag_moves_any_brush_freely_in_any_tool() {
        let mut e = editor();
        let a = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0));
        let b = e.doc.add_brush(v3(128.0, 0.0, 0.0), v3(192.0, 64.0, 64.0));
        e.doc.selection = vec![a];
        for tool in [cmd::TOOL_BLOCK, cmd::TOOL_VERTEX, cmd::TOOL_SELECT] {
            e.command(tool);
            let before = e.doc.brush(b).unwrap().bounds();
            // grab brush b on its body, off-center (not a handle), and drag +37 x, -5 y
            let (gx, gy) = (before.0.x + 22.0, before.0.y + 20.0);
            drag!(e, 1, sp(&e, 1, gx, gy), sp(&e, 1, gx + 37.0, gy - 5.0), mods::ALT);
            let after = e.doc.brush(b).unwrap().bounds();
            assert!((after.0.x - before.0.x - 37.0).abs() < 0.05, "tool {tool}: {:?} -> {:?}", before, after);
            assert!((after.0.y - before.0.y + 5.0).abs() < 0.05);
            assert_eq!(e.doc.selection, vec![b], "grabbing an unselected brush selects it");
            assert!(e.block_box.is_none(), "Alt+drag on a brush must not draw a block");
        }
        // Alt+Shift keeps grid snapping (min corner lands on the 16 grid)
        let lo = e.doc.brush(b).unwrap().bounds().0;
        drag!(e, 1, sp(&e, 1, lo.x + 20.0, lo.y + 20.0), sp(&e, 1, lo.x + 43.0, lo.y + 20.0), mods::ALT | mods::SHIFT);
        let lo = e.doc.brush(b).unwrap().bounds().0;
        assert_eq!(lo.x % 16.0, 0.0, "{lo:?}");
        // brush a was never touched
        assert_eq!(e.doc.brush(a).unwrap().bounds(), (v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0)));
    }

    #[test]
    fn g_in_3d_slides_along_floor_without_pinning() {
        let mut e = editor();
        let id = e.doc.add_brush(v3(-32.0, -32.0, 0.0), v3(32.0, 32.0, 64.0));
        e.doc.selection = vec![id];
        e.cam = Camera { pos: v3(0.0, -10.0, 600.0), yaw: 90.0, pitch: -89.0 };
        let c = (400.0, 300.0);
        e.mouse_move(0, c.0, c.1, 0);
        e.key(0, key::G, true, 0, false);
        assert_eq!(e.hold_view(), None, "nothing to pin in 3D");
        e.mouse_move(0, c.0 + 60.0, c.1 + 5.0, 0);
        e.key(0, key::G, false, 0, false);
        let (lo, hi) = e.doc.brush(id).unwrap().bounds();
        assert!(lo.x > -31.0, "moved along +x: {lo:?}");
        assert_eq!(lo.y, -32.0, "axis lock kept y");
        assert_eq!((lo.z, hi.z), (0.0, 64.0));
        assert!(e.drag.is_none());
    }

    #[test]
    fn alt_drag_in_3d_slides_along_floor() {
        let mut e = editor();
        let id = e.doc.add_brush(v3(-32.0, -32.0, 0.0), v3(32.0, 32.0, 64.0));
        // camera above, looking straight-ish down
        e.cam = Camera { pos: v3(0.0, -10.0, 600.0), yaw: 90.0, pitch: -89.0 };
        let c = (400.0, 300.0);
        e.mouse_press(0, c.0, c.1, button::LEFT, mods::ALT);
        e.mouse_move(0, c.0 + 60.0, c.1, mods::ALT);
        e.mouse_release(0, c.0 + 60.0, c.1, button::LEFT);
        let (lo, hi) = e.doc.brush(id).unwrap().bounds();
        assert!(lo.x > -31.0, "moved along +x on screen: {lo:?}");
        assert_eq!((lo.z, hi.z), (0.0, 64.0), "height unchanged");
        e.command(cmd::UNDO);
        assert_eq!(e.doc.brush(id).unwrap().bounds().0, v3(-32.0, -32.0, 0.0));
    }

    #[test]
    fn arrow_nudge_grid_and_fine_with_single_undo_per_hold() {
        let mut e = editor();
        let id = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0));
        e.doc.selection = vec![id];
        // Front view: h = Y, v = Z
        e.key(2, key::RIGHT, true, 0, false);
        e.key(2, key::RIGHT, true, 0, true); // held: auto-repeat
        e.key(2, key::RIGHT, true, 0, true);
        assert_eq!(e.doc.brush(id).unwrap().bounds().0, v3(0.0, 48.0, 0.0));
        e.key(2, key::UP, true, mods::ALT, false);
        assert_eq!(e.doc.brush(id).unwrap().bounds().0, v3(0.0, 48.0, 1.0));
        e.command(cmd::UNDO); // undo the Alt nudge
        e.command(cmd::UNDO); // the whole held run is one step
        assert_eq!(e.doc.brush(id).unwrap().bounds().0, v3(0.0, 0.0, 0.0));
        // arrows in the 3D view (not flying) nudge along the floor axis the camera faces
        let f = e.cam.forward();
        let ax = if f.x.abs() >= f.y.abs() { v3(f.x.signum(), 0.0, 0.0) } else { v3(0.0, f.y.signum(), 0.0) };
        e.key(0, key::UP, true, 0, false);
        assert_eq!(e.doc.brush(id).unwrap().bounds().0, ax * 16.0);
        e.key(0, key::UP, false, 0, false);
        e.command(cmd::UNDO);
        // flying (Z): arrows look around again
        e.captured = true;
        e.key(0, key::UP, true, 0, false);
        assert_eq!(e.doc.brush(id).unwrap().bounds().0, v3(0.0, 0.0, 0.0));
        assert!(e.tick(0.1) != 0);
    }

    #[test]
    fn arrow_nudge_vertices_and_block_box() {
        let mut e = editor();
        let id = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0));
        e.doc.selection = vec![id];
        e.command(cmd::TOOL_VERTEX);
        // select the top 4 vertices (side view marquee) and nudge them up
        drag!(e, 3, sp(&e, 3, -24.0, 80.0), sp(&e, 3, 72.0, 56.0), 0);
        e.key(3, key::UP, true, 0, false);
        assert_eq!(e.doc.brush(id).unwrap().bounds().1.z, 80.0);
        // block box nudge
        e.command(cmd::TOOL_BLOCK);
        drag!(e, 1, sp(&e, 1, 256.0, 256.0), sp(&e, 1, 320.0, 320.0), 0);
        e.key(1, key::LEFT, true, 0, false);
        assert_eq!(e.block_box.unwrap().0.x, 240.0);
    }

    #[test]
    fn quantize_snaps_sloppy_brushes() {
        let mut e = editor();
        let id = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0));
        e.doc.selection = vec![id];
        // make it sloppy: free-drag the right edge to 70.3
        drag!(e, 1, sp(&e, 1, 64.0, 32.0), sp(&e, 1, 70.3, 32.0), mods::ALT);
        assert!((e.doc.brush(id).unwrap().bounds().1.x - 70.3).abs() < 0.05);
        e.command(cmd::QUANTIZE);
        let b = e.doc.brush(id).unwrap();
        assert!(b.verts.iter().all(|p| p.x % 16.0 == 0.0 && p.y % 16.0 == 0.0 && p.z % 16.0 == 0.0), "{:?}", b.verts);
        assert_eq!(b.bounds().1.x, 64.0);
        assert!(e.message.as_deref().unwrap().starts_with("Quantized 1 brush"));
        // a sliver thinner than the grid would flatten -> skipped, untouched
        let thin = e.doc.add_brush(v3(0.0, 0.0, 100.0), v3(64.0, 64.0, 103.0)); // both rounds to z=96
        e.doc.selection = vec![thin];
        e.command(cmd::QUANTIZE);
        assert_eq!(e.doc.brush(thin).unwrap().bounds().1.z, 103.0);
        assert!(e.message.as_deref().unwrap().contains("skipped"));
        // a rotated brush comes back with every vertex on the grid
        let r = e.doc.add_brush(v3(-232.0, -32.0, 0.0), v3(-168.0, 32.0, 64.0)); // centered on (-200, 0)
        e.doc.selection = vec![r];
        e.command(cmd::TOOL_SELECT);
        click!(e, 1, sp(&e, 1, -200.0, 0.0), 0); // selected -> rotate mode
        assert_eq!(e.sel_mode, HandleMode::Rotate);
        let ang = (45.0f32 + 10.0).to_radians();
        drag!(e, 1, sp(&e, 1, -168.0, 32.0), sp(&e, 1, -200.0 + 45.25 * ang.cos(), 45.25 * ang.sin()), mods::ALT);
        assert!(e.doc.brush(r).unwrap().verts.iter().any(|p| p.x % 16.0 != 0.0));
        e.command(cmd::QUANTIZE);
        assert!(e.doc.brush(r).unwrap().verts.iter().all(|p| p.x % 16.0 == 0.0 && p.y % 16.0 == 0.0));
    }

    #[test]
    fn shift_drag_clones_and_click_without_drag_leaves_no_copy() {
        let mut e = editor();
        let a = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0));
        for tool in [cmd::TOOL_SELECT, cmd::TOOL_BLOCK] {
            e.command(tool);
            let n = e.doc.brushes.len();
            // Shift+click without moving: nothing added
            e.mouse_press(1, sp(&e, 1, 20.0, 20.0).0, sp(&e, 1, 20.0, 20.0).1, button::LEFT, mods::SHIFT);
            e.mouse_release(1, 0.0, 0.0, button::LEFT);
            assert_eq!(e.doc.brushes.len(), n, "no stray copy");
        }
        // Shift+drag: a copy follows the mouse (grid snapped), original stays
        drag!(e, 1, sp(&e, 1, 20.0, 20.0), sp(&e, 1, 101.0, 20.0), mods::SHIFT);
        assert_eq!(e.doc.brushes.len(), 2);
        assert_eq!(e.doc.brush(a).unwrap().bounds().0, v3(0.0, 0.0, 0.0));
        let copy = *e.doc.selection.first().unwrap();
        assert_ne!(copy, a);
        assert_eq!(e.doc.brush(copy).unwrap().bounds().0, v3(80.0, 0.0, 0.0));
        // one undo removes the copy entirely
        e.command(cmd::UNDO);
        assert_eq!(e.doc.brushes.len(), 1);
        assert_eq!(e.doc.selection, vec![a]);
    }

    #[test]
    fn hold_r_rotates_selection_and_esc_cancels() {
        let mut e = editor();
        let id = e.doc.add_brush(v3(-32.0, -32.0, 0.0), v3(32.0, 32.0, 64.0));
        e.doc.selection = vec![id];
        let p = sp(&e, 1, 20.0, 0.0); // over the brush, to the right of center
        e.mouse_move(1, p.0, p.1, 0);
        e.key(1, key::R, true, 0, false);
        let q = (p.0 - 180.0, p.1); // 180px left = 90 degrees counter-clockwise
        e.mouse_move(1, q.0, q.1, 0);
        e.mouse_press(1, q.0, q.1, button::LEFT, 0); // clicks are ignored while R is held
        e.mouse_release(1, q.0, q.1, button::LEFT);
        e.key(1, key::R, false, 0, false);
        assert_eq!(e.doc.brush(id).unwrap().rot, [0.0, 0.0, 90.0]);
        // R + Esc puts it back
        e.mouse_move(1, q.0, q.1, 0);
        e.key(1, key::R, true, 0, false);
        e.mouse_move(1, p.0, p.1, 0);
        assert_eq!(e.doc.brush(id).unwrap().rot, [0.0, 0.0, 0.0]);
        e.key(1, key::ESCAPE, true, 0, false);
        e.key(1, key::R, false, 0, false);
        assert_eq!(e.doc.brush(id).unwrap().rot, [0.0, 0.0, 90.0]);
        // R works from anywhere, even far off the brush…
        let far = sp(&e, 1, 400.0, 300.0);
        e.mouse_move(1, far.0, far.1, 0);
        e.key(1, key::R, true, 0, false);
        e.mouse_move(1, far.0 + 180.0, far.1, 0);
        e.key(1, key::R, false, 0, false);
        assert_eq!(e.doc.brush(id).unwrap().rot, [0.0, 0.0, 0.0]);
        // …but needs a selection
        e.doc.selection.clear();
        e.mouse_move(1, p.0, p.1, 0);
        assert_eq!(e.key(1, key::R, true, 0, false), 0);
    }

    #[test]
    fn rotation_snap_setting_and_swapped_alt() {
        let mut e = editor();
        let id = e.doc.add_brush(v3(-32.0, -32.0, 0.0), v3(32.0, 32.0, 64.0));
        e.doc.selection = vec![id];
        e.cfg.rot_snap = 45.0;
        let p = sp(&e, 1, 20.0, 0.0);
        e.mouse_move(1, p.0, p.1, 0);
        e.key(1, key::R, true, 0, false);
        e.mouse_move(1, p.0 - 60.0, p.1, 0); // 30 degrees -> snaps to 45 with a 45 step
        e.key(1, key::R, false, 0, false);
        assert_eq!(e.doc.brush(id).unwrap().rot[2], 45.0);
        // swapped: Alt+drag snaps, Alt+Shift+drag is free
        e.cfg.swap_alt = true;
        let b = e.doc.add_brush(v3(200.0, 0.0, 0.0), v3(264.0, 64.0, 64.0));
        drag!(e, 1, sp(&e, 1, 220.0, 20.0), sp(&e, 1, 227.0, 20.0), mods::ALT);
        assert_eq!(e.doc.brush(b).unwrap().bounds().0.x, 208.0, "Alt = on grid when swapped");
        drag!(e, 1, sp(&e, 1, 236.0, 40.0), sp(&e, 1, 239.0, 40.0), mods::ALT | mods::SHIFT);
        assert!((e.doc.brush(b).unwrap().bounds().0.x - 211.0).abs() < 0.05, "Alt+Shift = free when swapped: {:?}", e.doc.brush(b).unwrap().bounds());
        // nudging can be switched off
        e.cfg.nudge = false;
        let before = e.doc.brush(b).unwrap().bounds();
        e.key(1, key::RIGHT, true, 0, false);
        assert_eq!(e.doc.brush(b).unwrap().bounds(), before);
    }

    #[test]
    fn alt_q_straightens_rotation_to_45() {
        let mut e = editor();
        let id = e.doc.add_brush(v3(-32.0, -32.0, 0.0), v3(32.0, 32.0, 64.0));
        e.doc.selection = vec![id];
        let p = sp(&e, 1, 20.0, 0.0);
        e.mouse_move(1, p.0, p.1, 0);
        e.key(1, key::R, true, 0, false);
        e.mouse_move(1, p.0 - 66.0, p.1, mods::ALT); // free rotation: 33 degrees
        e.key(1, key::R, false, 0, false);
        assert!((e.doc.brush(id).unwrap().rot[2] - 33.0).abs() < 0.1);
        e.command(cmd::QUANTIZE);
        let b = e.doc.brush(id).unwrap();
        assert_eq!(b.rot[2], 45.0);
        assert!(b.verts.iter().all(|p| p.x % 16.0 == 0.0 && p.y % 16.0 == 0.0), "{:?}", b.verts);
        assert!(e.message.as_deref().unwrap().starts_with("Straightened"));
        // already at 45: Alt+Q leaves the rotation alone
        e.command(cmd::QUANTIZE);
        assert_eq!(e.doc.brush(id).unwrap().rot[2], 45.0);
    }

    fn hold_s(e: &mut Editor, id: usize, at: (f32, f32)) {
        e.mouse_move(id, at.0, at.1, 0);
        e.key(id, key::S, true, 0, false);
    }

    #[test]
    fn s_arrows_move_top_left_and_shift_moves_bottom_right() {
        let mut e = editor();
        let id = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0));
        e.doc.selection = vec![id];
        let b = |e: &Editor| e.doc.brush(id).unwrap().bounds();
        hold_s(&mut e, 1, (5.0, 5.0)); // cursor position doesn't matter for arrows
        // top view: top edge = max y, left edge = min x
        e.key(1, key::UP, true, 0, false);
        assert_eq!(b(&e).1.y, 80.0, "S+Up: top edge up (taller)");
        e.key(1, key::DOWN, true, 0, false);
        e.key(1, key::DOWN, true, 0, false);
        assert_eq!(b(&e).1.y, 48.0, "S+Down: top edge down (shorter)");
        e.key(1, key::LEFT, true, 0, false);
        assert_eq!(b(&e).0.x, -16.0, "S+Left: left edge out (wider)");
        e.key(1, key::RIGHT, true, 0, false);
        assert_eq!(b(&e).0.x, 0.0, "S+Right: left edge in (narrower)");
        // Shift: the opposite edges, still following the arrow
        e.key(1, key::DOWN, true, mods::SHIFT, false);
        assert_eq!(b(&e).0.y, -16.0, "S+Shift+Down: bottom edge down (taller)");
        e.key(1, key::RIGHT, true, mods::SHIFT, false);
        assert_eq!(b(&e).1.x, 80.0, "S+Shift+Right: right edge out (wider)");
        e.key(1, key::LEFT, true, mods::SHIFT | mods::ALT, false);
        assert_eq!(b(&e).1.x, 79.0, "Alt: 1 unit");
        // minimum size is one grid step
        for _ in 0..10 {
            e.key(1, key::DOWN, true, 0, false);
        }
        let (lo, hi) = b(&e);
        assert_eq!(hi.y - lo.y, 16.0);
        e.key(1, key::S, false, 0, false);
        // arrows nudge again once S is released
        let before = b(&e);
        e.key(1, key::RIGHT, true, 0, false);
        assert_eq!(b(&e).0.x, before.0.x + 16.0);
        // the front view sizes its own axes (h = Y, v = Z)
        hold_s(&mut e, 2, (5.0, 5.0));
        let z = b(&e).1.z;
        e.key(2, key::UP, true, 0, false);
        assert_eq!(b(&e).1.z, z + 16.0);
        e.key(2, key::S, false, 0, false);
    }

    #[test]
    fn s_arrow_hold_is_one_undo_step_and_works_on_block_box() {
        let mut e = editor();
        let id = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0));
        e.doc.selection = vec![id];
        hold_s(&mut e, 1, (5.0, 5.0));
        e.key(1, key::UP, true, 0, false);
        e.key(1, key::UP, true, 0, true);
        e.key(1, key::UP, true, 0, true);
        e.key(1, key::S, false, 0, false);
        assert_eq!(e.doc.brush(id).unwrap().bounds().1.y, 112.0);
        e.command(cmd::UNDO);
        assert_eq!(e.doc.brush(id).unwrap().bounds().1.y, 64.0);
        // pending block box
        drag!(e, 1, sp(&e, 1, 200.0, 200.0), sp(&e, 1, 264.0, 264.0), 0);
        let lo = e.block_box.unwrap().0.x;
        hold_s(&mut e, 1, (5.0, 5.0));
        e.key(1, key::LEFT, true, 0, false);
        assert_eq!(e.block_box.unwrap().0.x, lo - 16.0);
        e.key(1, key::S, false, 0, false);
    }

    #[test]
    fn hold_s_and_move_mouse_resizes_from_nearest_corner() {
        let mut e = editor();
        let id = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0));
        e.doc.selection = vec![id];
        // S down over the top-right quarter, then move right/up by 40 units
        let p = sp(&e, 1, 48.0, 48.0);
        hold_s(&mut e, 1, p);
        let q = sp(&e, 1, 82.0, 79.0); // +34 / +31 -> edges snap to 96
        e.mouse_move(1, q.0, q.1, 0);
        e.mouse_press(1, q.0, q.1, button::LEFT, 0); // clicks ignored while S-sizing
        e.mouse_release(1, q.0, q.1, button::LEFT);
        let (lo, hi) = e.doc.brush(id).unwrap().bounds();
        assert_eq!((lo.x, lo.y), (0.0, 0.0), "bottom-left corner anchored");
        assert_eq!((hi.x, hi.y), (96.0, 96.0), "edges snapped to grid: {hi:?}");
        e.key(1, key::S, false, 0, false);
        // Ctrl keeps proportions
        e.command(cmd::UNDO);
        let p = sp(&e, 1, 48.0, 48.0);
        hold_s(&mut e, 1, p);
        let q = sp(&e, 1, 112.0, 52.0); // mostly horizontal drag
        e.mouse_move(1, q.0, q.1, mods::CTRL);
        let (lo, hi) = e.doc.brush(id).unwrap().bounds();
        assert!(((hi.x - lo.x) - (hi.y - lo.y)).abs() < 0.01, "square stays square: {lo:?} {hi:?}");
        assert!(hi.x > 64.0);
        // Esc cancels
        e.key(1, key::ESCAPE, true, 0, false);
        e.key(1, key::S, false, 0, false);
        assert_eq!(e.doc.brush(id).unwrap().bounds(), (v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0)));
        // a jiggle inside the dead zone does nothing
        let p = sp(&e, 1, 48.0, 48.0);
        hold_s(&mut e, 1, p);
        e.mouse_move(1, p.0 + 3.0, p.1, 0);
        e.key(1, key::S, false, 0, false);
        assert_eq!(e.doc.brush(id).unwrap().bounds().1.x, 64.0);
    }

    #[test]
    fn bisect_split_and_delete_with_shift_flip() {
        let mut e = editor();
        let id = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(128.0, 64.0, 64.0));
        e.doc.selection = vec![id];
        e.command(cmd::TOOL_BISECT);
        assert_eq!(e.tool, Tool::Bisect);
        // vertical cut at x = 32 in the top view
        drag!(e, 1, sp(&e, 1, 32.0, -32.0), sp(&e, 1, 32.0, 96.0), 0);
        assert!(e.bisect_line.is_some());
        let pv = e.bisect_preview().unwrap();
        assert_eq!(pv.pieces.len(), 1);
        e.key(1, key::RETURN, true, 0, false);
        assert_eq!(e.doc.brushes.len(), 2, "Split keeps both halves");
        assert_eq!(e.doc.selection.len(), 2);
        let mut widths: Vec<f32> = e.doc.brushes.iter().map(|b| b.bounds().1.x - b.bounds().0.x).collect();
        widths.sort_by(|a, b| a.partial_cmp(b).unwrap());
        assert_eq!(widths, vec![32.0, 96.0]);
        e.command(cmd::UNDO);
        assert_eq!(e.doc.brushes.len(), 1);

        // pressing 4 again toggles Split & Delete
        e.command(cmd::TOOL_BISECT);
        assert!(e.bisect_delete);
        e.doc.selection = vec![id];
        drag!(e, 1, sp(&e, 1, 32.0, -32.0), sp(&e, 1, 32.0, 96.0), 0);
        e.key(1, key::RETURN, true, 0, false);
        assert_eq!(e.doc.brushes.len(), 1);
        let (lo, hi) = e.doc.brush(id).unwrap().bounds();
        assert_eq!((lo.x, hi.x), (32.0, 128.0), "the smaller (x<32) side was removed");
        e.command(cmd::UNDO);

        // Shift held flips which side goes
        e.doc.selection = vec![id];
        drag!(e, 1, sp(&e, 1, 32.0, -32.0), sp(&e, 1, 32.0, 96.0), 0);
        e.key(1, key::SHIFT, true, mods::SHIFT, false);
        e.key(1, key::RETURN, true, mods::SHIFT, false);
        e.key(1, key::SHIFT, false, 0, false);
        let (lo, hi) = e.doc.brush(id).unwrap().bounds();
        assert_eq!((lo.x, hi.x), (0.0, 32.0), "Shift: the bigger side was removed");
    }

    #[test]
    fn bisect_snaps_to_edge_midpoints_and_diagonal_makes_wedges() {
        let mut e = editor();
        let id = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0));
        e.doc.selection = vec![id];
        e.command(cmd::TOOL_BISECT);
        // ends dropped near (but not on) the top/bottom edge midpoints snap onto
        // them, even when the grid (64 here) would put them somewhere else
        e.grid = 64.0;
        let a = sp(&e, 1, 33.0, 66.0);
        let b = sp(&e, 1, 30.0, -3.0);
        drag!(e, 1, a, b, 0);
        let (_, p, q) = e.bisect_line.unwrap();
        assert_eq!((p, q), ((32.0, 64.0), (32.0, 0.0)));
        e.grid = 16.0;
        e.key(1, key::ESCAPE, true, 0, false);
        assert!(e.bisect_line.is_none());
        // diagonal corner-to-corner cut: two wedges
        drag!(e, 1, sp(&e, 1, 0.0, 0.0), sp(&e, 1, 64.0, 64.0), 0);
        e.key(1, key::RETURN, true, 0, false);
        assert_eq!(e.doc.brushes.len(), 2);
        assert!(e.doc.brushes.iter().all(|b| b.verts.len() == 6));
        // a line that misses everything just reports it
        e.doc.selection = vec![id];
        drag!(e, 1, sp(&e, 1, 300.0, 0.0), sp(&e, 1, 300.0, 64.0), 0);
        e.key(1, key::RETURN, true, 0, false);
        assert!(e.message.as_deref().unwrap().contains("doesn't pass"));
        // a plain click in the bisect tool selects a brush
        e.key(1, key::ESCAPE, true, 0, false);
        e.doc.selection.clear();
        click!(e, 1, sp(&e, 1, 10.0, 50.0), 0);
        assert_eq!(e.doc.selection.len(), 1);
    }

    #[test]
    fn hammer_keys_shift_s_tap_selects_tool_but_sizing_does_not() {
        let mut e = editor();
        let id = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0));
        e.doc.selection = vec![id];
        e.command(cmd::TOOL_BLOCK);
        // default keys: Shift+S tap does nothing special
        e.key(1, key::SHIFT, true, mods::SHIFT, false);
        e.key(1, key::S, true, mods::SHIFT, false);
        e.key(1, key::S, false, mods::SHIFT, false);
        assert_eq!(e.tool, Tool::Block);
        // Hammer keys: a tap switches to Selection…
        e.cfg.hammer_keys = true;
        e.key(1, key::S, true, mods::SHIFT, false);
        let f = e.key(1, key::S, false, mods::SHIFT, false);
        assert_eq!(e.tool, Tool::Select);
        assert!(f & flags::TOOL != 0);
        // …but Shift+S used for sizing does not
        e.command(cmd::TOOL_BLOCK);
        e.key(1, key::S, true, mods::SHIFT, false);
        e.key(1, key::DOWN, true, mods::SHIFT, false);
        e.key(1, key::S, false, mods::SHIFT, false);
        assert_eq!(e.tool, Tool::Block);
        assert_eq!(e.doc.brush(id).unwrap().bounds().0.y, -16.0);
    }

    #[test]
    fn s_highlight_shows_arrow_edges_or_the_zone_under_the_mouse() {
        let mut e = editor();
        let id = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(96.0, 96.0, 64.0));
        e.doc.selection = vec![id];
        // S away from the brush, up and to the right: that corner's two edges
        { let p = sp(&e, 1, 300.0, 300.0); hold_s(&mut e, 1, p); }
        assert_eq!(e.s_edges().unwrap().2, vec![(true, true), (false, true)]);
        e.key(1, key::S, false, 0, false);
        assert!(e.s_edges().is_none());
        // S with the mouse in another view: the arrow edges, left + top, flipping with Shift
        e.hover = Some((2, 10.0, 10.0));
        e.key(1, key::S, true, 0, false);
        assert_eq!(e.s_edges().unwrap().2, vec![(true, false), (false, true)]);
        e.key(1, key::SHIFT, true, mods::SHIFT, false);
        assert_eq!(e.s_edges().unwrap().2, vec![(true, true), (false, false)]);
        e.key(1, key::SHIFT, false, 0, false);
        e.key(1, key::S, false, 0, false);
        // S over the middle of the right side: only the right edge
        { let p = sp(&e, 1, 90.0, 48.0); hold_s(&mut e, 1, p); }
        assert_eq!(e.s_edges().unwrap().2, vec![(true, true)]);
        // …and dragging moves only that edge
        let q = sp(&e, 1, 122.0, 70.0);
        e.mouse_move(1, q.0, q.1, 0);
        let (lo, hi) = e.doc.brush(id).unwrap().bounds();
        assert_eq!((lo.x, lo.y, hi.x, hi.y), (0.0, 0.0, 128.0, 96.0));
        e.key(1, key::S, false, 0, false);
        // bottom-left corner: two edges
        { let p = sp(&e, 1, 5.0, 5.0); hold_s(&mut e, 1, p); }
        assert_eq!(e.s_edges().unwrap().2, vec![(true, false), (false, false)]);
        // pressing an arrow switches the highlight to the arrow edges
        e.key(1, key::UP, true, 0, false);
        assert_eq!(e.s_edges().unwrap().2, vec![(true, false), (false, true)]);
        e.key(1, key::S, false, 0, false);
    }

    #[test]
    fn hold_s_outside_a_small_brush_grabs_the_nearest_side_without_jumping() {
        let mut e = editor();
        let id = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(16.0, 16.0, 16.0));
        e.doc.selection = vec![id];
        // far to the right, within the brush's y span: just the right edge
        { let p = sp(&e, 1, 200.0, 8.0); hold_s(&mut e, 1, p); }
        assert_eq!(e.s_edges().unwrap().2, vec![(true, true)]);
        // moving 32 units right grows it by 32, it doesn't jump to the cursor
        let q = sp(&e, 1, 232.0, 8.0);
        e.mouse_move(1, q.0, q.1, 0);
        let (lo, hi) = e.doc.brush(id).unwrap().bounds();
        assert_eq!((lo.x, lo.y, hi.x, hi.y), (0.0, 0.0, 48.0, 16.0));
        e.key(1, key::S, false, 0, false);
        // below and to the left of the corner: both edges of that corner
        { let p = sp(&e, 1, -100.0, -100.0); hold_s(&mut e, 1, p); }
        assert_eq!(e.s_edges().unwrap().2, vec![(true, false), (false, false)]);
        let q = sp(&e, 1, -116.0, -132.0);
        e.mouse_move(1, q.0, q.1, 0);
        let (lo, hi) = e.doc.brush(id).unwrap().bounds();
        assert_eq!((lo.x, lo.y, hi.x, hi.y), (-16.0, -32.0, 48.0, 16.0));
        e.key(1, key::S, false, 0, false);
        // nothing selected: S does nothing with the mouse
        e.doc.selection.clear();
        { let p = sp(&e, 1, 200.0, 8.0); hold_s(&mut e, 1, p); }
        assert!(e.s_edges().is_none());
        e.key(1, key::S, false, 0, false);
    }

    #[test]
    fn hold_shift_and_move_clones_release_places_and_guards() {
        let mut e = editor();
        let id = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(32.0, 32.0, 32.0));
        e.doc.selection = vec![id];
        let far = sp(&e, 1, 200.0, 16.0);
        e.mouse_move(1, far.0, far.1, 0);
        e.key(1, key::SHIFT, true, mods::SHIFT, false);
        // a small wobble does nothing
        e.mouse_move(1, far.0 + 4.0, far.1, mods::SHIFT);
        assert_eq!(e.doc.brushes.len(), 1);
        // past the dead zone: a copy follows the mouse, 64 units right
        let q = sp(&e, 1, 264.0, 16.0);
        e.mouse_move(1, q.0, q.1, mods::SHIFT);
        assert_eq!(e.doc.brushes.len(), 2);
        assert_eq!(e.hold_view(), Some(1));
        e.key(1, key::SHIFT, false, 0, false);
        assert!(e.drag.is_none());
        assert_eq!(e.hold_view(), None);
        let copy = e.doc.selection[0];
        assert_ne!(copy, id);
        assert_eq!(e.doc.brush(copy).unwrap().bounds().0.x, 64.0);
        // clone + move undo as one step
        e.command(cmd::UNDO);
        assert_eq!(e.doc.brushes.len(), 1);
        // Shift+S (or any other key) while held is not a clone
        e.doc.selection = vec![id];
        e.key(1, key::SHIFT, true, mods::SHIFT, false);
        e.key(1, key::S, true, mods::SHIFT, false);
        e.key(1, key::S, false, mods::SHIFT, false);
        let q = sp(&e, 1, 300.0, 16.0);
        e.mouse_move(1, q.0, q.1, mods::SHIFT);
        e.key(1, key::SHIFT, false, 0, false);
        assert_eq!(e.doc.brushes.len(), 1);
        // the setting turns it off
        e.shift_clone = false;
        e.key(1, key::SHIFT, true, mods::SHIFT, false);
        let q = sp(&e, 1, 360.0, 16.0);
        e.mouse_move(1, q.0, q.1, mods::SHIFT);
        e.key(1, key::SHIFT, false, 0, false);
        assert_eq!(e.doc.brushes.len(), 1);
    }

    #[test]
    fn hold_g_moves_the_selection_from_anywhere_and_esc_cancels() {
        let mut e = editor();
        e.axis_lock = false;
        let id = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(32.0, 32.0, 32.0));
        e.doc.selection = vec![id];
        let far = sp(&e, 1, 300.0, 16.0);
        e.mouse_move(1, far.0, far.1, 0);
        e.key(1, key::G, true, 0, false);
        let q = sp(&e, 1, 332.0, 48.0);
        e.mouse_move(1, q.0, q.1, 0);
        assert_eq!(e.hold_view(), Some(1));
        e.key(1, key::G, false, 0, false);
        assert_eq!(e.doc.brushes.len(), 1);
        assert_eq!(e.doc.brush(id).unwrap().bounds().0, v3(32.0, 32.0, 0.0));
        assert_eq!(e.hold_view(), None);
        // Esc mid-move puts it back
        e.key(1, key::G, true, 0, false);
        let q2 = sp(&e, 1, 364.0, 48.0);
        e.mouse_move(1, q2.0, q2.1, 0);
        e.key(1, key::ESCAPE, true, 0, false);
        e.key(1, key::G, false, 0, false);
        assert_eq!(e.doc.brush(id).unwrap().bounds().0, v3(32.0, 32.0, 0.0));
        // undo takes back the first move
        e.command(cmd::UNDO);
        assert_eq!(e.doc.brush(id).unwrap().bounds().0, v3(0.0, 0.0, 0.0));
        assert!(e.ripple.is_some(), "a drop leaves a ripple");
    }

    #[test]
    fn g_axis_lock_keeps_one_axis_and_alt_flips_it() {
        let mut e = editor();
        assert!(e.axis_lock, "on by default");
        let id = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(32.0, 32.0, 32.0));
        e.doc.selection = vec![id];
        let p = sp(&e, 1, 16.0, 16.0);
        e.mouse_move(1, p.0, p.1, 0);
        e.key(1, key::G, true, 0, false);
        let q = sp(&e, 1, 80.0, 32.0); // mostly sideways
        e.mouse_move(1, q.0, q.1, 0);
        assert_eq!(e.doc.brush(id).unwrap().bounds().0, v3(64.0, 0.0, 0.0));
        assert!(e.hold_hint().is_some_and(|h| h.1 == 1 && h.2.contains("[G]")));
        // Alt during the hold frees it (and isn't free placement)
        e.key(1, key::ALT, true, mods::ALT, false);
        assert!(!e.free);
        e.mouse_move(1, q.0, q.1 + 0.01, mods::ALT);
        e.mouse_move(1, q.0, q.1, mods::ALT);
        assert_eq!(e.doc.brush(id).unwrap().bounds().0, v3(64.0, 16.0, 0.0));
        e.key(1, key::ALT, false, 0, false);
        e.key(1, key::G, false, 0, false);
        assert!(e.hold_hint().is_none());
        // next hold starts locked again
        e.key(1, key::G, true, 0, false);
        assert!(e.axis_locked());
        e.key(1, key::G, false, 0, false);
    }

    #[test]
    fn locked_axis_sticks_for_the_hold_and_start_snaps_back() {
        let mut e = editor();
        let id = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(32.0, 32.0, 32.0));
        e.doc.selection = vec![id];
        let p = sp(&e, 1, 16.0, 16.0);
        e.mouse_move(1, p.0, p.1, 0);
        e.key(1, key::G, true, 0, false);
        let q = sp(&e, 1, 16.0, 80.0); // straight up first: commits to the vertical
        e.mouse_move(1, q.0, q.1, 0);
        assert_eq!(e.doc.brush(id).unwrap().bounds().0, v3(0.0, 64.0, 0.0));
        let r = sp(&e, 1, 200.0, 96.0); // now mostly sideways: still only vertical
        e.mouse_move(1, r.0, r.1, 0);
        assert_eq!(e.doc.brush(id).unwrap().bounds().0, v3(0.0, 80.0, 0.0));
        e.mouse_move(1, p.0 + 3.0, p.1 - 2.0, 0); // back by the start (pixels)
        assert_eq!(e.doc.brush(id).unwrap().bounds().0, v3(0.0, 0.0, 0.0));
        e.key(1, key::G, false, 0, false);
    }

    #[test]
    fn off_grid_brush_returns_exactly_when_dragged_back() {
        let mut e = editor();
        e.axis_lock = false;
        let id = e.doc.add_brush(v3(3.0, 5.0, 0.0), v3(35.0, 37.0, 32.0));
        e.doc.selection = vec![id];
        let p = sp(&e, 1, 20.0, 20.0);
        e.mouse_move(1, p.0, p.1, 0);
        e.key(1, key::G, true, 0, false);
        let q = sp(&e, 1, 90.0, 60.0);
        e.mouse_move(1, q.0, q.1, 0);
        assert_ne!(e.doc.brush(id).unwrap().bounds().0, v3(3.0, 5.0, 0.0));
        e.mouse_move(1, p.0 + 1.0, p.1 + 1.0, 0);
        assert_eq!(e.doc.brush(id).unwrap().bounds().0, v3(3.0, 5.0, 0.0));
        e.key(1, key::G, false, 0, false);
    }

    #[test]
    fn quick_draw_right_drag_makes_a_one_unit_block_on_release() {
        let mut e = editor();
        e.command(cmd::TOOL_BLOCK);
        let (a, b) = (sp(&e, 1, 0.0, 0.0), sp(&e, 1, 64.0, 32.0));
        e.mouse_press(1, a.0, a.1, button::RIGHT, 0);
        e.mouse_move(1, b.0, b.1, button::RIGHT);
        e.mouse_release(1, b.0, b.1, button::RIGHT);
        assert_eq!(e.doc.brushes.len(), 1);
        let id = e.doc.brushes[0].id;
        assert_eq!(e.doc.brush(id).unwrap().bounds(), (v3(0.0, 0.0, 0.0), v3(64.0, 32.0, 16.0)));
        assert_eq!(e.doc.selection, vec![id]);
        assert!(e.block_box.is_none());
        // with a selection, the next one sits on top of it
        e.grid = 8.0;
        let (a, b) = (sp(&e, 1, 0.0, 0.0), sp(&e, 1, 32.0, 32.0));
        e.mouse_press(1, a.0, a.1, button::RIGHT, 0);
        e.mouse_move(1, b.0, b.1, button::RIGHT);
        e.mouse_release(1, b.0, b.1, button::RIGHT);
        let top = e.doc.brushes.last().unwrap().bounds();
        assert_eq!((top.0.z, top.1.z), (16.0, 24.0));
        // a right-click without dragging makes nothing
        e.mouse_press(1, a.0, a.1, button::RIGHT, 0);
        e.mouse_release(1, a.0, a.1, button::RIGHT);
        assert_eq!(e.doc.brushes.len(), 2);
    }

    #[test]
    fn block_tool_makes_primitives() {
        let mut e = editor();
        e.command(cmd::TOOL_BLOCK);
        e.block_shape = Shape::Cylinder;
        e.block_faces = 12;
        drag!(e, 1, sp(&e, 1, 0.0, 0.0), sp(&e, 1, 64.0, 64.0), 0);
        assert!(e.block_preview().is_some());
        e.command(cmd::CREATE_BLOCK);
        let b = e.doc.brushes.last().unwrap();
        assert_eq!(b.faces.len(), 14);
        assert!(b.faces.iter().all(|f| f.material == geom::NODRAW));
        // drawn in the front view: lies along X
        e.block_shape = Shape::Spike;
        e.block_faces = 5;
        e.doc.selection.clear();
        drag!(e, 2, sp(&e, 2, 200.0, 200.0), sp(&e, 2, 264.0, 264.0), 0);
        e.command(cmd::CREATE_BLOCK);
        let b = e.doc.brushes.last().unwrap();
        assert_eq!(b.faces.len(), 6);
        assert_eq!(b.faces.iter().filter(|f| f.normal.x < -0.99).count(), 1, "the base faces -X");
    }

    #[test]
    fn vertex_select_picks_on_any_brush_and_g_r_s_edit_them() {
        let mut e = editor();
        e.axis_lock = false;
        let a = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0));
        let b = e.doc.add_brush(v3(128.0, 0.0, 0.0), v3(192.0, 64.0, 64.0));
        e.command(cmd::TOGGLE_VERTEX_PICK);
        assert_eq!(e.tool, Tool::Vertex);
        assert!(e.vertex_pick);
        // box the right-hand column of A and the left of B in the top view (both Z levels each)
        drag!(e, 1, sp(&e, 1, 44.0, -20.0), sp(&e, 1, 148.0, 84.0), 0);
        assert_eq!(e.vsel.len(), 8);
        assert!(e.doc.is_selected(a) && e.doc.is_selected(b));
        // G moves only those vertices: A gets wider, B narrower from the left
        let p = sp(&e, 1, 96.0, 32.0);
        e.mouse_move(1, p.0, p.1, 0);
        e.key(1, key::G, true, 0, false);
        let q = sp(&e, 1, 112.0, 32.0);
        e.mouse_move(1, q.0, q.1, 0);
        e.key(1, key::G, false, 0, false);
        assert_eq!(e.doc.brush(a).unwrap().bounds().1.x, 80.0);
        assert_eq!(e.doc.brush(b).unwrap().bounds().0.x, 144.0);
        assert_eq!(e.vsel.len(), 8, "still selected after the move");
        // S scales them about their center (in the top view: x and y)
        let c = e.vsel_center().unwrap();
        let v = e.view(1);
        let cp = v.to_screen(c.x, c.y);
        e.mouse_move(1, cp.0 + 40.0, cp.1, 0);
        e.key(1, key::S, true, 0, false);
        e.mouse_move(1, cp.0 + 20.0, cp.1, 0); // half the distance: 50%
        e.key(1, key::S, false, 0, false);
        let ab = e.doc.brush(a).unwrap().bounds();
        assert_eq!((ab.0.y, ab.1.y), (0.0, 64.0), "A's left side never moved");
        assert!((e.doc.brush(a).unwrap().bounds().1.x - 96.0).abs() < 0.01, "{:?}", e.doc.brush(a).unwrap().bounds());
        // Esc mid-R puts everything back
        let before: Vec<Brush> = e.doc.selected_brushes();
        e.mouse_move(1, cp.0, cp.1, 0);
        e.key(1, key::R, true, 0, false);
        e.mouse_move(1, cp.0 - 90.0, cp.1, 0);
        assert_ne!(e.doc.brush(a).unwrap().verts, before.iter().find(|x| x.id == a).unwrap().verts);
        e.key(1, key::ESCAPE, true, 0, false);
        e.key(1, key::R, false, 0, false);
        assert_eq!(e.doc.brush(a).unwrap().verts, before.iter().find(|x| x.id == a).unwrap().verts);
        assert_eq!(e.vsel.len(), 8);
        // a click on one vertex picks just that one
        let one = e.vsel[0].1;
        let sp1 = e.view(1).project(one);
        click!(e, 1, sp1, 0);
        assert!(e.vsel.iter().all(|&(_, p)| (p.x - one.x).abs() < 1e-3 && (p.y - one.y).abs() < 1e-3));
    }

    #[test]
    fn a_vertex_edit_that_would_go_concave_snaps_that_brush_back() {
        let mut e = editor();
        let a = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0));
        e.command(cmd::TOGGLE_VERTEX_PICK);
        // one top-right corner column only, then pull it inward past the middle diagonal
        drag!(e, 1, sp(&e, 1, 56.0, 56.0), sp(&e, 1, 72.0, 72.0), 0);
        assert_eq!(e.vsel.len(), 2);
        let before = e.doc.brush(a).unwrap().verts.clone();
        let orig = e.doc.selected_brushes();
        let sel = e.vsel.clone();
        let mut changed = false;
        e.apply_vertex_map(&orig, &sel, &|p| p - v3(60.0, 60.0, 0.0), &mut changed);
        assert_eq!(e.doc.brush(a).unwrap().verts, before);
        assert!(e.message.is_some());
    }

    #[test]
    fn copy_paste_between_models_keeps_groups_and_textures() {
        let mut e = editor();
        let a = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(32.0, 32.0, 32.0));
        let b = e.doc.add_brush(v3(64.0, 0.0, 0.0), v3(96.0, 32.0, 32.0));
        e.doc.brush_mut(a).unwrap().faces[0].material = "walls/brick".into();
        e.doc.mat_overrides.push(("walls/brick".into(), "surface=brick".into()));
        e.doc.selection = vec![a, b];
        e.command(cmd::GROUP);
        e.doc.rename_group(e.doc.groups[0].id, "Window");
        let clip = crate::io::write_clip(&e, "/projects/a/assets/Textures");
        assert!(clip.contains("source_textures"));
        // another model
        let mut f = editor();
        f.doc.add_brush(v3(0.0, 0.0, 0.0), v3(8.0, 8.0, 8.0));
        let n = f.paste(&clip, false).unwrap();
        assert_eq!(n, 2);
        assert_eq!(f.doc.brushes.len(), 3);
        assert_eq!(f.doc.selection.len(), 2);
        assert_eq!(f.doc.selection_bounds(), Some((v3(0.0, 0.0, 0.0), v3(96.0, 32.0, 32.0))), "same spot");
        assert_eq!(f.doc.groups.len(), 1);
        assert_eq!(f.doc.groups[0].name, "Window");
        assert!(f.doc.brushes.iter().any(|x| x.faces.iter().any(|fc| fc.material == "walls/brick")));
        assert_eq!(f.doc.mat_overrides.len(), 1);
        // under the mouse
        let p = sp(&f, 1, 300.0, 300.0);
        f.mouse_move(1, p.0, p.1, 0);
        f.paste(&clip, true).unwrap();
        let (lo, hi) = f.doc.selection_bounds().unwrap();
        assert!(((lo.x + hi.x) * 0.5 - 300.0).abs() <= 16.0 && ((lo.y + hi.y) * 0.5 - 300.0).abs() <= 16.0, "{lo:?} {hi:?}");
        assert_eq!(f.doc.groups.len(), 2);
        // one undo takes a paste back
        f.command(cmd::UNDO);
        assert_eq!(f.doc.brushes.len(), 3);
    }

    #[test]
    fn prefabs_land_on_the_surface_keeping_their_height() {
        // a shelf hanging 32 units above a floor, saved as a prefab
        let mut e = editor();
        e.doc.add_brush(v3(-512.0, -512.0, -16.0), v3(512.0, 512.0, 0.0));
        let shelf = e.doc.add_brush(v3(0.0, 0.0, 32.0), v3(32.0, 64.0, 40.0));
        e.doc.selection = vec![shelf];
        assert_eq!(e.selection_lift(), 32.0);
        let text = crate::io::write_clip(&e, "");
        // another model with a table 48 high at (256, 256)
        let mut f = editor();
        f.doc.add_brush(v3(-512.0, -512.0, -16.0), v3(512.0, 512.0, 0.0));
        f.doc.add_brush(v3(192.0, 192.0, 0.0), v3(320.0, 320.0, 48.0));
        // dropped in the top view on the table, keeping its height: 48 + 32
        let p = sp(&f, 1, 256.0, 256.0);
        f.place_prefab(&text, Some((1, p.0, p.1)), Some(32.0)).unwrap();
        let (lo, hi) = f.doc.selection_bounds().unwrap();
        assert_eq!(lo.z, 80.0, "{lo:?}");
        assert!(((lo.x + hi.x) * 0.5 - 256.0).abs() <= 16.0 && ((lo.y + hi.y) * 0.5 - 256.0).abs() <= 16.0, "{lo:?} {hi:?}");
        assert_eq!(hi.z - lo.z, 8.0, "same shape");
        // without keeping the height it sits on the table (beside the first one)
        let p2 = sp(&f, 1, 300.0, 300.0);
        f.place_prefab(&text, Some((1, p2.0, p2.1)), None).unwrap();
        assert_eq!(f.doc.selection_bounds().unwrap().0.z, 48.0);
        // off the table, on the floor
        let q = sp(&f, 1, -256.0, -256.0);
        f.place_prefab(&text, Some((1, q.0, q.1)), Some(32.0)).unwrap();
        assert_eq!(f.doc.selection_bounds().unwrap().0.z, 32.0);
        // the front view puts its bottom where the mouse is, depth unchanged
        let r = sp(&f, 2, 100.0, 64.0);
        f.place_prefab(&text, Some((2, r.0, r.1)), Some(32.0)).unwrap();
        let (lo, _) = f.doc.selection_bounds().unwrap();
        assert_eq!((lo.x, lo.z), (0.0, 64.0), "{lo:?}");
        // one undo per placement
        let n = f.doc.brushes.len();
        f.command(cmd::UNDO);
        assert_eq!(f.doc.brushes.len(), n - 1);
    }

    #[test]
    fn car_buddy_places_turns_its_footprint_and_saves() {
        let mut e = editor();
        e.command(cmd::TOOL_CAR);
        assert_eq!(e.tool, Tool::Entity);
        let p = sp(&e, 1, 0.0, 0.0);
        click!(e, 1, p, 0);
        let id = e.doc.selection[0];
        let b = e.doc.brush(id).unwrap();
        assert_eq!(b.entity, Some(Entity::Car));
        let (lo, hi) = b.bounds();
        assert_eq!((hi.x - lo.x, hi.y - lo.y, hi.z - lo.z), (176.0, 96.0, 80.0));
        // turned 90°, it's long the other way
        let mut b2 = b.clone();
        b2.rot[2] = 90.0;
        let base = b2.base();
        b2.place_entity(base);
        let (lo, hi) = b2.bounds();
        assert!(((hi.x - lo.x) - 96.0).abs() < 0.01 && ((hi.y - lo.y) - 176.0).abs() < 0.01);
        assert!(crate::io::write(&e).contains("\"car\""));
    }

    #[test]
    fn select_faces_with_a_texture_and_replace_it() {
        let mut e = editor();
        let a = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(32.0, 32.0, 32.0));
        let b = e.doc.add_brush(v3(64.0, 0.0, 0.0), v3(96.0, 32.0, 32.0));
        for (id, n) in [(a, 2), (b, 3)] {
            for i in 0..n {
                e.doc.brush_mut(id).unwrap().faces[i].material = "walls/brick".into();
            }
        }
        e.doc.brush_mut(a).unwrap().faces[0].tex.shift = [5.0, 7.0];
        assert_eq!(e.select_faces_with("walls/brick"), 5);
        assert_eq!(e.tool, Tool::Face);
        assert_eq!(e.selected_face_materials(), vec!["walls/brick".to_string()]);
        // only selected: one face
        e.fsel = vec![(a, 0)];
        assert_eq!(e.replace_material("walls/brick", "walls/stone", true), 1);
        assert_eq!(e.doc.brush(a).unwrap().faces[0].tex.shift, [5.0, 7.0], "alignment stays");
        // everywhere
        assert_eq!(e.replace_material("walls/brick", "walls/stone", false), 4);
        assert!(!e.doc.brushes.iter().any(|x| x.faces.iter().any(|f| f.material == "walls/brick")));
        e.command(cmd::UNDO);
        assert_eq!(e.doc.brushes.iter().flat_map(|x| x.faces.iter()).filter(|f| f.material == "walls/brick").count(), 4);
    }

    #[test]
    fn right_drag_paints_every_face_swept_over_in_one_undo() {
        let mut e = editor();
        let a = e.doc.add_brush(v3(-200.0, -32.0, 0.0), v3(-136.0, 32.0, 64.0));
        let b = e.doc.add_brush(v3(136.0, -32.0, 0.0), v3(200.0, 32.0, 64.0));
        e.cam = Camera { pos: v3(0.0, -10.0, 600.0), yaw: 90.0, pitch: -89.0 };
        e.command(cmd::TOOL_FACE);
        e.current_material = "walls/brick".into();
        let pa = e.persp_project(v3(-168.0, 0.0, 64.0)).unwrap();
        let pb = e.persp_project(v3(168.0, 0.0, 64.0)).unwrap();
        e.mouse_press(0, pa.0, pa.1, button::RIGHT, 0);
        e.mouse_move(0, (pa.0 + pb.0) * 0.5, pa.1, button::RIGHT);
        e.mouse_move(0, pb.0, pb.1, button::RIGHT);
        e.mouse_release(0, pb.0, pb.1, button::RIGHT);
        let top = |e: &Editor, id: u32| e.doc.brush(id).unwrap().faces.iter().any(|f| f.normal.z > 0.9 && f.material == "walls/brick");
        assert!(top(&e, a) && top(&e, b));
        e.command(cmd::UNDO);
        assert!(!top(&e, a) && !top(&e, b), "one undo step for the whole sweep");
        // flying: right button paints at the crosshair
        e.captured = true;
        e.cam = Camera { pos: v3(-168.0, 0.0, 600.0), yaw: 90.0, pitch: -89.0 };
        e.mouse_press(0, 0.0, 0.0, button::RIGHT, 0);
        e.mouse_release(0, 0.0, 0.0, button::RIGHT);
        assert!(top(&e, a));
    }

    #[test]
    fn join_brushes_exact_hull_and_exact_only() {
        let mut e = editor();
        let a = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(32.0, 32.0, 32.0));
        let b = e.doc.add_brush(v3(32.0, 0.0, 0.0), v3(64.0, 32.0, 32.0));
        e.doc.brush_mut(b).unwrap().faces.iter_mut().for_each(|f| f.material = "walls/brick".into());
        e.doc.selection = vec![a, b];
        e.command(cmd::JOIN);
        assert_eq!(e.doc.brushes.len(), 1);
        let j = &e.doc.brushes[0];
        assert_eq!(j.bounds(), (v3(0.0, 0.0, 0.0), v3(64.0, 32.0, 32.0)));
        assert_eq!(j.faces.len(), 6);
        assert!(j.faces.iter().any(|f| f.normal.x > 0.9 && f.material == "walls/brick"), "the +X face came from B");
        e.command(cmd::UNDO);
        assert_eq!(e.doc.brushes.len(), 2);
        // an L: exact only refuses, hull wraps
        let c = e.doc.add_brush(v3(0.0, 32.0, 0.0), v3(32.0, 64.0, 32.0));
        e.doc.selection = vec![b, c];
        e.join_exact_only = true;
        e.command(cmd::JOIN);
        assert_eq!(e.doc.brushes.len(), 3, "exact only: nothing joined");
        e.command(cmd::JOIN_HULL);
        assert_eq!(e.doc.brushes.len(), 2);
    }

    #[test]
    fn g_moves_the_block_tools_box() {
        let mut e = editor();
        e.axis_lock = false;
        e.command(cmd::TOOL_BLOCK);
        e.block_box = Some((v3(0.0, 0.0, 0.0), v3(32.0, 32.0, 32.0)));
        let p = sp(&e, 1, 16.0, 16.0);
        e.mouse_move(1, p.0, p.1, 0);
        e.key(1, key::G, true, 0, false);
        let q = sp(&e, 1, 48.0, 48.0);
        e.mouse_move(1, q.0, q.1, 0);
        e.key(1, key::G, false, 0, false);
        assert_eq!(e.block_box.unwrap().0, v3(32.0, 32.0, 0.0));
        assert!(e.doc.brushes.is_empty(), "still just a box, not a brush");
    }

    #[test]
    fn h_hides_selection_and_h_again_shows() {
        let mut e = editor();
        let a = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(32.0, 32.0, 32.0));
        let b = e.doc.add_brush(v3(64.0, 0.0, 0.0), v3(96.0, 32.0, 32.0));
        e.doc.selection = vec![a];
        e.key(1, key::H, true, 0, false);
        assert!(e.doc.selection.is_empty());
        assert!(!e.drawable(e.doc.brush(a).unwrap()));
        assert!(!e.pickable(e.doc.brush(a).unwrap()));
        assert!(e.drawable(e.doc.brush(b).unwrap()));
        e.command(cmd::SELECT_ALL);
        assert!(!e.doc.selection.contains(&a), "hidden brushes stay out of select all");
        e.doc.selection.clear();
        e.key(0, key::H, true, 0, false); // works in the 3D view too
        assert!(e.drawable(e.doc.brush(a).unwrap()));
    }

    #[test]
    fn comma_and_period_mirror_the_selection() {
        let mut e = editor();
        // a wedge: mirroring it is visible
        let id = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(64.0, 32.0, 32.0));
        e.doc.selection = vec![id];
        {
            let b = e.doc.brush_mut(id).unwrap();
            for v in b.verts.iter_mut() {
                if v.x > 32.0 && v.z > 16.0 {
                    v.z = 0.0 + 8.0;
                }
            }
        }
        let before = e.doc.brush(id).unwrap().clone();
        let p = sp(&e, 1, 10.0, 10.0);
        e.mouse_move(1, p.0, p.1, 0); // top view: horizontal = X
        e.key(1, key::COMMA, true, 0, false);
        let after = e.doc.brush(id).unwrap().clone();
        assert_eq!(after.bounds(), before.bounds(), "mirrors in place");
        for (v, w) in before.verts.iter().zip(after.verts.iter()) {
            assert_eq!(*w, v3(64.0 - v.x, v.y, v.z));
        }
        // faces still point outward
        let c = v3(32.0, 16.0, 8.0);
        for f in &after.faces {
            assert!(f.normal.dot(c) < f.dist + 1e-3, "inside stays inside");
        }
        // twice is back where it started
        e.key(1, key::COMMA, true, 0, false);
        e.key(1, key::PERIOD, true, 0, false);
        e.key(1, key::PERIOD, true, 0, false);
        assert_eq!(e.doc.brush(id).unwrap().verts, before.verts);
        e.command(cmd::UNDO);
        e.command(cmd::UNDO);
        e.command(cmd::UNDO);
        e.command(cmd::UNDO);
        assert_eq!(e.doc.brush(id).unwrap().verts, before.verts);
    }

    #[test]
    fn arrows_nudge_the_origin_in_the_origin_tool() {
        let mut e = editor();
        e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(32.0, 32.0, 32.0));
        e.command(cmd::TOOL_ORIGIN);
        let o = e.active_origin();
        e.key(1, key::RIGHT, true, 0, false);
        assert_eq!(e.active_origin(), o + v3(16.0, 0.0, 0.0));
    }

    #[test]
    fn hold_hint_and_middle_pan_pins() {
        let mut e = editor();
        assert!(e.hold_hint().is_none());
        e.drag = Some(Drag::Pan { view: 2, last: (0.0, 0.0) });
        assert_eq!(e.hold_view(), Some(2));
        assert_eq!(e.hold_hint().map(|h| (h.0, h.1)), Some((2, 5)));
    }

    #[test]
    fn groups_select_as_one_copy_with_numbers_nest_and_ungroup() {
        let mut e = editor();
        let a = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(16.0, 16.0, 16.0));
        let b = e.doc.add_brush(v3(32.0, 0.0, 0.0), v3(48.0, 16.0, 16.0));
        let c = e.doc.add_brush(v3(96.0, 0.0, 0.0), v3(112.0, 16.0, 16.0));
        e.doc.selection = vec![a, b];
        e.command(cmd::GROUP);
        let g = e.doc.groups[0].id;
        e.doc.groups[0].name = "Window".into();
        // clicking one member selects the whole group
        click!(e, 1, sp(&e, 1, 8.0, 8.0), 0);
        assert_eq!(e.doc.selection.len(), 2);
        // …unless groups are ignored
        e.command(cmd::IGNORE_GROUPS);
        e.doc.selection.clear();
        click!(e, 1, sp(&e, 1, 8.0, 8.0), 0);
        assert_eq!(e.doc.selection, vec![a]);
        e.command(cmd::IGNORE_GROUPS);
        // copies of a whole group are new groups, numbered
        e.doc.selection = e.doc.group_members(g);
        e.doc.checkpoint();
        let sel = e.doc.selection.clone();
        let c1 = e.doc.duplicate(&sel);
        let c2 = e.doc.duplicate(&sel);
        let name = |e: &Editor, id: u32| e.doc.group(e.doc.top_group(id).unwrap()).unwrap().name.clone();
        assert_eq!(name(&e, c1[0]), "Window (1)");
        assert_eq!(name(&e, c2[0]), "Window (2)");
        assert_ne!(e.doc.top_group(c1[0]), Some(g));
        // nesting: group the window with c → the window becomes a sub-group
        e.doc.selection = vec![a, b, c];
        e.command(cmd::GROUP);
        let outer = e.doc.top_group(a).unwrap();
        assert_ne!(outer, g);
        assert_eq!(e.doc.group(g).unwrap().parent, Some(outer));
        assert_eq!(e.doc.top_group(c), Some(outer));
        // ungroup takes one level off
        e.command(cmd::UNGROUP);
        assert_eq!(e.doc.top_group(a), Some(g));
        assert_eq!(e.doc.top_group(c), None);
        // deleting every member removes the group
        e.doc.selection = e.doc.group_members(g);
        e.command(cmd::DELETE);
        assert!(e.doc.group(g).is_none());
        // and it all saves
        let text = crate::io::write(&e);
        let mut e2 = editor();
        crate::io::apply(&mut e2, crate::io::read(&text).unwrap());
        assert_eq!(e2.doc.groups.len(), e.doc.groups.len());
        assert_eq!(e2.doc.top_group(c1[0]).and_then(|t| e2.doc.group(t)).map(|x| x.name.clone()), Some("Window (1)".into()));
    }

    #[test]
    fn new_collision_layer_copies_the_visual_model_and_ghosts_it() {
        let mut e = editor();
        let a = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0));
        let id = crate::bonk_test_add_collision(&mut e);
        assert!(e.editing_collision());
        let copies: Vec<&Brush> = e.doc.brushes.iter().filter(|b| b.layer == id).collect();
        assert_eq!(copies.len(), 1);
        assert_eq!(copies[0].bounds(), e.doc.brush(a).unwrap().bounds());
        // the visual original is a ghost: drawn, but can't be picked
        let orig = e.doc.brush(a).unwrap().clone();
        assert!(e.ghosted(&orig) && !e.pickable(&orig));
        click!(e, 1, sp(&e, 1, 32.0, 32.0), 0);
        assert_eq!(e.doc.selection.len(), 1);
        assert_ne!(e.doc.selection[0], a);
        // own origin for the collision layer: the Origin tool moves that one
        e.doc.layers.iter_mut().find(|l| l.id == id).unwrap().origin = Some(V3::default());
        e.command(cmd::TOOL_ORIGIN);
        click!(e, 1, sp(&e, 1, 48.0, 16.0), 0);
        assert_eq!(e.doc.layer(id).unwrap().origin, Some(v3(48.0, 16.0, 0.0)));
        assert_eq!(e.doc.origin, V3::default());
    }

    #[test]
    fn selection_box_inside_or_touching_enter_selects_and_s_m_adjust() {
        let mut e = editor();
        let a = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(32.0, 32.0, 32.0));
        let b = e.doc.add_brush(v3(48.0, 0.0, 0.0), v3(96.0, 32.0, 32.0));
        e.command(cmd::TOOL_SELECT);
        // point and click: a drag on empty space draws nothing
        drag!(e, 1, sp(&e, 1, -16.0, -16.0), sp(&e, 1, 64.0, 48.0), 0);
        assert!(e.sel_box.is_none() && e.doc.selection.is_empty());
        // the box half, instant: what it touches is selected while you drag, and it stays up
        e.command(cmd::TOGGLE_SEL_BOX_MODE);
        assert!(e.sel_box_only && !e.sel_box_wait);
        let (p0, p1) = (sp(&e, 1, 16.0, 16.0), sp(&e, 1, 34.0, 24.0)); // starts on brush A: still a box, no grab
        e.mouse_press(1, p0.0, p0.1, button::LEFT, 0);
        e.mouse_move(1, p1.0, p1.1, button::LEFT);
        assert_eq!(e.doc.selection, vec![a], "live while dragging");
        let p2 = sp(&e, 1, 64.0, 24.0);
        e.mouse_move(1, p2.0, p2.1, button::LEFT);
        assert_eq!(e.doc.selection.len(), 2);
        e.mouse_release(1, p2.0, p2.1, button::LEFT);
        assert!(e.sel_box.is_some(), "the box stays up");
        assert_eq!(e.doc.brush(a).unwrap().bounds().0, v3(0.0, 0.0, 0.0), "nothing got grabbed");
        // Delete removes what it touches, and the box goes
        e.command(cmd::DELETE);
        assert_eq!(e.doc.brushes.len(), 0);
        assert!(e.sel_box.is_none());
        e.command(cmd::UNDO);
        e.doc.selection.clear();
        // the box half again: Enter mode (the old way)
        e.command(cmd::TOGGLE_SEL_BOX_MODE);
        assert!(e.sel_box_wait);
        drag!(e, 1, sp(&e, 1, -16.0, -16.0), sp(&e, 1, 64.0, 48.0), 0);
        assert!(e.doc.selection.is_empty(), "waits for Enter");
        assert!(e.sel_box.is_some());
        assert_eq!(e.sel_box_catch().len(), 2, "by default the box takes what it touches");
        e.sel_touch = false;
        assert_eq!(e.sel_box_catch(), vec![a], "the option: only fully inside");
        // S grows it from outside (right side)
        { let q = sp(&e, 1, 200.0, 16.0); hold_s(&mut e, 1, q); }
        let q2 = sp(&e, 1, 240.0, 16.0);
        e.mouse_move(1, q2.0, q2.1, 0);
        e.key(1, key::S, false, 0, false);
        assert_eq!(e.sel_box_catch().len(), 2);
        // Enter selects; the box goes away
        e.key(1, key::RETURN, true, 0, false);
        assert_eq!(e.doc.selection.len(), 2);
        assert!(e.sel_box.is_none());
        let _ = b;
        // a plain click on empty space still deselects
        click!(e, 1, sp(&e, 1, 300.0, 300.0), 0);
        assert!(e.doc.selection.is_empty());
        // the left half (1) goes back to point and click
        e.command(cmd::TOOL_SELECT);
        assert!(!e.sel_box_only);
        e.command(cmd::TOGGLE_SEL_BOX_MODE);
        // Esc drops a box
        drag!(e, 1, sp(&e, 1, -16.0, -16.0), sp(&e, 1, 64.0, 48.0), 0);
        e.key(1, key::ESCAPE, true, 0, false);
        assert!(e.sel_box.is_none());
    }

    #[test]
    fn budget_counts_what_gets_exported_and_rename_keeps_faces_linked() {
        let mut e = editor();
        let a = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(32.0, 32.0, 32.0));
        for f in &mut e.doc.brush_mut(a).unwrap().faces {
            f.material = "walls/brick".into();
        }
        // a box: 6 quads = 12 triangles, 24 vertices, 1 material
        assert_eq!(e.budget(), (12, 12, 24, 1, 0));
        // nodraw faces don't count
        e.doc.brush_mut(a).unwrap().faces[0].material = geom::NODRAW.into();
        assert_eq!(e.budget(), (10, 10, 20, 1, 0));
        // a collision layer copy adds collision pieces, not triangles
        crate::bonk_test_add_collision(&mut e);
        assert_eq!(e.budget().4, 1);
        assert_eq!(e.budget().0, 10);
        assert_eq!(e.budget_offenders(0)[0].0, a);
        // moving a texture: faces follow, undo puts it back
        let n = e.rename_material("walls/brick", "walls/old/brick");
        assert!(n >= 5);
        assert_eq!(e.material_usage("walls/brick"), 0);
        e.command(cmd::UNDO);
        assert!(e.material_usage("walls/brick") >= 5);
    }

    #[test]
    fn d_deselects_in_2d_and_undo_brings_it_back() {
        let mut e = editor();
        let a = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(32.0, 32.0, 32.0));
        e.doc.selection = vec![a];
        e.key(1, key::D, true, 0, false);
        assert!(e.doc.selection.is_empty());
        e.command(cmd::UNDO);
        assert_eq!(e.doc.selection, vec![a]);
        // in the 3D view D flies (always), it doesn't deselect
        e.key(0, key::D, true, 0, false);
        assert_eq!(e.doc.selection, vec![a]);
        assert!(e.tick(0.1) != 0);
        e.key(0, key::D, false, 0, false);
        // Ctrl+D (the menu command) works anywhere
        e.command(cmd::DESELECT);
        assert!(e.doc.selection.is_empty());
    }

    #[test]
    fn size_zone_thirds_and_centre() {
        let b = (v3(0.0, 0.0, 0.0), v3(90.0, 90.0, 0.0));
        assert_eq!(size_zone((80.0, 80.0), b, 0, 1), (1, 1));
        // outside: beside = one edge, past a corner = both
        assert_eq!(size_zone((500.0, 10.0), b, 0, 1), (1, 0));
        assert_eq!(size_zone((45.0, -300.0), b, 0, 1), (0, -1));
        assert_eq!(size_zone((-5.0, 95.0), b, 0, 1), (-1, 1));
        assert_eq!(size_zone((45.0, 85.0), b, 0, 1), (0, 1));
        assert_eq!(size_zone((5.0, 45.0), b, 0, 1), (-1, 0));
        assert_eq!(size_zone((44.0, 32.0), b, 0, 1), (0, -1), "center: nearest edge");
    }

    #[test]
    fn middle_drag_orbits_around_view_centre_and_shift_pans() {
        let mut e = editor();
        e.doc.add_brush(v3(-32.0, -32.0, 0.0), v3(32.0, 32.0, 64.0));
        e.cam = Camera { pos: v3(-300.0, 0.0, 32.0), yaw: 0.0, pitch: 0.0 };
        let c = (400.0, 300.0);
        e.mouse_press(0, c.0, c.1, button::MIDDLE, 0);
        let pivot = e.orbit_pivot.expect("pivot shown while orbiting");
        assert!((pivot - v3(-32.0, 0.0, 32.0)).len() < 0.5, "{pivot:?}");
        e.mouse_move(0, c.0 + 300.0, c.1 - 80.0, 0);
        // the pivot is still dead center and at the same distance
        let d = (pivot - e.cam.pos).len();
        assert!((d - 268.0).abs() < 0.5);
        let (o, dir) = e.persp_ray(c.0, c.1);
        assert!(((o + dir * d) - pivot).len() < 0.5);
        assert!(e.cam.yaw != 0.0 && e.cam.pitch != 0.0);
        e.mouse_release(0, c.0, c.1, button::MIDDLE);
        assert!(e.orbit_pivot.is_none());
        // Shift+middle pans sideways without turning
        let (yaw, pos) = (e.cam.yaw, e.cam.pos);
        e.mouse_press(0, c.0, c.1, button::MIDDLE, mods::SHIFT);
        e.mouse_move(0, c.0 - 100.0, c.1, mods::SHIFT);
        e.mouse_release(0, c.0, c.1, button::MIDDLE);
        assert_eq!(e.cam.yaw, yaw);
        let moved = e.cam.pos - pos;
        assert!(moved.len() > 10.0 && moved.dot(e.cam.right()) > 0.0, "drag left moves the view right: {moved:?}");
    }

    #[test]
    fn shift_ctrl_paint_select_in_3d() {
        let mut e = editor();
        let ids: Vec<u32> = (0..4).map(|i| e.doc.add_brush(v3(-16.0, i as f32 * 64.0 - 112.0, 0.0), v3(16.0, i as f32 * 64.0 - 80.0, 32.0))).collect();
        e.cam = Camera { pos: v3(-400.0, 0.0, 16.0), yaw: 0.0, pitch: 0.0 };
        let px = |e: &Editor, y: f32| {
            let m = e.persp_mvp();
            let n = m.project(v3(-16.0, y, 16.0)).unwrap();
            ((n.x * 0.5 + 0.5) * 800.0, (0.5 - n.y * 0.5) * 600.0)
        };
        let sc = mods::SHIFT | mods::CTRL;
        let a = px(&e, -96.0);
        e.mouse_press(0, a.0, a.1, button::LEFT, sc);
        for y in [-32.0, 32.0, 96.0] {
            let p = px(&e, y);
            e.mouse_move(0, p.0, p.1, sc);
        }
        e.mouse_release(0, a.0, a.1, button::LEFT);
        assert_eq!(e.doc.selection.len(), 4, "swept across all four");
        // starting on a selected brush deselects as it sweeps
        let p = px(&e, 32.0);
        e.mouse_press(0, p.0, p.1, button::LEFT, sc);
        let q = px(&e, 96.0);
        e.mouse_move(0, q.0, q.1, sc);
        e.mouse_release(0, q.0, q.1, button::LEFT);
        assert_eq!(e.doc.selection, vec![ids[0], ids[1]]);
    }

    #[test]
    fn mannequin_stands_on_clicked_face_and_moves_without_resizing() {
        let mut e = editor();
        e.doc.add_brush(v3(-64.0, -64.0, 0.0), v3(64.0, 64.0, 48.0));
        e.command(cmd::TOOL_MANNEQUIN);
        assert_eq!(e.tool, Tool::Entity);
        // 3D: look down at the top face
        e.cam = Camera { pos: v3(0.0, 0.0, 500.0), yaw: 0.0, pitch: -89.0 };
        let f = e.mouse_press(0, 400.0, 300.0, button::LEFT, 0);
        assert!(f & flags::TOOL != 0);
        assert_eq!(e.tool, Tool::Select, "back to the selection tool after placing");
        let m = e.doc.brush(e.doc.selection[0]).unwrap().clone();
        assert_eq!(m.entity, Some(Entity::Mannequin));
        assert!((m.base().z - 48.0).abs() < 0.01, "stands on the top face: {:?}", m.base());
        assert_eq!(m.bounds().1.z - m.bounds().0.z, 72.0);
        // 2D top view: lands on the highest surface under the click
        e.command(cmd::TOOL_MANNEQUIN);
        click!(e, 1, sp(&e, 1, 32.0, 32.0), 0);
        let m2 = e.doc.brush(e.doc.selection[0]).unwrap().clone();
        assert_eq!(m2.base(), v3(32.0, 32.0, 48.0));
        // off the brush: the floor
        e.command(cmd::TOOL_MANNEQUIN);
        click!(e, 1, sp(&e, 1, 192.0, 0.0), 0);
        assert_eq!(e.doc.brush(e.doc.selection[0]).unwrap().base(), v3(192.0, 0.0, 0.0));
        // resizing a selection with a mannequin in it moves the mannequin, never scales it
        let id = e.doc.selection[0];
        let h = sp(&e, 1, 208.0, 0.0); // right edge handle of the 32-wide box
        drag!(e, 1, h, sp(&e, 1, 280.0, 0.0), 0);
        let bb = e.doc.brush(id).unwrap().bounds();
        assert_eq!(bb.1.x - bb.0.x, 32.0);
        // the vertex tool ignores it and the bisect tool won't cut it
        e.command(cmd::TOOL_VERTEX);
        let c = bb.1;
        assert!(e.hit_vertices(1, sp(&e, 1, c.x, c.y).0, sp(&e, 1, c.x, c.y).1).is_empty());
        // hiding mannequins drops them from the selection and from picking
        e.command(cmd::TOGGLE_MANNEQUINS);
        assert!(e.doc.selection.is_empty());
        e.command(cmd::TOOL_SELECT);
        let bc = e.doc.brush(id).unwrap().base();
        click!(e, 1, sp(&e, 1, bc.x, bc.y), 0);
        assert!(!e.doc.selection.contains(&id));
    }

    #[test]
    fn mannequin_rotation_turns_it_but_keeps_it_upright() {
        let mut e = editor();
        let id = e.doc.add_entity(Entity::Mannequin, v3(0.0, 0.0, 0.0));
        e.doc.selection = vec![id];
        e.command(cmd::TOOL_SELECT);
        let p = sp(&e, 1, 12.0, 0.0);
        e.mouse_move(1, p.0, p.1, 0);
        e.key(1, key::R, true, 0, false);
        e.mouse_move(1, p.0 - 180.0, p.1, 0);
        e.key(1, key::R, false, 0, false);
        let b = e.doc.brush(id).unwrap();
        assert_eq!(b.rot[2], 90.0);
        assert_eq!(b.bounds(), (v3(-16.0, -16.0, 0.0), v3(16.0, 16.0, 72.0)), "box stays axis-aligned");
    }

    #[test]
    fn origin_tool_places_and_drags_the_origin() {
        let mut e = editor();
        e.command(cmd::TOOL_ORIGIN);
        click!(e, 1, sp(&e, 1, 37.0, -21.0), 0);
        assert_eq!(e.doc.origin, v3(32.0, -16.0, 0.0));
        // drag it in the front view (h = Y, v = Z)
        let o = sp(&e, 2, -16.0, 0.0);
        drag!(e, 2, o, sp(&e, 2, -16.0, 50.0), 0);
        assert_eq!(e.doc.origin, v3(32.0, -16.0, 48.0));
        e.command(cmd::UNDO);
        assert_eq!(e.doc.origin, v3(32.0, -16.0, 0.0));
        // to selection bottom-center
        let id = e.doc.add_brush(v3(0.0, 0.0, 16.0), v3(64.0, 32.0, 80.0));
        e.doc.selection = vec![id];
        e.command(cmd::ORIGIN_TO_SEL_BOTTOM);
        assert_eq!(e.doc.origin, v3(32.0, 16.0, 16.0));
        e.command(cmd::ORIGIN_TO_SEL_CENTER);
        assert_eq!(e.doc.origin, v3(32.0, 16.0, 48.0));
        // Alt in the origin tool means free placement, not "grab the brush"
        e.command(cmd::TOOL_ORIGIN);
        let before = e.doc.brush(id).unwrap().bounds();
        click!(e, 1, sp(&e, 1, 10.5, 10.5), mods::ALT);
        assert_eq!(e.doc.brush(id).unwrap().bounds(), before);
        assert!((e.doc.origin.x - 10.5).abs() < 0.05);
    }

    #[test]
    fn face_select_and_texture_utility_apply_nodraw() {
        let mut e = editor();
        let id = e.doc.add_brush(v3(-32.0, -32.0, 0.0), v3(32.0, 32.0, 64.0));
        for f in &mut e.doc.brush_mut(id).unwrap().faces {
            f.material = "walls/plaster".into();
        }
        e.command(cmd::TOOL_FACE);
        e.cam = Camera { pos: v3(-300.0, 0.0, 32.0), yaw: 0.0, pitch: 0.0 };
        e.mouse_press(0, 400.0, 300.0, button::LEFT, 0);
        assert_eq!(e.fsel.len(), 1);
        let (bid, f) = e.fsel[0];
        assert_eq!(bid, id);
        assert!(e.doc.brush(id).unwrap().faces[f].normal.x < -0.9, "the face toward the camera");
        assert_eq!(e.material_target_desc(), "1 selected face");
        assert_eq!(e.apply_material(geom::NODRAW), 1);
        let b = e.doc.brush(id).unwrap();
        assert_eq!(b.faces.iter().filter(|f| f.material == geom::NODRAW).count(), 1);
        // with no face selection, it targets every face of the selected brushes
        e.command(cmd::TOOL_SELECT);
        assert!(e.fsel.is_empty());
        e.doc.selection = vec![id];
        assert_eq!(e.apply_material("walls/brick"), 6);
        assert!(e.doc.brush(id).unwrap().faces.iter().all(|f| f.material == "walls/brick"));
        // "" (the old "none") is nodraw now
        assert_eq!(e.apply_material(""), 6);
        assert!(e.doc.brush(id).unwrap().faces.iter().all(|f| f.material == geom::NODRAW));
        e.command(cmd::UNDO);
        assert!(e.doc.brush(id).unwrap().faces.iter().all(|f| f.material == "walls/brick"));
    }

    #[test]
    fn bisect_cut_faces_are_always_nodraw() {
        let mut e = editor();
        let id = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(128.0, 64.0, 64.0));
        for f in &mut e.doc.brush_mut(id).unwrap().faces {
            f.material = "props/wood".into();
        }
        e.doc.selection = vec![id];
        e.command(cmd::TOOL_BISECT);
        drag!(e, 1, sp(&e, 1, 32.0, -32.0), sp(&e, 1, 32.0, 96.0), 0);
        e.key(1, key::RETURN, true, 0, false);
        for b in &e.doc.brushes {
            let cut: Vec<&geom::Face> = b.faces.iter().filter(|f| f.normal.x.abs() > 0.99 && {
                let x = b.verts[f.idx[0]].x;
                (x - 32.0).abs() < 0.01
            }).collect();
            assert_eq!(cut.len(), 1);
            assert_eq!(cut[0].material, geom::NODRAW, "split: hidden cut faces are nodraw");
            assert_eq!(b.faces.iter().filter(|f| f.material == "props/wood").count(), 5, "others keep their material");
        }
        e.command(cmd::UNDO);
        e.command(cmd::BISECT_MODE_DELETE);
        e.doc.selection = vec![id];
        drag!(e, 1, sp(&e, 1, 32.0, -32.0), sp(&e, 1, 32.0, 96.0), 0);
        e.key(1, key::RETURN, true, 0, false);
        let b = e.doc.brush(id).unwrap();
        assert_eq!(b.faces.iter().filter(|f| f.material == geom::NODRAW).count(), 1, "delete: the cut face is nodraw too");
        assert_eq!(b.faces.iter().filter(|f| f.material == "props/wood").count(), 5);
    }

    #[test]
    fn locked_and_hidden_layers_cant_be_picked() {
        let mut e = editor();
        let a = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0));
        let l2 = e.doc.add_layer(LayerKind::Visual);
        let b = e.doc.add_brush(v3(100.0, 0.0, 0.0), v3(164.0, 64.0, 64.0));
        e.command(cmd::TOOL_SELECT);
        e.doc.set_layer_flags(l2, true, true);
        click!(e, 1, sp(&e, 1, 132.0, 32.0), 0);
        assert!(e.doc.selection.is_empty(), "locked layer can't be clicked");
        e.command(cmd::SELECT_ALL);
        assert_eq!(e.doc.selection, vec![a]);
        // creating a block on a locked layer is refused
        e.command(cmd::TOOL_BLOCK);
        drag!(e, 1, sp(&e, 1, 300.0, 0.0), sp(&e, 1, 364.0, 64.0), 0);
        e.key(1, key::RETURN, true, 0, false);
        assert_eq!(e.doc.brushes.len(), 2);
        assert!(e.message.as_deref().unwrap().contains("locked"));
        // hidden: not drawn either
        e.doc.set_layer_flags(l2, false, false);
        assert!(!e.drawable(e.doc.brush(b).unwrap()));
    }

    #[test]
    fn face_paint_click_selects_source_right_click_paints_with_alignment() {
        let mut e = editor();
        let a = e.doc.add_brush(v3(-32.0, -32.0, 0.0), v3(32.0, 32.0, 64.0));
        let b = e.doc.add_brush(v3(-32.0, 64.0, 0.0), v3(32.0, 128.0, 64.0));
        e.cam = Camera { pos: v3(-300.0, 0.0, 32.0), yaw: 0.0, pitch: 0.0 };
        // outside the Face tool, a right click does nothing to faces
        e.current_material = "wood/planks".into();
        e.mouse_press(0, 400.0, 300.0, button::RIGHT, 0);
        e.mouse_release(0, 400.0, 300.0, button::RIGHT);
        assert!(e.doc.brush(a).unwrap().faces.iter().all(|f| f.material != "wood/planks"));
        // Face tool: right click paints the current texture on that face
        e.command(cmd::TOOL_FACE);
        e.mouse_press(0, 400.0, 300.0, button::RIGHT, 0);
        e.mouse_release(0, 400.0, 300.0, button::RIGHT);
        let fa = e.doc.brush(a).unwrap().faces.iter().position(|f| f.material == "wood/planks").expect("painted");
        // shift that face's texture, then click it: it becomes the source…
        e.doc.brush_mut(a).unwrap().faces[fa].tex.shift = [10.0, 20.0];
        e.current_material = "metal/x".into();
        let f = e.mouse_press(0, 400.0, 300.0, button::LEFT, 0);
        e.mouse_release(0, 400.0, 300.0, button::LEFT);
        assert!(f & flags::TOOL != 0);
        assert_eq!(e.current_material, "wood/planks");
        assert_eq!(e.fsel, vec![(a, fa)]);
        // …and right-click copies texture + alignment onto another brush's face
        e.cam = Camera { pos: v3(-300.0, 96.0, 32.0), yaw: 0.0, pitch: 0.0 };
        e.mouse_press(0, 400.0, 300.0, button::RIGHT, 0);
        e.mouse_release(0, 400.0, 300.0, button::RIGHT);
        let bf = e.doc.brush(b).unwrap().faces.iter().find(|f| f.material == "wood/planks").expect("copied").tex;
        assert_eq!(bf.shift, [10.0, 20.0]);
        // Shift+right-click paints the whole brush
        e.key(0, key::SHIFT, true, mods::SHIFT, false);
        e.mouse_press(0, 400.0, 300.0, button::RIGHT, mods::SHIFT);
        e.mouse_release(0, 400.0, 300.0, button::RIGHT);
        e.key(0, key::SHIFT, false, 0, false);
        assert!(e.doc.brush(b).unwrap().faces.iter().all(|f| f.material == "wood/planks"));
        // Alt+click picks up without changing the selection
        e.doc.brush_mut(b).unwrap().faces.iter_mut().for_each(|f| f.material = "stone".into());
        e.mouse_press(0, 400.0, 300.0, button::LEFT, mods::ALT);
        e.mouse_release(0, 400.0, 300.0, button::LEFT);
        assert_eq!(e.current_material, "stone");
        assert_eq!(e.fsel, vec![(a, fa)]);
        // right *drag* paints (since 0.14), and doesn't turn the camera
        let yaw = e.cam.yaw;
        e.mouse_press(0, 400.0, 300.0, button::RIGHT, 0);
        e.mouse_move(0, 460.0, 300.0, 0);
        e.mouse_release(0, 460.0, 300.0, button::RIGHT);
        assert_eq!(e.cam.yaw, yaw);
        // Esc clears the face selection
        e.key(0, key::ESCAPE, true, 0, false);
        assert!(e.fsel.is_empty());
    }

    #[test]
    fn texture_lock_moves_textures_with_the_brush() {
        let mut e = editor();
        let id = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0));
        e.doc.selection = vec![id];
        let top = |e: &Editor| e.doc.brush(id).unwrap().faces.iter().find(|f| f.normal.z > 0.9).unwrap().tex;
        let p = v3(10.0, 10.0, 64.0);
        let before = top(&e).uv(p);
        e.key(1, key::RIGHT, true, 0, false); // nudge +x by the grid (16)
        let after = top(&e).uv(p + v3(16.0, 0.0, 0.0));
        assert!((before.0 - after.0).abs() < 1e-3 && (before.1 - after.1).abs() < 1e-3);
        // lock off: the texture stays put in the world
        e.tex_lock = false;
        let t0 = top(&e);
        e.key(1, key::RIGHT, true, 0, false);
        assert_eq!(top(&e).shift, t0.shift);
        // resizing (lock on, no stretch) keeps the texture's size
        e.tex_lock = true;
        let s0 = top(&e).scale;
        { let q = sp(&e, 1, 90.0, 30.0); hold_s(&mut e, 1, q); }
        e.key(1, key::RIGHT, true, mods::SHIFT, false);
        e.key(1, key::S, false, 0, false);
        assert_eq!(top(&e).scale, s0);
    }

    #[test]
    fn wrap_justify_fit_and_treat_as_one() {
        let mut e = editor();
        let id = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0));
        e.tool = Tool::Face;
        let fi = |e: &Editor, n: V3| e.doc.brush(id).unwrap().faces.iter().position(|f| f.normal.dot(n) > 0.9).unwrap();
        let (top, side) = (fi(&e, v3(0.0, 0.0, 1.0)), fi(&e, v3(1.0, 0.0, 0.0)));
        for f in &mut e.doc.brush_mut(id).unwrap().faces {
            f.material = "brick".into();
        }
        e.doc.brush_mut(id).unwrap().faces[top].tex.shift = [13.0, 5.0];
        e.make_source((id, top));
        e.fsel = vec![(id, top)];
        // Alt+right-click wraps over the shared edge: no seam, and the side becomes the source
        e.alt = true;
        e.paint_face((id, side), false, true);
        e.alt = false;
        let (t, s) = (e.face_tex((id, top)).unwrap(), e.face_tex((id, side)).unwrap());
        for y in [0.0, 32.0, 64.0] {
            let p = v3(64.0, y, 64.0);
            assert!((t.uv(p).0 - s.uv(p).0).abs() < 1e-2 && (t.uv(p).1 - s.uv(p).1).abs() < 1e-2);
        }
        assert_eq!(e.src_face, Some((id, side)));
        // fit: one image (512 when not loaded) across the 64-unit face
        e.fsel = vec![(id, top)];
        e.src_face = Some((id, top));
        e.face_justify(5);
        let t = e.face_tex((id, top)).unwrap();
        let (lo, hi) = t.bounds(&e.face_points((id, top)));
        assert!((hi[0] - lo[0] - 512.0).abs() < 0.05 && lo[0].abs() < 0.05);
        // fields, one by one: absolute values
        e.face_set(2, 0.5);
        assert_eq!(e.face_tex((id, top)).unwrap().scale[0], 0.5);
        // treat as one: fit across top + side as one sheet, seam stays closed
        e.fsel = vec![(id, top), (id, side)];
        e.tex_group = true;
        e.face_justify(5);
        let (t, s) = (e.face_tex((id, top)).unwrap(), e.face_tex((id, side)).unwrap());
        let p = v3(64.0, 20.0, 64.0);
        assert!((t.uv(p).0 - s.uv(p).0).abs() < 1e-2 && (t.uv(p).1 - s.uv(p).1).abs() < 1e-2);
        let (lo, hi) = e.tex_bounds(&e.fsel.clone());
        assert!((hi[0] - lo[0] - 512.0).abs() < 0.1 && (hi[1] - lo[1] - 512.0).abs() < 0.1);
        // undo puts it back
        e.command(cmd::UNDO);
        assert!(e.face_tex((id, top)).unwrap().scale[0] == 0.5);
    }

    #[test]
    fn z_toggles_mouselook_and_wasd_flies() {
        let mut e = editor();
        let f = e.key(0, key::Z, true, 0, false);
        assert!(f & flags::CAP_ON != 0 && e.captured);
        let before = e.cam.pos;
        e.mouse_delta(100.0, 0.0);
        assert!((e.cam.yaw - (45.0 - 100.0 * LOOK_SENS)).abs() < 1e-3);
        e.key(0, key::W, true, 0, false);
        assert!(e.tick(0.5) != 0);
        let moved = e.cam.pos - before;
        assert!((moved.len() - FLY_SPEED * 0.5).abs() < 1e-2);
        let f = e.key(0, key::ESCAPE, true, 0, false);
        assert!(f & flags::CAP_OFF != 0 && !e.captured);
        assert_eq!(e.tick(0.5), 0, "keys released when capture ends");
    }

    #[test]
    fn click_in_3d_view_picks_brush() {
        let mut e = editor();
        let id = e.doc.add_brush(v3(-32.0, -32.0, -32.0), v3(32.0, 32.0, 32.0));
        e.cam = Camera { pos: v3(-256.0, 0.0, 0.0), yaw: 0.0, pitch: 0.0 };
        e.mouse_press(0, 400.0, 300.0, button::LEFT, 0);
        assert_eq!(e.doc.selection, vec![id]);
        e.mouse_press(0, 5.0, 5.0, button::LEFT, 0);
        assert!(e.doc.selection.is_empty());
    }

    #[test]
    fn wheel_zoom_keeps_point_under_cursor() {
        let mut e = editor();
        let before = e.view(1).to_world(100.0, 100.0);
        e.wheel(1, 100.0, 100.0, 1.0);
        let after = e.view(1).to_world(100.0, 100.0);
        assert!((before.0 - after.0).abs() < 1e-3 && (before.1 - after.1).abs() < 1e-3);
    }

    #[test]
    fn bisect_three_modes_and_f_flips_the_cut() {
        let mut e = editor();
        let id = e.doc.add_brush(v3(0.0, 0.0, 0.0), v3(128.0, 64.0, 64.0));
        e.doc.selection = vec![id];
        e.command(cmd::TOOL_BISECT);
        assert!(!e.bisect_delete);
        e.command(cmd::TOOL_BISECT);
        assert!(e.bisect_delete && !e.bisect_larger);
        e.command(cmd::TOOL_BISECT);
        assert!(e.bisect_delete && e.bisect_larger);
        e.command(cmd::TOOL_BISECT);
        assert!(!e.bisect_delete && !e.bisect_larger, "4 / B cycles back to Split");
        let cut = |e: &mut Editor, flips: usize| {
            e.doc.selection = vec![id];
            let (a, b) = (sp(e, 1, 32.0, -32.0), sp(e, 1, 32.0, 96.0));
            drag(e, 1, a, b, 0);
            for _ in 0..flips {
                e.key(1, key::F, true, 0, false);
                e.key(1, key::F, false, 0, false);
            }
            e.key(1, key::RETURN, true, 0, false);
        };
        // larger-side mode removes x > 32
        e.command(cmd::BISECT_MODE_DELETE_LARGER);
        cut(&mut e, 0);
        let (lo, hi) = e.doc.brush(id).unwrap().bounds();
        assert_eq!((lo.x, hi.x), (0.0, 32.0));
        e.command(cmd::UNDO);
        // F flips it back to removing the smaller side
        cut(&mut e, 1);
        let (lo, hi) = e.doc.brush(id).unwrap().bounds();
        assert_eq!((lo.x, hi.x), (32.0, 128.0));
        assert_eq!(e.bisect_flip, 0, "the flip lasts one cut");
        e.command(cmd::UNDO);
        // smaller-side mode, flipped twice = unflipped
        e.command(cmd::BISECT_MODE_DELETE);
        cut(&mut e, 2);
        let (lo, hi) = e.doc.brush(id).unwrap().bounds();
        assert_eq!((lo.x, hi.x), (32.0, 128.0));
        e.command(cmd::UNDO);
        // Split: F picks what stays selected: both, smaller, larger
        e.command(cmd::BISECT_MODE_SPLIT);
        for (flips, want) in [(0usize, vec![32.0f32, 96.0]), (1, vec![32.0]), (2, vec![96.0])] {
            cut(&mut e, flips);
            assert_eq!(e.doc.brushes.len(), 2);
            let mut w: Vec<f32> = e.doc.selection.iter().map(|&s| { let (lo, hi) = e.doc.brush(s).unwrap().bounds(); hi.x - lo.x }).collect();
            w.sort_by(|a, b| a.partial_cmp(b).unwrap());
            assert_eq!(w, want, "{flips} F presses");
            e.command(cmd::UNDO);
        }
    }

    #[test]
    fn new_blocks_are_nodraw() {
        let mut e = editor();
        e.command(cmd::TOOL_BLOCK);
        drag!(e, 1, sp(&e, 1, 0.0, 0.0), sp(&e, 1, 64.0, 64.0), 0);
        e.key(1, key::RETURN, true, 0, false);
        assert_eq!(e.doc.brushes.len(), 1);
        assert!(e.doc.brushes[0].faces.iter().all(|f| f.material == geom::NODRAW));
    }
}
