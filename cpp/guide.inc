// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Steventechno and the Bonk! contributors

// The Bonk! Guide (Help → Bonk! Guide, F1): every page of the in-app help.
// Included into host.cpp inside its namespace.

struct GuideSub {
    QString title, html;
};
struct GuidePage {
    QString id, section, title, keywords;
    QList<QPair<QString, QString>> keys; // key table at the top
    QString html;                        // text under the table
    QList<GuideSub> subs;                // smaller sections, each searchable on its own
    QStringList see;                     // "See also" page ids
};

/// "Ctrl+Shift+D" → keycaps. "(hold)", "drag", "wheel"… stay as words.
QString guideKbd(const QString &k) {
    static const QSet<QString> words = {QStringLiteral("drag"), QStringLiteral("click"), QStringLiteral("wheel"), QStringLiteral("move"),
                                        QStringLiteral("right-click"), QStringLiteral("double-click"), QStringLiteral("right-drag"),
                                        QStringLiteral("middle-drag"), QStringLiteral("arrows"), QStringLiteral("(hold)"), QStringLiteral("drop")};
    QStringList out;
    for (const QString &alt : k.split(QStringLiteral(" / "))) {
        QStringList parts;
        for (QString p : alt.split(QLatin1Char('+'))) {
            p = p.trimmed();
            QString word, rest;
            const int sp = p.indexOf(QLatin1Char(' '));
            if (sp > 0) {
                word = p.left(sp);
                rest = p.mid(sp);
            } else {
                word = p;
            }
            if (words.contains(word.toLower()) || word.isEmpty()) parts << p.toHtmlEscaped();
            else parts << QStringLiteral("<span class='kbd'>&nbsp;%1&nbsp;</span>%2").arg(word.toHtmlEscaped(), rest.toHtmlEscaped());
        }
        out << parts.join(QStringLiteral("+"));
    }
    return out.join(QStringLiteral(" / "));
}

