# Changelog

All notable changes to Bonk! Model Prop-inator!, newest first.

## 1.0 Beta 3

- **gmpublish works through Proton:** it was reaching Steam all along, but Windows console programs
  only print when they have a terminal under Wine, so Bonk saw nothing. gmad and gmpublish now run
  in one (`script`, from util-linux), and the terminal's escape codes are cleaned out of what they
  print. Listing and publishing both use it.
- The details box shows gmpublish's exit code.

## 1.0 Beta 2

- **Manage Workshop lists your addons even when gmpublish can't reach Steam:** it reads them from
  your public profile's Workshop page instead (your Steam account from loginusers.vdf). The count
  under the list says when it did ("from your profile").
- **"Couldn't list your addons: details"** shows exactly what gmpublish printed (with the command),
  with a Copy button, so the reason it can't reach Steam can be found.
- Fix: closing Manage Workshop while it was still fetching thumbnails could crash Bonk.
- gmpublish through Proton also gets `STEAM_COMPAT_APP_ID=4000`.

## 1.0 Beta 1 — "Variants"

Bonk leaves alpha: everything planned for 1.0 is in.

### Skins
- The **Variants** tab (beside Layers, the Texture Picker and Prefabs). **+ Skin** adds a skin; while
  it's picked, the views show it and **painting swaps materials in it** (every face with that
  material, as Source skins work). Swaps are listed; Delete undoes one.
- **Skins can hide brushes**: Hide Selection / Show Selection / Show All. Exported as copies of
  those brushes' materials that the skin swaps for an invisible one (`bonk_hidden`, `$no_draw`).
- Written as `$texturegroup "skinfamilies"`; swapped-in textures are converted and get VMTs too.

### Bodygroups
- Mark groups as options of a bodygroup (**Make Group an Option…**), named after the group; add a
  **None** option when you want one. Click an option to show it while you work (the others hide).
- Written as `$bodygroup` blocks, one SMD per option; brushes in no option stay the model's body.

### Levels of detail
- **+ LOD**: a switch distance (`$lod`) and a size under which brushes are dropped, with the
  triangles each keeps. Every body and option gets its LOD version (`replacemodel`).

### Model Preview (View → Model Preview, F7)
- The model as it compiles, with a skin, each bodygroup's option and a LOD to choose; drag to
  turn, right-drag to move, wheel to zoom, floor grid. It follows your edits.

### Also
- **Arch** shape for the Block tool: a half ring of brushes (Segments, Thickness), grouped.
- Collision outlines far from the camera are heavier while you edit collision, so pieces stay
  apart when you look at a whole house.
- Version 1.0.0-beta.1.

## 0.18.0 — "Workshop"

- **File → Manage Workshop…** publishes the project's addon to the Steam Workshop. It checks that
  Steam is running and signed in, and the first time asks before using your Steam profile (OK /
  No, with "Don't ask again").
- **Your addons** on the left: thumbnail, name, ID and last update, from `gmpublish list` and
  Steam's public Workshop lookup (cached, so the list shows straight away next time). Duplicates
  are hidden; a filter and Refresh.