QList<GuidePage> guidePages(bool hammer) {
    QList<GuidePage> g;
    const QString MK = QStringLiteral("Mouse & Keyboard"), GD = QStringLiteral("Guides");
    auto page = [&](const QString &id, const QString &sec, const QString &title, const QString &kw) -> GuidePage & {
        g.append(GuidePage{id, sec, title, kw, {}, {}, {}, {}});
        return g.last();
    };

    // ------------------------------------------------------------ Mouse & Keyboard
    {
        GuidePage &p = page(QStringLiteral("tools"), MK, QStringLiteral("Tools & hotkeys"), QStringLiteral("tool keys hotkeys shortcuts hammer panel dock layout mannequin car measurement buddies"));
        p.keys = {{QStringLiteral("1"), QStringLiteral("Selection tool")},
                  {QStringLiteral("2"), QStringLiteral("Block tool: draw new brushes")},
                  {QStringLiteral("3"), QStringLiteral("Vertex tool")},
                  {QStringLiteral("4 / B"), QStringLiteral("Bisect tool; press again to cycle Split → Split & Delete smaller → larger side")},
                  {QStringLiteral("5"), QStringLiteral("Origin tool: place the model origin")},
                  {QStringLiteral("6"), QStringLiteral("Face Select & Paint")},
                  {QStringLiteral("T"), QStringLiteral("Texture Picker: show it, bring it to the front, or hide it")},
                  {QStringLiteral("F1"), QStringLiteral("This guide, open at the page for the tool you're using")},
                  {QStringLiteral("F9"), QStringLiteral("Compile the model")},
                  {QStringLiteral("J"), QStringLiteral("Join the selected brushes into one")},
                  {QStringLiteral("A"), QStringLiteral("2D views: Axis Lock on / off (Advanced mode)")},
                  {QStringLiteral("Shift+Z"), QStringLiteral("The 3D view fills the views' area; again for all four")},
                  {QStringLiteral("F4"), QStringLiteral("The 3D view full screen, nothing else; again to come back")},
                  {QStringLiteral("`"), QStringLiteral("Walk mode: walk the model on its collision, first person")},
                  {QStringLiteral("mouse 4 / mouse 5"), QStringLiteral("Back / forward side buttons: Selection tool / Face Select & Paint")}};
        if (hammer)
            p.keys += {{QStringLiteral("Shift+S / Shift+B / Shift+V / Shift+X"), QStringLiteral("Hammer-style: Selection / Block / Vertex / Bisect")},
                       {QStringLiteral("Shift+A"), QStringLiteral("Hammer-style: Texture Picker")}};
        p.html = QStringLiteral("<p>The tool panel on the left shows every tool; right-click it to choose Icons, List or Compact list. "
                                "%1</p>").arg(hammer ? QStringLiteral("Hammer-style keys are on (Settings → Tools &amp; keys), so Shift+S/B/V/X/A work too.")
                                                     : QStringLiteral("Settings → Tools &amp; keys can add Hammer's Shift+S / B / V / X / A as well."));
        p.subs = {{QStringLiteral("Simple and Advanced"), QStringLiteral("<p>Bonk starts in <b>Simple</b> mode, which keeps the panel short: no Vertex "
                                                                         "select, Origin tool or Axis Lock, the selection box takes what it touches, no "
                                                                         "Prefabs or Variants, no Import VMF, and collision shows solid. "
                                                                         "Settings → Editing → <b>Advanced mode</b> brings everything back.</p>"
                                                                         "<p>While you edit a collision layer, Paint and Texture step aside (they don't apply "
                                                                         "to collision); Paint switches to Select.</p>")},
                  {QStringLiteral("Panels"), QStringLiteral("<p>Every panel is a dock: drag its title bar to another edge, onto another panel to make tabs, "
                                                            "or out to float it. Window → Reset Layout puts them back: tools on the left, Texture Picker and "
                                                            "Layers tabbed on the right, the Budget bar along the bottom.</p>")},
                  {QStringLiteral("Measurement Buddies"), QStringLiteral("<p>Two scale references in the tool panel: the <b>Mannequin</b> (32×32×72, the player) "
                                                                "and a <b>Car</b> (176×96×80, about the size of Half-Life 2's jeep). Click a surface in the 3D view "
                                                                "(or a spot in a 2D view) to place one. They move, turn and mirror like brushes but are never "
                                                                "compiled. View → Show Measurement Buddies hides them.</p>")}};
        p.see = {QStringLiteral("drawing"), QStringLiteral("selecting"), QStringLiteral("cheat")};
    }
    {
        GuidePage &p = page(QStringLiteral("drawing"), MK, QStringLiteral("Drawing & shapes"), QStringLiteral("block draw quick draw right-drag shape primitive cylinder wedge spike cone orb sphere arch faces sides segments thickness"));
        p.keys = {{QStringLiteral("2"), QStringLiteral("Block tool")},
                  {QStringLiteral("drag"), QStringLiteral("Draw a box in a 2D view; Enter makes it")},
                  {QStringLiteral("right-drag"), QStringLiteral("Quick Draw: a block one grid unit deep, made as soon as you let go")},
                  {QStringLiteral("G (hold) / S (hold)"), QStringLiteral("Move / size the box before you make it")},
                  {QStringLiteral("Esc"), QStringLiteral("Drop the box")}};
        p.html = QStringLiteral("<p><b>Shape</b> (in the tool panel while the Block tool is on): Block, Cylinder, Wedge, Spike or Orb, each filling "
                                "the box you draw, as one convex brush. <b>Faces</b> sets how many sides round shapes get (3–32; orbs use 4–16, "
                                "since every ring adds triangles). The box shows the shape it will become.</p>"
                                "<p>Round shapes stand along the depth of the view you draw in: drawn in the top view a cylinder stands up, "
                                "in the front or side view it lies on its side. A wedge always sits on its bottom and rises toward the right "
                                "of the view you drew it in.</p>"
                                "<p><b>Quick Draw</b> makes walls and floors fast: right-drag in a 2D view and the block is made at once, "
                                "one grid unit deep. In the top view it sits on the floor, or on top of what's selected; in the front and side "
                                "views it starts where the Block tool would. It comes out selected, ready for G, S or R.</p>"
                                "<p><b>Arch</b> makes several brushes: a half ring filling the box, its feet on the box's bottom and its crown "
                                "at the top, in the plane of the view you draw in (drawn in the front view it's a doorway arch; in the top "
                                "view, a curved wall). <b>Segments</b> sets how many brushes, <b>Thickness</b> how thick the ring is. They "
                                "come out grouped as \"Arch 1\".</p>"
                                "<p>New brushes are nodraw until you paint them.</p>");
        p.see = {QStringLiteral("grid"), QStringLiteral("vertices"), QStringLiteral("face")};
    }
    {
        GuidePage &p = page(QStringLiteral("selecting"), MK, QStringLiteral("Selecting"), QStringLiteral("select deselect selection box handles click ctrl"));
        p.keys = {{QStringLiteral("click"), QStringLiteral("Select a brush (its whole group, unless groups are ignored)")},
                  {QStringLiteral("Ctrl+click"), QStringLiteral("Add or remove a brush")},
                  {QStringLiteral("click again"), QStringLiteral("Switch between resize and rotate handles")},
                  {QStringLiteral("drag"), QStringLiteral("Select half: move what you grab. Box half: draw a selection box, even over brushes")},
                  {QStringLiteral("Ctrl+drag"), QStringLiteral("Box half: a box that adds to the selection")},
                  {QStringLiteral("D"), QStringLiteral("Deselect (in a 2D view; in 3D, D flies)")},
                  {QStringLiteral("H"), QStringLiteral("Hide the selection for now; H with nothing selected shows it again")},
                  {QStringLiteral("Ctrl+D"), QStringLiteral("Deselect, anywhere")},
                  {QStringLiteral("Ctrl+A"), QStringLiteral("Select all")},
                  {QStringLiteral("Esc"), QStringLiteral("Cancel what's in progress, else deselect")},
                  {QStringLiteral("Shift+Ctrl+drag"), QStringLiteral("3D view: paint a selection across brushes (or faces)")},
                  {QStringLiteral("Delete"), QStringLiteral("Delete the selection")}};
        p.html = QStringLiteral("<p>Deselecting can be undone with Ctrl+Z, so a stray D never loses a careful selection.</p>"
                                "<p>H only hides things for now: hidden brushes can't be clicked, aren't saved as hidden, and still "
                                "compile. Use a layer's Show box for something lasting.</p>");
        p.subs = {{QStringLiteral("Selection box"), QStringLiteral("<p>The Select button has two halves. The left half (or 1) is <b>point and click</b>: "
                                                                   "click to select, drag to move. The right half is <b>box select</b>: every drag draws a box, "
                                                                   "even one that starts on a brush. Click the right half again to switch its mode:</p><ul>"
                                                                   "<li><b>Instant</b> (lightning): what the box touches is selected as you drag, and the box stays "
                                                                   "up: drag its edges or move it and the selection follows. Delete, G, S, R, H and the mirror keys "
                                                                   "work on what it caught (the box steps aside).</li>"
                                                                   "<li><b>Select + Enter</b> (arrow): the box waits; hold <b>S</b> to resize it and <b>G</b> to move it, "
                                                                   "then Enter selects (Ctrl+Enter adds).</li></ul>"
                                                                   "<p>The box takes every brush it touches; the <b>Box</b> button under Selection Options "
                                                                   "switches it to only brushes fully inside.</p>")},
                  {QStringLiteral("Selection Options"), QStringLiteral("<p>Three toggles at the bottom of the tool panel: <b>Ignore Groups</b> (Ctrl+W), "
                                                                       "<b>Box: Touching / Inside</b>, and <b>Axis Lock</b> for G and Shift moves.</p>")},
                  {QStringLiteral("Resize handles"), QStringLiteral("<p>A selected brush shows square handles in the 2D views: drag one to resize "
                                                                    "from that edge or corner. Click the brush again for round rotate handles.</p>")},
                  {QStringLiteral("Painting a selection"), QStringLiteral("<p>In the 3D view, Shift+Ctrl+drag across brushes: starting on an unselected brush "
                                                                          "selects everything you sweep over, starting on a selected one deselects. Works in "
                                                                          "mouselook too, and on faces in Face Select &amp; Paint.</p>")}};
        p.see = {QStringLiteral("groups"), QStringLiteral("moving"), QStringLiteral("sizing")};
    }
    {
        GuidePage &p = page(QStringLiteral("moving"), MK, QStringLiteral("Moving & copying"), QStringLiteral("move copy paste cut clipboard clone duplicate nudge drag shift alt g grab mirror axis lock"));
        p.keys = {{QStringLiteral("drag"), QStringLiteral("Move the selection")},
                  {QStringLiteral("Alt+drag"), QStringLiteral("Move any brush, in any tool (slides along the floor in 3D)")},
                  {QStringLiteral("G (hold) + move"), QStringLiteral("Grab: move the selection from anywhere (2D views, or along the floor in 3D); release G to drop it")},
                  {QStringLiteral("Alt (while holding)"), QStringLiteral("Flip the axis lock for this move")},
                  {QStringLiteral("Shift+drag"), QStringLiteral("Drag out a copy")},
                  {QStringLiteral("Shift (hold) + move"), QStringLiteral("A copy follows the mouse; release Shift to place it")},
                  {QStringLiteral("arrows"), QStringLiteral("Nudge one grid step (in 3D, up moves away from you)")},
                  {QStringLiteral("Alt+arrows"), QStringLiteral("Nudge 1 unit")},
                  {QStringLiteral(", / ."), QStringLiteral("Mirror the selection horizontally / vertically (as the view under the mouse sees it)")},
                  {QStringLiteral("Ctrl+C / Ctrl+X"), QStringLiteral("Copy / cut the selection to the clipboard")},
                  {QStringLiteral("Ctrl+V"), QStringLiteral("Paste in the same spot (also into another model or project)")},
                  {QStringLiteral("Ctrl+Shift+V"), QStringLiteral("Paste under the mouse")},
                  {QStringLiteral("Esc"), QStringLiteral("Cancel the move or copy")}};
        p.html = QStringLiteral("<p><b>Copy and paste</b> work between models, projects and even two open Bonk windows, like Hammer. Groups come "
                                "along, pasted brushes land on the active layer, and textures this project doesn't have yet are copied in "
                                "from the project they came from.</p>"
                                "<p><b>Axis lock</b> (on by default): a G or Shift move picks the axis you first move along and stays on it "
                                "for the whole hold, which keeps things lined up. Bring the mouse back near where you started and it sits "
                                "exactly where it was. Alt while holding frees it for that move; the Axis Lock button under Selection "
                                "Options turns it off for good.</p>"
                                "<p>While a key is held, the corner of the view says what it's doing, the cursor shows a badge, and a "
                                "ripple marks where the brush landed when you let go.</p>"
                                "<p>While G, S, Shift or R is held in a 2D view (or you pan with the middle button), the cursor stays pinned "
                                "where it was, so you never run into the edge of the window; it comes back to the same spot when you let go. "
                                "Settings → Editing can turn pinning, and hold-Shift copying, off.</p>"
                                "<p>G also moves the Block tool's box before you create it, and the Origin tool's origin moves with the arrows.</p>"
                                "<p>Copies of a group become new groups named <i>Window (1)</i>, <i>Window (2)</i>…</p>");
        p.see = {QStringLiteral("grid"), QStringLiteral("rotating"), QStringLiteral("groups")};
    }
    {
        GuidePage &p = page(QStringLiteral("sizing"), MK, QStringLiteral("Sizing"), QStringLiteral("size resize scale stretch s edge corner"));
        p.keys = {{QStringLiteral("S (hold) + move"), QStringLiteral("Resize the selection from the nearest edge or corner, from anywhere in a 2D view")},
                  {QStringLiteral("S+arrows"), QStringLiteral("Move the top / left edge one grid step")},
                  {QStringLiteral("S+Shift+arrows"), QStringLiteral("Move the bottom / right edge instead")},
                  {QStringLiteral("Ctrl"), QStringLiteral("While sizing: keep proportions")},
                  {QStringLiteral("Alt"), QStringLiteral("Off the grid (S+Alt+arrows: 1 unit)")},
                  {QStringLiteral("Esc"), QStringLiteral("Cancel")}};
        p.html = QStringLiteral("<p>Hold <b>S</b> with brushes selected and move the mouse. Beside the brush grabs that side; past a corner grabs "
                                "that corner; inside, the outer thirds pick the side. The edges that will move are highlighted in bold yellow, "
                                "and the edge follows the mouse without jumping. Release S to finish.</p>"
                                "<p>The square handles of the Selection and Block tools resize too.</p>");
        p.see = {QStringLiteral("grid"), QStringLiteral("moving"), QStringLiteral("face")};
    }
    {
        GuidePage &p = page(QStringLiteral("rotating"), MK, QStringLiteral("Rotating"), QStringLiteral("rotate turn spin angle r snap degrees"));
        p.keys = {{QStringLiteral("R (hold) + move"), QStringLiteral("Rotate the selection: mouse right turns one way, left the other")},
                  {QStringLiteral("click again"), QStringLiteral("Selection tool: round rotate handles to drag")},
                  {QStringLiteral("Alt"), QStringLiteral("Rotate freely, without the angle snap")},
                  {QStringLiteral("Alt+Q"), QStringLiteral("Straighten to the nearest 45° and snap vertices to the grid")},
                  {QStringLiteral("Esc"), QStringLiteral("Cancel")}};
        p.html = QStringLiteral("<p>Rotation snaps in 15° steps (Settings → Editing changes it). Holding R works from anywhere in the 2D view: "
                                "about 180 pixels of mouse movement is a quarter turn. Groups turn about the group's center.</p>");
        p.see = {QStringLiteral("grid"), QStringLiteral("moving")};
    }
    {
        GuidePage &p = page(QStringLiteral("grid"), MK, QStringLiteral("Grid & snapping"), QStringLiteral("grid snap size alt wheel quantize bigger smaller"));
        p.keys = {{QStringLiteral("Alt+wheel"), QStringLiteral("Grid size: wheel up = bigger, wheel down = smaller")},
                  {QStringLiteral("[ / ]"), QStringLiteral("Smaller / bigger grid")},
                  {QStringLiteral("Alt+drag"), QStringLiteral("Place freely, off the grid")},
                  {QStringLiteral("Alt+Shift+drag"), QStringLiteral("Stay on the grid (Settings can swap these two)")},
                  {QStringLiteral("Alt+Q"), QStringLiteral("Quantize: snap vertices back to the grid, straighten rotation to 45°")}};
        p.html = QStringLiteral("<p>Everything snaps to the grid: drawing, moving, sizing, vertices and the selection box. The size shows in the "
                                "status bar and, for a moment, in the corner of the view when you change it. Settings → Grid sets how "
                                "strongly the lines show.</p>");
        p.see = {QStringLiteral("sizing"), QStringLiteral("moving")};
    }
    {
        GuidePage &p = page(QStringLiteral("view3d"), MK, QStringLiteral("3D view & flying"), QStringLiteral("camera fly mouselook orbit pan zoom wasd edges 2d views speed walk walk mode first person jump crouch fullscreen full screen f4 expand shift+z"));
        p.keys = {{QStringLiteral("Z"), QStringLiteral("Mouselook (flying) on / off")},
                  {QStringLiteral("W / A / S / D"), QStringLiteral("Fly, whenever the 3D view has the keys (mouselook or not)")},
                  {QStringLiteral("Q / E"), QStringLiteral("Down / up")},
                  {QStringLiteral("Shift"), QStringLiteral("Fly three times faster")},
                  {QStringLiteral("Alt"), QStringLiteral("Fly at half speed")},
                  {QStringLiteral("Shift+Z"), QStringLiteral("Expand the 3D view to the whole views' area (again: all four views)")},
                  {QStringLiteral("F4"), QStringLiteral("3D view full screen (again: back)")},
                  {QStringLiteral("`"), QStringLiteral("Walk mode")},
                  {QStringLiteral("right-drag"), QStringLiteral("Look around")},
                  {QStringLiteral("middle-drag"), QStringLiteral("Orbit around what's in the center of the view")},
                  {QStringLiteral("Shift+middle-drag"), QStringLiteral("Pan")},
                  {QStringLiteral("wheel"), QStringLiteral("3D: move forward / back. 2D: zoom")},
                  {QStringLiteral("Space+drag"), QStringLiteral("2D: pan (middle-drag too)")},
                  {QStringLiteral("Ctrl+E"), QStringLiteral("Center the 2D views on the selection")}};
        p.html = QStringLiteral("<p>In mouselook the mouse looks, left-click selects at the crosshair, and Z or Esc lets go.</p>"
                                "<p>Out of mouselook, WASD / QE still fly, and the 3D view takes the editing keys too: H, G (slides along the "
                                "floor), R (turns about the vertical), the mirror keys, and the arrows nudge. S sizing and Shift copying stay in the 2D views, "
                                "where there's a clear edge to grab.</p>"
                                "<p>Flying starts at a minimum speed and eases up to a maximum while you keep moving: Settings → Tools &amp; Keys → "
                                "<b>Flying</b> sets both and how quickly it speeds up.</p>"
                                "<p><b>Walk mode</b> (the <b>`</b> key) puts you in the model as the player: 72 units tall with eyes at 64, "
                                "WASD walks (Shift runs), Space jumps, Ctrl crouches, the mouse aims, and steps up to 18 units are "
                                "climbed. You walk on the collision model, so it's a quick test of what players will bump into. ` or "
                                "Esc ends it and puts the camera back. A model without collision has nothing to walk on, so ` does nothing.</p>"
                                "<p>View → 3D field of view sets how wide it sees (75° by default, up to 120°; View → Unlock goes further).</p>"
                                "<p>View → Show Edges in 3D turns the black brush outlines off for a cleaner look; selected brushes keep "
                                "their red borders.</p>");
        p.see = {QStringLiteral("selecting")};
    }
    {
        GuidePage &p = page(QStringLiteral("vertices"), MK, QStringLiteral("Vertices & Bisect"), QStringLiteral("vertex vertices edge bisect cut split clip delete"));
        p.keys = {{QStringLiteral("3"), QStringLiteral("Vertex tool")},
                  {QStringLiteral("drag"), QStringLiteral("Square handles move vertices, small yellow ones move edges")},
                  {QStringLiteral("Ctrl+click"), QStringLiteral("Add or remove a vertex")},
                  {QStringLiteral("Ctrl+A"), QStringLiteral("Select all vertices")},
                  {QStringLiteral("G / R / S (hold)"), QStringLiteral("With vertices selected: move them / turn them / scale them about their center")},
                  {QStringLiteral("4 / B"), QStringLiteral("Bisect tool; again cycles Split → Split & Delete smaller side → larger side")},
                  {QStringLiteral("F"), QStringLiteral("Flip the pending cut: the other side goes (Split: what stays selected)")},
                  {QStringLiteral("Enter"), QStringLiteral("Make the cut")},
                  {QStringLiteral("Shift (hold)"), QStringLiteral("Flip while you press Enter")}};
        p.html = QStringLiteral("<p><b>Vertex tool:</b> click a brush to edit it. Stacked vertices in a 2D view move together; drag on empty "
                                "space to box-select. Moves that would make the brush concave are refused.</p>"
                                "<p><b>Vertex select</b> (the ↖ at the end of the Vertex button): clicks and boxes pick vertices on any brush, "
                                "several brushes at once (Ctrl adds), and nothing moves until you say so: hold <b>G</b> to move them, <b>R</b> "
                                "to turn them, <b>S</b> to scale them (away from their center grows, toward it shrinks; Alt for smooth "
                                "steps), or nudge with the arrows. A brush that would turn concave or flat snaps back; the others "
                                "still change.</p>"
                                "<p><b>Bisect:</b> draw a cut line across the selected brushes. Its ends snap to the grid and to edge "
                                "midpoints; drag the yellow ends to adjust.</p><ul>"
                                "<li><b>Split</b> keeps both pieces. F picks what stays selected: both, the smaller piece or the larger one.</li>"
                                "<li><b>Split &amp; Delete</b> removes the smaller side (red), or the larger side in the third mode. "
                                "F flips it for this cut; the next cut starts from the mode again.</li></ul>"
                                "<p>The new faces along a cut are always nodraw: paint the ones that end up showing.</p>");
        p.see = {QStringLiteral("grid")};
    }

    // ------------------------------------------------------------ Guides
    {
        GuidePage &p = page(QStringLiteral("projects"), GD, QStringLiteral("Projects & folders"), QStringLiteral("project folder new open recent model path assets addon"));
        p.keys = {{QStringLiteral("Ctrl+Alt+N"), QStringLiteral("New project")},
                  {QStringLiteral("Ctrl+Alt+O"), QStringLiteral("Open project")},
                  {QStringLiteral("Ctrl+N / Ctrl+O"), QStringLiteral("New / open model")},
                  {QStringLiteral("Ctrl+Shift+O"), QStringLiteral("Reopen the last model")},
                  {QStringLiteral("Ctrl+P"), QStringLiteral("Model Properties (the game path)")}};
        p.html = QStringLiteral("<p>A project is a folder holding everything for one addon:</p><ul>"
                                "<li><b>assets/Textures</b>: your PNG and JPG files (subfolders organize them)</li>"
                                "<li><b>Model Projects</b>: your .bpmdl models</li>"
                                "<li><b>addon/&lt;project&gt;</b>: the finished addon: models, VMTs, VTFs, addon.json</li></ul>"
                                "<p>File → Recent Models lists the last ten models as <i>model (project)</i>; opening one opens its project "
                                "too. The start screen has the same list on its Recent models tab.</p>");
        p.see = {QStringLiteral("importing"), QStringLiteral("textures"), QStringLiteral("compiling")};
        p.subs = {{QStringLiteral("Model path"), QStringLiteral("<p>File → Model Properties sets where the model goes in the game, e.g. "
                                                                "<tt>housepack/house_1</tt> becomes models/housepack/house_1.mdl. A new model gets one from "
                                                                "the project's base path the first time it's saved.</p>")}};
        p.see = {QStringLiteral("textures"), QStringLiteral("compiling")};
    }
    {
        GuidePage &p = page(QStringLiteral("importing"), GD, QStringLiteral("Importing from Hammer"),
                            QStringLiteral("import vmf hammer map materials vmt vtf gma workshop addon visgroup source textures"));
        p.keys = {{QStringLiteral("Ctrl+I"), QStringLiteral("Import VMF…")}};
        p.html = QStringLiteral("<p><b>File → Import VMF…</b> brings a Hammer map's brushes in, with their textures lined up exactly as they "
                                "were. It asks how:</p><ul>"
                                "<li><b>Create New Project</b>: a project named after the map, with the map as its first model.</li>"
                                "<li><b>Add to Existing Project</b>: the map becomes a new model in the open (or a chosen) project.</li>"
                                "<li><b>Just Open It</b>: the brushes only, as an unsaved model.</li></ul>"
                                "<p>If the map has visgroups you can untick the ones you don't want; each one becomes a layer. Hammer groups "
                                "become groups. The import is moved so its bottom center sits on the model origin.</p>");
        p.subs = {{QStringLiteral("Materials"), QStringLiteral("<p>With a project, Bonk then asks where the map's materials are: a <b>materials</b> folder "
                                                               "(e.g. sourcemods/hl2/materials), an addon folder with one inside, or a Workshop "
                                                               "<b>.gma</b> straight from Steam's downloads. Choose <i>Only what this model uses</i> or "
                                                               "<i>Everything in it</i>. Each VTF is decoded to a PNG in assets/Textures, in the same "
                                                               "folders as the original, and each VMT's settings become its Material Properties. "
                                                               "Map-only settings are dropped; ones Bonk has no control for go into the extra lines.</p>"
                                                               "<p><b>File → Import Source Textures…</b> runs the same step any time, for example "
                                                               "after Just Open It. Big imports (stock HL2) say how big they are first.</p>")},
                  {QStringLiteral("Tool textures"), QStringLiteral("<p>TOOLSNODRAW stays nodraw. Clip brushes become a <b>collision layer</b> "
                                                                   "(hidden at first, since clips usually wrap everything). Skybox, hint, skip "
                                                                   "and trigger brushes are left out.</p>")},
                  {QStringLiteral("What's skipped"), QStringLiteral("<p>Displacements, point entities (lights, spawns, prop_static) and trigger "
                                                                    "or logic brush entities. A summary after the import says what was left out.</p>")}};
        p.see = {QStringLiteral("projects"), QStringLiteral("material"), QStringLiteral("collision")};
    }
    {
        GuidePage &p = page(QStringLiteral("textures"), GD, QStringLiteral("Textures & the Picker"), QStringLiteral("texture picker swatch import folder color resize zoom convert vtf drop dolphin hide other models filter"));
        p.keys = {{QStringLiteral("T"), QStringLiteral("Show / hide the Texture Picker")},
                  {QStringLiteral("click"), QStringLiteral("Make a swatch the current texture")},
                  {QStringLiteral("double-click"), QStringLiteral("Texture: Material Properties. Folder: open it")},
                  {QStringLiteral("Alt+click"), QStringLiteral("3D view: pick up a face's texture")},
                  {QStringLiteral("Ctrl+wheel"), QStringLiteral("Zoom the swatches")},
                  {QStringLiteral("right-click"), QStringLiteral("Select faces using it, replace it, cut / copy / paste, rename, delete, new folder")},
                  {QStringLiteral("drop"), QStringLiteral("PNG / JPG files or folders from Dolphin: import them")}};
        p.html = QStringLiteral("<p><b>Apply</b> puts the current texture on the selected faces, or on every face of the selected brushes. "
                                "Clicking a swatch also switches to Face Select &amp; Paint (Settings can turn that off).</p>");
        p.subs = {{QStringLiteral("Folders"), QStringLiteral("<p>The picker works like a file manager: double-click a folder to open it, use the "
                                                             "breadcrumb to go back, drag swatches onto folders to move them. Moving or renaming a texture "
                                                             "updates every model in the project that uses it.</p>"
                                                             "<p><b>Hide other models' textures</b> leaves out textures only the project's other models "
                                                             "use (this model's own stay), and the models/ folder (other models' skins).</p>")},
                  {QStringLiteral("Animated textures"), QStringLiteral("<p><b>Import Animation…</b> makes a texture that plays frames: pick several "
                                                                       "images (they're used in name order, frame2 before frame10), one sprite sheet (you say how "
                                                                       "many columns and rows), or a GIF (its speed comes along). The frames are kept as one tall "
                                                                       "image with a ▶ badge, play in the 3D view, and compile to a multi-frame VTF with an "
                                                                       "AnimatedTexture proxy. Material Properties → Animation sets the frame rate.</p>")},
                  {QStringLiteral("Color textures"), QStringLiteral("<p>The Color tile makes a solid color as a texture (a tiny 16×16 PNG in "
                                                                    "colors/, named by its hex code; rename it if you like). Compiled, its material is "
                                                                    "named after the model, <tt>house_colors_ffffff</tt>, so two models' colors "
                                                                    "never share a file.</p>")},
                  {QStringLiteral("Resize offer"), QStringLiteral("<p>Source needs power-of-two sizes (64, 128 … 2048, 4096; width and height can differ). "
                                                                  "Images that aren't are offered a resize to the nearest one; the originals go to "
                                                                  "assets/Original Textures. Power-of-two images are never touched, whatever their size.</p>")},
                  {QStringLiteral("Converting to VTF"), QStringLiteral("<p>Convert All writes VTFs into the addon folder. Compiling converts whatever "
                                                                       "the model uses, so you rarely need to. A banner warns when an image changed "
                                                                       "after its VTF was made.</p>")}};
        p.see = {QStringLiteral("face"), QStringLiteral("material")};
    }
    {
        GuidePage &p = page(QStringLiteral("face"), GD, QStringLiteral("Face Select & Paint"), QStringLiteral("face paint texture align shift scale rotate justify wrap lock fit nodraw new brush"));
        p.keys = {{QStringLiteral("click"), QStringLiteral("Select a face (Ctrl adds / removes); its texture becomes current")},
                  {QStringLiteral("right-click"), QStringLiteral("Paint the current texture, aligned like the source face (at the crosshair while flying)")},
                  {QStringLiteral("right-drag"), QStringLiteral("Paint every face you sweep across, in one undo step (flying too: look around while holding it)")},
                  {QStringLiteral("Shift+right-click"), QStringLiteral("Paint the whole brush")},
                  {QStringLiteral("Alt+right-click"), QStringLiteral("Wrap on as one continuous sheet (that face becomes the next source)")},
                  {QStringLiteral("drag"), QStringLiteral("Move the texture on the selected faces")},
                  {QStringLiteral("Ctrl+drag"), QStringLiteral("Rotate it (15° steps)")},
                  {QStringLiteral("Shift+drag"), QStringLiteral("Scale it")},
                  {QStringLiteral("Alt"), QStringLiteral("No snapping while dragging")}};
        p.html = QStringLiteral("<p>New brushes start out <b>nodraw</b>: they're invisible in game and cost nothing until you paint "
                                "the faces that should show (they wear the nodraw stripes in the 3D view until then).</p>"
                                "<p>The picker's Face section shows the selected face's texture and name, and has shift, scale and rotate "
                                "fields, justify buttons, Stretch and Reset.</p>"
                                "<p><b>Select All Using</b> selects every face in the model that uses that texture. <b>Replace…</b> swaps a "
                                "texture for another, everywhere in the model or only on the selected faces; alignment stays. Both are on a "
                                "swatch's right-click menu too.</p>");
        p.subs = {{QStringLiteral("Scale"), QStringLiteral("<p>Texture scale on a face: the Scale fields, or Shift+drag. 0.25 is Hammer's default.</p>")},
                  {QStringLiteral("Treat as one"), QStringLiteral("<p>With several faces selected, Treat as one aligns and justifies them as if they were one big face.</p>")},
                  {QStringLiteral("Texture Lock"), QStringLiteral("<p>Texture Lock keeps textures in place on the brush as you move it; Scale texture with brush "
                                                                  "stretches them as you size it.</p>")}};
        p.see = {QStringLiteral("textures"), QStringLiteral("material")};
    }
    {
        GuidePage &p = page(QStringLiteral("material"), GD, QStringLiteral("Material Properties"), QStringLiteral("material vmt shader surfaceprop transparent alpha reflection envmap phong normal bump detail mask preset override scroll pulse sine animated proxy monitorscreen"));
        p.html = QStringLiteral("<p>Double-click a texture in the picker to set what goes into its VMT, in four tabs:</p><ul>"
                                "<li><b>Basics:</b> preset (monitor, sign, glass), shader (VertexLitGeneric, UnlitGeneric, MonitorScreen), "
                                "surface type (footsteps, bullet impacts), transparency, double-sided, glow and its mask.</li>"
                                "<li><b>Maps:</b> normal map, reflections and their mask (or a mask texture), phong shine and a sharpness map, "
                                "and a detail texture with its scale, strength and blend mode.</li>"
                                "<li><b>Animation:</b> scrolling (TextureScroll: speed and direction), pulsing (Sine: brightness or "
                                "transparency between Low and High over a period) and the frame rate of an imported animation. They all "
                                "play in the 3D view.</li>"
                                "<li><b>Extra:</b> lines that go straight into the VMT.</li></ul>"
                                "<p>The VMT on the right is live, and you can type into it: from then on your text is used as is (Back to "
                                "Generated returns to the settings).</p>"
                                "<p>Settings are kept next to the image in a .bonkmat file, so they travel with the texture. <b>Only this model</b> "
                                "stores an override inside the open model instead.</p>");
        p.subs = {{QStringLiteral("Hand-edited VMTs"), QStringLiteral("<p>Bonk writes the VMTs when it compiles. If you edit one by hand, Bonk notices "
                                                                      "and leaves it alone from then on; delete it to let Bonk write it again.</p>")}};
        p.see = {QStringLiteral("compiling"), QStringLiteral("textures")};
    }
    {
        GuidePage &p = page(QStringLiteral("variants"), GD, QStringLiteral("Skins, bodygroups & LODs"),
                            QStringLiteral("variants skin skins texturegroup bodygroup bodygroups option none lod lods level of detail preview model preview hide"));
        p.keys = {{QStringLiteral("F7"), QStringLiteral("Model Preview")}};
        p.html = QStringLiteral("<p>The <b>Variants</b> tab (beside Layers, the Texture Picker and Prefabs) makes one model into several: skins "
                                "players pick in game, bodygroups they switch, and simpler versions for a distance.</p>");
        p.subs = {{QStringLiteral("Skins"), QStringLiteral("<p>Skin 0 is the model as painted. <b>+ Skin</b> adds one (a copy of the skin picked). "
                                                           "While a skin is picked, the views show it, and <b>painting swaps materials in that "
                                                           "skin</b>: every face with that material shows the new one, since Source skins swap "
                                                           "whole materials. The swaps are listed; select one and press Delete to undo it.</p>"
                                                           "<p>A skin can also <b>hide brushes</b>: select them and press <b>Hide Selection</b> "
                                                           "(Show Selection and Show All bring them back). Bonk exports them with their own "
                                                           "copies of their materials, which that skin swaps for an invisible one.</p>")},
                  {QStringLiteral("Bodygroups"), QStringLiteral("<p>Group the brushes for each choice (Ctrl+G), select a group and press "
                                                                "<b>Make Group an Option…</b>, naming the bodygroup (Door, say). Each group "
                                                                "marked with the same bodygroup is one of its options, named after the group. "
                                                                "<b>+ None Option</b> lets players show nothing. Click an option in the list "
                                                                "to show it in the views; the others are hidden while you work. Brushes in no "
                                                                "option are the model's body, always there.</p>")},
                  {QStringLiteral("Levels of detail"), QStringLiteral("<p><b>+ LOD</b> adds a simpler version: <b>Switch at</b> is studiomdl's "
                                                                      "$lod distance (bigger is farther; 30 and 60 are usual), <b>Drop under</b> "
                                                                      "leaves out brushes smaller than that many units across. The list shows "
                                                                      "how many triangles each keeps.</p>")},
                  {QStringLiteral("Model Preview"), QStringLiteral("<p><b>View → Model Preview</b> (F7) shows the model as it compiles: "
                                                                   "pick a skin, each bodygroup's option and a LOD; drag to turn it, "
                                                                   "right-drag to move, wheel to zoom. It updates as you edit.</p>")}};
        p.see = {QStringLiteral("groups"), QStringLiteral("face"), QStringLiteral("compiling")};
    }
    {
        GuidePage &p = page(QStringLiteral("prefabs"), GD, QStringLiteral("Prefabs"),
                            QStringLiteral("prefab prefabs reuse library furniture save selection drop drag place keep height floor shared project"));
        p.html = QStringLiteral("<p>A prefab is a set of brushes you've saved to use again: a window, a shelf, a whole room. Select the "
                                "brushes and use <b>Save Selection as Prefab…</b> (on the Prefabs tab or in the Edit menu): give it a name, "
                                "a folder if you like (<tt>Furniture/Kitchen</tt>), and where it goes.</p>"
                                "<p>To use one, <b>drag it from the Prefabs tab into a view</b>, or double-click it to drop it under the mouse. "
                                "It arrives selected, so G moves it straight away; one undo takes it back.</p>");
        p.subs = {{QStringLiteral("Shared or this project"), QStringLiteral("<p><b>Shared</b> prefabs work in every project: they're kept in "
                                                                            "~/.local/share/bonk/prefabs with copies of their textures, which are "
                                                                            "copied into a project the first time you place one there. "
                                                                            "<b>This project</b>'s prefabs live in the project's Prefabs folder and "
                                                                            "use its textures. The tab shows both.</p>")},
                  {QStringLiteral("Keep height above floor"), QStringLiteral("<p>On (the default), a prefab remembers how high it was above "
                                                                             "whatever was under it when you saved it: a shelf saved 32 units up "
                                                                             "lands 32 units above the floor or table you drop it on. Off, it sits "
                                                                             "right on the surface. In the 3D and top views it lands on what's under "
                                                                             "the mouse; in the front and side views it goes where the mouse is.</p>")},
                  {QStringLiteral("Organizing"), QStringLiteral("<p>Right-click a prefab to rename it, move it to another folder, retake its "
                                                                "thumbnail, show it in your file manager or delete it. The filter box finds "
                                                                "prefabs by name or folder; the Size slider sets the thumbnail size.</p>")}};
        p.see = {QStringLiteral("moving"), QStringLiteral("textures")};
    }
    {
        GuidePage &p = page(QStringLiteral("groups"), GD, QStringLiteral("Layers & Groups"), QStringLiteral("layer group ungroup ignore visual collision origin ghost"));
        p.keys = {{QStringLiteral("Ctrl+G"), QStringLiteral("Group the selection (groups can hold groups)")},
                  {QStringLiteral("Ctrl+U"), QStringLiteral("Ungroup one level")},
                  {QStringLiteral("Ctrl+W"), QStringLiteral("Ignore groups: clicks pick single brushes")}};
        p.html = QStringLiteral("<p><b>Layers:</b> click a layer to make it active (new brushes go there), double-click to select its contents, "
                                "right-click (or F2) to set its name, delete it, or move / copy the selection onto it. A Visual or Collision "
                                "heading with one layer under it names that layer too. The Show box hides a layer. "
                                "Right-click → Own Origin gives a layer its own origin.</p>"
                                "<p><b>Groups</b> appear under their layer: click to select (it stays highlighted), double-click or F2 to "
                                "rename, right-click to duplicate or ungroup. Selecting a whole group in a view highlights it here too. Folded "
                                "layers and headings stay folded. A group on a hidden, ghosted or locked layer can't be "
                                "selected; the status bar says so.</p>");
        p.see = {QStringLiteral("collision"), QStringLiteral("selecting")};
    }
    {
        GuidePage &p = page(QStringLiteral("collision"), GD, QStringLiteral("Collision models"), QStringLiteral("collision physics phy convex pieces ghost layer"));
        p.html = QStringLiteral("<p>The collision model is what Garry's Mod uses for physics: picking the prop up, bullets, things landing on it. "
                                "<b>+ Collision</b> in the Layers pane (or Tools → New Collision Layer) starts one as <b>Complex</b>, "
                                "a copy of every visual brush. In Advanced mode it asks first, and offers <b>Simplified</b>: walls, floors, roofs and stairs stay (stairs become "
                                "ramps), windows with a pane or frame are filled in solid, open doorways stay open, loose little details go, "
                                "and pieces that line up are merged; corners stay square. The Detail size (Grid 8 by default) sets what counts as little; the "
                                "dialog shows how many pieces each way gives. While a collision layer is active, the visual model "
                                "is a see-through ghost (the slider sets how visible) and the 3D view tells you you're editing collision.</p>"
                                "<p>Each collision brush is one convex piece. Keep them few and simple: studiomdl allows 20 by default "
                                "(Bonk raises the limit when it needs to). Compiling notes anything over 128 pieces; under 500 that has "
                                "worked fine in testing, from 500 up it warns that studiomdl may refuse.</p>"
                                "<p>Try it out with walk mode (the <b>`</b> key): you walk on the collision model.</p>"
                                "<p><b>Collision look</b> (View menu, or the button under the layers): <i>Ghosted</i> draws collision as orange "
                                "glass; <i>Solid</i> draws each piece opaque in its own color, so you can see where one ends and the next "
                                "begins. It only changes the look.</p>"
                                "<p><b>J</b> (Edit → Join Brushes) turns the selected brushes into one: exactly their shape when "
                                "they make one convex brush together, otherwise a hull around them (Settings → Editing can make J "
                                "exact only). Handy for cutting collision pieces down.</p>"
                                "<p>No collision layer when you compile? Bonk offers to <b>Generate</b> one (complex or simplified) from the visual brushes, "
                                "or to compile without physics.</p>"
                                "<p>While you edit collision, its brushes get thick two-tone outlines; far from the camera they're heavier still, "
                                "so pieces don't blend together when you look at a whole house.</p>");
        p.see = {QStringLiteral("budget"), QStringLiteral("compiling")};
    }
    {
        GuidePage &p = page(QStringLiteral("budget"), GD, QStringLiteral("Budget"), QStringLiteral("budget limit triangles vertices materials pieces meter over overlap overlapping duplicate"));
        p.html = QStringLiteral("<p>The Budget bar along the bottom shows what the model costs, live, against Garry's Mod's studiomdl limits:</p><ul>"
                                "<li><b>Triangles</b> of the busiest material (10,922 each: 32,768 indices)</li>"
                                "<li><b>Vertices</b> (65,536)</li><li><b>Materials</b> (64)</li><li><b>Collision pieces</b>: no hard limit (Bonk "
                                "raises studiomdl's); past 1,200 it turns yellow: an expensive collision mesh, but the model may still work fine</li></ul>"
                                "<p>Hover a meter for the heaviest brushes, click it to select them. Nodraw faces cost nothing, so nodraw "
                                "faces nobody will see. Bonk warns before compiling something over budget. The limits are in Settings.</p>"
                                "<p><b>Tools → Find Overlapping Brushes</b> lists brushes that share space (touching is fine), exact duplicates "
                                "first. Click one to select the pair and center the views on it. Visual and collision brushes are checked "
                                "separately.</p>");
        p.see = {QStringLiteral("collision"), QStringLiteral("compiling")};
    }
    {
        GuidePage &p = page(QStringLiteral("compiling"), GD, QStringLiteral("Compiling & exporting"), QStringLiteral("compile export studiomdl proton mdl qc smd addon gmod garrysmod workshop"));
        p.keys = {{QStringLiteral("F9"), QStringLiteral("Compile the model")},
                  {QStringLiteral("Ctrl+Shift+E"), QStringLiteral("Export to the GMod addons folder")}};
        p.html = QStringLiteral("<p><b>File → Compile Model</b> writes the model's SMDs and QC, converts the textures it uses to VTF, writes "
                                "its VMTs, then runs Garry's Mod's own <tt>studiomdl.exe</tt> through Proton. The result lands in the project's "
                                "<tt>addon/&lt;project&gt;</tt> folder, ready for the Workshop.</p>"
                                "<p><b>File → Export to GMod Addons Folder</b> copies that folder to <tt>garrysmod/addons/&lt;project&gt;</tt> "
                                "so you can try it in game. It asks before replacing an earlier export. Settings can export after every compile, "
                                "and start Garry's Mod after exporting (also File → Start Garry's Mod) so you can test right away.</p>");
        p.subs = {{QStringLiteral("Garry's Mod folder"), QStringLiteral("<p>Bonk finds Garry's Mod through Steam's library folders (Flatpak Steam too). It "
                                                                        "only uses that folder for three things: running bin/studiomdl.exe, writing "
                                                                        "garrysmod/addons/&lt;project&gt; when you export, and running gmad / "
                                                                        "gmpublish when you publish from Manage Workshop. Settings → Garry's Mod can point "
                                                                        "it elsewhere.</p>")},
                  {QStringLiteral("Proton"), QStringLiteral("<p>Bonk uses the newest stable Proton Steam has installed, with its own prefix in "
                                                            "~/.local/share/bonk/proton (the first compile sets it up, which takes a minute). "
                                                            "Settings → Garry's Mod picks a different Proton.</p>")},
                  {QStringLiteral("Compiler"), QStringLiteral("<p>Settings → Garry's Mod → Compiler can point at another studiomdl.exe, such as "
                                                              "StudioMDL CE; it runs through Proton the same way. Empty means Garry's Mod's own.</p>")},
                  {QStringLiteral("Mass"), QStringLiteral("<p>Tools → Set Mass… sets how heavy the prop is in game ($mass): automatic, a number in "
                                                          "kg, or a preset from Feather-light (1 kg) to Very Heavy (house, 50,000 kg, Source's maximum).</p>")},
                  {QStringLiteral("When it fails"), QStringLiteral("<p>The compile window shows studiomdl's errors in plain words; tick Show studiomdl's "
                                                                   "log for everything. Too many collision pieces or vertices show up in the Budget bar first.</p>")},
                  {QStringLiteral("Which way it faces"), QStringLiteral("<p>The side of the model facing <b>south</b> in the top view (toward the "
                                                                        "bottom of the screen) is the side that faces you when you spawn the prop.</p>")}};
        p.see = {QStringLiteral("budget"), QStringLiteral("material"), QStringLiteral("collision"), QStringLiteral("spawnlist"), QStringLiteral("workshop")};
    }
    {
        GuidePage &p = page(QStringLiteral("workshop"), GD, QStringLiteral("Publishing to the Workshop"),
                            QStringLiteral("workshop publish upload steam gmpublish gmad gma update thumbnail tags type change notes"));
        p.html = QStringLiteral("<p><b>File → Manage Workshop…</b> publishes the project's addon to the Steam Workshop. Steam has to be "
                                "running and signed in; the first time, Bonk asks before using your Steam profile, which it only uses to "
                                "list your Workshop addons and to upload the ones you publish from here.</p>"
                                "<p>Your addons are on the left, with their thumbnails and when they were last updated. On the right:</p>"
                                "<ul><li><b>New submission</b>: a thumbnail, the title and description, the type (Model) and up to two tags. "
                                "<b>Publish to Workshop</b> packs the addon with gmad and uploads it with gmpublish; the project remembers "
                                "its Workshop ID.</li>"
                                "<li><b>Update</b>: click an addon on the left (the project's own is picked for you). Write change notes, and "
                                "tick <i>Replace the thumbnail too</i> to send a new one. Title, description, type and tags are changed on "
                                "the Workshop page.</li></ul>"
                                "<p>When it's done, Bonk opens the addon's Workshop page in Steam (or your browser) so you can see it's up; "
                                "turn that off in Settings → Garry's Mod. New items start out hidden on the Workshop: make yours public from "
                                "its page when you're ready.</p>");
        p.subs = {{QStringLiteral("Thumbnails"), QStringLiteral("<p>The Workshop wants a 512 × 512 JPG. <b>Choose Image…</b> converts any PNG or "
                                                                "JPG; <b>Snap from Model…</b> takes a picture of one of the project's models with "
                                                                "the spawn icon camera (drag to turn, right-drag to move, wheel to zoom), on a "
                                                                "background color, with a floor grid if you like. <b>Use Spawn Icon Angle</b> "
                                                                "reuses the angle from the Spawnlist Editor. It's kept as Workshop/thumbnail.jpg "
                                                                "in the project.</p>")},
                  {QStringLiteral("Before publishing"), QStringLiteral("<p>Bonk checks the addon first: compiled models, a compile newer than "
                                                                       "the models, addon.json, the spawnlist, and files the Workshop doesn't "
                                                                       "accept (those are left out).</p>")},
                  {QStringLiteral("The tools"), QStringLiteral("<p>gmad and gmpublish come with Garry's Mod. Bonk uses its Linux builds when "
                                                               "it has them, otherwise the Windows ones through Proton, like studiomdl. The "
                                                               "publishing window shows their output. Under Proton they only print "
                                                               "inside a terminal, so Bonk runs them in one with <i>script</i> (part of "
                                                               "util-linux, on nearly every Linux). If your addons don't list, the "
                                                               "<i>details</i> link under the list shows what gmpublish said.</p>")}};
        p.see = {QStringLiteral("compiling"), QStringLiteral("spawnlist")};
    }
    {
        GuidePage &p = page(QStringLiteral("spawnlist"), GD, QStringLiteral("Spawnlists & spawn icons"),
                            QStringLiteral("spawnlist spawn menu spawnicon icon thumbnail category header silk icon16 lua props badge addons creator standalone"));
        p.html = QStringLiteral("<p><b>Tools → Spawnlist Editor…</b> builds the project's spawn menu entry: categories (with their own "
                                "icons), headers, and your models with spawn icons you frame yourself. It goes into the addon as a small Lua "
                                "file, so it works from the Workshop too: players find your props under Spawnlists, like any pack.</p>"
                                "<p>Every project starts with one spawnlist named after it. <b>+ Subcategory</b> nests one under the selected "
                                "category; <b>+ Spawnlist</b> adds another top-level list, if you want more than one. Drag categories to "
                                "reorder or nest them; double-click to rename.</p>"
                                "<p>The middle pane is the category's page, laid out as in game. <b>Add Models…</b> picks from the project's "
                                "models (in Advanced mode its <b>From Addons</b> tab adds compiled models from any addon folder you choose, "
                                "with spawn icons drawn from the models and their textures), <b>Add Header</b> adds a line like \"Beds - Full\". Drag tiles and headers to rearrange them, or "
                                "drop them on a category on the left to move them there (hold Ctrl to copy). ◀ ▶ (or Ctrl+←/→) "
                                "move the selection too; Delete removes it.</p>");
        p.subs = {{QStringLiteral("Spawn icons"), QStringLiteral("<p>Click a model to frame its icon: drag to turn it, right-drag (or Shift+drag) to "
                                                                 "move it in the frame, wheel to zoom. <b>Fit</b> frames the whole model snugly from "
                                                                 "the current angle; <b>Use for Whole Category</b> gives every model in the category "
                                                                 "the same angle and size. The icon has a transparent background, like GMod's.</p>"
                                                                 "<p><b>Tile size</b> is the icon's size in the spawn menu, wide × tall: 64 × 64 is GMod's "
                                                                 "usual, 128 or 256 suits big models, and a wide tile (256 × 128) suits a long house. "
                                                                 "Pick a preset or type any size; a new shape frames the model again. Icons are drawn from the saved models, so Bonk saves "
                                                                 "your model before opening the editor.</p>")},
                  {QStringLiteral("Badges"), QStringLiteral("<p>A model's icon can carry a <b>badge</b>: a built-in shape, a Silk icon or an "
                                                            "image, up to 32 × 32 on a 64 × 64 icon (bigger tiles scale it), in the corner you "
                                                            "pick. A star for what's new, a lock for what's done.</p>")},
                  {QStringLiteral("Spawnlist Creator"), QStringLiteral("<p><b>Bonk! Spawnlist Creator</b> (its own launcher, or <tt>bonk --spawnlist</tt>) "
                                                                       "is this editor on its own, for any addon folder with models/ and materials/ "
                                                                       "in it: categories, headers, tile sizes, icons and badges for its compiled "
                                                                       "models. <b>Write Spawnlist</b> puts the Lua, category icons and spawn icons "
                                                                       "into that addon folder; the Creator's own notes stay in "
                                                                       "~/.local/share/bonk/spawnlist-creator.</p>")},
                  {QStringLiteral("Category icons"), QStringLiteral("<p>Pick a base: a built-in shape, a Silk icon or an image file; tint it; and add "
                                                                    "a sub-icon in a corner (a star on the house, say). The preview shows it at the "
                                                                    "game's 16 × 16 pixels. A plain Silk icon is GMod's own; anything else is written "
                                                                    "into the addon as a PNG.</p>")},
                  {QStringLiteral("Silk icons"), QStringLiteral("<p>GMod's category icons are the famfamfam Silk set (icon16). Bonk doesn't include "
                                                                "it, to stay small: download Silk (famfamfam.com, CC BY 2.5), unzip it, and use "
                                                                "<b>Import Silk Icons…</b> on its icons folder. Bonk keeps a copy in "
                                                                "~/.local/share/bonk/silk for every project.</p>")},
                  {QStringLiteral("What's written"), QStringLiteral("<p>Compiling (and exporting, or <b>Write to Addon</b>) writes "
                                                                    "<tt>lua/autorun/client/bonk_spawnlist_&lt;project&gt;.lua</tt>, the category icons in "
                                                                    "<tt>materials/&lt;project&gt;/spawnlist</tt>, and each model's spawn icon in "
                                                                    "<tt>materials/spawnicons/models/…</tt>: the 64 × 64 one, plus "
                                                                    "<tt>&lt;name&gt;_&lt;size&gt;.png</tt> for bigger square tiles or "
                                                                    "<tt>&lt;name&gt;_&lt;wide&gt;x&lt;tall&gt;.png</tt> for other shapes. The spawnlist itself is kept in "
                                                                    "the project as <tt>spawnlist.json</tt>.</p>"
                                                                    "<p>If GMod shows an icon of its own for a model, it drew one earlier and keeps it in "
                                                                    "garrysmod/materials/spawnicons; delete that file to see yours.</p>")}};
        p.see = {QStringLiteral("compiling"), QStringLiteral("projects")};
    }
    {
        GuidePage &p = page(QStringLiteral("settings"), GD, QStringLiteral("Settings"),
                            QStringLiteral("settings preferences interface size compact low resolution pixel font bonk pixel nodraw stripe rainbow background colors hammer keys tabs simple advanced mode fly speed lerp"));
        p.keys = {{QStringLiteral("Ctrl+Shift+,"), QStringLiteral("Configure Bonk!")}};
        p.html = QStringLiteral("<p>Settings come in tabs: <b>Editing</b>, <b>Tools & Keys</b>, <b>Appearance</b>, <b>Garry's Mod</b> and "
                                "<b>Advanced</b> (the budget limits). It opens on the tab you used last. Changes preview live; Cancel puts "
                                "everything back.</p>");
        p.subs = {{QStringLiteral("Advanced mode"), QStringLiteral("<p>Editing → <b>Advanced mode</b>: off (Simple, the default) hides Vertex select, the "
                                                                   "Origin tool, Axis Lock, the selection box options, Prefabs and Variants, Import VMF, the "
                                                                   "Visual ghost and Collision look, Simplified collision and addon models in the Spawnlist "
                                                                   "Editor. On shows all of it.</p>")},
                  {QStringLiteral("Flying"), QStringLiteral("<p>Tools &amp; Keys → Flying: the speed a move starts at, the speed it eases up to, and how "
                                                            "quickly (the lerp: 0 stays at the minimum, about 3 takes a second, 20 and up is almost "
                                                            "instant). Alt halves it while held, Shift triples it.</p>")},
                  {QStringLiteral("Interface size"), QStringLiteral("<p>Appearance → Interface size. Auto makes the interface compact on screens under "
                                                                    "1440p and regular above. <b>Compact (Low Resolution)</b> goes further for small "
                                                                    "screens: tighter panels and <b>Bonk Pixel</b>, Bonk's own pixel font, so every letter "
                                                                    "lands on whole pixels. Pick its size: 6, 12 or 18 px. The preview shows it at "
                                                                    "actual size and magnified. Takes effect after a restart.</p>")},
                  {QStringLiteral("Nodraw look"), QStringLiteral("<p>Nodraw faces get stripes: a solid color, an RGB cycle, or a rainbow flow, with "
                                                                 "intensity and speed. <b>Stripe length</b> sets how long each stripe is, and the gap "
                                                                 "after it, so longer bars come with longer gaps. <b>Rainbow flow length</b> sets how far "
                                                                 "one whole rainbow stretches. Stripes are hidden in the 2D views unless you ask for them.</p>")},
                  {QStringLiteral("Background & grid"), QStringLiteral("<p>Presets from Hammer Black to Paper, or any color from the wheel; the grid "
                                                                       "adapts to stay readable.</p>")}};
        p.see = {QStringLiteral("tools")};
    }
    return g;
}

/// Help → Bonk! Guide: a page-style help window with a sidebar, search and
/// one continuous scrolling document.
class GuideWindow : public QDialog {
public:
    explicit GuideWindow(QWidget *parent) : QDialog(parent) {
        setWindowTitle(QStringLiteral("Bonk! Guide"));
        setWindowFlag(Qt::WindowContextHelpButtonHint, false);
        auto *h = new QHBoxLayout(this);
        h->setContentsMargins(0, 0, 0, 0);
        auto *split = new QSplitter;
        h->addWidget(split);
        auto *left = new QWidget;
        auto *lv = new QVBoxLayout(left);
        lv->setContentsMargins(8, 8, 4, 8);
        find = new QLineEdit;
        find->setPlaceholderText(QStringLiteral("Search the guide…  (Ctrl+F)"));
        find->setClearButtonEnabled(true);
        lv->addWidget(find);
        tree = new QTreeWidget;
        tree->setHeaderHidden(true);
        tree->setIndentation(14);
        lv->addWidget(tree, 1);
        results = new QListWidget;
        results->setWordWrap(true);
        results->setVisible(false);
        lv->addWidget(results, 1);
        hint = new QLabel;
        hint->setWordWrap(true);
        QFont hf = hint->font();
        hf = uiScaled(hf, 0.85);
        hint->setFont(hf);
        hint->setStyleSheet(QStringLiteral("color: palette(placeholder-text)"));
        lv->addWidget(hint);
        split->addWidget(left);
        page = new QTextBrowser;
        page->setOpenLinks(false);
        split->addWidget(page);
        split->setStretchFactor(1, 1);
        split->setSizes({gCompact ? 220 : 270, gCompact ? 620 : 730});
        resize(gCompact ? 860 : 1000, gCompact ? 560 : 660);

        QObject::connect(tree, &QTreeWidget::currentItemChanged, this, [this](QTreeWidgetItem *it) {
            if (it && !it->data(0, Qt::UserRole).toString().isEmpty()) go(it->data(0, Qt::UserRole).toString());
        });
        QObject::connect(page, &QTextBrowser::anchorClicked, this, [this](const QUrl &u) {
            if (u.scheme().startsWith(QStringLiteral("http"))) QDesktopServices::openUrl(u);
            else showPage(u.fragment());
        });
        QObject::connect(find, &QLineEdit::textChanged, this, [this](const QString &q) { search(q); });
        QObject::connect(results, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *it) {
            if (!it) return;
            go(it->data(Qt::UserRole).toString());
        });
        auto *ctrlF = new QAction(this);
        ctrlF->setShortcut(QKeySequence::Find);
        QObject::connect(ctrlF, &QAction::triggered, this, [this] { find->setFocus(); find->selectAll(); });
        addAction(ctrlF);
    }

    /// Rebuild for the current settings (the cheat sheet follows Hammer-style keys).
    void build(bool hammer) {
        pages = guidePages(hammer);
        hammerKeys = hammer;
        const QSignalBlocker b(tree);
        tree->clear();
        QBrush dim = palette().brush(QPalette::PlaceholderText);
        QFont bold = tree->font();
        bold.setBold(true);
        QMap<QString, QTreeWidgetItem *> secs;
        for (const GuidePage &p : pages) {
            QTreeWidgetItem *&s = secs[p.section];
            if (!s) {
                s = new QTreeWidgetItem(tree, {p.section});
                s->setFont(0, bold);
                s->setFlags(Qt::ItemIsEnabled);
            }
            auto *it = new QTreeWidgetItem(s, {p.title});
            it->setData(0, Qt::UserRole, p.id);
        }
        auto *cs = new QTreeWidgetItem(tree, {QStringLiteral("Cheat sheet")});
        cs->setData(0, Qt::UserRole, QStringLiteral("cheat"));
        auto *ab = new QTreeWidgetItem(tree, {QStringLiteral("About Bonk!")});
        ab->setData(0, Qt::UserRole, QStringLiteral("about"));
        tree->expandAll();
        page->document()->setDefaultStyleSheet(css());
        page->setHtml(document());
        hint->setText(QStringLiteral("Tip: F1 opens the page for the tool you're using."));
        (void)dim;
    }

    /// Show page `id` (and select it in the sidebar).
    void showPage(const QString &id) {
        QDialog::show();
        raise();
        activateWindow();
        for (QTreeWidgetItemIterator it(tree); *it; ++it)
            if ((*it)->data(0, Qt::UserRole).toString() == id) {
                tree->setCurrentItem(*it);
                break;
            }
        go(id);
    }

    QLineEdit *searchBox() const { return find; }
    QTextBrowser *browser() const { return page; }