- **New submission:** thumbnail, title, description, type (Model) and up to two tags, written into
  `addon.json`. **Update:** pick an addon (the project's own is picked for you), with a banner
  saying which, change notes, and optionally a new thumbnail.
- **Before publishing:** compiled models, a compile newer than the models, addon.json, the
  spawnlist, and files the Workshop won't take.
- **Publishing window:** checks, packs with gmad, uploads with gmpublish, each step ticking off,
  with their output, Copy Workshop Link and Open in Steam. The project remembers its Workshop ID
  (in `Workshop/workshop.json`).
- **Snap from Model…:** a 512 × 512 Workshop thumbnail with the spawn icon camera, on a background
  color, with an optional floor grid; Use Spawn Icon Angle. Choose Image… converts any picture to
  the baseline 4:2:0 JPG the Workshop wants.
- gmad / gmpublish: Garry's Mod's Linux builds when it has them, else the Windows ones through
  Proton. This is the third (and last) thing Bonk uses the Garry's Mod folder for.
- **Low Resolution** now always uses Bonk Pixel (the system-font option is gone).
- Spawn icons with see-through textures come out right (alpha no longer fades twice).

## 0.17.0 — "Interface"

### Prefabs
- **A Prefabs tab** beside Layers and the Texture Picker: brush sets to reuse, with thumbnails,
  in folders. **Save Selection as Prefab…** (on the tab, or in the Edit menu) saves the selected
  brushes with a name and a folder, either **Shared** (every project, in
  `~/.local/share/bonk/prefabs`, carrying copies of its textures) or for **this project only**
  (its `Prefabs` folder). The tab shows both.
- **Drag a prefab into a view** to place it, or double-click it to drop it under the mouse. In the
  3D and top views it lands on what's under the mouse; in the front and side views it goes where
  the mouse is. It arrives selected, one undo per placement, and textures it needs are copied
  into the project.
- **Keep height above floor** (on by default): a shelf saved 32 units above the floor lands 32
  units above the surface you drop it on. Off, it sits right on the surface.
- Right-click to rename, move to a folder, retake the thumbnail, show in the file manager or
  delete. A filter box and a size slider too.

### Settings
- **In tabs:** Editing, Tools & Keys, Appearance, Garry's Mod and Advanced (budget limits). It
  opens on the tab you used last.
- **Compact (Low Resolution)** (was "Compact"): tighter panels and small text without smoothing.
  Pick the font: your system font at 6 to 11 px, or **Bonk Pixel**, a pixel font drawn for Bonk
  (built in, 11 KB), crisp at 6, 12 or 18 px. A preview shows it at actual size and ×2.
- **Nodraw look:** a **Stripe length** slider (2 to 48 px; each stripe and the gap after it) in
  place of Thin / Medium / Chunky, and a **Rainbow flow length** slider for how far one whole
  rainbow stretches.

## 0.16.4

- **Fix:** dragging categories in the Spawnlist Editor only moved them on screen; the spawnlist
  kept the old nesting, so the next change (and the game) put them back. Drags now stick.

## 0.16.3

- **Fix:** in the Spawnlist Editor, pressing Enter after typing (renaming a category, a header)
  pressed "+ Subcategory" and added a category. No button is the default any more, so it isn't
  highlighted either.

## 0.16.2

- **Drag and drop in the Spawnlist Editor:** drag tiles and headers around the page to rearrange
  them (a bar shows where they'll land), or drop them on a category on the left to move them
  there. Hold Ctrl to copy instead. Works on several selected entries at once.

## 0.16.1

- **Any tile shape:** spawn icons can be wide or tall, not just square: presets (128 × 64,
  64 × 128, 256 × 128, 512 × 256…) or any size from 16 to 1,024 px in each direction. The
  preview and the page take the tile's shape, and changing the shape frames the model again from
  the same angle. Written as `wide` / `tall` in the Lua; the icon goes in as
  `<name>_<wide>x<tall>.png`, next to a square 64 × 64 one framed for GMod's usual tile.

## 0.16.0 — "Spawnlists"

- **Spawnlist Editor (Tools → Spawnlist Editor…):** your models in GMod's spawn menu, as a pack.
  Categories nest (drag to reorder or nest), and each page has headers ("Beds - Full") and model
  tiles, laid out the way the game shows them. One spawnlist named after the project to start;
  add more top-level lists if you want them.
- **Spawn icons you frame:** click a model, then drag to turn it, right-drag to move it in the
  frame, wheel to zoom. Fit frames it snugly from that angle; Use for Whole Category copies the
  angle to every model there. Transparent background, like GMod's. Tile size 64, 128, 256 or 512.
- **Category icons:** a built-in shape (house, folder, box, bed, chair, door, car, tree…), a Silk
  icon or an image, with a tint and a sub-icon in any corner, previewed at the game's 16 × 16.
- **Silk icons on request:** Import Silk Icons… copies GMod's icon16 set (famfamfam Silk) from a
  folder you downloaded; Bonk doesn't ship it.
- **In the addon, Workshop-ready:** compiling (and exporting, or Write to Addon) writes
  `lua/autorun/client/bonk_spawnlist_<project>.lua`, the category icons and the spawn icons
  (`materials/spawnicons/models/…png`, plus `_<size>.png` for bigger tiles).
- **Fix:** the Field of View setting now changes the 3D view (it only changed the setting).

## 0.15.0 — "Physics"

- **Simplified collision.** + Collision (or Tools → New Collision Layer, or Generate when compiling
  without one) asks: **Complex**, a copy of every visual brush, or **Simplified**: walls, floors,
  roofs and stairs stay (stairs become ramps), windows with a pane or frame are filled in solid,
  open doorways stay open, loose little details go, and pieces that line up are merged. A
  **Detail size** (Grid 8 by default) sets what counts as little; the dialog shows the piece count
  either way. Your 2s_house_5: 1,503 pieces complex, 1,120 simplified at Grid 8, 426 at Grid 16.
- **Join Brushes (Ctrl+J):** one convex brush from the selection, exactly their shape when it can
  be, else a hull around them. Faces keep their textures. Settings → Editing can make Ctrl+J exact
  only; Edit → Join Brushes (Hull) / (Exact Only) are there either way.
- **Tools → Set Mass…:** automatic or a number in kg, with presets Feather-light (1), Light
  (ragdoll, 90), Moderate (500), Heavy (5,000) and Very Heavy (house, 50,000). Written as `$mass`.
- **Collision pieces meter:** no longer red at 20; it turns yellow past 1,200 ("expensive, may
  still work") and never blocks a compile.
- **Compiler choice:** Settings → Garry's Mod → Compiler can point at another studiomdl.exe
  (e.g. StudioMDL CE), run through Proton the same way.
- **Start Garry's Mod** after exporting (a setting), or any time from the File menu.

## 0.14.1

- The animation badge (▶ and the frame count) moved to the swatch's bottom-right corner: the
  material-properties dot was drawn over it, which is the circle you saw.

## 0.14.0 — "Textures"

### Texture tools
- **Select All Using / Replace…** in the Face section (and on a swatch's right-click menu): select
  every face that uses a texture, or swap it for another everywhere in the model or only on the
  selected faces. Alignment stays; one undo step.
- **Face preview:** the Face section shows the selected face's texture and its name.
- **Paint by dragging:** in Face Select & Paint, hold the right button and sweep across faces to
  paint them all (one undo step; Shift paints whole brushes, Alt wraps). While flying, the right
  button paints at the crosshair and keeps painting as you look around.

### Shaders
- **Material Properties in tabs** (Basics, Maps, Animation, Extra) with a **MonitorScreen** shader
  choice.
- **More texture slots:** detail texture (scale, strength, blend mode), reflection mask texture
  ($envmapmask), glow mask ($selfillummask) and a phong sharpness map ($phongexponenttexture).
  Compiling converts them too.
- **Live VMT:** the VMT is shown as you change settings and you can type into it; your text is then
  used as is until "Back to Generated".
- **Moving materials:** scrolling (TextureScroll), pulsing brightness or transparency (Sine) and
  animated frames (AnimatedTexture), all playing in the 3D view.
- **Import Animation…** in the Texture Picker: frames from several images (name order), a sprite
  sheet (columns × rows) or a GIF (with its speed). Kept as one strip image with a ▶ badge and
  compiled to a multi-frame VTF.

## 0.13.5

- **Collision edges stand out while you edit the collision model:** thick, light lines with a dark
  outline, hidden where something is in front (they used to show through everything in the same
  orange as the faces). Each piece gets a slightly different shade so neighbors read apart; in the
  Solid look the edges are a light version of each piece's color.

## 0.13.4

- **WASD / QE fly the 3D camera again whenever the 3D view has the keys**, in mouselook or not
  (0.12 had limited them to mouselook). D in the 3D view flies; D deselects in the 2D views, and
  Ctrl+D anywhere. The arrows still nudge in 3D.

## 0.13.3

- **The Select button is split in two.** The left half (or 1) is point and click: click selects,
  drag moves, and a drag on empty space no longer draws a box. The right half is box select: every
  drag draws a box, even one that starts on a brush, so it never grabs one by accident.
- **Instant box select is live:** what the box touches is selected while you drag, and the box
  stays up; drag its edges or move it and the selection follows. Delete, G, S, R, H and the mirror
  keys work on what it caught. Click the right half again for select + Enter mode (the same as
  Settings → Editing).

## 0.13.2

- **Layers pane follows the views:** select a whole group in a 2D or 3D view and it's highlighted
  in the list (its parent groups open). Inside a layer you folded, the layer row lights up instead,
  and stays folded.
- **Folded layers and headings stay folded** when you work on another layer, toggle a Show box,
  or anything else rebuilds the list.

## 0.13.1

- **Fixed: a 2D view went dark while you held G, S, Shift or R, or panned.** The hold hint left
  its dark fill behind and the view's focus frame painted the whole view with it.
- The dark "Project saved!" blink is back (0.13.0 had taken it out by mistake).

## 0.13.0 — "Modeling power"

### Drawing
- **Quick Draw:** in the Block tool, right-drag in a 2D view and the block is made as soon as you
  let go, one grid unit deep. In the top view it sits on the floor or on top of the selection.
- **Primitives:** a Shape section in the tool panel while the Block tool is on: Block, Cylinder,
  Wedge, Spike and Orb, each one convex brush filling the box you draw, with a **Faces** slider
  (3–32 sides; orbs 4–16). Round shapes stand along the depth of the view you draw in. The box
  previews the shape.

### Vertices
- **Vertex select** (the ↖ at the end of the Vertex button): click or box vertices on any brush,
  several brushes at once (Ctrl adds), then hold **G** to move them, **R** to turn them or **S** to
  scale them about their center (Alt: smooth). A brush that would turn concave or flat snaps
  back; the rest still change.

### Copy & paste
- **Ctrl+C / Ctrl+X / Ctrl+V** through the system clipboard: between models, projects and Bonk
  windows. Ctrl+V pastes in the same spot (like Hammer), **Ctrl+Shift+V** under the mouse. Groups
  come along, and textures the project doesn't have are copied in from the project they came from.

### Checking
- **Tools → Find Overlapping Brushes:** brushes that share space (touching is fine), exact
  duplicates first; click one to select the pair and center the views. Visual and collision are
  checked separately.

### Measurement Buddies
- The Entities section is now **Measurement Buddies**, with a **Car** next to the mannequin: a
  rough buggy about the size of Half-Life 2's jeep (176 × 96 × 80). Like the mannequin, it's
  never compiled.

### Feel
- **Axis lock commits:** a G or Shift move picks the axis you first move along and keeps it for the
  whole hold (Alt still frees it).
- **Back to the start:** bringing the mouse back near where a G or Shift move started puts the
  selection exactly where it was, even off the grid.
- With the Texture Picker's swatches focused, Ctrl+C / X / V, Delete and F2 still act on texture
  files.

## 0.12.1

- **No more 1024 cap on textures.** A power-of-two image of any size up to Source's 4096 limit
  (e.g. 1024×2048) is left alone; only sizes that aren't powers of two are offered a resize.

## 0.12.0 — "Feel"

### Hold keys
- **G is grab** (it replaces M): hold G and move the mouse to move the selection from anywhere in
  a 2D view, or along the floor in the 3D view. G also moves the Block tool's box before you
  create it.
- **Axis lock:** G and Shift moves stay on the axis you move along most, which keeps things lined
  up. Alt while holding frees it (or locks it, if the lock is off). On by default.
- **What's held, on screen:** the bottom-left of the view says what the held key does, with a
  colored bar along the bottom; the cursor wears a badge (G, copy, R, S) and a paintbrush in
  Face Select & Paint. Dropping a brush sends out a ripple of squares.
- **Middle-button panning pins the cursor** like the other hold actions.

### New keys
- **H** hides the selection for now; H with nothing selected shows it all again.
- **, and .** mirror the selection horizontally and vertically, as the view under the mouse sees
  it (textures follow).
- **Mouse 4 / mouse 5** (the side buttons) switch to the Selection tool / Face Select & Paint,
  with the tool's icon flashing by the cursor.
- **Arrow keys move the origin** in the Origin tool.
- **3D view without flying:** D, G, H, R, the mirror keys and the arrows (nudging, "up" is away
  from you) work there. WASD / QE fly only in mouselook (Z). S sizing and Shift copying stay in
  the 2D views.

### Selecting
- **The selection box selects when you let go,** ready for G / S / R (Ctrl+drag adds). Settings →
  Editing → "Selection box waits for Enter" brings the old way back.
- **Selection Options** at the bottom of the tool panel: Ignore Groups (Ctrl+W), Box: Touching /
  Inside, and Axis Lock.
- A group you click in the Layers pane stays highlighted, and the status bar says when a group
  can't be selected (hidden, ghosted or locked layer).

### Look & feel
- **The list layout is the tool panel's default** (everyone is moved to it once; right-click the
  panel for Icons or Compact List).
- **Collision Look:** View → Collision Look → Ghosted (orange glass) or Solid (each piece opaque in
  its own color), also by the ghost slider in the Layers pane. Remembered between sessions.
- **3D field of view:** View → 3D field of view, 75° by default, up to 120° (or more after View →
  Unlock Field of View).
- **"Project saved!"**: every view blinks a dark tint and the 3D view says so.
- The Alt+wheel grid size shows as an outlined label without the dark box behind it.
- **Set Layer Name…** in the Layers right-click menu (also on a Visual / Collision heading with one
  layer under it) and **F2** (Edit → Set Layer Name).
- New Project's name field suggests "Name of your project, e.g. My Model, My Pack".

### Textures
- Textures imported from a Source game, folder or `.gma` are never offered a resize (they already
  work in Source). Importing many images asks once, at the end. Settings → Tools & keys can turn
  the resize question off; compiling still converts what it needs without asking.

### Compiling
- The compile log leaves out Proton / Wine / driver chatter (`Proton:`, `fixme:`, `ntsync`,
  `WARNING: radv`…), and only studiomdl's real `ERROR` lines are shown in red.

## 0.11.3

- **Collision models compile as convex pieces again** instead of one shrink-wrap. Each corner of
  a brush is now written the same way on every face, so studiomdl keeps a brush together as one
  piece (before, every face was its own flat piece: "2-dimensional geometry", "33 parts").
  Pieces thinner than half a unit are left out (with a note), since studiomdl can't use them.
- **Layers pane:** clicking a Show box no longer rebuilds the list while Qt is still handling the
  click, a likely cause of the crash when toggling visibility. The Show column has a fixed width.
- **Edit → Tidy Nested Groups** removes groups that only wrap another group in models imported
  before 0.11.2 (undoable).
- **Crash log:** if Bonk crashes it writes `~/.local/share/bonk/crash.log` and mentions it next
  time it starts.

## 0.11.2

- **Tidier groups from Hammer maps:** groups that only wrap another group (Ctrl+G pressed on
  something already grouped) are dissolved on import, so a window is one group, not six nested ones.
- **Layers pane:** nesting is shown in the name column, groups start folded (the ones you open
  stay open), and the Show boxes line up.
- **Import Source Textures:** the "already in the project" question appears on top of the
  progress window instead of behind it.

## 0.11.1

- **Big imported maps are fast again.** Group bookkeeping was quadratic: on a map with thousands
  of brushes and hundreds of groups every edit stalled for seconds (3,600 brushes / 360 groups:
  1.5 s per edit, now under a millisecond; importing it went from 3.1 s to 0.13 s).
- **Transparent textures show in the 3D view:** alpha-tested materials are cut out and
  translucent ones blend, following each texture's Material Properties.

## 0.11.0

- **File → Import VMF…** (Ctrl+I) brings a Hammer map's brushes in with their texture alignment:
  Create New Project, Add to Existing Project, or Just Open It. Visgroups become layers (untick
  the ones you don't want), Hammer groups become groups, and the import is moved so its bottom
  center sits on the model origin. TOOLSNODRAW stays nodraw, clip brushes become a collision
  layer, and skybox / hint / trigger brushes, displacements and point entities are skipped.
- **Source materials come along:** point Bonk at a `materials` folder, an addon folder or a
  Workshop **.gma**, and take only what the model uses or everything. VTFs are decoded to PNG in
  `assets/Textures` (same folder layout) and each VMT's settings become Material Properties.
  **File → Import Source Textures…** does the same any time.
- **Fresh brushes are nodraw**, and the old "none" material is gone (old files convert on open).
- **Bisect:** cut faces are always nodraw; 4 / B now cycles Split → Split & Delete smaller side →
  larger side, and **F** flips a pending cut.
- **The selection box takes what it touches** by default; Settings can make it fully-inside only.

## 0.10.0

- **Compiling:** File → Compile Model writes SMDs, QC, VMTs and VTFs and runs Garry's Mod's
  `studiomdl.exe` through Proton (newest stable by default, Bonk's own prefix in
  `~/.local/share/bonk/proton`). Steam libraries are found automatically, Flatpak Steam included.
  It warns about going over budget and about a missing collision model (Generate / Compile
  Without), and keeps any VMT you edited by hand.
- **Export to GMod Addons Folder** copies the project's addon into `garrysmod/addons/<project>`,
  asking before replacing it. Settings → Garry's Mod can export after every compile (off by default).
  Bonk only touches the Garry's Mod folder for these two things: running studiomdl and this export.
- **Recent Models** under File (Ctrl+Shift+O reopens the last one) and on the start screen.
- **Alt+wheel** changes the grid size (wheel up = bigger); **D** deselects in a 2D view, **Ctrl+D** anywhere.
- **View → Show Edges in 3D** for a cleaner, SketchUp-style 3D view.
- **Bonk! Guide** (F1) replaces the old Controls box; **Help → About Bonk!** with System Info.
- US English throughout.


## 0.9.1

**Budget bar** (docked under the viewports). Live meters for what the model will cost once compiled, against what Garry's Mod's studiomdl handles:

| Meter | Counts | Limit |
|---|---|---|
| Triangles | the busiest material's triangles (studiomdl draws each material in one go, capped at 32,768 indices) | 10,922 |
| Vertices | each face's corners, as the compiler counts them | 65,536 |
| Materials | different materials on visible faces | 64 |
| Collision pieces | brushes on collision layers (each one is a convex piece) | 20 (`$maxconvexpieces` default) |

Only what gets exported counts: nodraw faces, mannequins and collision layers don't add triangles, so nodraw on hidden faces visibly frees budget. Meters go yellow at 75 % and red at the limit (with ▲ / ✖ marks). Hover for the heaviest brushes; click a meter to select them. *Settings → Budget limits* changes the numbers.

**Colours as textures.** The **Color** tile in the picker (or right-click → New Color…) makes a 16×16 image named by its hex code in `assets/Textures/colors/` (`colors/ff8800`). It's an ordinary texture from then on: paint it, give it properties, convert it, rename it. Picking a color you already have reuses it.

**The picker is a file manager.** *Folders* view (the default) shows folder tiles and a breadcrumb (*Textures › walls*); double-click a folder to open it, Backspace goes up. The folders are exactly the ones under `materials/<texture pool>/` in the addon.
- Right-click a texture: **Cut**, **Copy**, **Move to Folder…**, **Rename…** (F2), **Delete…** (to the Trash), plus Material Properties, Convert and Show in Folder. Right-click a folder: Open, Paste Into, Rename Folder…, Delete Folder…. Right-click empty space: New Folder…, Paste, New Color…, Import….
- **Drag a swatch onto a folder** to move it there. Ctrl+X / C / V and Delete work in the grid.
- A texture's `.bonkmat`, its original and its VTF move with it.
- **Faces stay linked**: moving or renaming a texture updates this model (one undo step) and every other model in the project's *Model Projects* folder.
- Deleting warns when a texture is in use ("used on 14 faces…"), and everything goes to the Trash, not gone for good.
- *All textures* shows everything in one grid; typing in the filter searches every folder.

## 0.9

**Groups.** **Ctrl+G** groups the selection, **Ctrl+U** ungroups (one level at a time), **Ctrl+W** toggles *Ignore groups*. Clicking any member selects the whole group, and moving, cloning, M, R and S all treat it as one. Groups can hold groups (windows inside a house front). Copy a group (Shift-hold, Shift+drag, or Duplicate in the Layers pane) and the copy is a new group: *Window (1)*, *Window (2)*… The Layers pane shows them under their layer:

```
▾ House 1            ← layer
   ▣ Window (2)      ← group: click selects, double-click renames,
   ▣ Window (1)         right-click: Duplicate, Ungroup, Delete
   ▣ Door
```

**Collision layers start from the visual model.** **+ Collision** makes a new collision layer that starts as a copy of the visual model (with the same group names), ready to simplify into the physics hull. While a collision layer is active:
- the 3D view says **"You are editing the collision model"**;
- the visual model is a **ghost**: see-through and not clickable, and the **Visual ghost** slider in the Layers pane sets how visible it is (its Show box hides it completely);
- collision brushes draw solid instead of translucent.

Right-click a layer → **Own Origin** to give it an origin of its own (place it with the Origin tool while that layer is active; it's drawn in orange). Normally every layer uses the model origin.

**Selection box** (Select tool). Drag on empty space in a 2D view: a dashed box appears and everything it would catch turns **yellow** in every view. **S** resizes it (from outside too), **M** moves it, the handles work, **Enter** selects, **Ctrl+Enter** adds to the selection, **Esc** cancels. It takes brushes **fully inside** by default; *Settings → Selection box takes brushes it touches* changes that. The box reaches through the whole depth of the scene, and groups count only when the box catches all of them.

**R rotates from anywhere**, by moving the mouse **left / right** (½° per pixel, snapped to your rotation step, Alt for free), with the cursor pinned.

**Nodraw look.** Nodraw no longer clutters the 2D views (*View → Show Nodraw in 2D Views* brings it back). *Settings → Nodraw look*: **Solid color** (pick any), **RGB cycle**, or **Rainbow flow**, plus intensity, stripe width (thin / medium / chunky) and speed, all previewed live. *Classic yellow* resets it.

**Tool panel layouts.** *Icons* (sized to the panel's width, labels drop when they don't fit), *List* and *Compact list*: right-click the Tools panel, or Settings.

**Compact interface.** On screens under 1440p Bonk uses a tighter layout: smaller fonts and spacing, smaller tool buttons and swatches, help lines moved into tooltips, narrower docks. *Settings → Interface size*: Auto / Compact / Regular (after a restart).

**Default layout:** Tools on the left, Texture Picker and Layers tabbed on the right. (Your saved layout is reset once for this.)

**Fixed:** ticking a layer's Lock box crashed Bonk. The Lock column is gone for now; Show stays.

## 0.8.3

- **Hold M to move.** With something selected, hold **M** anywhere in a 2D view and move the mouse: the selection follows (snapped to the grid, Alt for free) with the cursor pinned, like S and Shift. Release M to place it, Esc cancels, and it undoes as one step.
- **B for Bisect.** B does exactly what 4 does: picks the Bisect tool, and pressed again switches Split / Split & Delete.

## 0.8.2

**Pinned cursor while holding S, Shift or R.** Once a hold-key action starts (S sizing, Shift cloning, R rotating), the cursor freezes where it is and Bonk reads the mouse's movement instead, so you can keep going without hitting the window or screen edge. Letting go of the key places the result and the cursor is right where it started. Losing focus mid-action (Alt+Tab) places it as it stands. *Settings → Pin the cursor while holding S / Shift / R* turns it off.

## 0.8.1

**Hold Shift to clone.** With brushes selected, hold **Shift** on its own anywhere in a 2D view (Select or Block tool) and move the mouse. Past a small dead zone, a copy of the selection follows the mouse, snapped to the grid (Alt for free). **Release Shift** to place it; Esc cancels. The copy becomes the selection, so Shift+move again makes a row, and each clone undoes as one step. It never starts when Shift is part of something else (Shift+S, Shift+arrows, Shift+wheel, Shift+click), or in the 3D view. *Settings → Hold Shift and move to clone the selection* turns it off.

## 0.8 — the texture tools

**Face Select & Paint (6)**, the Face tool's new name. In the 3D view:

| Mouse | Does |
|---|---|
| Click a face | Selects it (Ctrl adds/removes); its texture becomes the current one, replacing the picker's swatch |
| Right-click a face | Paints the current texture, with the source face's alignment |
| Shift+right-click | Paints the whole brush |
| Alt+right-click | Continuity paste: the texture carries on as one sheet, folded around a shared edge, carried straight over on the same plane, projected otherwise. The painted face becomes the next source, so you can walk a texture around an object |
| Alt+click | Picks up a face's texture without changing the selection |
| Drag a selected face | Moves the texture with the mouse (whole texels; Alt for free) |
| Ctrl+drag / Shift+drag | Rotates (15° steps) / scales (0.05 steps); Alt for free |
| Right-drag | Still looks around |
| Esc | Deselects the faces |

Outside this tool, right-click is normal again, and clicking a swatch switches to Face Select & Paint (*Settings → Texture Picker: clicking a swatch switches to Face Select & Paint* turns that off). The greyed-out "inactive" picker from 0.7.1 is gone.

**Face section** in the Texture Picker (shown in Face Select & Paint):
- **Shift**, **Scale** and **Rotate** fields, showing the source face's values. One by one (like Hammer), every selected face gets the value you type.
- **L / R / T / B / C** justify, **Stretch** (one copy of the image across the face), **Reset** (world-aligned, scale 0.25).
- **Treat as one** (2 or more faces): the selection works as one surface. Stretch, justify, shift, scale and rotate use the group's edges and center, and faces around corners are unfolded from the source face so the seams line up.
- **Texture Lock** (on by default): textures travel with brushes you move, nudge, copy or rotate. **Scale texture with brush** (off by default): with it off, resizing keeps textures at their size and anchored; on, they stretch with the brush.

Each face's texture alignment is saved in the `.bpmdl` (format 3; older files still open).

**Material Properties.** Double-click a texture swatch (or right-click → Material Properties…):
- shader (VertexLitGeneric / UnlitGeneric), **surface type** ($surfaceprop), transparency (alpha test with a cut-off, or translucent), double-sided, glow ($selfillum), normal map ($bumpmap, picked from your textures), "this texture is a normal map", reflections ($envmap with strength and mask), shine ($phong with boost and sharpness), and an **Extra lines** box for anything else;
- presets: **Monitor screen** (`_rt_Camera`), **Unlit sign**, **Glass**;
- a live **VMT preview**.

The settings are saved next to the image as `<name>.bonkmat`, so they travel with it and every model shares them. **Override for this model only** saves them into the current model instead. A blue dot on a swatch means it has properties. VMTs are written at compile time (with the build tools), and a VMT you've edited by hand is never overwritten.

**VTF conversion.** **Convert All** converts every texture that isn't converted yet into `addon/<project>/<texture pool>/`, and right-click → *Convert to VTF* does one. Bonk writes the VTFs itself: DXT1 for opaque images, DXT5 with alpha, uncompressed for normal maps, a full mip chain and the thumbnail Source expects. No extra tools needed. If an image changes after it was converted, Bonk deletes the stale VTF and a banner tells you to *convert it before it gets Propper'd*.

## 0.7.1

- Right-click painting with an "active" picker (replaced in 0.8 by Face Select & Paint).
- **Drag and drop into the picker.** Drop PNG/JPG files or whole folders from Dolphin onto the picker. They go into the group it's showing; on *All groups*, Bonk asks which one. A folder becomes a group of its own, subfolders included. If a texture is already there, you get Replace, Keep Both (`name_2`), or Skip. Big or non-power-of-two images get the usual resize offer. Dropping doesn't turn the picker on or off.
- **Hold S from outside the brush.** With a brush selected, S-sizing works with the cursor anywhere in the 2D view. Beside the brush grabs that side, diagonally past a corner grabs that corner, and inside works like before. The edge moves with the mouse instead of jumping to it, so small brushes are easy to size while zoomed out.

## 0.7

**Projects.** When Bonk starts, a start screen offers **New Project**, **Open Project**, your recent projects, or *Just Start Modeling* without one. A project is a folder:

```
housepack/
├── housepack.bonkpj          the project file (double-click it in Dolphin)
├── assets/
│   ├── Textures/             drop PNG / JPG here; subfolders become groups
│   └── Original Textures/    originals of textures Bonk resized
├── Model Projects/           your .bpmdl models (a project holds as many as you like)
├── .build/                   compiler scratch space (hidden, never uploaded)
└── addon/housepack/          what goes to the Workshop
    ├── models/…
    └── materials/…
```

- **New Project** asks for a name, a location (default `~/Bonk Projects`), and three game paths, with a live preview of the folders:
  - **Model base path**: new models go to `models/<base path>/<model>.mdl`. The default is the project name.
  - **Shared texture pool**: where the VTFs go, shared by every model. The default is `materials/<project name>`.
  - **Model materials**: where each model's VMTs go. The default is `materials/models/{model}`.
- **File → Project Settings** changes the paths later. The File menu also has Open Project, Recent Projects, and Close Project.
- Opening a model that lives inside a project opens its project too.

**Model paths.** Each model stores its own game path inside the `.bpmdl`, like Propper 0.31 did, so you set it once. The first save inside a project fills in `<base path>/<file name>`. **File → Model Properties** (Ctrl+P) changes it, for example to `housepack/onestory/house_1`, and previews the `.mdl`, VMT, `$cdmaterials`, and VTF paths. Bonk cleans paths up for Source: lowercase, `/` separators, and `_` for spaces and other characters Source can't use.

**Texture Picker** (T, or the Texture button):
- It lists **nodraw**, **none**, and every image in `assets/Textures`. Bonk watches the folder, so images you drop in, edit, or delete show up right away.
- The group menu and the filter box narrow the list down. The **Zoom** slider (or **Ctrl+wheel** over the swatches) sizes the swatches, and Bonk remembers the size.
- **Click** a swatch to make it current. **Apply** puts it on the selected faces, or on every face of the selected brushes (double-click now opens Material Properties, see 0.8). *Settings → Texture Picker: clicking a swatch applies it* makes a single click apply.
- **Import…** copies images into a group you pick or type.
- An orange corner marks textures that aren't converted to VTF yet (conversion arrives with the build tools).
- **Resizing for Source:** an image larger than 1024 or not a power of two gets a resize offer. The image is fitted within 1024, and each side goes to the nearest power of two. The resized file replaces the original, and the original moves to `assets/Original Textures` (same subfolder).

**Textures in the 3D view.** Faces show their actual images, aligned to the world the way Hammer does by default (scale 0.25: a 512px texture covers 128 units). Moving, scaling, and rotating textures on a face arrives in the next build.

**Textures in the 3D view** use each face's own alignment (see 0.8 above).

**Modular layout.** Tools, Texture Picker, and Layers are now docks:
- Drag a dock's title bar to any edge, onto another dock to stack them as tabs, or out to float it.
- **Window** menu: show or hide each dock, and **Reset Layout** to put everything back.
- Bonk remembers your layout between sessions.
- Layer names now get the full width of their dock.