private:
    QList<GuidePage> pages;
    bool hammerKeys = false;
    QLineEdit *find;
    QTreeWidget *tree;
    QListWidget *results;
    QLabel *hint;
    QTextBrowser *page;

    QString css() const {
        const QPalette pal = palette();
        const bool dark = pal.color(QPalette::Base).lightness() < 128;
        return QStringLiteral(
                   "h1{font-size:15pt;margin:0 0 6px 0;} h2{font-size:12pt;margin:16px 0 4px 0;}"
                   "table.k{margin:6px 0 10px 0;} table.k td{padding:3px 10px 3px 0;border-bottom:1px solid %1;}"
                   "td.key{white-space:nowrap;} .kbd{background-color:%2;color:%3;font-family:monospace;font-weight:bold;}"
                   ".see{color:%4;} .crumb{color:%4;font-size:9pt;} a{color:%5;}")
            .arg(dark ? QStringLiteral("#444") : QStringLiteral("#ccc"), dark ? QStringLiteral("#3a3f4a") : QStringLiteral("#e4e6ea"),
                 dark ? QStringLiteral("#fff") : QStringLiteral("#111"), pal.color(QPalette::PlaceholderText).name(), pal.color(QPalette::Link).name());
    }

    static QString table(const QList<QPair<QString, QString>> &rows) {
        if (rows.isEmpty()) return QString();
        QString t = QStringLiteral("<table class='k' width='100%' cellspacing='0'>");
        for (const auto &r : rows)
            t += QStringLiteral("<tr><td class='key' width='34%'>%1</td><td>%2</td></tr>").arg(guideKbd(r.first), r.second.toHtmlEscaped());
        return t + QStringLiteral("</table>");
    }

    QString titleOf(const QString &id) const {
        for (const GuidePage &p : pages)
            if (p.id == id) return p.title;
        return id == QStringLiteral("cheat") ? QStringLiteral("Cheat sheet") : id;
    }

    QString pageHtml(const GuidePage &p) const {
        QString h = QStringLiteral("<a name='%1'></a><div class='crumb'>%2 › %3</div><h1>%3</h1>")
                        .arg(p.id, p.section.toHtmlEscaped(), p.title.toHtmlEscaped());
        h += table(p.keys) + p.html;
        for (int i = 0; i < p.subs.size(); ++i)
            h += QStringLiteral("<a name='%1-%2'></a><h2>%3</h2>%4").arg(p.id).arg(i).arg(p.subs[i].title.toHtmlEscaped(), p.subs[i].html);
        if (!p.see.isEmpty()) {
            QStringList links;
            for (const QString &s : p.see) links << QStringLiteral("<a href='#%1'>%2</a>").arg(s, titleOf(s).toHtmlEscaped());
            h += QStringLiteral("<p class='see'>See also: %1</p>").arg(links.join(QStringLiteral(" · ")));
        }
        return h;
    }

    QString cheatHtml() const {
        QString h = QStringLiteral("<a name='cheat'></a><div class='crumb'>Cheat sheet</div><h1>Cheat sheet</h1>"
                                   "<p class='see'>Every key on one page%1.</p>").arg(hammerKeys ? QStringLiteral(", with Hammer-style keys") : QString());
        for (const GuidePage &p : pages) {
            if (p.keys.isEmpty()) continue;
            h += QStringLiteral("<h2>%1</h2>").arg(p.title.toHtmlEscaped()) + table(p.keys);
        }
        return h;
    }

    QString aboutHtml() const {
        return QStringLiteral("<a name='about'></a><div class='crumb'>About</div><h1>About Bonk!</h1>"
                              "<p><b>%1</b> %2: your Garry's Mod modeling tool for Linux.</p>"
                              "<p>Authored by Claude Opus 5.5. Designed with input from Steventechno. Designs inspired by Valve's Hammer "
                              "editor, VTFEdit, SketchUp and Blender.</p><p>Help → About Bonk! has the details and System Info.</p>")
            .arg(QString::fromUtf8(kAppName).toHtmlEscaped(), QStringLiteral(BONK_VERSION));
    }

    QString document() const {
        QString h;
        for (const GuidePage &p : pages) h += pageHtml(p) + QStringLiteral("<hr>");
        return h + cheatHtml() + QStringLiteral("<hr>") + aboutHtml() + QStringLiteral("<p style='margin-bottom:400px'></p>");
    }

    void go(const QString &anchor) {
        page->scrollToAnchor(anchor);
        // scrollToAnchor puts the anchor at the top only if there's room below; the padding at the end makes sure there is.
    }

    static QString plain(const QString &html) {
        QString t = html;
        static const QRegularExpression tags(QStringLiteral("<[^>]*>"));
        t.replace(tags, QStringLiteral(" "));
        t.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
        t.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
        t.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
        t.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
        return t.simplified();
    }

    static QString snippet(const QString &text, const QString &term) {
        const int at = text.indexOf(term, 0, Qt::CaseInsensitive);
        if (at < 0) return text.left(80).toHtmlEscaped() + (text.size() > 80 ? QStringLiteral("…") : QString());
        const int from = std::max(0, at - 28);
        QString s = (from > 0 ? QStringLiteral("…") : QString()) + text.mid(from, at - from).toHtmlEscaped() +
                    QStringLiteral("<span style='background-color:#e8c547;color:#000'>%1</span>").arg(text.mid(at, term.size()).toHtmlEscaped()) +
                    text.mid(at + term.size(), 52).toHtmlEscaped();
        if (at + term.size() + 52 < text.size()) s += QStringLiteral("…");
        return s;
    }

    void search(const QString &q) {
        const QStringList terms = q.simplified().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        tree->setVisible(terms.isEmpty());
        results->setVisible(!terms.isEmpty());
        if (terms.isEmpty()) {
            hint->setText(QStringLiteral("Tip: F1 opens the page for the tool you're using."));
            return;
        }
        struct Hit { int score; QString title, where, anchor, snip; };
        std::vector<Hit> hits;
        auto test = [&](const QString &title, const QString &kw, const QString &text, int *score) {
            int s = 0;
            for (const QString &t : terms) {
                int one = 0;
                if (title.contains(t, Qt::CaseInsensitive)) one = 3;
                else if (kw.contains(t, Qt::CaseInsensitive)) one = 3;
                else if (text.contains(t, Qt::CaseInsensitive)) one = 1;
                if (!one) return false;
                s += one;
            }
            *score = s;
            return true;
        };
        for (const GuidePage &p : pages) {
            QString keyText;
            for (const auto &k : p.keys) keyText += k.first + QLatin1Char(' ') + k.second + QStringLiteral(". ");
            const QString main = plain(keyText + p.html);
            int sc = 0;
            if (test(p.title, p.keywords, main, &sc)) {
                // the snippet: the first key row or sentence that mentions the term
                QString snipSrc = main;
                for (const auto &k : p.keys)
                    if ((k.first + QLatin1Char(' ') + k.second).contains(terms.first(), Qt::CaseInsensitive)) {
                        snipSrc = k.first + QStringLiteral(": ") + k.second;
                        break;
                    }
                hits.push_back({sc + 1, p.title, p.section, p.id, snippet(snipSrc, terms.first())});
            }
            const size_t pageHit = hits.size();
            bool subHit = false;
            for (int i = 0; i < p.subs.size(); ++i) {
                const QString text = plain(p.subs[i].html);
                if (test(p.subs[i].title, QString(), text, &sc)) {
                    hits.push_back({sc, p.title + QStringLiteral(" › ") + p.subs[i].title, p.section, QStringLiteral("%1-%2").arg(p.id).arg(i),
                                    snippet(p.subs[i].title + QStringLiteral(": ") + text, terms.first())});
                    subHit = true;
                }
            }
            // a page found only through its keywords, when one of its sections says it better: show the section
            if (subHit && pageHit > 0 && hits[pageHit - 1].anchor == p.id && !main.contains(terms.first(), Qt::CaseInsensitive) &&
                !p.title.contains(terms.first(), Qt::CaseInsensitive))
                hits.erase(hits.begin() + long(pageHit - 1));
        }
        std::stable_sort(hits.begin(), hits.end(), [](const Hit &a, const Hit &b) { return a.score > b.score; });
        const QSignalBlocker b(results);
        results->clear();
        for (const Hit &hit : hits) {
            auto *it = new QListWidgetItem(results);
            it->setData(Qt::UserRole, hit.anchor);
            auto *lab = new QLabel(QStringLiteral("<b>%1</b> <span style='color:gray;font-size:8pt'>%2</span><br><span style='font-size:9pt'>%3</span>")
                                       .arg(hit.title.toHtmlEscaped(), hit.where.toHtmlEscaped(), hit.snip));
            lab->setWordWrap(true);
            lab->setContentsMargins(4, 3, 4, 3);
            lab->setAttribute(Qt::WA_TransparentForMouseEvents);
            it->setSizeHint(QSize(160, lab->heightForWidth(std::max(160, results->viewport()->width() - 8)) + 2));
            results->setItemWidget(it, lab);
        }
        hint->setText(hits.empty() ? QStringLiteral("Nothing matches \"%1\".").arg(q.simplified())
                                   : QStringLiteral("%1 topic%2 match \"%3\"").arg(hits.size()).arg(hits.size() == 1 ? QString() : QStringLiteral("s"), q.simplified()));
        if (!hits.empty()) {
            results->setCurrentRow(0);
            go(hits.front().anchor);
        }
    }
};

/// Help → About Bonk!
class AboutDialog : public QDialog {
public:
    explicit AboutDialog(QWidget *parent) : QDialog(parent) {
        setWindowTitle(QStringLiteral("About Bonk!"));
        auto *v = new QVBoxLayout(this);
        v->setContentsMargins(28, 24, 28, 18);
        auto *top = new QHBoxLayout;
        auto *logo = new QLabel;
        logo->setPixmap(QApplication::windowIcon().pixmap(QSize(96, 96), devicePixelRatioF()));
        top->addWidget(logo, 0, Qt::AlignTop);
        top->addSpacing(16);
        const bool flatpak = QFileInfo::exists(QStringLiteral("/.flatpak-info"));
        const QString where = flatpak ? QStringLiteral("<p style='margin-top:12px'>📦 <b>Proudly on Flathub!</b> Thanks for choosing Bonk! for your "
                                                       "Linux Garry's Mod workflow.</p>")
                                      : QStringLiteral("<p style='margin-top:12px;color:gray'>Built from source.</p>");
        auto *txt = new QLabel(
            QStringLiteral("<div style='font-size:18pt;font-weight:bold'>Bonk! Model Prop-inator!</div>"
                           "<div style='font-size:11pt;margin-top:2px'>Your Garry's Mod modeling tool for Linux!</div>"
                           "<div style='color:gray;margin-top:6px'>Version %1 · Rust + Qt %2</div>"
                           "<p style='margin-top:14px'>Authored by <b>Claude Opus 5.5</b>.<br>Designed with input from <b>Steventechno</b>.</p>"
                           "<p style='margin-top:10px;font-style:italic'>Steventechno tools: replacing Windows tools, one at a time. Your Steam Deck, "
                           "Steam Machine or Linux PC is now your workstation!</p>"
                           "<p style='margin-top:10px'>Designs inspired by Valve's Hammer editor, VTFEdit, SketchUp and Blender.</p>%3"
                           "<p style='margin-top:10px;color:gray'>Free software under the GNU General Public License, version 3 or later. "
                           "<a href='https://github.com/steventechno/bonk'>Source code</a></p>")
                .arg(QStringLiteral(BONK_VERSION), QString::fromLatin1(qVersion()), where));
        txt->setWordWrap(true);
        txt->setOpenExternalLinks(true);
        txt->setMinimumWidth(gCompact ? 360 : 420);
        top->addWidget(txt, 1);
        v->addLayout(top);
        auto *bb = new QDialogButtonBox;
        QPushButton *info = bb->addButton(QStringLiteral("System Info"), QDialogButtonBox::ActionRole);
        info->setToolTip(QStringLiteral("Versions and paths, for bug reports (copied to the clipboard)"));
        bb->addButton(QDialogButtonBox::Close);
        v->addSpacing(8);
        v->addWidget(bb);
        QObject::connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
        QObject::connect(info, &QPushButton::clicked, this, [this] { systemInfo(); });
    }

    static QString systemInfoText() {
        SteamFind sf;
        sf.detect();
        QStringList l;
        l << QStringLiteral("Bonk! %1 (%2)").arg(QStringLiteral(BONK_VERSION), QFileInfo::exists(QStringLiteral("/.flatpak-info")) ? QStringLiteral("Flatpak") : QStringLiteral("built from source"));
        l << QStringLiteral("Qt %1, %2").arg(QString::fromLatin1(qVersion()), QGuiApplication::platformName());
        l << QStringLiteral("OS: %1 (kernel %2)").arg(QSysInfo::prettyProductName(), QSysInfo::kernelVersion());
        l << QStringLiteral("Desktop: %1").arg(qEnvironmentVariable("XDG_CURRENT_DESKTOP", QStringLiteral("unknown")));
        if (QScreen *s = QGuiApplication::primaryScreen())
            l << QStringLiteral("Screen: %1×%2 @ %3x, %4 interface").arg(s->size().width()).arg(s->size().height()).arg(s->devicePixelRatio())
                     .arg(gCompact ? QStringLiteral("compact") : QStringLiteral("regular"));
        if (QOpenGLContext *c = QOpenGLContext::globalShareContext())
            l << QStringLiteral("OpenGL: %1.%2").arg(c->format().majorVersion()).arg(c->format().minorVersion());
        l << QStringLiteral("Steam: %1").arg(sf.steamRoot.isEmpty() ? QStringLiteral("not found") : tildePath(sf.steamRoot));
        l << QStringLiteral("Garry's Mod: %1").arg(sf.gmodDir.isEmpty() ? QStringLiteral("not found") : tildePath(sf.gmodDir));
        l << QStringLiteral("studiomdl: %1").arg(sf.studiomdl.isEmpty() ? QStringLiteral("not found") : tildePath(sf.studiomdl));
        QStringList ps;
        for (const auto &p : sf.protons) ps << p.name;
        l << QStringLiteral("Proton: %1 (using %2)").arg(ps.isEmpty() ? QStringLiteral("none") : ps.join(QStringLiteral(", ")),
                                                        sf.proton().isEmpty() ? QStringLiteral("none") : QFileInfo(sf.proton()).absoluteDir().dirName());
        return l.join(QLatin1Char('\n'));
    }

private:
    void systemInfo() {
        const QString t = systemInfoText();
        QGuiApplication::clipboard()->setText(t);
        QMessageBox box(QMessageBox::Information, QStringLiteral("System Info"), QStringLiteral("Copied to the clipboard:"), QMessageBox::Ok, this);
        box.setInformativeText(t);
        box.exec();
    }
};
