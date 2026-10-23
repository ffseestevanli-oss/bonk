// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Steventechno and the Bonk! contributors

// Qt6 host for Bonk! Model Prop-inator!: main window, four OpenGL viewports,
// menus, toolbar, settings dialog, file handling and the Hammer-style "Z"
// mouselook capture. All editor logic and drawing is done in Rust through the
// bonk_* functions declared below.
//
// No Q_OBJECT classes are used, so no moc step is needed.

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QCheckBox>
#include <QCloseEvent>
#include <QColorDialog>
#include <QComboBox>
#include <QCursor>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDateTime>
#include <QDir>
#include <QDirIterator>
#include <QDockWidget>
#include <QFileSystemWatcher>
#include <QFontDatabase>
#include <QRegularExpression>
#include <QSet>
#include <QSignalBlocker>
#include <QElapsedTimer>
#include <QFileDialog>
#include <QFileInfo>
#include <QFocusEvent>
#include <QFontMetrics>
#include <QFormLayout>
#include <QGraphicsEffect>
#include <QDoubleSpinBox>
#include <QPlainTextEdit>
#include <QProcess>
#include <QProcessEnvironment>
#include <QCryptographicHash>
#include <QStandardPaths>
#include <QLocale>
#include <QProgressDialog>
#include <QProgressBar>
#include <QPointer>
#include <QImageWriter>
#include <QDesktopServices>
#include <QUrl>
#include <QGridLayout>
#include <QMimeData>
#include <QDrag>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMenu>
#include <QTreeWidget>
#include <QListWidget>
#include <QInputDialog>
#include <QCollator>
#include <QHeaderView>
#include <QAbstractButton>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QIcon>
#include <QImage>
#include <QImageReader>
#include <QKeyEvent>
#include <QLabel>
#include <QLineEdit>
#include <QMainWindow>
#include <QMenuBar>
#include <QMessageBox>
#include <QMouseEvent>
#include <QOpenGLContext>
#include <QOpenGLFramebufferObject>
#include <QOpenGLFunctions>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStackedWidget>
#include <QTableWidget>
#include <QStyledItemDelegate>
#include <QOpenGLWidget>
#include <QPainter>
#include <QPushButton>
#include <QProxyStyle>
#include <QScreen>
#include <QScrollArea>
#include <QScrollBar>
#include <QSettings>
#include <QSlider>
#include <QSpinBox>
#include <QWidgetAction>
#include <QToolButton>
#include <QSplitter>
#include <QStatusBar>
#include <QStyleFactory>
#include <QSurfaceFormat>
#include <QTimer>
#include <QTabWidget>
#include <QRadioButton>
#include <QTextBrowser>
#include <QClipboard>
#include <QSysInfo>
#include <QTreeWidgetItemIterator>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWheelEvent>

#include <algorithm>
#include <csignal>
#include <map>
#include <cmath>
#include <functional>
#include <vector>

extern "C" {
typedef const void *(*BonkGetProc)(const char *);
typedef void (*BonkTextCb)(void *ctx, float x, float y, int align, const char *s, unsigned rgba);

void bonk_view_init(int view, BonkGetProc gp);
void bonk_view_paint(int view, float w, float h, float dpr, BonkTextCb cb, void *ctx);
void bonk_view_resize(int view, float w, float h);
unsigned bonk_mouse(int view, int kind, float x, float y, int button, int mods);
unsigned bonk_mouse_delta(float dx, float dy);
unsigned bonk_wheel(int view, float x, float y, float delta);
unsigned bonk_key(int view, int key, int pressed, int mods, int repeat);
unsigned bonk_focus_lost(int view);
unsigned bonk_tick(float dt);
unsigned bonk_command(int cmd);
int bonk_tool();
int bonk_render_preview(int view, int skin, const char *choices, int lod, float yaw, float pitch, float dist, float tx, float ty, float tz, float fov, int w, int h, int grid);
int bonk_preview_fit(int skin, const char *choices, int lod, float yaw, float pitch, float fov, float aspect, float *out);
const char *bonk_variants_info();
unsigned bonk_skin_op(int op, int index, const char *text);
unsigned bonk_bodygroup_op(int op, int id, const char *text, const char *text2);
unsigned bonk_set_lods(const char *text);
void bonk_lod_tris(float detail, int *out);
void bonk_thumb_grid(int on);
const char *bonk_prefab_text(const char *source);
float bonk_selection_lift();
unsigned bonk_place_prefab(const char *text, int view, float x, float y, int keep, float lift);
const unsigned char *bonk_pixel_font(int *len);
int bonk_render_thumb(int view, const char *path, float yaw, float pitch, float dist, float tx, float ty, float tz, float fov, int w, int h);
int bonk_model_bounds(const char *path, float *out);
int bonk_model_fit(const char *path, float yaw, float pitch, float fov, float aspect, float *out);
int bonk_is_captured();
const char *bonk_status();
int bonk_save(const char *path);
int bonk_load(const char *path);
const char *bonk_last_error();
int bonk_is_dirty();
void bonk_set_config(int nudge, int swap_alt, float grid_intensity, float r, float g, float b, float rot_snap, int hammer_keys);
int bonk_bisect_delete(); // the Bisect mode: 0 Split, 1 Split & Delete smaller, 2 larger
unsigned long long bonk_layers_rev();
int bonk_layer_count();
const char *bonk_layer_info(int i, int *out);
unsigned bonk_layer_op(int op, int id);
unsigned bonk_layer_rename(int id, const char *name);
int bonk_apply_material(const char *name);
const char *bonk_material_target();
int bonk_show_mannequins();
void bonk_set_current_material(const char *name);
const char *bonk_current_material();
void bonk_texture_set(const char *name, int w, int h, const unsigned char *rgba);
void bonk_texture_remove(const char *name);
void bonk_texture_clear();
void bonk_texture_alpha(const char *name, int mode, float reference);
const char *bonk_model_path();
const char *bonk_mat_override(const char *name);
void bonk_set_mat_override(const char *name, const char *props);
int bonk_write_vtf(const char *path, int w, int h, const unsigned char *rgba, int flags);
int bonk_write_vtf_frames(const char *path, int w, int frame_h, int frames, const unsigned char *rgba, int flags);
void bonk_texture_anim(const char *name, int frames, float fps, float scroll_rate, float scroll_angle, int pulse_mode, float pmin, float pmax, float period);
int bonk_face_info(float *out);
unsigned bonk_face_set(int field, float value);
unsigned bonk_face_justify(int mode);
void bonk_set_tex_group(int on);
void bonk_set_texlock(int lock, int scale);
void bonk_set_shift_clone(int on);
int bonk_group_count();
const char *bonk_group_info(int i, int *out);
unsigned bonk_group_op(int op, int id);
unsigned bonk_group_rename(int id, const char *name);
void bonk_set_ghost_opacity(float a);
int bonk_editing_collision();
int bonk_view_flags();
void bonk_set_nodraw_look(int style, float r, float g, float b, float intensity, float width, float speed, int show2d);
void bonk_set_nodraw_flow(float px);
void bonk_set_sel_touch(int on);
void bonk_budget(int *out);
float bonk_grid();
int bonk_show_edges();
void bonk_set_show_edges(int on);
const char *bonk_budget_offenders(int kind, int select);
int bonk_material_usage(const char *name);
int bonk_rename_material(const char *old_name, const char *new_name);
int bonk_hold_view();
void bonk_set_model_path(const char *path);
int bonk_export_smd(const char *reference, const char *phys, int phys_mode, int *out);
int bonk_collision_pieces();
const char *bonk_used_materials();
const char *bonk_variant_export_info();
const char *bonk_set_color_prefix(const char *modelPath);
void bonk_set_fly(float min, float max, float lerp);
int bonk_debug_sel_bounds(float *out);
const char *bonk_vmf_scan(const char *path);
int bonk_vmf_import(const char *path, const char *skip);
const char *bonk_vmf_report();
int bonk_vtf_info(const unsigned char *data, size_t len, int *out);
const unsigned char *bonk_vtf_decode(const unsigned char *data, size_t len, int *w, int *h);
const char *bonk_parse_vmt(const char *text);
const char *bonk_clean_texture_path(const char *p);
int bonk_tidy_groups();
const char *bonk_hold_hint(int *out);
void bonk_selection_options(int *out);
void bonk_set_axis_lock(int on);
void bonk_set_collision_solid(int on);
void bonk_set_sel_box_wait(int on);
void bonk_set_fov(float deg);
int bonk_vertex_pick();
unsigned bonk_set_block_shape(int shape, int faces);
unsigned bonk_set_arch_thickness(float t);
void bonk_block_shape(int *out);
int bonk_place_kind();
const char *bonk_copy(const char *source, int cut);
unsigned bonk_paste(const char *text, int at_mouse);
const char *bonk_overlap_scan();
unsigned bonk_select_ids(const char *ids);
int bonk_selected_group();
int bonk_sel_box_mode();
int bonk_select_faces_with(const char *material);
int bonk_new_collision(int mode, float detail);
int bonk_simplify_count(float detail, int *out);
void bonk_set_simplify_detail(float d);
void bonk_set_join_exact(int on);
float bonk_mass();
void bonk_set_mass(float kg);
int bonk_replace_material(const char *find, const char *with, int selected_only);
const char *bonk_selected_face_materials();
}

#ifndef BONK_VERSION
#define BONK_VERSION "dev"
#endif

namespace {

const char *kAppName = "Bonk! Model Prop-inator!";
/// Compact UI (screens under 1440p unless Settings says otherwise)
bool gCompact = false;
/// Low-resolution mode (Settings → Appearance → Compact (Low Resolution)): Bonk Pixel,
/// unsmoothed, gLowPx pixels tall.
bool gLowRes = false, gPixelFont = false;
int gLowPx = 6;
QString gPixelFamily; // "Bonk Pixel", once registered

/// `f` at `rel` times the normal text size. In low-resolution mode that's a whole number of
/// pixels (whole multiples for Bonk Pixel, so it stays crisp); otherwise it scales the point size.
QFont uiScaled(QFont f, double rel) {
    if (gLowRes) {
        f.setPixelSize(gPixelFont ? gLowPx * std::max(1, int(std::lround(rel))) : std::max(6, int(std::lround(gLowPx * rel))));
        return f;
    }
    if (f.pointSizeF() > 0) f.setPointSizeF(f.pointSizeF() * rel);
    else if (f.pixelSize() > 0) f.setPixelSize(std::max(6, int(std::lround(f.pixelSize() * rel))));
    return f;
}
/// A compact size, or a smaller one in low-resolution mode.
int lowOr(int compact, int low) { return gLowRes ? low : compact; }
/// The system font, smoothed: for text drawn into icons and cursors, whatever the interface uses.
QFont smoothFont() {
    QFont f = QFontDatabase::systemFont(QFontDatabase::GeneralFont);
    f.setStyleStrategy(QFont::PreferAntialias);
    return f;
}
/// `f` at `px` pixels as the regular interface draws it (≈12 px text), scaled for low resolution.
QFont uiPx(QFont f, int px) {
    if (gLowRes) return uiScaled(f, px / 12.0);
    f.setPixelSize(px);
    return f;
}
/// `f` at `pt` points as the regular interface draws it (≈9 pt text), scaled for low resolution.
QFont uiPt(QFont f, double pt) {
    if (gLowRes) return uiScaled(f, pt / 9.0);
    f.setPointSizeF(pt);
    return f;
}
constexpr double kPi = 3.14159265358979323846;

// Keep in sync with src/editor.rs
enum : unsigned { F_R3D = 1, F_R2D = 2, F_CAP_ON = 8, F_CAP_OFF = 16, F_TOOL = 32 };
enum Cmd {
    CMD_UNDO = 1, CMD_REDO, CMD_DELETE, CMD_SELECT_ALL, CMD_GRID_SMALLER, CMD_GRID_LARGER,
    CMD_TOOL_SELECT, CMD_TOOL_BLOCK, CMD_NEW, CMD_CENTER_2D, CMD_RELEASE_CAPTURE, CMD_CREATE_BLOCK,
    CMD_TOOL_VERTEX, CMD_QUANTIZE, CMD_TOOL_BISECT, CMD_BISECT_MODE_SPLIT, CMD_BISECT_MODE_DELETE, CMD_BISECT_APPLY,
    CMD_TOOL_MANNEQUIN, CMD_TOOL_ORIGIN, CMD_TOOL_FACE, CMD_ORIGIN_TO_SEL_BOTTOM, CMD_ORIGIN_TO_SEL_CENTER, CMD_TOGGLE_MANNEQUINS,
    CMD_GROUP, CMD_UNGROUP, CMD_IGNORE_GROUPS, CMD_TOGGLE_NODRAW_2D, CMD_DESELECT, CMD_TOGGLE_EDGES_3D,
    CMD_BISECT_MODE_DELETE_LARGER, CMD_HIDE_TOGGLE, CMD_MIRROR_H, CMD_MIRROR_V, CMD_TOGGLE_AXIS_LOCK, CMD_TOGGLE_BOX_TOUCH,
    CMD_TOGGLE_COLLISION_SOLID, CMD_TOGGLE_VERTEX_PICK, CMD_TOOL_CAR, CMD_TOGGLE_SEL_BOX_MODE,
    CMD_JOIN, CMD_JOIN_HULL, CMD_JOIN_EXACT
};

const char *kViewNames[4] = {"camera", "top (x/y)", "front (y/z)", "side (x/z)"};

const void *getProc(const char *name) {
    QOpenGLContext *c = QOpenGLContext::currentContext();
    return c ? reinterpret_cast<const void *>(c->getProcAddress(name)) : nullptr;
}

int modsOf(Qt::KeyboardModifiers m) {
    int r = 0;
    if (m & Qt::ShiftModifier) r |= 1;
    if (m & Qt::ControlModifier) r |= 2;
    if (m & Qt::AltModifier) r |= 4;
    return r;
}

Qt::CursorShape cursorOf(unsigned c) {
    switch (c) {
    case 2: return Qt::CrossCursor;
    case 3: return Qt::SizeAllCursor;
    case 4: return Qt::SizeHorCursor;
    case 5: return Qt::SizeVerCursor;
    case 6: return Qt::SizeFDiagCursor;
    case 7: return Qt::SizeBDiagCursor;
    case 8: return Qt::ClosedHandCursor;
    case 9: return Qt::PointingHandCursor;
    case 10: return Qt::DragCopyCursor;
    default: return Qt::ArrowCursor;
    }
}

/// Lines Proton / Wine / the GPU driver print that have nothing to do with the model.
bool protonChatter(const QString &l) {
    if (l.isEmpty()) return false;
    static const QRegularExpression wineDbg(QStringLiteral("^[0-9a-f]{4,}:(fixme|err|warn|trace):"));
    return l.startsWith(QStringLiteral("Proton:")) || l.startsWith(QStringLiteral("fixme:")) || l.startsWith(QStringLiteral("wine:")) ||
           l.startsWith(QStringLiteral("WARNING: radv")) || l.contains(QStringLiteral("ntsync"), Qt::CaseInsensitive) ||
           l.startsWith(QStringLiteral("esync:")) || l.startsWith(QStringLiteral("fsync:")) || l.startsWith(QStringLiteral("ProtonFixes")) ||
           l.startsWith(QStringLiteral("pressure-vessel")) || wineDbg.match(l).hasMatch();
}

// ------------------------------------------------------------------ preferences

struct Prefs {
    bool nudge = true;
    bool swapAlt = false;
    int gridIntensity = 100; // percent
    QColor bg = QColor(0, 0, 0);
    int rotSnap = 15; // degrees
    bool greyTools = false;      // tool buttons: system accent (false) or plain gray
    bool rememberBisect = false; // keep the Bisect mode between sessions
    int bisectMode = 0;          // the remembered Bisect mode (0 split, 1 delete smaller, 2 delete larger)
    bool hammerKeys = false;     // Hammer-style tool keys (Shift+S/B/V/X)
    bool applyOnClick = false;   // Texture Picker: clicking a swatch applies it
    bool swatchSwitchesTool = true; // clicking a swatch switches to Face Select & Paint
    bool shiftClone = true;      // hold Shift and move to clone the selection
    bool pinCursor = true;       // pin the cursor while holding S / Shift / R
    bool selInsideOnly = false;  // selection box takes only brushes fully inside it (default: what it touches)
    int toolLayout = 1;          // 0 icons, 1 list (default since 0.12), 2 compact list
    bool axisLock = true;        // G / Shift moves stay on one axis (Alt flips it while holding)
    bool selBoxWait = false;     // the selection box waits for Enter (the old way)
    bool collisionSolid = false; // View → Collision Look: Solid
    int fov = 75;                // 3D view field of view
    bool fovUnlocked = false;    // allow more than 120°
    bool askResize = true;       // ask to shrink oversized textures when they're added
    int density = 0;             // 0 auto (compact under 1440p), 1 compact (low resolution), 2 regular
    int lowPx = 6;               // low resolution: Bonk Pixel's size in px (6, 12 or 18)
    int settingsTab = 0;         // the Settings tab last open
    // Nodraw look
    int ndStyle = 0;             // 0 solid, 1 RGB cycle, 2 rainbow flow
    QColor ndColor = QColor(219, 189, 64);
    int ndIntensity = 60;        // percent
    int ndWidth = 6;             // stripe length in px (a stripe and the gap after it are each this long)
    int ndFlow = 72;             // Rainbow flow: px for one full rainbow
    int ndSpeed = 25;            // percent of 1 cycle/second
    bool nd2d = false;           // nodraw stripes in the 2D views
    // Budget limits (GMod studiomdl defaults)
    int budTris = 10922, budVerts = 65536, budMats = 64, budPieces = 1200; // pieces: the bar turns yellow above it
    bool joinExactOnly = false;  // J only joins when the shape stays exactly the same
    double simplifyDetail = 8.0; // simplified collision: what counts as a detail (units)
    bool launchGmod = false;     // start Garry's Mod after exporting
    bool openAfterPublish = true; // show the Workshop page after publishing
    int flyMin = 300, flyMax = 600; // 3D flying speed (units / second): starts at min, eases up to max
    double flyLerp = 3.0;           // how quickly it eases up (per second)
    bool advanced = false;        // Advanced mode: every feature (Simple hides the expert ones)
    QString studiomdl;           // a custom studiomdl.exe (empty = Garry's Mod's own)
    bool startScreen = true;     // show the start screen when Bonk opens
    // Garry's Mod
    QString gmodPath;            // empty = find it through Steam
    QString protonPath;          // empty = newest stable Proton
    bool exportAfterCompile = false;

    void load() {
        QSettings s;
        greyTools = s.value("tools/greyButtons", false).toBool();
        rememberBisect = s.value("tools/rememberBisectMode", false).toBool();
        bisectMode = s.value("tools/bisectMode", s.value("tools/bisectDelete", false).toBool() ? 1 : 0).toInt();
        hammerKeys = s.value("keys/hammerStyle", false).toBool();
        applyOnClick = s.value("picker/applyOnClick", false).toBool();
        swatchSwitchesTool = s.value("picker/switchToFaceTool", true).toBool();
        shiftClone = s.value("editing/holdShiftClone", true).toBool();
        pinCursor = s.value("editing/pinCursor", true).toBool();
        selInsideOnly = s.value("editing/selectionBoxInsideOnly", false).toBool(); // (0.11: replaces selectionBoxTouching)
        toolLayout = s.value("tools/layout", 1).toInt();
        if (!s.value("tools/layoutDefault012", false).toBool()) {
            toolLayout = 1; // 0.12 makes the list the default layout: everyone starts there once
            s.setValue("tools/layout", 1);
            s.setValue("tools/layoutDefault012", true);
        }
        axisLock = s.value("editing/axisLock", true).toBool();
        selBoxWait = s.value("editing/selectionBoxWaitsForEnter", false).toBool();
        collisionSolid = s.value("view/collisionSolid", false).toBool();
        fovUnlocked = s.value("view/fovUnlocked", false).toBool();
        fov = qBound(30, s.value("view/fov", 75).toInt(), fovUnlocked ? 170 : 120);
        askResize = s.value("textures/askResize", true).toBool();
        density = s.value("view/density", 0).toInt();
        lowPx = s.value("view/lowPx", 6).toInt();
        settingsTab = s.value("view/settingsTab", 0).toInt();
        ndStyle = s.value("nodraw/style", 0).toInt();
        ndColor = QColor(s.value("nodraw/color", "#dbbd40").toString());
        if (!ndColor.isValid()) ndColor = QColor(219, 189, 64);
        ndIntensity = s.value("nodraw/intensity", 60).toInt();
        ndWidth = s.value("nodraw/width", 6).toInt();
        ndFlow = s.value("nodraw/flow", 72).toInt();
        ndSpeed = s.value("nodraw/speed", 25).toInt();
        nd2d = s.value("nodraw/show2d", false).toBool();
        budTris = s.value("budget/trianglesPerMaterial", 10922).toInt();
        budVerts = s.value("budget/vertices", 65536).toInt();
        budMats = s.value("budget/materials", 64).toInt();
        budPieces = s.value("budget/collisionPieces", 1200).toInt();
        if (budPieces == 20) budPieces = 1200; // 0.15: the pieces meter warns (yellow) instead of a hard limit
        joinExactOnly = s.value("editing/joinExactOnly", false).toBool();
        simplifyDetail = s.value("collision/simplifyDetail", 8.0).toDouble();
        launchGmod = s.value("gmod/launchAfterExport", false).toBool();
        openAfterPublish = s.value("workshop/openAfterPublish", true).toBool();
        advanced = s.value("ui/advanced", false).toBool();
        flyMin = s.value("fly/min", 300).toInt();
        flyMax = s.value("fly/max", 600).toInt();
        flyLerp = s.value("fly/lerp", 3.0).toDouble();
        studiomdl = s.value("gmod/studiomdl").toString();
        startScreen = s.value("ui/startScreen", true).toBool();
        gmodPath = s.value("gmod/path").toString();
        protonPath = s.value("gmod/proton").toString();
        exportAfterCompile = s.value("gmod/exportAfterCompile", false).toBool();
        nudge = s.value("editing/arrowNudge", true).toBool();
        swapAlt = s.value("editing/swapAltDrag", false).toBool();
        rotSnap = s.value("editing/rotationSnap", 15).toInt();
        gridIntensity = s.value("view/gridIntensity", 100).toInt();
        bg = QColor(s.value("view/background", "#000000").toString());
        if (!bg.isValid()) bg = QColor(0, 0, 0);
    }
    void save() const {
        QSettings s;
        s.setValue("editing/arrowNudge", nudge);
        s.setValue("editing/swapAltDrag", swapAlt);
        s.setValue("editing/rotationSnap", rotSnap);
        s.setValue("view/gridIntensity", gridIntensity);
        s.setValue("view/background", bg.name());
        s.setValue("tools/greyButtons", greyTools);
        s.setValue("tools/rememberBisectMode", rememberBisect);
        s.setValue("keys/hammerStyle", hammerKeys);
        s.setValue("picker/applyOnClick", applyOnClick);
        s.setValue("picker/switchToFaceTool", swatchSwitchesTool);
        s.setValue("editing/holdShiftClone", shiftClone);
        s.setValue("editing/pinCursor", pinCursor);
        s.setValue("editing/selectionBoxInsideOnly", selInsideOnly);
        s.remove("editing/selectionBoxTouching");
        s.setValue("tools/layout", toolLayout);
        s.setValue("editing/axisLock", axisLock);
        s.setValue("editing/selectionBoxWaitsForEnter", selBoxWait);
        s.setValue("view/collisionSolid", collisionSolid);
        s.setValue("view/fov", fov);
        s.setValue("view/fovUnlocked", fovUnlocked);
        s.setValue("textures/askResize", askResize);
        s.setValue("view/density", density);
        s.setValue("view/lowPx", lowPx);
        s.setValue("view/settingsTab", settingsTab);
        s.setValue("nodraw/style", ndStyle);
        s.setValue("nodraw/color", ndColor.name());
        s.setValue("nodraw/intensity", ndIntensity);
        s.setValue("nodraw/width", ndWidth);
        s.setValue("nodraw/flow", ndFlow);
        s.setValue("nodraw/speed", ndSpeed);
        s.setValue("nodraw/show2d", nd2d);
        s.setValue("budget/trianglesPerMaterial", budTris);
        s.setValue("budget/vertices", budVerts);
        s.setValue("budget/materials", budMats);
        s.setValue("budget/collisionPieces", budPieces);
        s.setValue("editing/joinExactOnly", joinExactOnly);
        s.setValue("collision/simplifyDetail", simplifyDetail);
        s.setValue("gmod/launchAfterExport", launchGmod);
        s.setValue("workshop/openAfterPublish", openAfterPublish);
        s.setValue("ui/advanced", advanced);
        s.setValue("fly/min", flyMin);
        s.setValue("fly/max", flyMax);
        s.setValue("fly/lerp", flyLerp);
        s.setValue("gmod/studiomdl", studiomdl);
        s.setValue("ui/startScreen", startScreen);
        s.setValue("gmod/path", gmodPath);
        s.setValue("gmod/proton", protonPath);
        s.setValue("gmod/exportAfterCompile", exportAfterCompile);
    }
    void apply() const {
        bonk_set_fly(float(flyMin), float(flyMax), float(flyLerp));
        bonk_set_config(nudge, swapAlt, gridIntensity / 100.0f, float(bg.redF()), float(bg.greenF()), float(bg.blueF()), float(rotSnap), hammerKeys);
        bonk_set_shift_clone(shiftClone);
        bonk_set_sel_touch(!selInsideOnly);
        bonk_set_axis_lock(axisLock);
        bonk_set_sel_box_wait(selBoxWait);
        bonk_set_collision_solid(collisionSolid);
        bonk_set_fov(float(fov));
        bonk_set_join_exact(joinExactOnly);
        bonk_set_simplify_detail(float(simplifyDetail));
        bonk_set_nodraw_look(ndStyle, float(ndColor.redF()), float(ndColor.greenF()), float(ndColor.blueF()), ndIntensity / 100.0f,
                             float(ndWidth), ndSpeed / 100.0f, nd2d);
        bonk_set_nodraw_flow(float(ndFlow));
    }
    bool lightBg() const { return 0.2126 * bg.redF() + 0.7152 * bg.greenF() + 0.0722 * bg.blueF() > 0.5; }
};

struct Preset { const char *name; const char *color; };
const Preset kPresets[4] = {
    {"Hammer Black", "#000000"},
    {"Graphite", "#1d2026"},
    {"Midnight", "#0f1a33"},
    {"Paper", "#d8d8d2"},
};

// ------------------------------------------------------------------ color wheel

// Hue around the rim, saturation from the center outwards; brightness comes
// from the slider next to it.
class ColorWheel : public QWidget {
public:
    std::function<void(QColor)> onChange;

    explicit ColorWheel(QWidget *parent = nullptr) : QWidget(parent) {
        setMinimumSize(170, 170);
        setCursor(Qt::CrossCursor);
    }
    void setColor(const QColor &c) {
        float h, s, v;
        c.getHsvF(&h, &s, &v);
        if (h >= 0) hue = h; // keep the hue for grays
        sat = s;
        val = v;
        cache = QImage();
        update();
    }
    void setValue(float v) {
        val = v;
        cache = QImage();
        update();
        emitColor();
    }
    QColor color() const { return QColor::fromHsvF(hue, sat, val); }
    float value() const { return val; }

protected:
    void paintEvent(QPaintEvent *) override {
        int d = std::min(width(), height()) - 4;
        if (cache.width() != d) buildImage(d);
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QPoint o((width() - d) / 2, (height() - d) / 2);
        p.drawImage(o, cache);
        double r = d / 2.0;
        double a = hue * 2 * kPi;
        QPointF m(o.x() + r + std::cos(a) * sat * r, o.y() + r - std::sin(a) * sat * r);
        p.setPen(QPen(Qt::black, 2));
        p.drawEllipse(m, 6, 6);
        p.setPen(QPen(Qt::white, 1.5));
        p.drawEllipse(m, 5, 5);
    }
    void mousePressEvent(QMouseEvent *e) override { pick(e->position()); }
    void mouseMoveEvent(QMouseEvent *e) override {
        if (e->buttons() & Qt::LeftButton) pick(e->position());
    }

private:
    float hue = 0, sat = 0, val = 0;
    QImage cache;

    void buildImage(int d) {
        cache = QImage(d, d, QImage::Format_ARGB32_Premultiplied);
        cache.fill(Qt::transparent);
        double r = d / 2.0;
        for (int y = 0; y < d; ++y) {
            for (int x = 0; x < d; ++x) {
                double dx = x + 0.5 - r, dy = r - (y + 0.5);
                double dist = std::sqrt(dx * dx + dy * dy);
                if (dist > r) continue;
                double h = std::atan2(dy, dx) / (2 * kPi);
                if (h < 0) h += 1;
                QColor c = QColor::fromHsvF(float(h), float(std::min(1.0, dist / r)), 1.0f);
                // soft edge
                double edge = std::min(1.0, r - dist);
                c.setAlphaF(float(edge));
                cache.setPixelColor(x, y, c);
            }
        }
    }
    void pick(QPointF pos) {
        int d = std::min(width(), height()) - 4;
        double r = d / 2.0;
        double dx = pos.x() - (width() / 2.0), dy = (height() / 2.0) - pos.y();
        double h = std::atan2(dy, dx) / (2 * kPi);
        if (h < 0) h += 1;
        hue = float(h);
        sat = float(std::min(1.0, std::sqrt(dx * dx + dy * dy) / r));
        if (val < 0.05f) val = 0.5f; // picking a hue on a black background should show something
        cache = QImage();
        update();
        emitColor();
    }
    void emitColor() {
        if (onChange) onChange(color());
    }
};


/// A path for display: native separators, home folder shown as ~.
QString tildePath(const QString &p) {
    QString n = QDir::cleanPath(p), h = QDir::homePath();
    if (n == h) return QStringLiteral("~");
    if (n.startsWith(h + QLatin1Char('/'))) n = QStringLiteral("~") + n.mid(h.size());
    return QDir::toNativeSeparators(n);
}

// ------------------------------------------------------------------ Garry's Mod & Proton

/// Where Steam, Garry's Mod, its studiomdl and Proton live. Bonk only uses the
/// GMod folder to run bin/studiomdl.exe, when asked to write
/// garrysmod/addons/<project>, and to run gmad / gmpublish from Manage Workshop.
struct SteamFind {
    QString steamRoot;
    QStringList libraries;
    QString gmodDir;    // .../steamapps/common/GarrysMod
    QString studiomdl;  // .../GarrysMod/bin/studiomdl.exe
    struct Proton { QString name, path; };
    std::vector<Proton> protons; // newest stable first
    bool inFlatpak = QFileInfo::exists(QStringLiteral("/.flatpak-info"));

    static std::vector<QString> roots() {
        const QString h = QDir::homePath();
        return {h + QStringLiteral("/.local/share/Steam"), h + QStringLiteral("/.steam/steam"), h + QStringLiteral("/.steam/root"),
                h + QStringLiteral("/.var/app/com.valvesoftware.Steam/.local/share/Steam"),
                h + QStringLiteral("/.var/app/com.valvesoftware.Steam/data/Steam")};
    }

    static QString findStudiomdl(const QString &gmod) {
        for (const QString &rel : {QStringLiteral("/bin/studiomdl.exe"), QStringLiteral("/bin/win64/studiomdl.exe")})
            if (QFileInfo::exists(gmod + rel)) return gmod + rel;
        return QString();
    }

    void detect() {
        libraries.clear();
        protons.clear();
        steamRoot.clear();
        for (const QString &r : roots()) {
            const QString vdf = r + QStringLiteral("/steamapps/libraryfolders.vdf");
            if (!QFileInfo::exists(r + QStringLiteral("/steamapps"))) continue;
            const QString canon = QFileInfo(r).canonicalFilePath();
            if (steamRoot.isEmpty()) steamRoot = canon;
            if (!libraries.contains(canon)) libraries << canon;
            QFile f(vdf);
            if (f.open(QIODevice::ReadOnly | QIODevice::Text)) {
                static const QRegularExpression re(QStringLiteral("\"path\"\\s+\"([^\"]+)\""));
                auto it = re.globalMatch(QString::fromUtf8(f.readAll()));
                while (it.hasNext()) {
                    QString p = QFileInfo(it.next().captured(1).replace(QStringLiteral("\\\\"), QStringLiteral("/"))).canonicalFilePath();
                    if (!p.isEmpty() && !libraries.contains(p)) libraries << p;
                }
            }
        }
        // Garry's Mod (a folder chosen in Settings wins)
        gmodDir = QSettings().value("gmod/path").toString();
        if (gmodDir.isEmpty() || findStudiomdl(gmodDir).isEmpty()) {
            gmodDir.clear();
            for (const QString &lib : libraries) {
                const QString g = lib + QStringLiteral("/steamapps/common/GarrysMod");
                if (QFileInfo::exists(g + QStringLiteral("/garrysmod"))) {
                    gmodDir = g;
                    break;
                }
            }
        }
        studiomdl = gmodDir.isEmpty() ? QString() : findStudiomdl(gmodDir);
        // Proton: Steam's, then custom ones (compatibilitytools.d)
        auto add = [this](const QString &dir) {
            if (QFileInfo::exists(dir + QStringLiteral("/proton"))) {
                const QString n = QFileInfo(dir).fileName();
                for (const Proton &p : protons)
                    if (p.name == n) return;
                protons.push_back({n, dir + QStringLiteral("/proton")});
            }
        };
        for (const QString &lib : libraries) {
            QDir common(lib + QStringLiteral("/steamapps/common"));
            for (const QString &d : common.entryList({QStringLiteral("Proton*")}, QDir::Dirs | QDir::NoDotAndDotDot)) add(common.absoluteFilePath(d));
        }
        for (const QString &r : roots()) {
            QDir ct(r + QStringLiteral("/compatibilitytools.d"));
            for (const QString &d : ct.entryList(QDir::Dirs | QDir::NoDotAndDotDot)) add(ct.absoluteFilePath(d));
        }
        // newest stable first ("Proton 9.0" > "Proton 8.0"), then Experimental / Hotfix / others
        auto rank = [](const QString &n) -> double {
            static const QRegularExpression v(QStringLiteral("^Proton (\\d+)\\.(\\d+)"));
            auto m = v.match(n);
            const bool stable = !n.contains(QStringLiteral("Beta"), Qt::CaseInsensitive) && !n.contains(QStringLiteral("Hotfix"), Qt::CaseInsensitive);
            if (m.hasMatch()) return m.captured(1).toDouble() * 100 + m.captured(2).toDouble() + (stable ? 100000 : 0);
            return n.contains(QStringLiteral("Experimental")) ? -1 : -2;
        };
        std::sort(protons.begin(), protons.end(), [&](const Proton &a, const Proton &b) { return rank(a.name) > rank(b.name); });
    }

    /// The Proton to use: the one chosen in Settings, else newest stable.
    QString proton() const {
        const QString chosen = QSettings().value("gmod/proton").toString();
        for (const Proton &p : protons)
            if (p.path == chosen) return p.path;
        return protons.empty() ? QString() : protons.front().path;
    }

    QString addonsDir() const { return gmodDir.isEmpty() ? QString() : gmodDir + QStringLiteral("/garrysmod/addons"); }
};

/// A Linux path as a Windows program running under Proton / Wine sees it.
QString winPath(const QString &p) {
    QString w = QFileInfo(p).absoluteFilePath();
    w.replace(QLatin1Char('/'), QLatin1Char('\\'));
    return QStringLiteral("Z:") + w;
}

/// Copy a folder's contents into another, replacing files. Returns files copied.
int copyTree(const QString &from, const QString &to, QStringList *errors) {
    int n = 0;
    QDirIterator it(from, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString f = it.next();
        const QString dst = to + QLatin1Char('/') + QDir(from).relativeFilePath(f);
        QDir().mkpath(QFileInfo(dst).absolutePath());
        QFile::remove(dst);
        if (QFile::copy(f, dst)) ++n;
        else if (errors) *errors << QStringLiteral("Couldn't copy %1").arg(QDir(from).relativeFilePath(f));
    }
    return n;
}

/// The compile window: studiomdl's output, live, with the problems in plain words.
class CompileWindow : public QDialog {
public:
    std::function<void()> exportReq, openFolderReq;

    explicit CompileWindow(QWidget *parent) : QDialog(parent) {
        setWindowTitle(QStringLiteral("Compile"));
        auto *v = new QVBoxLayout(this);
        headline = new QLabel;
        headline->setWordWrap(true);
        QFont hf = headline->font();
        hf.setBold(true);
        hf = uiScaled(hf, 1.15);
        headline->setFont(hf);
        v->addWidget(headline);
        summary = new QLabel;
        summary->setWordWrap(true);
        summary->setTextFormat(Qt::RichText);
        v->addWidget(summary);
        headline->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
        summary->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
        spacer = new QWidget;
        spacer->setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Expanding);
        v->addWidget(spacer, 1);
        log = new QPlainTextEdit;
        log->setReadOnly(true);
        log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        log->setMaximumBlockCount(20000);
        log->setLineWrapMode(QPlainTextEdit::NoWrap);
        v->addWidget(log, 1);
        auto *row = new QHBoxLayout;
        showLog = new QCheckBox(QStringLiteral("Show studiomdl's log"));
        showLog->setChecked(QSettings().value("compile/showLog", false).toBool());
        row->addWidget(showLog);
        row->addStretch(1);
        folderBtn = new QPushButton(QStringLiteral("Open Addon Folder"));
        exportBtn = new QPushButton(QStringLiteral("Export to GMod"));
        auto *close = new QPushButton(QStringLiteral("Close"));
        row->addWidget(folderBtn);
        row->addWidget(exportBtn);
        row->addWidget(close);
        v->addLayout(row);
        QObject::connect(showLog, &QCheckBox::toggled, this, [this](bool on) {
            QSettings().setValue("compile/showLog", on);
            log->setVisible(on);
            spacer->setVisible(!on);
            if (!on) resize(width(), minimumSizeHint().height() + 20);
            else if (height() < 420) resize(width(), 520);
        });
        QObject::connect(close, &QPushButton::clicked, this, &QDialog::hide);
        QObject::connect(exportBtn, &QPushButton::clicked, this, [this] { if (exportReq) exportReq(); });
        QObject::connect(folderBtn, &QPushButton::clicked, this, [this] { if (openFolderReq) openFolderReq(); });
        log->setVisible(showLog->isChecked());
        spacer->setVisible(!showLog->isChecked());
        resize(760, showLog->isChecked() ? 520 : 220);
    }

    void start(const QString &title) {
        setWindowTitle(QStringLiteral("Compile — %1").arg(title));
        headline->setText(QStringLiteral("Compiling %1…").arg(title));
        headline->setStyleSheet(QString());
        summary->clear();
        log->clear();
        problems.clear();
        exportBtn->setEnabled(false);
    }
    void line(const QString &l, bool error = false) {
        if (error) log->appendHtml(QStringLiteral("<span style='color:#ff6b5b'>%1</span>").arg(l.toHtmlEscaped()));
        else log->appendPlainText(l);
    }
    void note(const QString &html) { problems << html; }
    void finish(bool ok, const QString &what) {
        headline->setText(ok ? QStringLiteral("✔ Compiled! %1").arg(what) : QStringLiteral("✖ Compile failed: %1").arg(what));
        headline->setStyleSheet(ok ? QStringLiteral("color:#5cc46f") : QStringLiteral("color:#ff6b5b"));
        summary->setText(problems.isEmpty() ? QString() : QStringLiteral("<ul style='margin:0'><li>%1</li></ul>").arg(problems.join(QStringLiteral("</li><li>"))));
        exportBtn->setEnabled(ok);
        if (!ok && !showLog->isChecked()) showLog->setChecked(true);
    }

private:
    QLabel *headline, *summary;
    QWidget *spacer;
    QPlainTextEdit *log;
    QCheckBox *showLog;
    QPushButton *folderBtn, *exportBtn;
    QStringList problems;
};

// ------------------------------------------------------------------ settings dialog

class SettingsDialog : public QDialog {
public:
    SettingsDialog(Prefs &prefs, std::function<void()> onChanged, QWidget *parent)
        : QDialog(parent), p(prefs), original(prefs), changed(std::move(onChanged)) {
        setWindowTitle(QStringLiteral("Configure Bonk!"));
        // tabs, each scrolling on short screens (1366×768); the buttons stay put
        auto *outer = new QVBoxLayout(this);
        tabs = new QTabWidget;
        outer->addWidget(tabs, 1);
        QList<QWidget *> pages;
        auto page = [&](const QString &name) {
            auto *scroll = new QScrollArea;
            scroll->setWidgetResizable(true);
            scroll->setFrameShape(QFrame::NoFrame);
            scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
            auto *inner = new QWidget;
            scroll->setWidget(inner);
            tabs->addTab(scroll, name);
            pages << inner;
            auto *l = new QVBoxLayout(inner);
            return l;
        };
        QVBoxLayout *editTab = page(QStringLiteral("Editing"));
        QVBoxLayout *toolsTab = page(QStringLiteral("Tools && Keys"));
        QVBoxLayout *lookTab = page(QStringLiteral("Appearance"));
        QVBoxLayout *gmodTab = page(QStringLiteral("Garry's Mod"));
        QVBoxLayout *advTab = page(QStringLiteral("Advanced"));

        // Simple / Advanced
        auto *modeBox = new QGroupBox(QStringLiteral("Mode"));
        auto *mf = new QVBoxLayout(modeBox);
        advanced = new QCheckBox(QStringLiteral("Advanced mode: show every feature"));
        auto *modeNote = new QLabel(QStringLiteral("Simple keeps Bonk easy: it hides Vertex select, the Origin tool, Axis Lock, the selection box options "
                                                   "(the box takes what it touches), Prefabs and Variants, Import VMF, the Visual ghost and Collision look "
                                                   "(collision shows solid), Simplified collision and addon models in the Spawnlist Editor."));
        modeNote->setWordWrap(true);
        modeNote->setEnabled(false);
        mf->addWidget(advanced);
        mf->addWidget(modeNote);
        editTab->addWidget(modeBox);
        QObject::connect(advanced, &QCheckBox::toggled, this, [this](bool v) {
            if (syncing) return;
            p.advanced = v;
            for (QWidget *w : {static_cast<QWidget *>(selTouch), static_cast<QWidget *>(axisLock)}) w->setVisible(v);
            changed();
        });

        // Editing
        auto *editing = new QGroupBox(QStringLiteral("Editing"));
        auto *ef = new QFormLayout(editing);
        nudge = new QCheckBox(QStringLiteral("Arrow keys nudge the selection (any view, unless flying)"));
        swapAlt = new QCheckBox(QStringLiteral("Swap Alt-drag behavior"));
        swapAlt->setToolTip(QStringLiteral("Off: Alt+drag moves freely, Alt+Shift+drag stays on the grid.\n"
                                           "On: Alt+drag stays on the grid, Alt+Shift+drag moves freely."));
        rotSnap = new QComboBox;
        for (int deg : {5, 15, 45, 90}) rotSnap->addItem(QStringLiteral("%1°").arg(deg), deg);
        shiftClone = new QCheckBox(QStringLiteral("Hold Shift and move to clone the selection (2D views)"));
        shiftClone->setToolTip(QStringLiteral("With brushes selected, hold Shift on its own and move the mouse: a copy follows it.\n"
                                              "Release Shift to place it, Esc cancels."));
        ef->addRow(nudge);
        ef->addRow(shiftClone);
        pinCursor = new QCheckBox(QStringLiteral("Pin the cursor while holding G / S / Shift / R or panning"));
        pinCursor->setToolTip(QStringLiteral("The cursor stays put while you size, clone or rotate with a held key,\n"
                                             "so you never run out of room at the window edge. Turn off if cursor warping misbehaves."));
        ef->addRow(pinCursor);
        selTouch = new QCheckBox(QStringLiteral("Selection box only takes brushes fully inside it"));
        selTouch->setToolTip(QStringLiteral("Off (default): the box takes every brush it touches.\nOn: only brushes entirely inside the box."));
        ef->addRow(selTouch);
        selWait = new QCheckBox(QStringLiteral("Box select waits for Enter (select + Enter mode)"));
        selWait->setToolTip(QStringLiteral("The Select button's box half. Off (default): instant, what the box touches is selected as you drag.\n"
                                           "On: the box stays up to be adjusted; Enter selects (Ctrl+Enter adds).\n"
                                           "Clicking the box half while it's on switches between the two."));
        ef->addRow(selWait);
        axisLock = new QCheckBox(QStringLiteral("Lock G / Shift moves to one axis"));
        axisLock->setToolTip(QStringLiteral("The move stays on the axis you move along most. Alt while holding flips it for that move.\n"
                                            "Same as the Axis Lock button under Selection Options."));
        ef->addRow(axisLock);
        joinExact = new QCheckBox(QStringLiteral("J (Join Brushes) only joins when the shape stays exactly the same"));
        joinExact->setToolTip(QStringLiteral("Off (default): brushes that don't make one convex shape are wrapped in a hull.\n"
                                             "On: J refuses those; Edit → Join Brushes (Hull) still wraps them."));
        ef->addRow(joinExact);
        QObject::connect(joinExact, &QCheckBox::toggled, this, [this](bool v) { if (!syncing) { p.joinExactOnly = v; changed(); } });
        ef->addRow(swapAlt);
        ef->addRow(QStringLiteral("Rotation snaps in steps of"), rotSnap);
        editTab->addWidget(editing);
        editTab->addStretch(1);

        // Tools & keys
        auto *toolsBox = new QGroupBox(QStringLiteral("Tools && keys"));
        auto *tf = new QFormLayout(toolsBox);
        toolColor = new QComboBox;
        toolColor->addItem(QStringLiteral("System accent color"));
        toolColor->addItem(QStringLiteral("Plain gray"));
        rememberBisect = new QCheckBox(QStringLiteral("Remember the Bisect mode between sessions"));
        hammerKeys = new QCheckBox(QStringLiteral("Hammer-style keybinds (Shift+S / B / V / X)"));
        hammerKeys->setToolTip(QStringLiteral("Adds Hammer's tool shortcuts. The 1–4 keys keep working.\n"
                                              "Shift+S switches to the Selection tool when S is released\n"
                                              "without having been used for sizing."));
        tf->addRow(QStringLiteral("Tool buttons use"), toolColor);
        toolLayout = new QComboBox;
        toolLayout->addItems({QStringLiteral("Icons"), QStringLiteral("List"), QStringLiteral("Compact list")});
        tf->addRow(QStringLiteral("Tool panel layout"), toolLayout);
        tf->addRow(rememberBisect);
        tf->addRow(hammerKeys);
        applyOnClick = new QCheckBox(QStringLiteral("Texture Picker: clicking a swatch applies it right away"));
        applyOnClick->setToolTip(QStringLiteral("Off: a click selects the material; Apply or double-click applies it.\n"
                                                "On: a single click applies it."));
        startScreen = new QCheckBox(QStringLiteral("Show the start screen when Bonk opens"));
        swatchTool = new QCheckBox(QStringLiteral("Texture Picker: clicking a swatch switches to Face Select && Paint"));
        tf->addRow(swatchTool);
        tf->addRow(applyOnClick);
        tf->addRow(startScreen);
        askResize = new QCheckBox(QStringLiteral("Offer to resize images that aren't Source-sized when they're added"));
        askResize->setToolTip(QStringLiteral("Textures imported from a Source game or addon are never asked about.\n"
                                             "Compiling converts whatever it needs without asking either way."));
        tf->addRow(askResize);
        toolsTab->addWidget(toolsBox);

        // Flying in the 3D view
        auto *flyBox = new QGroupBox(QStringLiteral("Flying (3D view)"));
        auto *ff = new QFormLayout(flyBox);
        flyMin = new QSpinBox;
        flyMax = new QSpinBox;
        for (QSpinBox *b : {flyMin, flyMax}) {
            b->setRange(10, 20000);
            b->setSingleStep(50);
            b->setSuffix(QStringLiteral(" units/s"));
        }
        flyLerp = new QDoubleSpinBox;
        flyLerp->setRange(0.0, 50.0);
        flyLerp->setSingleStep(0.5);
        flyLerp->setDecimals(1);
        flyMin->setToolTip(QStringLiteral("The speed a move starts at"));
        flyMax->setToolTip(QStringLiteral("The speed it eases up to while you keep moving"));
        flyLerp->setToolTip(QStringLiteral("How quickly it gets there: 0 stays at the minimum, 3 is about a second, 20+ is almost instant"));
        ff->addRow(QStringLiteral("Minimum speed"), flyMin);
        ff->addRow(QStringLiteral("Maximum speed"), flyMax);
        ff->addRow(QStringLiteral("Speed-up (lerp)"), flyLerp);
        auto *flyNote = new QLabel(QStringLiteral("Hold Alt to fly at half speed, Shift for three times."));
        flyNote->setEnabled(false);
        ff->addRow(flyNote);
        toolsTab->addWidget(flyBox);
        QObject::connect(flyMin, &QSpinBox::valueChanged, this, [this](int v) { if (!syncing) { p.flyMin = v; changed(); } });
        QObject::connect(flyMax, &QSpinBox::valueChanged, this, [this](int v) { if (!syncing) { p.flyMax = v; changed(); } });
        QObject::connect(flyLerp, &QDoubleSpinBox::valueChanged, this, [this](double v) { if (!syncing) { p.flyLerp = v; changed(); } });
        toolsTab->addStretch(1);

        // Interface size (Appearance)
        auto *uiBox = new QGroupBox(QStringLiteral("Interface size (after restart)"));
        auto *uf = new QFormLayout(uiBox);
        density = new QComboBox;
        density->addItems({QStringLiteral("Auto (compact under 1440p)"), QStringLiteral("Compact (Low Resolution)"), QStringLiteral("Regular")});
        density->setToolTip(QStringLiteral("Takes effect the next time Bonk starts.\n"
                                           "Compact (Low Resolution): small, unsmoothed text and tighter panels, for small screens."));
        uf->addRow(QStringLiteral("Size"), density);
        lowPx = new QComboBox;
        lowPx->setToolTip(QStringLiteral("Low Resolution uses Bonk Pixel, Bonk's own pixel font: crisp at 6, 12 and 18 px."));
        uf->addRow(QStringLiteral("Text size"), lowPx);
        lowPreview = new QLabel;
        lowPreview->setToolTip(QStringLiteral("Actual size, then magnified ×2"));
        uf->addRow(lowPreview);
        lookTab->addWidget(uiBox);

        // Nodraw look
        auto *ndBox = new QGroupBox(QStringLiteral("Nodraw look"));
        auto *nf = new QFormLayout(ndBox);
        ndStyle = new QComboBox;
        ndStyle->addItems({QStringLiteral("Solid color"), QStringLiteral("RGB cycle"), QStringLiteral("Rainbow flow")});
        ndColor = new QPushButton;
        ndColor->setToolTip(QStringLiteral("Stripe color (Solid color style)"));
        ndIntensity = new QSlider(Qt::Horizontal);
        ndIntensity->setRange(0, 100);
        ndWidth = new QSlider(Qt::Horizontal);
        ndWidth->setRange(2, 48);
        ndWidth->setToolTip(QStringLiteral("How long each stripe is, and the gap after it: longer bars, longer gaps"));
        ndWidthLabel = new QLabel;
        ndWidthLabel->setMinimumWidth(44);
        ndFlow = new QSlider(Qt::Horizontal);
        ndFlow->setRange(12, 600);
        ndFlow->setToolTip(QStringLiteral("Rainbow flow: how far one whole rainbow stretches across the stripes"));
        ndFlowLabel = new QLabel;
        ndFlowLabel->setMinimumWidth(44);
        ndSpeed = new QSlider(Qt::Horizontal);
        ndSpeed->setRange(0, 200);
        ndSpeed->setToolTip(QStringLiteral("RGB cycle / Rainbow flow speed: slow drift to party mode"));
        nd2d = new QCheckBox(QStringLiteral("Show nodraw in the 2D views too"));
        auto *ndReset = new QPushButton(QStringLiteral("Classic yellow"));
        auto *colRow = new QHBoxLayout;
        colRow->addWidget(ndColor, 1);
        colRow->addWidget(ndReset);
        nf->addRow(QStringLiteral("Style"), ndStyle);
        nf->addRow(QStringLiteral("Color"), colRow);
        nf->addRow(QStringLiteral("Intensity"), ndIntensity);
        auto *wRow = new QHBoxLayout;
        wRow->addWidget(ndWidth, 1);
        wRow->addWidget(ndWidthLabel);
        nf->addRow(QStringLiteral("Stripe length"), wRow);
        auto *fRow = new QHBoxLayout;
        fRow->addWidget(ndFlow, 1);
        fRow->addWidget(ndFlowLabel);
        nf->addRow(QStringLiteral("Rainbow flow length"), fRow);
        nf->addRow(QStringLiteral("Speed"), ndSpeed);
        nf->addRow(nd2d);
        lookTab->addWidget(ndBox);

        // Budget
        auto *budBox = new QGroupBox(QStringLiteral("Budget limits (Garry's Mod studiomdl)"));
        auto *bf = new QFormLayout(budBox);
        auto bspin = [](int lo, int hi) { auto *b = new QSpinBox; b->setRange(lo, hi); b->setGroupSeparatorShown(true); return b; };
        budTris = bspin(100, 1000000);
        budVerts = bspin(100, 10000000);
        budMats = bspin(1, 1024);
        budPieces = bspin(1, 100000);
        budPieces->setToolTip(QStringLiteral("Not a hard limit: past this, the Collision pieces meter turns yellow\n"
                                             "(\"Expensive collision mesh, model may still work fine\")."));
        budTris->setToolTip(QStringLiteral("Triangles of one material: 32,768 indices / 3 = 10,922"));
        bf->addRow(QStringLiteral("Triangles per material"), budTris);
        bf->addRow(QStringLiteral("Vertices"), budVerts);
        bf->addRow(QStringLiteral("Materials"), budMats);
        bf->addRow(QStringLiteral("Collision pieces: yellow above"), budPieces);
        advTab->addWidget(budBox);
        advTab->addStretch(1);

        // Garry's Mod
        auto *gmBox = new QGroupBox(QStringLiteral("Garry's Mod && compiling"));
        auto *gf = new QFormLayout(gmBox);
        SteamFind sf;
        sf.detect();
        gmodPath = new QLineEdit;
        gmodPath->setPlaceholderText(sf.gmodDir.isEmpty() ? QStringLiteral("Not found: choose the GarrysMod folder") : QStringLiteral("Found: ") + tildePath(sf.gmodDir));
        gmodPath->setToolTip(QStringLiteral("Bonk only uses this folder for three things: running its bin/studiomdl.exe,\n"
                                            "File → Export to GMod Addons Folder (garrysmod/addons/<project>),\n"
                                            "and gmad / gmpublish when you publish from File → Manage Workshop."));
        auto *browse = new QPushButton(QStringLiteral("Browse…"));
        auto *gRow = new QHBoxLayout;
        gRow->addWidget(gmodPath, 1);
        gRow->addWidget(browse);
        gf->addRow(QStringLiteral("Garry's Mod folder"), gRow);
        proton = new QComboBox;
        proton->addItem(sf.protons.empty() ? QStringLiteral("Automatic (no Proton found)")
                                           : QStringLiteral("Automatic: %1").arg(sf.protons.front().name), QString());
        for (const auto &pr : sf.protons) proton->addItem(pr.name, pr.path);
        proton->setToolTip(QStringLiteral("studiomdl is a Windows program, so Bonk runs it through Proton.\n"
                                          "Automatic picks the newest stable Proton Steam has installed."));
        gf->addRow(QStringLiteral("Proton"), proton);
        exportAfter = new QCheckBox(QStringLiteral("Export to the GMod addons folder after every compile"));
        gf->addRow(exportAfter);
        launchGmod = new QCheckBox(QStringLiteral("Start Garry's Mod after exporting, to test it right away"));
        launchGmod->setToolTip(QStringLiteral("Through Steam (steam://rungameid/4000). If it's already running, change map to load new models."));
        gf->addRow(launchGmod);
        openAfterPublish = new QCheckBox(QStringLiteral("Open the addon's Workshop page after publishing"));
        openAfterPublish->setToolTip(QStringLiteral("In Steam when it's running, otherwise in your browser."));
        gf->addRow(openAfterPublish);
        studiomdl = new QLineEdit;
        studiomdl->setPlaceholderText(QStringLiteral("Garry's Mod's own bin/studiomdl.exe"));
        studiomdl->setToolTip(QStringLiteral("Another studiomdl.exe to compile with (e.g. StudioMDL CE), run through Proton the same way.\n"
                                             "Empty: Garry's Mod's own."));
        auto *smBrowse = new QPushButton(QStringLiteral("Browse…"));
        auto *smRow = new QHBoxLayout;
        smRow->addWidget(studiomdl, 1);
        smRow->addWidget(smBrowse);
        gf->addRow(QStringLiteral("Compiler"), smRow);
        QObject::connect(smBrowse, &QPushButton::clicked, this, [this] {
            const QString f = QFileDialog::getOpenFileName(this, QStringLiteral("studiomdl.exe to compile with"),
                                                           studiomdl->text().isEmpty() ? QDir::homePath() : QFileInfo(studiomdl->text()).absolutePath(),
                                                           QStringLiteral("studiomdl (*.exe)"));
            if (!f.isEmpty()) studiomdl->setText(f);
        });
        QObject::connect(launchGmod, &QCheckBox::toggled, this, [this](bool v) { if (!syncing) { p.launchGmod = v; changed(); } });
        QObject::connect(openAfterPublish, &QCheckBox::toggled, this, [this](bool v) { if (!syncing) { p.openAfterPublish = v; changed(); } });
        QObject::connect(studiomdl, &QLineEdit::textChanged, this, [this](const QString &v) { if (!syncing) { p.studiomdl = v.trimmed(); changed(); } });
        gmodTab->addWidget(gmBox);
        gmodTab->addStretch(1);
        QObject::connect(browse, &QPushButton::clicked, this, [this] {
            QString d = QFileDialog::getExistingDirectory(this, QStringLiteral("Garry's Mod folder (…/steamapps/common/GarrysMod)"),
                                                          gmodPath->text().isEmpty() ? QDir::homePath() : gmodPath->text());
            if (!d.isEmpty()) {
                gmodPath->setText(d);
                p.gmodPath = d;
                changed();
            }
        });
        QObject::connect(gmodPath, &QLineEdit::editingFinished, this, [this] { p.gmodPath = gmodPath->text().trimmed(); changed(); });
        QObject::connect(proton, &QComboBox::currentIndexChanged, this, [this] { if (!syncing) { p.protonPath = proton->currentData().toString(); changed(); } });
        QObject::connect(exportAfter, &QCheckBox::toggled, this, [this](bool v) { if (!syncing) { p.exportAfterCompile = v; changed(); } });

        // Grid
        auto *gridBox = new QGroupBox(QStringLiteral("Grid"));
        auto *gl = new QHBoxLayout(gridBox);
        grid = new QSlider(Qt::Horizontal);
        grid->setRange(0, 200);
        gridLabel = new QLabel;
        gridLabel->setMinimumWidth(48);
        gl->addWidget(new QLabel(QStringLiteral("Intensity")));
        gl->addWidget(grid, 1);
        gl->addWidget(gridLabel);
        lookTab->addWidget(gridBox);

        // Background
        auto *bgBox = new QGroupBox(QStringLiteral("Background color"));
        auto *bl = new QVBoxLayout(bgBox);
        auto *presetRow = new QHBoxLayout;
        for (const Preset &pr : kPresets) {
            auto *b = new QPushButton(QString::fromUtf8(pr.name));
            QColor c(pr.color);
            b->setIcon(swatch(c));
            QObject::connect(b, &QPushButton::clicked, this, [this, c] { setBg(c); });
            presetRow->addWidget(b);
        }
        bl->addLayout(presetRow);
        auto *customRow = new QHBoxLayout;
        wheel = new ColorWheel;
        brightness = new QSlider(Qt::Vertical);
        brightness->setRange(0, 1000);
        brightness->setToolTip(QStringLiteral("Brightness"));
        auto *side = new QVBoxLayout;
        preview = new QLabel;
        preview->setFixedSize(64, 40);
        hex = new QLineEdit;
        hex->setMaximumWidth(90);
        hex->setPlaceholderText(QStringLiteral("#rrggbb"));
        side->addWidget(new QLabel(QStringLiteral("Custom")));
        side->addWidget(preview);
        side->addWidget(hex);
        side->addStretch(1);
        customRow->addWidget(wheel, 1);
        customRow->addWidget(brightness);
        customRow->addLayout(side);
        bl->addLayout(customRow);
        lookTab->addWidget(bgBox);
        lookTab->addStretch(1);

        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults);
        outer->addWidget(buttons);
        {
            QSize want(0, 0);
            for (QWidget *w : pages) want = want.expandedTo(w->sizeHint());
            tabs->setMinimumWidth(want.width() + 30);
            want += QSize(60, 110);
            const QRect avail = QGuiApplication::primaryScreen() ? QGuiApplication::primaryScreen()->availableGeometry() : QRect(0, 0, 1366, 768);
            resize(std::min(want.width(), avail.width() - 40), std::min(want.height(), avail.height() - 60));
        }

        load(p);

        QObject::connect(nudge, &QCheckBox::toggled, this, [this](bool v) { p.nudge = v; changed(); });
        QObject::connect(swapAlt, &QCheckBox::toggled, this, [this](bool v) { p.swapAlt = v; changed(); });
        QObject::connect(shiftClone, &QCheckBox::toggled, this, [this](bool v) { p.shiftClone = v; changed(); });
        QObject::connect(pinCursor, &QCheckBox::toggled, this, [this](bool v) { p.pinCursor = v; changed(); });
        QObject::connect(selTouch, &QCheckBox::toggled, this, [this](bool v) { p.selInsideOnly = v; changed(); });
        QObject::connect(selWait, &QCheckBox::toggled, this, [this](bool v) { p.selBoxWait = v; changed(); });
        QObject::connect(axisLock, &QCheckBox::toggled, this, [this](bool v) { p.axisLock = v; changed(); });
        QObject::connect(askResize, &QCheckBox::toggled, this, [this](bool v) { p.askResize = v; changed(); });
        QObject::connect(budTris, &QSpinBox::valueChanged, this, [this](int v) { if (!syncing) { p.budTris = v; changed(); } });
        QObject::connect(budVerts, &QSpinBox::valueChanged, this, [this](int v) { if (!syncing) { p.budVerts = v; changed(); } });
        QObject::connect(budMats, &QSpinBox::valueChanged, this, [this](int v) { if (!syncing) { p.budMats = v; changed(); } });
        QObject::connect(budPieces, &QSpinBox::valueChanged, this, [this](int v) { if (!syncing) { p.budPieces = v; changed(); } });
        QObject::connect(toolLayout, &QComboBox::currentIndexChanged, this, [this](int i) { if (!syncing) { p.toolLayout = i; changed(); } });
        QObject::connect(density, &QComboBox::currentIndexChanged, this, [this](int i) {
            if (syncing) return;
            p.density = i;
            showLow();
            changed();
        });
        QObject::connect(ndStyle, &QComboBox::currentIndexChanged, this, [this](int i) { if (!syncing) { p.ndStyle = i; showNd(); changed(); } });
        QObject::connect(ndIntensity, &QSlider::valueChanged, this, [this](int v) { if (!syncing) { p.ndIntensity = v; changed(); } });
        QObject::connect(ndWidth, &QSlider::valueChanged, this, [this](int v) {
            ndWidthLabel->setText(QStringLiteral("%1 px").arg(v));
            if (!syncing) { p.ndWidth = v; changed(); }
        });
        QObject::connect(ndFlow, &QSlider::valueChanged, this, [this](int v) {
            ndFlowLabel->setText(QStringLiteral("%1 px").arg(v));
            if (!syncing) { p.ndFlow = v; changed(); }
        });
        QObject::connect(lowPx, &QComboBox::currentIndexChanged, this, [this] {
            if (syncing) return;
            p.lowPx = lowPx->currentData().toInt();
            showLow();
            changed();
        });
        QObject::connect(tabs, &QTabWidget::currentChanged, this, [this](int i) { p.settingsTab = i; });
        QObject::connect(ndSpeed, &QSlider::valueChanged, this, [this](int v) { if (!syncing) { p.ndSpeed = v; changed(); } });
        QObject::connect(nd2d, &QCheckBox::toggled, this, [this](bool v) { if (!syncing) { p.nd2d = v; changed(); } });
        QObject::connect(ndColor, &QPushButton::clicked, this, [this] {
            QColor c = QColorDialog::getColor(p.ndColor, this, QStringLiteral("Nodraw color"));
            if (c.isValid()) {
                p.ndColor = c;
                showNd();
                changed();
            }
        });
        QObject::connect(ndReset, &QPushButton::clicked, this, [this] {
            Prefs d;
            p.ndStyle = d.ndStyle;
            p.ndColor = d.ndColor;
            p.ndIntensity = d.ndIntensity;
            p.ndWidth = d.ndWidth;
            p.ndFlow = d.ndFlow;
            p.ndSpeed = d.ndSpeed;
            load(p);
            changed();
        });
        QObject::connect(rotSnap, &QComboBox::currentIndexChanged, this, [this] { p.rotSnap = rotSnap->currentData().toInt(); changed(); });
        QObject::connect(toolColor, &QComboBox::currentIndexChanged, this, [this](int i) { p.greyTools = i == 1; changed(); });
        QObject::connect(rememberBisect, &QCheckBox::toggled, this, [this](bool v) { p.rememberBisect = v; changed(); });
        QObject::connect(hammerKeys, &QCheckBox::toggled, this, [this](bool v) { p.hammerKeys = v; changed(); });
        QObject::connect(applyOnClick, &QCheckBox::toggled, this, [this](bool v) { p.applyOnClick = v; changed(); });
        QObject::connect(swatchTool, &QCheckBox::toggled, this, [this](bool v) { p.swatchSwitchesTool = v; changed(); });
        QObject::connect(startScreen, &QCheckBox::toggled, this, [this](bool v) { p.startScreen = v; changed(); });
        QObject::connect(grid, &QSlider::valueChanged, this, [this](int v) {
            p.gridIntensity = v;
            gridLabel->setText(QStringLiteral("%1%").arg(v));
            changed();
        });
        wheel->onChange = [this](QColor c) { setBg(c, false); };
        QObject::connect(brightness, &QSlider::valueChanged, this, [this](int v) {
            if (!syncing) wheel->setValue(v / 1000.0f);
        });
        QObject::connect(hex, &QLineEdit::editingFinished, this, [this] {
            QColor c(hex->text().trimmed());
            if (c.isValid()) setBg(c);
        });
        QObject::connect(buttons, &QDialogButtonBox::accepted, this, [this] { p.save(); accept(); });
        QObject::connect(buttons, &QDialogButtonBox::rejected, this, [this] { reject(); });
        QObject::connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, [this] {
            Prefs d;
            d.bisectMode = p.bisectMode; // mode state, not a preference
            load(d);
            p = d;
            changed();
        });
    }

protected:
    void done(int r) override {
        if (r == QDialog::Rejected) { // Cancel / Esc / close: undo the live preview
            p = original;
            changed();
        }
        QDialog::done(r);
    }

private:
    Prefs &p;
    Prefs original;
    std::function<void()> changed;
    QTabWidget *tabs;
    QComboBox *toolLayout, *density, *ndStyle, *lowPx;
    QLabel *lowPreview, *ndWidthLabel, *ndFlowLabel;
    QPushButton *ndColor;
    QSlider *ndIntensity, *ndSpeed, *ndWidth, *ndFlow;
    QCheckBox *nd2d, *selTouch, *exportAfter, *selWait, *axisLock, *askResize, *launchGmod, *joinExact, *openAfterPublish, *advanced;
    QLineEdit *studiomdl;
    QLineEdit *gmodPath;
    QComboBox *proton;
    QSpinBox *budTris, *budVerts, *budMats, *budPieces, *flyMin, *flyMax;
    QDoubleSpinBox *flyLerp;
    QCheckBox *nudge, *shiftClone, *pinCursor, *swapAlt, *rememberBisect, *hammerKeys, *applyOnClick, *startScreen, *swatchTool;
    QComboBox *rotSnap, *toolColor;
    QSlider *grid, *brightness;
    QLabel *gridLabel, *preview;
    QLineEdit *hex;
    ColorWheel *wheel;
    bool syncing = false;

    static QIcon swatch(const QColor &c) {
        QPixmap px(16, 16);
        px.fill(c);
        QPainter q(&px);
        q.setPen(QColor(128, 128, 128));
        q.drawRect(0, 0, 15, 15);
        return QIcon(px);
    }

    /// The low-resolution choices: sizes that suit the font, and a preview of it.
    void showLow(int px = -1) {
        if (px < 0) px = p.lowPx;
        const bool low = density->currentIndex() == 1, pixel = !gPixelFamily.isEmpty();
        lowPx->setEnabled(low);
        const bool was = syncing;
        syncing = true;
        lowPx->clear();
        if (pixel) for (int px : {6, 12, 18}) lowPx->addItem(QStringLiteral("%1 px (×%2)").arg(px).arg(px / 6), px);
        else for (int px = 6; px <= 11; ++px) lowPx->addItem(QStringLiteral("%1 px").arg(px), px);
        int want = pixel ? std::clamp((px + 3) / 6 * 6, 6, 18) : std::clamp(px, 6, 11);
        lowPx->setCurrentIndex(std::max(0, lowPx->findData(want)));
        syncing = was;
        // the preview: actual size, then ×2
        QFont f = pixel && !gPixelFamily.isEmpty() ? QFont(gPixelFamily) : QFontDatabase::systemFont(QFontDatabase::GeneralFont);
        f.setPixelSize(lowPx->currentData().toInt());
        f.setStyleStrategy(QFont::StyleStrategy(QFont::NoAntialias | QFont::PreferMatch));
        f.setHintingPreference(pixel ? QFont::PreferNoHinting : QFont::PreferDefaultHinting);
        const QStringList lines = {QStringLiteral("File  Edit  View  Tools — Compile Model…"), QStringLiteral("Grid 16 · 1,503 brushes · Layers × Budget")};
        const QFontMetrics fm(f);
        int w = 0;
        for (const QString &l : lines) w = std::max(w, fm.horizontalAdvance(l));
        QImage one(w + 8, fm.height() * lines.size() + 6, QImage::Format_RGB32);
        one.fill(palette().color(QPalette::Base));
        {
            QPainter q(&one);
            q.setFont(f);
            q.setPen(palette().color(QPalette::Text));
            for (int i = 0; i < lines.size(); ++i) q.drawText(4, 3 + fm.ascent() + i * fm.height(), lines[i]);
        }
        const QImage two = one.scaled(one.size() * 2, Qt::IgnoreAspectRatio, Qt::FastTransformation);
        QImage both(two.width(), one.height() + two.height() + 6, QImage::Format_RGB32);
        both.fill(palette().color(QPalette::Window));
        {
            QPainter q(&both);
            q.drawImage(0, 0, one);
            q.drawImage(0, one.height() + 6, two);
        }
        lowPreview->setPixmap(QPixmap::fromImage(both));
        lowPreview->setEnabled(low);
        lowPreview->setVisible(low);
    }

    void load(const Prefs &v) {
        syncing = true;
        nudge->setChecked(v.nudge);
        swapAlt->setChecked(v.swapAlt);
        shiftClone->setChecked(v.shiftClone);
        pinCursor->setChecked(v.pinCursor);
        selTouch->setChecked(v.selInsideOnly);
        selWait->setChecked(v.selBoxWait);
        axisLock->setChecked(v.axisLock);
        askResize->setChecked(v.askResize);
        budTris->setValue(v.budTris);
        budVerts->setValue(v.budVerts);
        budMats->setValue(v.budMats);
        budPieces->setValue(v.budPieces);
        toolLayout->setCurrentIndex(std::clamp(v.toolLayout, 0, 2));
        density->setCurrentIndex(std::clamp(v.density, 0, 2));
        tabs->setCurrentIndex(std::clamp(v.settingsTab, 0, tabs->count() - 1));
        ndStyle->setCurrentIndex(std::clamp(v.ndStyle, 0, 2));
        ndIntensity->setValue(v.ndIntensity);
        ndWidth->setValue(std::clamp(v.ndWidth, 2, 48));
        ndWidthLabel->setText(QStringLiteral("%1 px").arg(ndWidth->value()));
        ndFlow->setValue(std::clamp(v.ndFlow, 12, 600));
        ndFlowLabel->setText(QStringLiteral("%1 px").arg(ndFlow->value()));

        ndSpeed->setValue(v.ndSpeed);
        nd2d->setChecked(v.nd2d);
        showNd();
        showLow(v.lowPx);
        int i = rotSnap->findData(v.rotSnap);
        rotSnap->setCurrentIndex(i < 0 ? 1 : i);
        toolColor->setCurrentIndex(v.greyTools ? 1 : 0);
        rememberBisect->setChecked(v.rememberBisect);
        hammerKeys->setChecked(v.hammerKeys);
        applyOnClick->setChecked(v.applyOnClick);
        swatchTool->setChecked(v.swatchSwitchesTool);
        startScreen->setChecked(v.startScreen);
        gmodPath->setText(v.gmodPath);
        proton->setCurrentIndex(std::max(0, proton->findData(v.protonPath)));
        exportAfter->setChecked(v.exportAfterCompile);
        launchGmod->setChecked(v.launchGmod);
        openAfterPublish->setChecked(v.openAfterPublish);
        advanced->setChecked(v.advanced);
        flyMin->setValue(v.flyMin);
        flyMax->setValue(v.flyMax);
        flyLerp->setValue(v.flyLerp);
        selTouch->setVisible(v.advanced);
        axisLock->setVisible(v.advanced);
        studiomdl->setText(v.studiomdl);
        joinExact->setChecked(v.joinExactOnly);
        grid->setValue(v.gridIntensity);
        gridLabel->setText(QStringLiteral("%1%").arg(v.gridIntensity));
        showBg(v.bg, true);
        syncing = false;
    }

    void showNd() {
        ndColor->setText(p.ndColor.name());
        ndColor->setIcon(swatch(p.ndColor));
        ndColor->setEnabled(p.ndStyle == 0);
        ndSpeed->setEnabled(p.ndStyle != 0);
    }

    void showBg(const QColor &c, bool moveWheel) {
        syncing = true;
        if (moveWheel) wheel->setColor(c);
        brightness->setValue(int(c.valueF() * 1000));
        preview->setStyleSheet(QStringLiteral("background:%1; border:1px solid palette(mid);").arg(c.name()));
        hex->setText(c.name());
        syncing = false;
    }

    void setBg(const QColor &c, bool moveWheel = true) {
        p.bg = c;
        showBg(c, moveWheel);
        changed();
    }
};

// ------------------------------------------------------------------ viewports

class MainWindow;

class Viewport : public QOpenGLWidget {
public:
    Viewport(int id, MainWindow *mw);

    int id;
    MainWindow *mw;
    QPoint center; // capture center (3D view only)
    /// Relative mouse motion while the cursor is held in place (mouselook,
    /// hold keys). Each move counts from the previous event, not from where
    /// the cursor should be: warping back is late on X11 and ignored on
    /// Wayland, and measuring from the home point then counted the same
    /// motion again and again (brushes flying off, rotations spinning).
    struct RelMouse {
        QPointF last;
        bool valid = false, warpPending = false;
        void start(QPointF at) { last = at; valid = true; warpPending = false; }
        void reset() { valid = false; warpPending = false; }
    };
    RelMouse rel;
    /// The movement since the last event; puts the cursor back home when it
    /// strays (where the system allows it).
    QPointF relDelta(QPointF pos, QPoint home) {
        if (!rel.valid) {
            rel.start(pos);
            return {};
        }
        if (rel.warpPending && QLineF(pos, QPointF(home)).length() < 1.0) {
            rel.warpPending = false; // our own warp arriving
            rel.last = pos;
            return {};
        }
        const QPointF d = pos - rel.last;
        rel.last = pos;
        if (QLineF(pos, QPointF(home)).length() > 32.0) {
            QCursor::setPos(mapToGlobal(home));
            rel.warpPending = true;
        }
        return d;
    }

    struct Label { float x, y; int align; QString s; QColor c; };
    std::vector<Label> labels;
    QElapsedTimer gridFlash; // Alt+wheel: show the new grid size for a moment
    QElapsedTimer toolFlash; // mouse 4 / 5: the new tool's icon by the cursor
    QString toolFlashIcon;
    QPointF toolFlashAt;
    int wheelAcc = 0;
    int cursorKind = -1; // the decorated cursor in use (see MainWindow::decorateCursor)

    static void textCb(void *ctx, float x, float y, int align, const char *s, unsigned rgba) {
        auto *v = static_cast<Viewport *>(ctx);
        QColor c((rgba >> 24) & 0xff, (rgba >> 16) & 0xff, (rgba >> 8) & 0xff, rgba & 0xff);
        v->labels.push_back({x, y, align, QString::fromUtf8(s), c});
    }

protected:
    bool event(QEvent *e) override {
        // In the camera view, fly keys win over menu shortcuts (so Shift+S
        // flies backwards fast instead of switching to the selection tool).
        if (id == 0 && e->type() == QEvent::ShortcutOverride) {
            auto *k = static_cast<QKeyEvent *>(e);
            switch (k->key()) {
            case Qt::Key_W: case Qt::Key_A: case Qt::Key_S: case Qt::Key_D:
            case Qt::Key_Q: case Qt::Key_E: case Qt::Key_Z:
                // (Alt too: Alt+W flies at half speed instead of opening the Window menu)
                if (!(k->modifiers() & Qt::ControlModifier) && (k->key() != Qt::Key_Z || !(k->modifiers() & Qt::AltModifier))) {
                    e->accept();
                    return true;
                }
                break;
            default: break;
            }
        }
        return QOpenGLWidget::event(e);
    }
    void initializeGL() override { bonk_view_init(id, getProc); }
    void resizeGL(int w, int h) override { bonk_view_resize(id, float(w), float(h)); }
    void paintGL() override;
public:
    void paintOverlay(QPainter &p);
protected:
    void mousePressEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;
    void keyPressEvent(QKeyEvent *e) override;
    void keyReleaseEvent(QKeyEvent *e) override;
    void focusOutEvent(QFocusEvent *e) override;
    void enterEvent(QEnterEvent *e) override;
    void dragEnterEvent(QDragEnterEvent *e) override;
    void dragMoveEvent(QDragMoveEvent *e) override;
    void dropEvent(QDropEvent *e) override;
};

// ------------------------------------------------------------------ tool panel

/// Find a bundled asset: the icons/ folder next to the binary (installed
/// builds), else the source tree (cargo run).
QString toolIconPath(const QString &name) {
    QString installed = QCoreApplication::applicationDirPath() + QStringLiteral("/icons/") + name + QStringLiteral(".svg");
    if (QFileInfo::exists(installed)) return installed;
#ifdef BONK_ASSET_DIR
    QString dev = QStringLiteral(BONK_ASSET_DIR) + QStringLiteral("/tools/") + name + QStringLiteral(".svg");
    if (QFileInfo::exists(dev)) return dev;
#endif
    return QString();
}

/// Tool button: rounded fill in the accent color (or gray), a line-art icon,
/// and the key that selects it in the bottom-right corner. Wide buttons show
/// their name next to the icon when it fits. Everything scales with the size.
class ToolButton : public QAbstractButton {
public:
    QString iconName, badge, label;
    bool active = false;
    bool grey = false;
    bool small = false;           // a companion segment (no badge, icon only)
    ToolButton *corner = nullptr; // a segment inside this button (the Vertex tool's ↖)

    explicit ToolButton(QWidget *parent = nullptr) : QAbstractButton(parent) {
        setCursor(Qt::PointingHandCursor);
        setFocusPolicy(Qt::NoFocus);
    }

    /// Where the corner segment sits: the right end in a wide button, the top-right corner in a square one.
    void placeCorner() {
        if (!corner) return;
        const bool wide = width() > height() * 1.6;
        const int s = wide ? height() - 6 : std::max(14, height() * 2 / 5);
        corner->setGeometry(width() - s - 3, 3, s, s);
        corner->raise();
    }

protected:
    void resizeEvent(QResizeEvent *e) override { QAbstractButton::resizeEvent(e); placeCorner(); }
    void enterEvent(QEnterEvent *e) override { hovered = true; update(); QAbstractButton::enterEvent(e); }
    void leaveEvent(QEvent *e) override { hovered = false; update(); QAbstractButton::leaveEvent(e); }
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        QRectF r = QRectF(rect()).adjusted(1, 1, -1, -1);
        QColor base = grey ? QColor(0x55, 0x5a, 0x64) : palette().color(QPalette::Highlight);
        QColor fill = active ? base.lighter(122) : base.darker(hovered || isDown() ? 104 : 125);
        p.setPen(Qt::NoPen);
        p.setBrush(fill);
        const qreal rad = std::max(3.0, height() * 0.11);
        p.drawRoundedRect(r, rad, rad);
        if (active) {
            p.setPen(QPen(Qt::white, height() < 34 ? 1.5 : 2.0));
            p.setBrush(Qt::NoBrush);
            p.drawRoundedRect(r.adjusted(1, 1, -1, -1), rad - 1, rad - 1);
        }
        const bool wide = width() > height() * 1.6;
        if (small) {
            // a companion segment: just the icon, a hairline separating it from its button
            const qreal s2 = std::round(height() * 0.72);
            QString sp = toolIconPath(iconName);
            if (!sp.isEmpty())
                p.drawPixmap(QRectF((width() - s2) / 2, (height() - s2) / 2, s2, s2).toRect(), QIcon(sp).pixmap(QSize(int(s2), int(s2)), devicePixelRatioF()));
            return;
        }
        const qreal is = std::round(height() * 0.68);
        QRectF iconRect = wide ? QRectF(height() * 0.18, (height() - is) / 2 - 1, is, is) : QRectF((width() - is) / 2 - height() * 0.05, height() * 0.07, is, is);
        QString path = toolIconPath(iconName);
        QPixmap px = path.isEmpty() ? QPixmap() : QIcon(path).pixmap(QSize(int(is), int(is)), devicePixelRatioF());
        p.setOpacity(active ? 1.0 : 0.85);
        p.setPen(Qt::white);
        if (!px.isNull()) {
            p.drawPixmap(iconRect, px, QRectF(px.rect()));
        } else if (!wide) {
            QFont f = uiPx(font(), 9);
            p.setFont(f);
            p.drawText(r.adjusted(2, 2, -2, -12), Qt::AlignCenter | Qt::TextWordWrap, label);
        }
        if (wide) {
            QFont f = uiPx(font(), std::clamp(int(height() * 0.29), 10, 13));
            f.setBold(active);
            p.setFont(f);
            QRectF tr(iconRect.right() + height() * 0.18, 0,
                      width() - iconRect.right() - height() * 0.18 - (badge.isEmpty() ? 6 : 18) - (corner ? corner->width() + 4 : 0), height());
            // a label that doesn't fit gets smaller, then shortened; it never disappears
            for (int px = f.pixelSize(); px > 8 && QFontMetrics(f).horizontalAdvance(label) > tr.width(); --px) f.setPixelSize(px - 1);
            p.setFont(f);
            p.drawText(tr, Qt::AlignVCenter | Qt::AlignLeft, QFontMetrics(f).elidedText(label, Qt::ElideRight, int(tr.width())));
        }
        p.setOpacity(1.0);
        QFont f = uiPx(font(), std::clamp(int(height() * 0.25), 8, 12));
        f.setBold(true);
        p.setFont(f);
        const qreal inset = corner && wide ? corner->width() + 4 : 0;
        QRectF kr(r.right() - 14 - inset, r.bottom() - 14, 12, 13);
        p.setPen(QColor(0, 0, 0, 130));
        p.drawText(kr.translated(1, 1), Qt::AlignRight | Qt::AlignBottom, badge);
        p.setPen(Qt::white);
        p.drawText(kr, Qt::AlignRight | Qt::AlignBottom, badge);
    }

private:
    bool hovered = false;
};

/// Centered section title with a full-width underline.
class SectionHeader : public QWidget {
public:
    explicit SectionHeader(const QString &t) : text(t) { setFixedHeight(gCompact ? lowOr(20, 15) : 24); }

protected:
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setPen(palette().color(QPalette::WindowText));
        QFont f = uiPx(font(), gCompact ? 11 : 12);
        p.setFont(f);
        p.drawText(QRect(0, 0, width(), height() - 5), Qt::AlignHCenter | Qt::AlignBottom, text);
        p.setPen(QPen(palette().color(QPalette::WindowText), 1.5));
        p.drawLine(QPointF(0, height() - 1.5), QPointF(width(), height() - 1.5));
    }

private:
    QString text;
};

/// What a panel button does and how it is labeled.
struct ToolDef {
    const char *icon;
    const char *label;
    const char *name;    // tooltip
    int cmd;             // command to send, or -1 for a custom action
    int tool;            // bonk_tool() value that lights it up, or -1
    const char *key;     // default badge
    const char *hkey;    // badge with Hammer-style keys
};

const ToolDef kBrushTools[] = {
    {"select", "Select", "Selection tool", CMD_TOOL_SELECT, 0, "1", "S"},
    {"block", "Block", "Block tool", CMD_TOOL_BLOCK, 1, "2", "B"},
    {"vertex", "Vertex", "Vertex tool", CMD_TOOL_VERTEX, 2, "3", "V"},
    {"bisect-split", "Bisect", "Bisect tool", CMD_TOOL_BISECT, 3, "4", "X"},
};
const ToolDef kEntityTools[] = {
    {"mannequin", "Mannequin", "Scale mannequin (32 × 32 × 72, the player): click a surface to stand one on it", CMD_TOOL_MANNEQUIN, 4, "", ""},
    {"car", "Car", "Car (176 × 96 × 80, about Half-Life 2's jeep): click a surface to park one there.\nMeasurement buddies are never compiled.", CMD_TOOL_CAR, 4, "", ""},
};
const ToolDef kModelTools[] = {
    {"origin", "Origin", "Origin tool: place the model origin", CMD_TOOL_ORIGIN, 5, "5", "5"},
    {"face", "Paint", "Face Select & Paint: click faces to select, right-click to paint, drag to move the texture", CMD_TOOL_FACE, 6, "6", "6"},
    {"texture", "Texture", "Texture Picker", -1, -1, "T", "A"},
};
/// Toggles: lit while on
const ToolDef kSelectionOptions[] = {
    {"ignore-groups", "Ignore Groups", "Ignore Groups: clicks pick single brushes instead of their whole group", CMD_IGNORE_GROUPS, -1, "", ""},
    {"box-touch", "Box: Touching", "Selection box", CMD_TOGGLE_BOX_TOUCH, -1, "", ""},
    {"axis-lock", "Axis Lock", "Lock movement to axis: G / Shift moves stay on the axis you move along most", CMD_TOGGLE_AXIS_LOCK, -1, "", ""},
};

/// Left-hand panel: Brush Manipulation, Entities and Model Tools sections.
/// Layouts: 0 icons (three per row, sized to the pane; short rows stretch),
/// 1 list (one labeled button per row), 2 compact list.
class ToolPanel : public QWidget {
public:
    struct Entry { ToolDef def; ToolButton *button; bool inRowOf3; };
    std::vector<Entry> entries;
    std::function<void(int)> layoutChosen; // right-click menu picked a layout

    ToolPanel(std::function<void(int)> onCommand, std::function<void()> onTexture, int layoutMode, QWidget *parent = nullptr)
        : QWidget(parent), onCommand(std::move(onCommand)), onTexture(std::move(onTexture)) {
        setMinimumWidth(gCompact ? lowOr(120, 104) : 140);
        setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);
        setContextMenuPolicy(Qt::CustomContextMenu);
        QObject::connect(this, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
            QMenu m;
            const char *names[3] = {"Icons", "List", "Compact List"};
            for (int i = 0; i < 3; ++i) {
                QAction *a = m.addAction(QString::fromLatin1(names[i]), [this, i] {
                    setLayoutMode(i);
                    if (layoutChosen) layoutChosen(i);
                });
                a->setCheckable(true);
                a->setChecked(i == mode);
            }
            m.exec(mapToGlobal(pos));
        });
        setLayoutMode(layoutMode);
    }

    void setLayoutMode(int m) {
        mode = std::clamp(m, 0, 2);
        // rebuild the buttons
        if (QLayout *old = layout()) {
            QLayoutItem *it;
            while ((it = old->takeAt(0))) {
                if (it->layout()) {
                    QLayoutItem *c;
                    while ((c = it->layout()->takeAt(0))) {
                        delete c->widget();
                        delete c;
                    }
                }
                delete it->widget();
                delete it;
            }
            delete old;
        }
        entries.clear();
        auto *v = new QVBoxLayout(this);
        v->setContentsMargins(gCompact ? 6 : 10, gCompact ? 2 : 4, gCompact ? 6 : 10, gCompact ? 4 : 8);
        v->setSpacing(gCompact ? 3 : 6);
        auto section = [&](const char *title, const ToolDef *all, int count) {
            // tools hidden right now (collision editing, Simple mode) are left out
            std::vector<ToolDef> shown;
            for (int i = 0; i < count; ++i)
                if (!hidden.contains(QString::fromLatin1(all[i].icon))) shown.push_back(all[i]);
            const ToolDef *defs = shown.data();
            const int n = int(shown.size());
            if (n == 0) return;
            v->addWidget(new SectionHeader(QString::fromUtf8(title)));
            const int perRow = mode == 0 ? 3 : 1;
            for (int row = 0; row < n; row += perRow) {
                auto *h = new QHBoxLayout;
                h->setSpacing(4);
                int inRow = std::min(perRow, n - row);
                for (int k = 0; k < inRow; ++k) {
                    const ToolDef &d = defs[row + k];
                    auto *b = new ToolButton;
                    b->iconName = QString::fromLatin1(d.icon);
                    b->label = QString::fromUtf8(d.label);
                    b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
                    int c = d.cmd;
                    auto oc = onCommand;
                    auto ot = onTexture;
                    if (c >= 0) QObject::connect(b, &QAbstractButton::clicked, this, [oc, c] { oc(c); });
                    else QObject::connect(b, &QAbstractButton::clicked, this, [ot] { ot(); });
                    entries.push_back({d, b, mode == 0 && inRow == 3});
                    h->addWidget(b);
                    if (d.cmd == CMD_TOOL_SELECT) {
                        // [ Select  1 |▭] : point and click on the left, box select on the right
                        selectBox = new ToolButton(b);
                        selectBox->small = true;
                        selectBox->iconName = QStringLiteral("box-instant");
                        QObject::connect(selectBox, &QAbstractButton::clicked, this, [oc] { oc(CMD_TOGGLE_SEL_BOX_MODE); });
                        b->corner = selectBox;
                        b->placeCorner();
                    }
                    if (d.cmd == CMD_TOOL_VERTEX && !hidden.contains(QStringLiteral("vertex-pick"))) {
                        // [ Vertex  3 |↖] : the segment turns on vertex select
                        vertexPick = new ToolButton(b);
                        vertexPick->small = true;
                        vertexPick->iconName = QStringLiteral("select");
                        vertexPick->setToolTip(QStringLiteral("Vertex select: click or box vertices on any brush (Ctrl adds),\n"
                                                              "then G moves them, R turns them and S scales them.\nClick again to turn it off."));
                        QObject::connect(vertexPick, &QAbstractButton::clicked, this, [oc] { oc(CMD_TOGGLE_VERTEX_PICK); });
                        b->corner = vertexPick;
                        b->placeCorner();
                    }
                }
                if (mode == 0 && inRow == 3) h->addStretch(1);
                v->addLayout(h);
            }
            v->addSpacing(gCompact ? 3 : 6);
        };
        vertexPick = nullptr;
        selectBox = nullptr;
        section("Brush Manipulation", kBrushTools, 4);
        // Block tool: what it makes (shown while the Block tool is active)
        shapeBox = new QWidget;
        {
            auto *sv = new QVBoxLayout(shapeBox);
            sv->setContentsMargins(0, 0, 0, 0);
            sv->setSpacing(gCompact ? 2 : 4);
            sv->addWidget(new SectionHeader(QStringLiteral("Shape")));
            shapeCombo = new QComboBox;
            for (const char *n : {"Block", "Cylinder", "Wedge", "Spike", "Orb", "Arch"}) shapeCombo->addItem(QString::fromLatin1(n));
            shapeCombo->setToolTip(QStringLiteral("What the Block tool makes. Round shapes stand along the depth of the view you draw in:\n"
                                                  "drawn in the top view a cylinder stands up, in the front or side view it lies down.\n"
                                                  "Right-drag (Quick Draw) makes one a single grid unit deep."));
            sv->addWidget(shapeCombo);
            auto *fr = new QHBoxLayout;
            facesLabel = new QLabel;
            facesSlider = new QSlider(Qt::Horizontal);
            facesSlider->setRange(2, 32);
            facesSlider->setToolTip(QStringLiteral("How many sides round shapes get (orbs use 4–16)"));
            fr->addWidget(facesLabel);
            fr->addWidget(facesSlider, 1);
            sv->addLayout(fr);
            archRow = new QWidget;
            auto *ar = new QHBoxLayout(archRow);
            ar->setContentsMargins(0, 0, 0, 0);
            ar->addWidget(new QLabel(QStringLiteral("Thickness")));
            archThick = new QSpinBox;
            archThick->setRange(1, 1024);
            archThick->setSuffix(QStringLiteral(" u"));
            archThick->setToolTip(QStringLiteral("How thick the arch's ring is, in units"));
            ar->addWidget(archThick, 1);
            sv->addWidget(archRow);
            QSettings st;
            archThick->setValue(std::clamp(st.value("tools/archThickness", 16).toInt(), 1, 1024));
            const int sh = std::clamp(st.value("tools/blockShape", 0).toInt(), 0, 5);
            const int fc = std::clamp(st.value("tools/blockFaces", 8).toInt(), 3, 32);
            shapeCombo->setCurrentIndex(sh);
            facesSlider->setValue(fc);
            auto apply = [this] {
                const int sh2 = shapeCombo->currentIndex(), fc2 = facesSlider->value();
                QSettings st2;
                st2.setValue("tools/blockShape", sh2);
                st2.setValue("tools/blockFaces", fc2);
                st2.setValue("tools/archThickness", archThick->value());
                bonk_set_arch_thickness(float(archThick->value()));
                const unsigned f = bonk_set_block_shape(sh2, fc2);
                int out[3] = {0, 0, 0};
                bonk_block_shape(out);
                facesLabel->setText(sh2 == 5 ? QStringLiteral("Segments %1").arg(out[2]) : QStringLiteral("Faces %1").arg(out[2]));
                facesLabel->setEnabled(sh2 == 1 || sh2 == 3 || sh2 == 4 || sh2 == 5);
                archRow->setVisible(sh2 == 5);
                facesSlider->setEnabled(facesLabel->isEnabled());
                if (repaint) repaint(f);
            };
            QObject::connect(shapeCombo, &QComboBox::currentIndexChanged, this, apply);
            QObject::connect(facesSlider, &QSlider::valueChanged, this, apply);
            QObject::connect(archThick, &QSpinBox::valueChanged, this, apply);
            apply();
        }
        v->addWidget(shapeBox);
        section("Measurement Buddies", kEntityTools, 2);
        section("Model Tools", kModelTools, 3);
        section("Selection Options", kSelectionOptions, 3);
        v->addStretch(1);
        resizeButtons();
        if (lastSync) lastSync();
    }

    int layoutMode() const { return mode; }
    std::function<void(unsigned)> repaint; // after the shape changes
    /// Tools to leave out (by icon name, plus "vertex-pick"); rebuilds when it changes.
    QSet<QString> hidden;
    void setHidden(const QSet<QString> &h) {
        if (h == hidden) return;
        hidden = h;
        setLayoutMode(mode);
    }

    void sync(int tool, int bisectMode, bool hammer, bool grey, bool textureOpen) {
        lastSync = [this, tool, bisectMode, hammer, grey, textureOpen] { sync(tool, bisectMode, hammer, grey, textureOpen); };
        int opts[4] = {0, 1, 1, 0};
        bonk_selection_options(opts);
        if (shapeBox) shapeBox->setVisible(tool == 1);
        const int boxMode = bonk_sel_box_mode();
        if (selectBox) {
            selectBox->active = tool == 0 && boxMode != 0;
            selectBox->grey = grey;
            selectBox->iconName = QString::fromLatin1(boxMode == 2 ? "box-enter" : "box-instant");
            selectBox->setToolTip(boxMode == 0 ? QStringLiteral("Box select: every drag draws a selection box, even over brushes.\n"
                                                                  "Click again to switch between instant and select + Enter.")
                                  : boxMode == 1 ? QStringLiteral("Box select, instant: what the box touches is selected as you drag; the box\n"
                                                                  "stays up to adjust. Delete, G, S, R work on what it caught.\n"
                                                                  "Click: select + Enter mode. The Select half (or 1): point and click.")
                                                 : QStringLiteral("Box select + Enter: drag a box, adjust it (handles, S, G), Enter selects.\n"
                                                                  "Click: instant mode. The Select half (or 1): point and click."));
            selectBox->update();
        }
        if (vertexPick) {
            vertexPick->active = tool == 2 && bonk_vertex_pick();
            vertexPick->grey = grey;
            vertexPick->update();
        }
        for (Entry &e : entries) {
            ToolButton *b = e.button;
            const ToolDef &d = e.def;
            b->active = d.tool >= 0 ? d.tool == tool : textureOpen;
            if (d.cmd == CMD_TOOL_MANNEQUIN || d.cmd == CMD_TOOL_CAR) b->active = tool == 4 && bonk_place_kind() == (d.cmd == CMD_TOOL_CAR);
            if (d.cmd == CMD_TOOL_SELECT) b->active = tool == 0 && bonk_sel_box_mode() == 0;
            if (d.cmd == CMD_TOOL_VERTEX) b->active = tool == 2 && !bonk_vertex_pick();
            b->grey = grey;
            if (d.cmd == CMD_IGNORE_GROUPS || d.cmd == CMD_TOGGLE_BOX_TOUCH || d.cmd == CMD_TOGGLE_AXIS_LOCK) {
                QString tip = QString::fromUtf8(d.name);
                if (d.cmd == CMD_IGNORE_GROUPS) {
                    b->active = opts[0];
                    tip += QStringLiteral(" (Ctrl+W)");
                } else if (d.cmd == CMD_TOGGLE_BOX_TOUCH) {
                    b->active = opts[1];
                    b->iconName = QString::fromLatin1(opts[1] ? "box-touch" : "box-inside");
                    b->label = opts[1] ? QStringLiteral("Box: Touching") : QStringLiteral("Box: Inside");
                    tip = opts[1] ? QStringLiteral("Selection box takes everything it touches.\nClick: only what's fully inside")
                                  : QStringLiteral("Selection box takes only what's fully inside.\nClick: everything it touches");
                } else {
                    b->active = opts[2];
                    tip += QStringLiteral(".\nAlt while holding G / Shift flips it for that move");
                }
                b->badge.clear();
                b->setToolTip(tip);
                b->update();
                continue;
            }
            b->badge = QString::fromUtf8(hammer ? d.hkey : d.key);
            QString keys;
            if (d.cmd == -1) keys = hammer ? QStringLiteral("T or Shift+A") : QStringLiteral("T");
            else if (*d.key) keys = hammer && QString::fromUtf8(d.hkey) != QString::fromUtf8(d.key)
                                    ? QStringLiteral("%1 or Shift+%2").arg(QString::fromUtf8(d.key), QString::fromUtf8(d.hkey))
                                    : QString::fromUtf8(d.key);
            QString tip = keys.isEmpty() ? QString::fromUtf8(d.name) : QStringLiteral("%1 (%2)").arg(QString::fromUtf8(d.name), keys);
            if (d.cmd == CMD_TOOL_BISECT) {
                tip = QStringLiteral("%1 (%2 or B)").arg(QString::fromUtf8(d.name), keys);
                static const char *icons[3] = {"bisect-split", "bisect-delete", "bisect-delete-larger"};
                static const char *labels[3] = {"Bisect", "Bisect ✕", "Bisect ✕✕"};
                static const char *modes[3] = {"Split", "Split & Delete (smaller side)", "Split & Delete (larger side)"};
                const int m = std::clamp(bisectMode, 0, 2);
                b->iconName = QString::fromUtf8(icons[m]);
                b->label = QString::fromUtf8(labels[m]);
                tip += QStringLiteral("\nMode: %1. Click again (or press 4 / B) for %2.\nF flips a pending cut.")
                           .arg(QString::fromUtf8(modes[m]), QString::fromUtf8(modes[(m + 1) % 3]));
            }
            b->setToolTip(tip + QStringLiteral("\n(Right-click the panel: Icons / List / Compact List)"));
            b->update();
        }
    }

protected:
    void resizeEvent(QResizeEvent *e) override {
        QWidget::resizeEvent(e);
        resizeButtons();
    }

private:
    std::function<void(int)> onCommand;
    std::function<void()> onTexture;
    std::function<void()> lastSync;
    int mode = 0;
    ToolButton *vertexPick = nullptr;
    ToolButton *selectBox = nullptr;
    QWidget *shapeBox = nullptr;
    QComboBox *shapeCombo = nullptr;
    QSlider *facesSlider = nullptr;
    QWidget *archRow = nullptr;
    QSpinBox *archThick = nullptr;
    QLabel *facesLabel = nullptr;

    /// Icons follow the pane's width (3 per row); lists have a fixed height.
    void resizeButtons() {
        const int margins = gCompact ? lowOr(12, 8) : 20;
        int sq;
        if (mode == 0) {
            const int lo = gCompact ? lowOr(26, 18) : 30, hi = gCompact ? lowOr(40, 30) : 56;
            sq = std::clamp((width() - margins - 8) / 3, lo, hi);
        } else {
            sq = mode == 1 ? (gCompact ? lowOr(34, 24) : 44) : (gCompact ? lowOr(24, 18) : 30);
        }
        for (Entry &e : entries) {
            e.button->setFixedHeight(sq);
            if (e.inRowOf3) e.button->setFixedWidth(sq);
            else {
                e.button->setMinimumWidth(sq);
                e.button->setMaximumWidth(QWIDGETSIZE_MAX);
            }
        }
    }
};

// ------------------------------------------------------------------ layers pane

/// Layers and the groups on them. Click a layer to make it active (new
/// brushes go there), double-click to select its contents; click a group to
/// select it, double-click to rename. Right-click for more. While a
/// collision layer is active, the visual model is ghosted: the slider sets
/// how visible it is.
class LayersPane : public QWidget {
public:
    QWidget *ghostRow = nullptr;
    QWidget *lookRowW = nullptr;
    bool simple = false; // Simple mode: no Visual ghost / Collision look rows

    std::function<void(unsigned)> changed;
    std::function<void()> toggleCollisionLook;
    std::function<void()> newCollision; // + Collision: ask Complex or Simplified

    explicit LayersPane(QWidget *parent = nullptr) : QWidget(parent) {
        setMinimumWidth(gCompact ? 170 : 200);
        auto *v = new QVBoxLayout(this);
        v->setContentsMargins(gCompact ? 4 : 8, 4, gCompact ? 4 : 8, gCompact ? 4 : 8);
        v->setSpacing(gCompact ? 3 : 6);
        tree = new QTreeWidget;
        tree->setColumnCount(2);
        tree->setHeaderLabels({QStringLiteral("Show"), QStringLiteral("Layer / group")});
        tree->header()->setStretchLastSection(true);
        // a fixed narrow column for the Show boxes: sizing it to its contents
        // walked every row of big imported models on each refresh
        tree->header()->setSectionResizeMode(0, QHeaderView::Fixed);
        tree->header()->resizeSection(0, fontMetrics().horizontalAdvance(QStringLiteral("Show")) + (gCompact ? 22 : 28));
        tree->setRootIsDecorated(true);
        tree->setTreePosition(1); // nesting shows in the name column, the Show boxes stay in a straight line
        tree->setIndentation(gCompact ? 12 : 16);
        tree->setContextMenuPolicy(Qt::CustomContextMenu);
        tree->setFocusPolicy(Qt::NoFocus);
        v->addWidget(tree, 1);

        ghostRow = new QWidget;
        auto *gr = new QHBoxLayout(ghostRow);
        gr->setContentsMargins(0, 0, 0, 0);
        auto *gl = new QLabel(QStringLiteral("Visual ghost"));
        gl->setToolTip(QStringLiteral("While you edit a collision layer, the visual model is see-through and can't be clicked.\n"
                                      "This sets how visible it is. Its Show box hides it completely."));
        ghost = new QSlider(Qt::Horizontal);
        ghost->setRange(0, 100);
        ghost->setValue(QSettings().value("layers/ghostOpacity", 25).toInt());
        ghost->setToolTip(gl->toolTip());
        gr->addWidget(gl);
        gr->addWidget(ghost, 1);
        v->addWidget(ghostRow);
        // View → Collision Look, by the ghost slider: orange glass or solid pieces in their own colors
        lookRowW = new QWidget;
        auto *lookRow = new QHBoxLayout(lookRowW);
        lookRow->setContentsMargins(0, 0, 0, 0);
        auto *ll = new QLabel(QStringLiteral("Collision look"));
        lookBtn = new QToolButton;
        lookBtn->setCheckable(true);
        lookBtn->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
        lookBtn->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        lookBtn->setToolTip(QStringLiteral("Ghosted: collision is see-through orange glass.\n"
                                           "Solid: each collision piece is opaque in its own color, so you can tell them apart.\n"
                                           "Only changes how it looks (View → Collision Look)."));
        ll->setToolTip(lookBtn->toolTip());
        QObject::connect(lookBtn, &QToolButton::clicked, this, [this] { if (toggleCollisionLook) toggleCollisionLook(); });
        lookRow->addWidget(ll);
        lookRow->addWidget(lookBtn, 1);
        v->addWidget(lookRowW);
        bonk_set_ghost_opacity(ghost->value() / 100.0f);

        auto *addRow = new QHBoxLayout;
        auto *addVis = new QPushButton(QStringLiteral("+ Visual"));
        auto *addCol = new QPushButton(QStringLiteral("+ Collision"));
        addVis->setToolTip(QStringLiteral("Add a visual layer (becomes the active layer)"));
        addCol->setToolTip(QStringLiteral("Add a collision layer. If there's a visual model, it starts as a copy of it\n"
                                          "for you to simplify into the physics hull."));
        addRow->addWidget(addVis);
        addRow->addWidget(addCol);
        v->addLayout(addRow);
        auto *selRow = new QHBoxLayout;
        auto *moveBtn = new QPushButton(QStringLiteral("Move here"));
        auto *copyBtn = new QPushButton(QStringLiteral("Copy here"));
        moveBtn->setToolTip(QStringLiteral("Move the selection to the highlighted layer"));
        copyBtn->setToolTip(QStringLiteral("Copy the selection onto the highlighted layer"));
        selRow->addWidget(moveBtn);
        selRow->addWidget(copyBtn);
        v->addLayout(selRow);

        QObject::connect(ghost, &QSlider::valueChanged, this, [this](int val) {
            bonk_set_ghost_opacity(val / 100.0f);
            QSettings().setValue("layers/ghostOpacity", val);
            if (changed) changed(F_R3D | F_R2D);
        });
        // groups start folded; remember the ones you open
        // remember what you fold and unfold: layers, the Visual / Collision headings, groups
        QObject::connect(tree, &QTreeWidget::itemExpanded, this, [this](QTreeWidgetItem *it) {
            if (rebuilding) return;
            const int kind = it->data(1, Qt::UserRole + 3).toInt(), id = it->data(1, Qt::UserRole).toInt();
            if (kind == 2) openGroups.insert(id);
            else if (kind == 1) closedLayers.remove(id);
            else closedHeads.remove(it->data(0, Qt::UserRole + 5).toInt());
            // unfolded: the selected group inside may be showing now
            QTimer::singleShot(0, this, [this] { shownGroup = -1; syncSelection(); });
        });
        QObject::connect(tree, &QTreeWidget::itemCollapsed, this, [this](QTreeWidgetItem *it) {
            if (rebuilding) return;
            const int kind = it->data(1, Qt::UserRole + 3).toInt(), id = it->data(1, Qt::UserRole).toInt();
            if (kind == 2) openGroups.remove(id);
            else if (kind == 1) closedLayers.insert(id);
            else closedHeads.insert(it->data(0, Qt::UserRole + 5).toInt());
        });
        QObject::connect(addVis, &QPushButton::clicked, this, [this] { op(0, 0); });
        QObject::connect(addCol, &QPushButton::clicked, this, [this] { if (newCollision) newCollision(); else op(1, 0); });
        QObject::connect(moveBtn, &QPushButton::clicked, this, [this] { if (int id = currentLayer()) op(4, id); });
        QObject::connect(copyBtn, &QPushButton::clicked, this, [this] { if (int id = currentLayer()) op(5, id); });
        if (qEnvironmentVariableIsSet("BONK_DEBUG_LAYERS")) {
            QObject::connect(tree, &QTreeWidget::itemPressed, this, [](QTreeWidgetItem *it, int col) { fprintf(stderr, "pressed col %d %s\n", col, qPrintable(it->text(1))); });
            QObject::connect(tree, &QObject::destroyed, this, [] {});
        }
        QObject::connect(tree, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *it, int col) {
            if (qEnvironmentVariableIsSet("BONK_DEBUG_LAYERS")) fprintf(stderr, "itemClicked col %d kind %d id %d\n", col, it->data(1, Qt::UserRole + 3).toInt(), it->data(1, Qt::UserRole).toInt());
            if (col != 1) return;
            int kind = it->data(1, Qt::UserRole + 3).toInt();
            int id = it->data(1, Qt::UserRole).toInt();
            if (kind == 1) {
                keepGroup = 0;
                op(3, id);
            } else if (kind == 2) {
                keepGroup = id; // stays highlighted after the tree is rebuilt
                gop(0, id);
            }
        });
        QObject::connect(tree, &QTreeWidget::itemDoubleClicked, this, [this](QTreeWidgetItem *it, int col) {
            if (col != 1) return;
            int kind = it->data(1, Qt::UserRole + 3).toInt();
            int id = it->data(1, Qt::UserRole).toInt();
            if (kind == 1) op(6, id);
            else if (kind == 2) renameGroup(id, it->data(1, Qt::UserRole + 1).toString());
        });
        QObject::connect(tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *it, int col) {
            if (rebuilding || col != 0 || it->data(1, Qt::UserRole + 3).toInt() != 1) return;
            int id = it->data(1, Qt::UserRole).toInt();
            bool on = it->checkState(0) == Qt::Checked;
            if (on != it->data(0, Qt::UserRole).toBool()) op(7, id);
        });
        QObject::connect(tree, &QTreeWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
            QTreeWidgetItem *it = tree->itemAt(pos);
            int kind = it ? it->data(1, Qt::UserRole + 3).toInt() : 0;
            int id = it ? it->data(1, Qt::UserRole).toInt() : 0;
            QString name = it ? it->data(1, Qt::UserRole + 1).toString() : QString();
            QMenu m;
            // a Visual / Collision heading with one layer under it names that layer
            if (it && kind == 0 && it->childCount() == 1 && it->child(0)->data(1, Qt::UserRole + 3).toInt() == 1) {
                QTreeWidgetItem *only = it->child(0);
                const int lid = only->data(1, Qt::UserRole).toInt();
                const QString lname = only->data(1, Qt::UserRole + 1).toString();
                m.addAction(QStringLiteral("Set Layer Name…\tF2"), [this, lid, lname] { renameLayer(lid, lname); });
                m.addSeparator();
            }
            if (kind == 1) {
                m.addAction(QStringLiteral("Make Active"), [this, id] { op(3, id); });
                m.addAction(QStringLiteral("Set Layer Name…\tF2"), [this, id, name] { renameLayer(id, name); });
                m.addAction(QStringLiteral("Select Contents"), [this, id] { op(6, id); });
                QAction *own = m.addAction(QStringLiteral("Own Origin"), [this, id] { op(9, id); });
                own->setCheckable(true);
                own->setChecked(it->data(1, Qt::UserRole + 4).toBool());
                own->setToolTip(QStringLiteral("Give this layer its own origin (set it with the Origin tool while the layer is active)"));
                m.addSeparator();
                m.addAction(QStringLiteral("Move Selection Here"), [this, id] { op(4, id); });
                m.addAction(QStringLiteral("Copy Selection Here"), [this, id] { op(5, id); });
                m.addSeparator();
                int count = it->data(1, Qt::UserRole + 2).toInt();
                m.addAction(QStringLiteral("Delete Layer…"), [this, id, name, count] {
                    if (count > 0 && QMessageBox::question(this, QStringLiteral("Delete layer"),
                            QStringLiteral("Delete \"%1\" and the %2 item(s) on it?").arg(name).arg(count)) != QMessageBox::Yes)
                        return;
                    op(2, id);
                });
            } else if (kind == 2) {
                m.addAction(QStringLiteral("Select"), [this, id] { gop(0, id); });
                m.addAction(QStringLiteral("Duplicate"), [this, id] { gop(1, id); });
                m.addAction(QStringLiteral("Set Group Name…\tF2"), [this, id, name] { renameGroup(id, name); });
                m.addSeparator();
                m.addAction(QStringLiteral("Ungroup"), [this, id] { gop(2, id); });
                m.addAction(QStringLiteral("Delete Group and Its Brushes"), [this, id] { gop(3, id); });
            }
            m.addSeparator();
            m.addAction(QStringLiteral("Add Visual Layer"), [this] { op(0, 0); });
            m.addAction(QStringLiteral("Add Collision Layer…"), [this] { if (newCollision) newCollision(); else op(1, 0); });
            m.exec(tree->viewport()->mapToGlobal(pos));
        });
    }

    /// F2: name what's highlighted (a layer, a group, or the only layer under a heading).
    void renameCurrent() {
        QTreeWidgetItem *it = tree->currentItem();
        if (!it) return;
        int kind = it->data(1, Qt::UserRole + 3).toInt();
        if (kind == 0 && it->childCount() == 1) {
            it = it->child(0);
            kind = it->data(1, Qt::UserRole + 3).toInt();
        }
        const int id = it->data(1, Qt::UserRole).toInt();
        const QString name = it->data(1, Qt::UserRole + 1).toString();
        if (kind == 1) renameLayer(id, name);
        else if (kind == 2) renameGroup(id, name);
    }

    /// Developer aids: print the highlighted row and what's expanded; fold a layer like a click would.
    void debugCurrent() {
        QTreeWidgetItem *c = tree->currentItem();
        fprintf(stderr, "current: %s\n", c ? qPrintable(c->text(1).isEmpty() ? c->text(0) : c->text(1)) : "(none)");
        for (QTreeWidgetItemIterator it(tree); *it; ++it)
            if ((*it)->data(1, Qt::UserRole + 3).toInt() != 2) fprintf(stderr, "  %s %s\n", (*it)->isExpanded() ? "open  " : "folded", qPrintable((*it)->text(1).isEmpty() ? (*it)->text(0) : (*it)->text(1)));
    }
    void debugFold(int layerId) {
        auto it = layerRows.find(layerId);
        if (it != layerRows.end()) it->second->setExpanded(false);
    }

    /// A group picked in a view lights up here too (its parent groups open,
    /// but layers and headings you folded stay folded).
    void syncSelection() {
        const int g = bonk_selected_group();
        if (g == shownGroup) return;
        shownGroup = g;
        keepGroup = g;
        const QSignalBlocker block(tree);
        auto it = groupRows.find(g);
        if (g && it != groupRows.end()) {
            rebuilding = true; // expanding here isn't the user's choice: don't remember it as such…
            for (QTreeWidgetItem *p = it->second->parent(); p && p->data(1, Qt::UserRole + 3).toInt() == 2; p = p->parent()) {
                p->setExpanded(true);
                openGroups.insert(p->data(1, Qt::UserRole).toInt()); // …but keep them open on rebuilds
            }
            rebuilding = false;
            // highlighting a row inside something folded would unfold it: then the
            // folded row it's in lights up instead
            QTreeWidgetItem *target = it->second;
            for (QTreeWidgetItem *p = it->second->parent(); p; p = p->parent())
                if (!p->isExpanded()) target = p;
            if (!(target->flags() & Qt::ItemIsSelectable)) target = nullptr; // a folded heading
            tree->setCurrentItem(target);
            if (target) tree->scrollToItem(target);
        } else if (QTreeWidgetItem *cur = tree->currentItem(); cur && cur->data(1, Qt::UserRole + 3).toInt() == 2) {
            // the selection isn't a whole group any more: back to its layer
            QTreeWidgetItem *l = cur;
            while (l && l->data(1, Qt::UserRole + 3).toInt() == 2) l = l->parent();
            tree->setCurrentItem(l);
        }
    }

    void syncCollisionLook(bool solid) {
        lookBtn->setChecked(solid);
        lookBtn->setText(solid ? QStringLiteral("Solid") : QStringLiteral("Ghosted"));
        QString ic = toolIconPath(solid ? QStringLiteral("collision-solid") : QStringLiteral("collision-ghost"));
        lookBtn->setIcon(ic.isEmpty() ? QIcon() : QIcon(ic));
    }

    /// Developer aid: click a layer's Show box.
    void clickShow(int layerId) {
        for (QTreeWidgetItemIterator it(tree); *it; ++it)
            if ((*it)->data(1, Qt::UserRole + 3).toInt() == 1 && (*it)->data(1, Qt::UserRole).toInt() == layerId) {
                // a real click on the box, the way a mouse does it
                tree->scrollToItem(*it);
                const QRect r = tree->visualItemRect(*it);
                const int x = tree->header()->sectionPosition(0) + tree->header()->sectionSize(0) / 2;
                const QPoint p(std::min(x, 12), r.center().y());
                QMouseEvent press(QEvent::MouseButtonPress, p, tree->viewport()->mapToGlobal(p), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QMouseEvent release(QEvent::MouseButtonRelease, p, tree->viewport()->mapToGlobal(p), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(tree->viewport(), &press);
                QApplication::sendEvent(tree->viewport(), &release);
                return;
            }
    }

    /// Developer aid: click the n-th group row's name, the way a mouse does.
    void clickGroup(int n) {
        int k = 0;
        for (QTreeWidgetItemIterator it(tree); *it; ++it)
            if ((*it)->data(1, Qt::UserRole + 3).toInt() == 2 && (*it)->parent() && (*it)->parent()->data(1, Qt::UserRole + 3).toInt() == 1 && k++ == n) {
                tree->scrollToItem(*it);
                const QRect r = tree->visualItemRect(*it);
                const QPoint p(tree->header()->sectionPosition(1) + tree->header()->sectionSize(1) / 2, r.center().y());
                QMouseEvent press(QEvent::MouseButtonPress, p, tree->viewport()->mapToGlobal(p), Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
                QMouseEvent release(QEvent::MouseButtonRelease, p, tree->viewport()->mapToGlobal(p), Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
                QApplication::sendEvent(tree->viewport(), &press);
                QApplication::sendEvent(tree->viewport(), &release);
                fprintf(stderr, "clicked group row %d (%s)\n", n, qPrintable((*it)->text(1)));
                return;
            }
    }

    /// Rebuild the tree if the document's layers or groups changed.
    void refresh(bool force = false) {
        ghostRow->setVisible(bonk_editing_collision() && !simple);
        if (lookRowW) lookRowW->setVisible(!simple);
        unsigned long long rev = bonk_layers_rev();
        if (!force && rev == lastRev) return;
        lastRev = rev;
        const int keep = currentLayer();
        rebuilding = true;
        tree->setUpdatesEnabled(false);
        const QSignalBlocker block(tree);
        tree->clear();
        QFont groupFont = font();
        groupFont.setBold(true);
        QTreeWidgetItem *heads[2];
        const char *gnames[2] = {"Visual", "Collision"};
        for (int g = 0; g < 2; ++g) {
            heads[g] = new QTreeWidgetItem(tree);
            heads[g]->setText(0, QString::fromLatin1(gnames[g]));
            heads[g]->setFirstColumnSpanned(true);
            heads[g]->setFont(0, groupFont);
            heads[g]->setFlags(Qt::ItemIsEnabled);
            heads[g]->setData(0, Qt::UserRole + 5, g);
            if (g == 1) heads[g]->setForeground(0, QColor(255, 143, 41));
        }
        std::map<int, QTreeWidgetItem *> layerItems;
        QTreeWidgetItem *sel = nullptr;
        int n = bonk_layer_count();
        for (int i = 0; i < n; ++i) {
            int info[6] = {0};
            QString name = QString::fromUtf8(bonk_layer_info(i, info));
            auto *it = new QTreeWidgetItem(heads[info[1] ? 1 : 0]);
            it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
            it->setCheckState(0, info[2] ? Qt::Checked : Qt::Unchecked);
            it->setData(0, Qt::UserRole, bool(info[2]));
            it->setData(1, Qt::UserRole, info[0]);
            it->setData(1, Qt::UserRole + 1, name);
            it->setData(1, Qt::UserRole + 2, info[5]);
            it->setData(1, Qt::UserRole + 3, 1);
            it->setText(1, QStringLiteral("%1%2  (%3)").arg(info[4] ? QStringLiteral("● ") : QString(), name).arg(info[5]));
            it->setToolTip(1, info[4] ? QStringLiteral("Active layer: new brushes go here") : QStringLiteral("Click to make active, double-click to select its contents"));
            if (info[4]) {
                QFont bf = font();
                bf.setBold(true);
                it->setFont(1, bf);
            }
            layerItems[info[0]] = it;
            if (info[0] == keep || (!keep && info[4])) sel = it;
        }
        // groups under their layer (nested groups under their parent)
        int gn = bonk_group_count();
        struct G { int id, parent, layer, count; QString name; };
        std::vector<G> gs;
        for (int i = 0; i < gn; ++i) {
            int gi[4] = {0};
            QString nm = QString::fromUtf8(bonk_group_info(i, gi));
            gs.push_back({gi[0], gi[1], gi[2], gi[3], nm});
        }
        std::map<int, QTreeWidgetItem *> groupItems;
        std::function<void(int, QTreeWidgetItem *)> addChildren = [&](int parent, QTreeWidgetItem *under) {
            for (const G &g : gs) {
                if (g.parent != parent) continue;
                QTreeWidgetItem *host = under;
                if (!host) {
                    auto li = layerItems.find(g.layer);
                    if (li == layerItems.end()) continue;
                    host = li->second;
                }
                auto *gi = new QTreeWidgetItem(host);
                gi->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
                gi->setText(1, QStringLiteral("▣ %1  (%2)").arg(g.name).arg(g.count));
                gi->setData(1, Qt::UserRole, g.id);
                gi->setData(1, Qt::UserRole + 1, g.name);
                gi->setData(1, Qt::UserRole + 3, 2);
                gi->setToolTip(1, QStringLiteral("Group: click to select it, double-click to rename, right-click to duplicate or ungroup"));
                groupItems[g.id] = gi;
                addChildren(g.id, gi);
            }
        };
        addChildren(0, nullptr);
        for (int g = 0; g < 2; ++g) heads[g]->setExpanded(!closedHeads.contains(g));
        for (auto &li : layerItems) li.second->setExpanded(!closedLayers.contains(li.first));
        for (auto &gi : groupItems) gi.second->setExpanded(openGroups.contains(gi.first));
        groupRows = groupItems;
        layerRows = layerItems;
        // the selected group stays highlighted (not its layer): clicked here or picked in a view
        auto kg = keepGroup ? groupItems.find(keepGroup) : groupItems.end();
        if (kg != groupItems.end()) tree->setCurrentItem(kg->second);
        else if (sel) tree->setCurrentItem(sel);
        if (kg == groupItems.end()) keepGroup = 0;
        tree->setUpdatesEnabled(true);
        rebuilding = false;
    }

private:
    QTreeWidget *tree;
    QSlider *ghost;
    QToolButton *lookBtn;
    int keepGroup = 0, shownGroup = -1;
    QSet<int> closedLayers, closedHeads;
    std::map<int, QTreeWidgetItem *> groupRows, layerRows;
    unsigned long long lastRev = 0;
    bool rebuilding = false;
    QSet<int> openGroups;

    int currentLayer() const {
        QTreeWidgetItem *it = tree->currentItem();
        while (it && it->data(1, Qt::UserRole + 3).toInt() == 2) it = it->parent();
        return it && it->data(1, Qt::UserRole + 3).toInt() == 1 ? it->data(1, Qt::UserRole).toInt() : 0;
    }
    /// The tree is rebuilt after the click that caused the change has
    /// finished (rebuilding inside the item's own signal crashed).
    /// Everything that follows a click in the tree happens after the click's
    /// own handler has returned: refreshing the window rebuilds this tree,
    /// and deleting the item Qt is still in the middle of handling crashed.
    void emitChanged(unsigned f) {
        QTimer::singleShot(0, this, [this, f] {
            if (changed) changed(f);
            refresh(true);
        });
    }
    void op(int o, int id) { emitChanged(bonk_layer_op(o, id)); }
    void gop(int o, int id) { emitChanged(bonk_group_op(o, id)); }
    void renameLayer(int id, const QString &cur) {
        bool ok = false;
        QString n = QInputDialog::getText(this, QStringLiteral("Set layer name"), QStringLiteral("Name:"), QLineEdit::Normal, cur, &ok);
        if (ok && !n.trimmed().isEmpty()) {
            QByteArray b = n.trimmed().toUtf8();
            emitChanged(bonk_layer_rename(id, b.constData()));
        }
    }
    void renameGroup(int id, const QString &cur) {
        bool ok = false;
        QString n = QInputDialog::getText(this, QStringLiteral("Rename group"), QStringLiteral("Name (e.g. Door, Window):"), QLineEdit::Normal, cur, &ok);
        if (ok) {
            QByteArray b = n.toUtf8();
            emitChanged(bonk_group_rename(id, b.constData()));
        }
    }
};

// ------------------------------------------------------------------ paths

/// Clean a game path the way Source wants it: lowercase, forward slashes,
/// spaces become underscores, no doubled or trailing slashes. Characters that
/// can't appear in a game path become underscores and are reported in `bad`.
QString cleanGamePath(const QString &in, QString *bad = nullptr) {
    QString s = in.trimmed().toLower();
    s.replace(QLatin1Char('\\'), QLatin1Char('/'));
    QString out;
    for (QChar c : s) {
        if (c == QLatin1Char(' ')) {
            out += QLatin1Char('_');
        } else if ((c >= QLatin1Char('a') && c <= QLatin1Char('z')) || (c >= QLatin1Char('0') && c <= QLatin1Char('9')) ||
                   c == QLatin1Char('_') || c == QLatin1Char('-') || c == QLatin1Char('/') || c == QLatin1Char('.')) {
            out += c;
        } else {
            if (bad && !bad->contains(c)) *bad += c;
            out += QLatin1Char('_');
        }
    }
    while (out.contains(QStringLiteral("//"))) out.replace(QStringLiteral("//"), QStringLiteral("/"));
    while (out.startsWith(QLatin1Char('/'))) out.remove(0, 1);
    while (out.endsWith(QLatin1Char('/'))) out.chop(1);
    return out;
}

/// A model path as typed by the user, without "models/" or ".mdl".
QString cleanModelPath(const QString &in, QString *bad = nullptr) {
    QString s = cleanGamePath(in, bad);
    if (s.startsWith(QStringLiteral("models/"))) s.remove(0, 7);
    if (s.endsWith(QStringLiteral(".mdl"))) s.chop(4);
    return s;
}

/// A project name usable as a folder and a path component.
QString cleanProjectName(const QString &in) {
    QString s = cleanGamePath(in);
    s.replace(QLatin1Char('/'), QLatin1Char('_'));
    s.replace(QLatin1Char('.'), QLatin1Char('_'));
    return s;
}

// ------------------------------------------------------------------ project file

/// A Bonk project: a folder with a .bonkpj file, assets/, Model Projects/
/// and the addon/ folder that gets uploaded to the Workshop.
struct Project {
    QString file; // absolute path of the .bonkpj; empty = no project open
    QString name;
    QString basePath = QString();                              // models/<base>/<model>.mdl
    QString texturePool = QString();                           // shared VTFs
    QString modelMaterials = QStringLiteral("materials/models/{model}"); // per-model VMTs

    bool isOpen() const { return !file.isEmpty(); }
    QString dir() const { return QFileInfo(file).absolutePath(); }
    QString texturesDir() const { return dir() + QStringLiteral("/assets/Textures"); }
    QString originalsDir() const { return dir() + QStringLiteral("/assets/Original Textures"); }
    QString modelsDir() const { return dir() + QStringLiteral("/Model Projects"); }
    QString addonDir() const { return dir() + QStringLiteral("/addon/") + name; }
    QString buildDir() const { return dir() + QStringLiteral("/.build"); }
    QString materialsFor(const QString &modelPath) const {
        QString p = modelMaterials;
        p.replace(QStringLiteral("{model}"), modelPath.isEmpty() ? QStringLiteral("<model path>") : modelPath);
        return p;
    }
    /// Default model path for a new model file called `stem`.
    QString defaultModelPath(const QString &stem) const {
        QString s = cleanModelPath(stem);
        return basePath.isEmpty() ? s : basePath + QLatin1Char('/') + s;
    }

    static QString quoted(const QString &k, const QString &v) {
        QString e = v;
        e.replace(QLatin1Char('"'), QLatin1Char('\''));
        return QStringLiteral("\t\"%1\" \"%2\"\n").arg(k, e);
    }

    bool save(QString *err) const {
        QFile f(file);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
            if (err) *err = QStringLiteral("Can't write %1: %2").arg(file, f.errorString());
            return false;
        }
        QString o = QStringLiteral("// Bonk! Model Prop-inator! project\n\"BonkProject\"\n{\n");
        o += quoted(QStringLiteral("version"), QStringLiteral("1"));
        o += quoted(QStringLiteral("name"), name);
        o += quoted(QStringLiteral("base_path"), basePath);
        o += quoted(QStringLiteral("texture_pool"), texturePool);
        o += quoted(QStringLiteral("model_materials"), modelMaterials);
        o += QStringLiteral("}\n");
        f.write(o.toUtf8());
        return true;
    }

    bool load(const QString &path, QString *err) {
        QFile f(path);
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
            if (err) *err = QStringLiteral("Can't read %1: %2").arg(path, f.errorString());
            return false;
        }
        const QString text = QString::fromUtf8(f.readAll());
        if (!text.contains(QStringLiteral("\"BonkProject\""))) {
            if (err) *err = QStringLiteral("%1 isn't a Bonk! project file.").arg(QFileInfo(path).fileName());
            return false;
        }
        Project p;
        p.file = QFileInfo(path).absoluteFilePath();
        p.name = QFileInfo(path).completeBaseName();
        static const QRegularExpression kv(QStringLiteral("\"([^\"]*)\"\\s+\"([^\"]*)\""));
        auto it = kv.globalMatch(text);
        while (it.hasNext()) {
            auto m = it.next();
            const QString k = m.captured(1), v = m.captured(2);
            if (k == QStringLiteral("name") && !v.isEmpty()) p.name = v;
            else if (k == QStringLiteral("base_path")) p.basePath = v;
            else if (k == QStringLiteral("texture_pool")) p.texturePool = v;
            else if (k == QStringLiteral("model_materials") && !v.isEmpty()) p.modelMaterials = v;
        }
        if (p.texturePool.isEmpty()) p.texturePool = QStringLiteral("materials/") + p.name;
        *this = p;
        return true;
    }

    /// Create the folder layout (existing folders are left alone).
    bool createFolders(QString *err) const {
        QDir d;
        for (const QString &sub : {texturesDir(), originalsDir(), modelsDir(), addonDir() + QStringLiteral("/models"),
                                   addonDir() + QStringLiteral("/materials"), buildDir()}) {
            if (!d.mkpath(sub)) {
                if (err) *err = QStringLiteral("Couldn't create %1").arg(sub);
                return false;
            }
        }
        return true;
    }
};

/// Find the project a model file belongs to: a .bonkpj in its folder or up to
/// three folders above it.
QString findProjectFor(const QString &modelFile) {
    QDir d = QFileInfo(modelFile).absoluteDir();
    for (int up = 0; up < 4; ++up) {
        const QStringList pj = d.entryList({QStringLiteral("*.bonkpj")}, QDir::Files);
        if (!pj.isEmpty()) return d.absoluteFilePath(pj.first());
        if (!d.cdUp()) break;
    }
    return QString();
}


QStringList recentProjects() {
    QStringList out;
    for (const QString &p : QSettings().value("projects/recent").toStringList())
        if (QFileInfo::exists(p)) out << p;
    return out;
}

void addRecentProject(const QString &path) {
    QStringList r = QSettings().value("projects/recent").toStringList();
    r.removeAll(path);
    r.prepend(path);
    while (r.size() > 8) r.removeLast();
    QSettings().setValue("projects/recent", r);
}

/// Recently opened or saved models (missing files dropped), newest first.
QStringList recentModels() {
    QStringList out;
    for (const QString &p : QSettings().value("models/recent").toStringList())
        if (QFileInfo::exists(p)) out << p;
    return out;
}

void addRecentModel(const QString &path) {
    QStringList r = QSettings().value("models/recent").toStringList();
    r.removeAll(path);
    r.prepend(path);
    while (r.size() > 10) r.removeLast();
    QSettings().setValue("models/recent", r);
}

/// "house_1 (housepack)": the model and the project it belongs to.
QString modelLabel(const QString &path) {
    const QString pj = findProjectFor(path);
    QString name;
    if (!pj.isEmpty()) {
        Project p;
        QString e;
        name = p.load(pj, &e) ? p.name : QFileInfo(pj).completeBaseName();
    }
    return QStringLiteral("%1 (%2)").arg(QFileInfo(path).completeBaseName(), name.isEmpty() ? QStringLiteral("no project") : name);
}

QString defaultProjectsDir() {
    QString d = QSettings().value("projects/location").toString();
    return d.isEmpty() ? QDir::homePath() + QStringLiteral("/Bonk Projects") : d;
}

QLabel *hintLabel(const QString &html) {
    auto *l = new QLabel(html);
    l->setWordWrap(true);
    l->setFont(uiScaled(l->font(), 0.9));
    QPalette pal = l->palette();
    pal.setColor(QPalette::WindowText, pal.color(QPalette::PlaceholderText));
    l->setPalette(pal);
    return l;
}

// ------------------------------------------------------------------ new project dialog

/// New Project (or, with an existing project, Project Settings): name,
/// location and the game paths, with a live preview of the folders.
class ProjectDialog : public QDialog {
public:
    Project result;

    ProjectDialog(QWidget *parent, const Project *existing = nullptr) : QDialog(parent), editing(existing != nullptr) {
        setWindowTitle(editing ? QStringLiteral("Project Settings") : QStringLiteral("New Project"));
        auto *v = new QVBoxLayout(this);
        auto *form = new QFormLayout;
        name = new QLineEdit;
        name->setPlaceholderText(QStringLiteral("Name of your project, e.g. My Model, My Pack"));
        location = new QLineEdit(defaultProjectsDir());
        auto *browse = new QPushButton(QStringLiteral("Browse…"));
        auto *locRow = new QHBoxLayout;
        locRow->addWidget(location, 1);
        locRow->addWidget(browse);
        base = new QLineEdit;
        pool = new QLineEdit;
        mats = new QLineEdit(QStringLiteral("materials/models/{model}"));
        base->setToolTip(QStringLiteral("New models go to models/<base path>/<model name>.mdl.\n"
                                        "Each model can change its own path in Model Properties."));
        pool->setToolTip(QStringLiteral("Shared textures: every model's VMTs point at the VTFs in here."));
        mats->setToolTip(QStringLiteral("Where each model's VMTs go. {model} is the model's path."));
        form->addRow(QStringLiteral("Project name"), name);
        form->addRow(QStringLiteral("Location"), locRow);
        form->addRow(QStringLiteral("Model base path"), base);
        form->addRow(QStringLiteral("Shared texture pool"), pool);
        form->addRow(QStringLiteral("Model materials"), mats);
        v->addLayout(form);
        preview = new QLabel;
        preview->setTextFormat(Qt::PlainText);
        QFont mono = QFontDatabase::systemFont(QFontDatabase::FixedFont);
        preview->setFont(mono);
        preview->setFrameShape(QFrame::StyledPanel);
        preview->setMargin(8);
        preview->setTextInteractionFlags(Qt::TextSelectableByMouse);
        preview->setMinimumHeight(QFontMetrics(mono).lineSpacing() * 9 + 20);
        preview->setAlignment(Qt::AlignLeft | Qt::AlignTop);
        v->addWidget(preview);
        warn = new QLabel;
        warn->setWordWrap(true);
        warn->setStyleSheet(QStringLiteral("color:#e0823d"));
        v->addWidget(warn);
        v->addWidget(hintLabel(QStringLiteral("Drop PNG or JPG files into <b>assets/Textures</b> (subfolders become groups). "
                                              "The <b>addon</b> folder is what gets uploaded to the Workshop; the hidden "
                                              "<b>.build</b> folder is scratch space for the compiler.")));
        buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        buttons->button(QDialogButtonBox::Ok)->setText(editing ? QStringLiteral("Save") : QStringLiteral("Create Project"));
        v->addWidget(buttons);

        if (editing) {
            result = *existing;
            name->setText(existing->name);
            name->setEnabled(false);
            location->setText(QFileInfo(existing->dir()).absolutePath());
            location->setEnabled(false);
            browse->setEnabled(false);
            base->setText(existing->basePath);
            pool->setText(existing->texturePool);
            mats->setText(existing->modelMaterials);
            baseEdited = poolEdited = true;
        }

        QObject::connect(name, &QLineEdit::textChanged, this, [this] {
            QString n = cleanProjectName(name->text());
            if (!baseEdited) base->setText(n);
            if (!poolEdited) pool->setText(n.isEmpty() ? QString() : QStringLiteral("materials/") + n);
            update_();
        });
        QObject::connect(base, &QLineEdit::textEdited, this, [this] { baseEdited = true; update_(); });
        QObject::connect(pool, &QLineEdit::textEdited, this, [this] { poolEdited = true; update_(); });
        QObject::connect(mats, &QLineEdit::textEdited, this, [this] { update_(); });
        QObject::connect(location, &QLineEdit::textChanged, this, [this] { update_(); });
        QObject::connect(browse, &QPushButton::clicked, this, [this] {
            QDir().mkpath(location->text());
            QString d = QFileDialog::getExistingDirectory(this, QStringLiteral("Where should the project folder go?"), location->text());
            if (!d.isEmpty()) location->setText(d);
        });
        QObject::connect(buttons, &QDialogButtonBox::accepted, this, [this] { finish(); });
        QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        update_();
        resize(560, sizeHint().height());
    }

private:
    bool editing;
    bool baseEdited = false, poolEdited = false;
    QLineEdit *name, *location, *base, *pool, *mats;
    QLabel *preview, *warn;
    QDialogButtonBox *buttons;

    Project gather() const {
        Project p = editing ? result : Project();
        if (!editing) {
            p.name = cleanProjectName(name->text());
            QString dir = QDir(location->text().trimmed()).absoluteFilePath(p.name);
            p.file = dir + QLatin1Char('/') + p.name + QStringLiteral(".bonkpj");
        }
        p.basePath = cleanModelPath(base->text());
        p.texturePool = cleanGamePath(pool->text());
        QString m = mats->text().trimmed();
        m.replace(QStringLiteral("{model}"), QStringLiteral("__model__"));
        m = cleanGamePath(m);
        m.replace(QStringLiteral("__model__"), QStringLiteral("{model}"));
        if (!m.contains(QStringLiteral("{model}"))) m += QStringLiteral("/{model}");
        p.modelMaterials = m;
        return p;
    }

    void update_() {
        Project p = gather();
        QString issues;
        if (p.name.isEmpty()) issues += QStringLiteral("Give the project a name. ");
        if (!editing && !p.name.isEmpty() && QFileInfo::exists(p.dir()) && !QDir(p.dir()).isEmpty())
            issues += QStringLiteral("%1 already exists and isn't empty. ").arg(tildePath(p.dir()));
        if (!p.texturePool.startsWith(QStringLiteral("materials/")))
            issues += QStringLiteral("The texture pool should start with materials/. ");
        if (!p.modelMaterials.startsWith(QStringLiteral("materials/")))
            issues += QStringLiteral("Model materials should start with materials/. ");
        if (name->text().trimmed() != p.name && !name->text().trimmed().isEmpty() && !editing)
            issues += QStringLiteral("The folder will be called \"%1\" (lowercase, no spaces). ").arg(p.name);
        warn->setText(issues.trimmed());
        warn->setVisible(!issues.isEmpty());
        buttons->button(QDialogButtonBox::Ok)->setEnabled(!p.name.isEmpty() && (editing || !QFileInfo::exists(p.file)));

        const QString n = p.name.isEmpty() ? QStringLiteral("<name>") : p.name;
        const QString ex = p.defaultModelPath(QStringLiteral("house_1"));
        QString t;
        t += tildePath(p.name.isEmpty() ? location->text() + QStringLiteral("/<name>") : p.dir()) + QStringLiteral("/\n");
        t += QStringLiteral("├── %1.bonkpj\n").arg(n);
        t += QStringLiteral("├── assets/Textures/         your PNG / JPG files\n");
        t += QStringLiteral("├── Model Projects/          .bpmdl models\n");
        t += QStringLiteral("├── .build/                  compiler scratch (hidden)\n");
        t += QStringLiteral("└── addon/%1/\n").arg(n);
        t += QStringLiteral("    ├── models/%1.mdl\n").arg(ex);
        t += QStringLiteral("    ├── %1/   shared VTFs\n").arg(p.texturePool);
        t += QStringLiteral("    └── %1/   VMTs").arg(p.materialsFor(ex));
        preview->setText(t);
    }

    void finish() {
        Project p = gather();
        QString err;
        if (!editing) {
            QDir().mkpath(p.dir());
            QSettings().setValue("projects/location", QFileInfo(p.dir()).absolutePath());
        }
        if (!p.createFolders(&err) || !p.save(&err)) {
            QMessageBox::critical(this, windowTitle(), err);
            return;
        }
        result = p;
        accept();
    }
};

// ------------------------------------------------------------------ start screen

/// Shown when Bonk starts without a file: New Project, Open Project, recents.
class StartScreen : public QDialog {
public:
    enum Choice { None, NewProject, OpenProject, OpenRecent, OpenModel };
    Choice choice = None;
    QString recentPath;

    explicit StartScreen(QWidget *parent) : QDialog(parent) {
        setWindowTitle(QString::fromUtf8(kAppName));
        auto *h = new QHBoxLayout(this);
        h->setContentsMargins(24, 24, 24, 18);
        h->setSpacing(24);

        auto *left = new QVBoxLayout;
        auto *logo = new QLabel;
        QIcon ic = QApplication::windowIcon();
        if (!ic.isNull()) logo->setPixmap(ic.pixmap(QSize(112, 112), devicePixelRatioF()));
        logo->setAlignment(Qt::AlignHCenter);
        left->addWidget(logo);
        auto *title = new QLabel(QStringLiteral("<span style='font-size:17pt; font-weight:bold'>Bonk!</span><br>"
                                                "<span style='font-size:11pt'>Model Prop-inator!</span>"));
        title->setAlignment(Qt::AlignHCenter);
        left->addWidget(title);
        auto *ver = hintLabel(QStringLiteral("version %1").arg(QStringLiteral(BONK_VERSION)));
        ver->setAlignment(Qt::AlignHCenter);
        left->addWidget(ver);
        left->addSpacing(14);
        auto *newBtn = new QPushButton(QStringLiteral("New Project…"));
        auto *openBtn = new QPushButton(QStringLiteral("Open Project…"));
        auto *skipBtn = new QPushButton(QStringLiteral("Just Start Modeling"));
        newBtn->setDefault(true);
        for (QPushButton *b : {newBtn, openBtn, skipBtn}) {
            b->setMinimumHeight(34);
            left->addWidget(b);
        }
        skipBtn->setToolTip(QStringLiteral("Model without a project. You can create or open one later from the File menu."));
        left->addStretch(1);
        showAgain = new QCheckBox(QStringLiteral("Show at startup"));
        showAgain->setChecked(QSettings().value("ui/startScreen", true).toBool());
        left->addWidget(showAgain);
        h->addLayout(left);

        auto *right = new QVBoxLayout;
        tabs = new QTabWidget;
        right->addWidget(tabs, 1);
        list = new QListWidget;
        list->setMinimumWidth(330);
        list->setTextElideMode(Qt::ElideMiddle);
        list->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        list->setSpacing(2);
        list->setIconSize(QSize(28, 28));
        const QStringList rec = recentProjects();
        for (const QString &p : rec) {
            Project pj;
            QString e;
            QString nm = pj.load(p, &e) ? pj.name : QFileInfo(p).completeBaseName();
            int models = QDir(QFileInfo(p).absolutePath() + QStringLiteral("/Model Projects"))
                             .entryList({QStringLiteral("*.bpmdl")}, QDir::Files).size();
            auto *it = new QListWidgetItem(QStringLiteral("%1\n%2 · %3 model%4").arg(nm, tildePath(QFileInfo(p).absolutePath()))
                                               .arg(models).arg(models == 1 ? QString() : QStringLiteral("s")));
            it->setData(Qt::UserRole, p);
            it->setIcon(QApplication::style()->standardIcon(QStyle::SP_DirIcon));
            list->addItem(it);
        }
        if (rec.isEmpty()) {
            auto *it = new QListWidgetItem(QStringLiteral("No projects yet.\nStart one with New Project."));
            it->setFlags(Qt::NoItemFlags);
            list->addItem(it);
        } else {
            list->setCurrentRow(0);
        }
        tabs->addTab(list, QStringLiteral("Recent projects"));
        models = new QListWidget;
        models->setTextElideMode(Qt::ElideMiddle);
        models->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        models->setSpacing(2);
        models->setIconSize(QSize(28, 28));
        const QStringList mrec = recentModels();
        for (const QString &m : mrec) {
            auto *it = new QListWidgetItem(QStringLiteral("%1\n%2 · %3").arg(modelLabel(m), tildePath(QFileInfo(m).absolutePath()),
                                                                          QLocale().toString(QFileInfo(m).lastModified(), QLocale::ShortFormat)));
            it->setData(Qt::UserRole, m);
            it->setIcon(QApplication::style()->standardIcon(QStyle::SP_FileIcon));
            models->addItem(it);
        }
        if (mrec.isEmpty()) {
            auto *it = new QListWidgetItem(QStringLiteral("No models yet.\nModels you open or save show up here."));
            it->setFlags(Qt::NoItemFlags);
            models->addItem(it);
        } else {
            models->setCurrentRow(0);
        }
        tabs->addTab(models, QStringLiteral("Recent models"));
        tabs->setCurrentIndex(std::clamp(QSettings().value("ui/startTab", 0).toInt(), 0, 1));
        auto *openRecent = new QPushButton(QStringLiteral("Open"));
        auto syncOpen = [this, openRecent, rec, mrec] { openRecent->setEnabled(tabs->currentIndex() == 0 ? !rec.isEmpty() : !mrec.isEmpty()); };
        syncOpen();
        QObject::connect(tabs, &QTabWidget::currentChanged, this, [syncOpen](int i) {
            QSettings().setValue("ui/startTab", i);
            syncOpen();
        });
        auto *row = new QHBoxLayout;
        row->addStretch(1);
        row->addWidget(openRecent);
        right->addLayout(row);
        h->addLayout(right, 1);

        auto pick = [this](Choice c) {
            choice = c;
            QSettings().setValue("ui/startScreen", showAgain->isChecked());
            accept();
        };
        QObject::connect(newBtn, &QPushButton::clicked, this, [pick] { pick(NewProject); });
        QObject::connect(openBtn, &QPushButton::clicked, this, [pick] { pick(OpenProject); });
        QObject::connect(skipBtn, &QPushButton::clicked, this, [pick] { pick(None); });
        auto openSel = [this, pick] {
            const bool m = tabs->currentIndex() == 1;
            QListWidgetItem *it = (m ? models : list)->currentItem();
            if (!it || it->data(Qt::UserRole).toString().isEmpty()) return;
            recentPath = it->data(Qt::UserRole).toString();
            pick(m ? OpenModel : OpenRecent);
        };
        QObject::connect(openRecent, &QPushButton::clicked, this, openSel);
        QObject::connect(list, &QListWidget::itemDoubleClicked, this, openSel);
        QObject::connect(models, &QListWidget::itemDoubleClicked, this, openSel);
        resize(640, 400);
    }

    void done(int r) override {
        QSettings().setValue("ui/startScreen", showAgain->isChecked());
        QDialog::done(r);
    }

private:
    QTabWidget *tabs;
    QListWidget *list, *models;
    QCheckBox *showAgain;
};

// ------------------------------------------------------------------ model properties

/// The model's game path, stored in the .bpmdl itself.
class ModelPropertiesDialog : public QDialog {
public:
    QString modelPath;

    ModelPropertiesDialog(QWidget *parent, const QString &current, const Project &project, const QString &fileStem)
        : QDialog(parent), pj(project) {
        setWindowTitle(QStringLiteral("Model Properties"));
        auto *v = new QVBoxLayout(this);
        auto *form = new QFormLayout;
        path = new QLineEdit(current.isEmpty() ? (pj.isOpen() ? pj.defaultModelPath(fileStem) : cleanModelPath(fileStem)) : current);
        path->setPlaceholderText(QStringLiteral("housepack/onestory/house_1"));
        form->addRow(QStringLiteral("Model path"), path);
        v->addLayout(form);
        preview = new QLabel;
        preview->setTextFormat(Qt::RichText);
        preview->setFrameShape(QFrame::StyledPanel);
        preview->setMargin(8);
        preview->setTextInteractionFlags(Qt::TextSelectableByMouse);
        v->addWidget(preview);
        warn = new QLabel;
        warn->setWordWrap(true);
        warn->setStyleSheet(QStringLiteral("color:#e0823d"));
        v->addWidget(warn);
        v->addWidget(hintLabel(QStringLiteral("Saved inside this model's .bpmdl, so it's only set once. "
                                              "Paths are cleaned up for Source: lowercase, / separators, no spaces.")));
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        v->addWidget(buttons);
        QObject::connect(path, &QLineEdit::textChanged, this, [this] { update_(); });
        QObject::connect(buttons, &QDialogButtonBox::accepted, this, [this] {
            modelPath = cleanModelPath(path->text());
            accept();
        });
        QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        update_();
        resize(540, sizeHint().height());
    }

private:
    Project pj;
    QLineEdit *path;
    QLabel *preview, *warn;

    void update_() {
        QString bad;
        QString p = cleanModelPath(path->text(), &bad);
        const QString pool = pj.isOpen() ? pj.texturePool : QStringLiteral("materials/<project>");
        const QString mats = pj.isOpen() ? pj.materialsFor(p) : QStringLiteral("materials/models/") + p;
        QString cd = mats;
        if (cd.startsWith(QStringLiteral("materials/"))) cd.remove(0, 10);
        auto row = [](const QString &k, const QString &v) {
            return QStringLiteral("<tr><td style='padding-right:12px'>%1</td><td><tt>%2</tt></td></tr>").arg(k, v.toHtmlEscaped());
        };
        preview->setText(QStringLiteral("<table>") + row(QStringLiteral("Model"), QStringLiteral("models/%1.mdl").arg(p)) +
                         row(QStringLiteral("VMTs"), mats + QLatin1Char('/')) + row(QStringLiteral("$cdmaterials"), cd + QLatin1Char('/')) +
                         row(QStringLiteral("Shared VTFs"), pool + QLatin1Char('/')) + QStringLiteral("</table>"));
        QString w;
        if (p.isEmpty()) w = QStringLiteral("Enter a path, e.g. housepack/onestory/house_1.");
        else if (!bad.isEmpty()) w = QStringLiteral("Not allowed in game paths, replaced with _: %1").arg(bad.toHtmlEscaped());
        else if (path->text().trimmed() != p) w = QStringLiteral("Will be saved as: %1").arg(p);
        warn->setText(w);
        warn->setVisible(!w.isEmpty());
    }
};

// ------------------------------------------------------------------ material properties

/// What goes into a texture's VMT. Saved next to the image as <name>.bonkmat
/// (so it travels with the file), or inside a model as an override.
struct MatProps {
    QString shader = QStringLiteral("VertexLitGeneric");
    QString surfaceprop;          // empty = engine default
    int transparency = 0;         // 0 none, 1 alpha test, 2 translucent
    double alphaRef = 0.5;
    bool nocull = false;
    bool selfillum = false;
    bool envmap = false;
    double envTint = 0.25;
    int envMask = 0;              // 0 none, 1 base texture alpha, 2 normal map alpha
    bool phong = false;
    double phongBoost = 1.0;
    double phongExp = 20.0;
    QString bumpmap;              // material key of a project texture
    bool isNormalMap = false;     // this texture *is* a normal map (converted as one)
    QString preset;               // "", "monitor", "sign", "glass"
    QString extra;                // free VMT lines
    // more texture slots (material keys of project textures)
    QString detail;               // $detail
    double detailScale = 4.0;     // $detailscale
    double detailBlend = 0.5;     // $detailblendfactor
    int detailMode = 0;           // $detailblendmode
    QString envMaskTex;           // $envmapmask
    QString selfillumMask;        // $selfillummask
    QString phongExpTex;          // $phongexponenttexture
    // moving materials
    bool scroll = false;          // TextureScroll proxy
    double scrollRate = 0.25;     // texture widths per second
    double scrollAngle = 0.0;     // degrees
    int pulse = 0;                // Sine proxy: 0 off, 1 brightness ($color2 / $color), 2 alpha ($alpha)
    double pulseMin = 0.4, pulseMax = 1.0, pulsePeriod = 1.0;
    int frames = 1;               // an animation strip: frames stacked top to bottom (AnimatedTexture)
    double fps = 10.0;
    QString custom;               // the whole VMT, edited by hand (used as is when set)

    bool isDefault() const { return serialize() == MatProps().serialize(); }

    /// One line, for model overrides: k=v;k=v (values percent-encoded).
    QString serialize() const {
        QStringList kv;
        auto add = [&](const char *k, const QString &v) {
            kv << QString::fromLatin1(k) + QLatin1Char('=') + QString::fromLatin1(QUrl::toPercentEncoding(v));
        };
        add("shader", shader);
        add("surfaceprop", surfaceprop);
        add("transparency", QString::number(transparency));
        add("alpharef", QString::number(alphaRef));
        add("nocull", QString::number(nocull));
        add("selfillum", QString::number(selfillum));
        add("envmap", QString::number(envmap));
        add("envtint", QString::number(envTint));
        add("envmask", QString::number(envMask));
        add("phong", QString::number(phong));
        add("phongboost", QString::number(phongBoost));
        add("phongexp", QString::number(phongExp));
        add("bumpmap", bumpmap);
        add("normalmap", QString::number(isNormalMap));
        add("preset", preset);
        add("extra", extra);
        add("detail", detail);
        add("detailscale", QString::number(detailScale));
        add("detailblend", QString::number(detailBlend));
        add("detailmode", QString::number(detailMode));
        add("envmasktex", envMaskTex);
        add("selfillummask", selfillumMask);
        add("phongexptex", phongExpTex);
        add("scroll", QString::number(scroll));
        add("scrollrate", QString::number(scrollRate));
        add("scrollangle", QString::number(scrollAngle));
        add("pulse", QString::number(pulse));
        add("pulsemin", QString::number(pulseMin));
        add("pulsemax", QString::number(pulseMax));
        add("pulseperiod", QString::number(pulsePeriod));
        add("frames", QString::number(frames));
        add("fps", QString::number(fps));
        add("custom", custom);
        return kv.join(QLatin1Char(';'));
    }

    static MatProps parse(const QString &line) {
        MatProps p;
        for (const QString &item : line.split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
            const QString k = item.section(QLatin1Char('='), 0, 0).trimmed();
            const QString v = QUrl::fromPercentEncoding(item.section(QLatin1Char('='), 1).toLatin1());
            if (k == QStringLiteral("shader")) p.shader = v;
            else if (k == QStringLiteral("surfaceprop")) p.surfaceprop = v;
            else if (k == QStringLiteral("transparency")) p.transparency = v.toInt();
            else if (k == QStringLiteral("alpharef")) p.alphaRef = v.toDouble();
            else if (k == QStringLiteral("nocull")) p.nocull = v.toInt();
            else if (k == QStringLiteral("selfillum")) p.selfillum = v.toInt();
            else if (k == QStringLiteral("envmap")) p.envmap = v.toInt();
            else if (k == QStringLiteral("envtint")) p.envTint = v.toDouble();
            else if (k == QStringLiteral("envmask")) p.envMask = v.toInt();
            else if (k == QStringLiteral("phong")) p.phong = v.toInt();
            else if (k == QStringLiteral("phongboost")) p.phongBoost = v.toDouble();
            else if (k == QStringLiteral("phongexp")) p.phongExp = v.toDouble();
            else if (k == QStringLiteral("bumpmap")) p.bumpmap = v;
            else if (k == QStringLiteral("normalmap")) p.isNormalMap = v.toInt();
            else if (k == QStringLiteral("preset")) p.preset = v;
            else if (k == QStringLiteral("extra")) p.extra = v;
            else if (k == QStringLiteral("detail")) p.detail = v;
            else if (k == QStringLiteral("detailscale")) p.detailScale = v.toDouble();
            else if (k == QStringLiteral("detailblend")) p.detailBlend = v.toDouble();
            else if (k == QStringLiteral("detailmode")) p.detailMode = v.toInt();
            else if (k == QStringLiteral("envmasktex")) p.envMaskTex = v;
            else if (k == QStringLiteral("selfillummask")) p.selfillumMask = v;
            else if (k == QStringLiteral("phongexptex")) p.phongExpTex = v;
            else if (k == QStringLiteral("scroll")) p.scroll = v.toInt();
            else if (k == QStringLiteral("scrollrate")) p.scrollRate = v.toDouble();
            else if (k == QStringLiteral("scrollangle")) p.scrollAngle = v.toDouble();
            else if (k == QStringLiteral("pulse")) p.pulse = v.toInt();
            else if (k == QStringLiteral("pulsemin")) p.pulseMin = v.toDouble();
            else if (k == QStringLiteral("pulsemax")) p.pulseMax = v.toDouble();
            else if (k == QStringLiteral("pulseperiod")) p.pulsePeriod = v.toDouble();
            else if (k == QStringLiteral("frames")) p.frames = std::max(1, v.toInt());
            else if (k == QStringLiteral("fps")) p.fps = v.toDouble();
            else if (k == QStringLiteral("custom")) p.custom = v;
        }
        return p;
    }

    /// The sidecar file for an image: same name, .bonkmat
    static QString sidecarFor(const QString &imageFile) {
        QFileInfo fi(imageFile);
        return fi.absolutePath() + QLatin1Char('/') + fi.completeBaseName() + QStringLiteral(".bonkmat");
    }

    bool saveSidecar(const QString &imageFile) const {
        const QString path = sidecarFor(imageFile);
        if (isDefault()) return !QFileInfo::exists(path) || QFile::remove(path);
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return false;
        QString o = QStringLiteral("// Bonk! material properties for %1\n\"BonkMaterial\"\n{\n").arg(QFileInfo(imageFile).fileName());
        for (const QString &item : serialize().split(QLatin1Char(';'))) {
            const QString k = item.section(QLatin1Char('='), 0, 0);
            const QString v = item.section(QLatin1Char('='), 1); // stays percent-encoded: no quotes or newlines
            o += QStringLiteral("\t\"%1\" \"%2\"\n").arg(k, v);
        }
        o += QStringLiteral("}\n");
        f.write(o.toUtf8());
        return true;
    }

    static MatProps loadSidecar(const QString &imageFile) {
        QFile f(sidecarFor(imageFile));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return MatProps();
        static const QRegularExpression kv(QStringLiteral("\"([^\"]*)\"\\s+\"([^\"]*)\""));
        QStringList items;
        auto it = kv.globalMatch(QString::fromUtf8(f.readAll()));
        while (it.hasNext()) {
            auto m = it.next();
            items << m.captured(1) + QLatin1Char('=') + m.captured(2);
        }
        return parse(items.join(QLatin1Char(';')));
    }

    static QString num(double v) { return QString::number(v, 'g', 4); }

    /// The VMT text for material `key`, its VTFs living in `pool`
    /// (e.g. "materials/housepack").
    /// Every project texture this material uses besides its own image.
    QStringList extraTextures() const {
        QStringList k;
        for (const QString &t : {bumpmap, detail, envMaskTex, selfillumMask, phongExpTex})
            if (!t.isEmpty()) k << t;
        return k;
    }

    QString vmt(const QString &key, const QString &pool) const {
        if (!custom.trimmed().isEmpty()) return custom.endsWith(QLatin1Char('\n')) ? custom : custom + QLatin1Char('\n');
        return generatedVmt(key, pool);
    }

    QString generatedVmt(const QString &key, const QString &pool) const {
        QString vtfDir = pool;
        if (vtfDir.startsWith(QStringLiteral("materials/"))) vtfDir.remove(0, 10);
        auto tex = [&](const QString &k) { return vtfDir.isEmpty() ? k : vtfDir + QLatin1Char('/') + k; };
        QString sh = shader;
        QStringList lines;
        auto line = [&](const QString &k, const QString &v) { lines << QStringLiteral("\t\"%1\" \"%2\"").arg(k, v); };
        if (preset == QStringLiteral("monitor")) {
            sh = QStringLiteral("UnlitGeneric");
            line(QStringLiteral("$basetexture"), QStringLiteral("_rt_Camera"));
            line(QStringLiteral("$model"), QStringLiteral("1"));
        } else {
            line(QStringLiteral("$basetexture"), tex(key));
        }
        if (!surfaceprop.isEmpty()) line(QStringLiteral("$surfaceprop"), surfaceprop);
        if (!bumpmap.isEmpty()) line(QStringLiteral("$bumpmap"), tex(bumpmap));
        if (transparency == 1) {
            line(QStringLiteral("$alphatest"), QStringLiteral("1"));
            line(QStringLiteral("$alphatestreference"), num(alphaRef));
        } else if (transparency == 2) {
            line(QStringLiteral("$translucent"), QStringLiteral("1"));
        }
        if (nocull) line(QStringLiteral("$nocull"), QStringLiteral("1"));
        if (selfillum && sh != QStringLiteral("UnlitGeneric")) line(QStringLiteral("$selfillum"), QStringLiteral("1"));
        if (envmap) {
            line(QStringLiteral("$envmap"), QStringLiteral("env_cubemap"));
            line(QStringLiteral("$envmaptint"), QStringLiteral("[%1 %1 %1]").arg(num(envTint)));
            if (envMask == 1) line(QStringLiteral("$basealphaenvmapmask"), QStringLiteral("1"));
            else if (envMask == 2 && !bumpmap.isEmpty()) line(QStringLiteral("$normalmapalphaenvmapmask"), QStringLiteral("1"));
        }
        if (phong && sh == QStringLiteral("VertexLitGeneric")) {
            line(QStringLiteral("$phong"), QStringLiteral("1"));
            line(QStringLiteral("$phongboost"), num(phongBoost));
            if (!phongExpTex.isEmpty()) line(QStringLiteral("$phongexponenttexture"), tex(phongExpTex));
            else line(QStringLiteral("$phongexponent"), num(phongExp));
            line(QStringLiteral("$phongfresnelranges"), QStringLiteral("[0.2 0.6 1]"));
        }
        if (envmap && envMask == 3 && !envMaskTex.isEmpty()) line(QStringLiteral("$envmapmask"), tex(envMaskTex));
        if (selfillum && !selfillumMask.isEmpty() && sh == QStringLiteral("VertexLitGeneric")) line(QStringLiteral("$selfillummask"), tex(selfillumMask));
        if (!detail.isEmpty()) {
            line(QStringLiteral("$detail"), tex(detail));
            line(QStringLiteral("$detailscale"), num(detailScale));
            line(QStringLiteral("$detailblendfactor"), num(detailBlend));
            if (detailMode) line(QStringLiteral("$detailblendmode"), QString::number(detailMode));
        }
        if (frames > 1) line(QStringLiteral("$frame"), QStringLiteral("0"));
        for (const QString &x : extra.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
            if (!x.trimmed().isEmpty()) lines << QLatin1Char('\t') + x.trimmed();
        // moving materials: proxies
        QStringList px;
        auto block = [&](const QString &name, const QList<QPair<QString, QString>> &kv) {
            px << QStringLiteral("\t\t\"%1\"\n\t\t{").arg(name);
            for (const auto &p : kv) px << QStringLiteral("\t\t\t\"%1\" \"%2\"").arg(p.first, p.second);
            px << QStringLiteral("\t\t}");
        };
        if (scroll)
            block(QStringLiteral("TextureScroll"), {{QStringLiteral("texturescrollvar"), QStringLiteral("$basetexturetransform")},
                                                    {QStringLiteral("texturescrollrate"), num(scrollRate)},
                                                    {QStringLiteral("texturescrollangle"), num(scrollAngle)}});
        if (pulse)
            block(QStringLiteral("Sine"), {{QStringLiteral("sinemin"), num(pulseMin)},
                                           {QStringLiteral("sinemax"), num(pulseMax)},
                                           {QStringLiteral("sineperiod"), num(pulsePeriod)},
                                           {QStringLiteral("resultVar"), pulse == 2 ? QStringLiteral("$alpha")
                                                                         : sh == QStringLiteral("VertexLitGeneric") ? QStringLiteral("$color2") : QStringLiteral("$color")}});
        if (frames > 1)
            block(QStringLiteral("AnimatedTexture"), {{QStringLiteral("animatedtexturevar"), QStringLiteral("$basetexture")},
                                                      {QStringLiteral("animatedtextureframenumvar"), QStringLiteral("$frame")},
                                                      {QStringLiteral("animatedtextureframerate"), num(fps)}});
        if (!px.isEmpty()) lines << QStringLiteral("\t\"Proxies\"\n\t{\n%1\n\t}").arg(px.join(QLatin1Char('\n')));
        return QStringLiteral("\"%1\"\n{\n%2\n}\n").arg(sh, lines.join(QLatin1Char('\n')));
    }
};

const char *kSurfaceProps[] = {
    "", "brick", "concrete", "concrete_block", "rock", "gravel", "dirt", "grass", "mud", "sand", "snow", "ice",
    "wood", "wood_plank", "wood_panel", "wood_solid", "wood_crate", "wood_box", "wood_furniture",
    "metal", "solidmetal", "metalpanel", "metal_box", "metalvent", "metalgrate", "metal_barrel", "chainlink",
    "glass", "glassbottle", "tile", "porcelain", "plaster", "ceiling_tile", "carpet", "cardboard", "paper",
    "plastic", "plastic_box", "plastic_barrel", "rubber", "rubbertire", "computer", "pottery", "flesh",
    "alienflesh", "watermelon", "default",
};

/// Double-click a swatch: edit what goes into the texture's VMT.
class MaterialDialog : public QDialog {
public:
    MaterialDialog(QWidget *parent, const QString &key, const QString &imageFile, const QString &pool,
                   const QStringList &projectKeys, bool canOverride)
        : QDialog(parent), key(key), imageFile(imageFile), pool(pool) {
        setWindowTitle(QStringLiteral("Material Properties: %1").arg(key));
        const QString ov = QString::fromUtf8(bonk_mat_override(key.toUtf8().constData()));
        MatProps p = ov.isEmpty() ? MatProps::loadSidecar(imageFile) : MatProps::parse(ov);
        framesInImage = std::max(1, MatProps::loadSidecar(imageFile).frames); // a property of the image itself

        auto texCombo = [&] {
            auto *c = new QComboBox;
            c->addItem(QStringLiteral("(none)"), QString());
            for (const QString &k : projectKeys)
                if (k != key) c->addItem(k, k);
            return c;
        };
        auto dspin = [](double lo, double hi, double step) {
            auto *s = new QDoubleSpinBox;
            s->setRange(lo, hi);
            s->setSingleStep(step);
            s->setDecimals(step < 0.1 ? 2 : 1);
            return s;
        };

        auto *root = new QHBoxLayout(this);
        auto *left = new QVBoxLayout;
        auto *tabs = new QTabWidget;

        // --- Basics
        auto *basics = new QWidget;
        auto *form = new QFormLayout(basics);
        preset = new QComboBox;
        preset->addItem(QStringLiteral("None"), QString());
        preset->addItem(QStringLiteral("Monitor screen (shows a camera: _rt_Camera)"), QStringLiteral("monitor"));
        preset->addItem(QStringLiteral("Unlit sign (fullbright)"), QStringLiteral("sign"));
        preset->addItem(QStringLiteral("Glass (translucent, shiny)"), QStringLiteral("glass"));
        shader = new QComboBox;
        shader->addItem(QStringLiteral("VertexLitGeneric (lit, the usual for models)"), QStringLiteral("VertexLitGeneric"));
        shader->addItem(QStringLiteral("UnlitGeneric (fullbright)"), QStringLiteral("UnlitGeneric"));
        shader->addItem(QStringLiteral("MonitorScreen (screens: bright, like a display)"), QStringLiteral("MonitorScreen"));
        surf = new QComboBox;
        surf->setEditable(true);
        for (const char *s : kSurfaceProps) surf->addItem(QString::fromLatin1(s));
        surf->setItemText(0, QString());
        surf->lineEdit()->setPlaceholderText(QStringLiteral("default"));
        surf->setToolTip(QStringLiteral("$surfaceprop: impact sounds, bullet decals, footsteps and physics feel"));
        transp = new QComboBox;
        transp->addItems({QStringLiteral("None"), QStringLiteral("Alpha test (cut-out: fences, leaves)"), QStringLiteral("Translucent (glass, smoke)")});
        alphaRef = dspin(0.0, 1.0, 0.05);
        nocull = new QCheckBox(QStringLiteral("Double-sided ($nocull)"));
        selfillum = new QCheckBox(QStringLiteral("Glows ($selfillum, uses the image's alpha)"));
        selfillumMask = texCombo();
        selfillumMask->setToolTip(QStringLiteral("$selfillummask: a separate image whose brightness says where it glows"));
        form->addRow(QStringLiteral("Preset"), preset);
        form->addRow(QStringLiteral("Shader"), shader);
        form->addRow(QStringLiteral("Surface type"), surf);
        form->addRow(QStringLiteral("Transparency"), transp);
        form->addRow(QStringLiteral("Alpha test cut-off"), alphaRef);
        form->addRow(nocull);
        form->addRow(selfillum);
        form->addRow(QStringLiteral("Glow mask"), selfillumMask);
        tabs->addTab(basics, QStringLiteral("Basics"));

        // --- Maps
        auto *maps = new QWidget;
        auto *mf = new QFormLayout(maps);
        bump = texCombo();
        isNormal = new QCheckBox(QStringLiteral("This texture is a normal map (convert it as one)"));
        envmap = new QCheckBox(QStringLiteral("Reflections ($envmap env_cubemap)"));
        envTint = dspin(0.0, 1.0, 0.05);
        envMask = new QComboBox;
        envMask->addItems({QStringLiteral("Whole surface"), QStringLiteral("Base texture alpha"), QStringLiteral("Normal map alpha"), QStringLiteral("Mask texture")});
        envMaskTex = texCombo();
        envMaskTex->setToolTip(QStringLiteral("$envmapmask: bright where it should reflect"));
        phong = new QCheckBox(QStringLiteral("Shine ($phong)"));
        boost = dspin(0.0, 50.0, 0.5);
        exponent = dspin(1.0, 200.0, 1.0);
        phongExpTex = texCombo();
        phongExpTex->setToolTip(QStringLiteral("$phongexponenttexture: how sharp the shine is, per pixel (replaces Shine sharpness)"));
        detail = texCombo();
        detail->setToolTip(QStringLiteral("$detail: a small texture tiled over this one for close-up grain"));
        detailScale = dspin(0.01, 64.0, 0.5);
        detailBlend = dspin(0.0, 1.0, 0.05);
        detailMode = new QComboBox;
        detailMode->addItems({QStringLiteral("0: darken / lighten (default)"), QStringLiteral("1: add"), QStringLiteral("2: translucent detail"),
                              QStringLiteral("3: blend by base alpha"), QStringLiteral("4: multiply by detail alpha")});
        mf->addRow(QStringLiteral("Normal map"), bump);
        mf->addRow(isNormal);
        mf->addRow(envmap);
        mf->addRow(QStringLiteral("Reflection strength"), envTint);
        mf->addRow(QStringLiteral("Reflection mask"), envMask);
        mf->addRow(QStringLiteral("Mask texture"), envMaskTex);
        mf->addRow(phong);
        mf->addRow(QStringLiteral("Shine boost"), boost);
        mf->addRow(QStringLiteral("Shine sharpness"), exponent);
        mf->addRow(QStringLiteral("Sharpness map"), phongExpTex);
        mf->addRow(QStringLiteral("Detail texture"), detail);
        mf->addRow(QStringLiteral("Detail scale"), detailScale);
        mf->addRow(QStringLiteral("Detail strength"), detailBlend);
        mf->addRow(QStringLiteral("Detail blend"), detailMode);
        tabs->addTab(maps, QStringLiteral("Maps"));

        // --- Animation
        auto *anim = new QWidget;
        auto *af = new QFormLayout(anim);
        framesLabel = new QLabel;
        framesLabel->setWordWrap(true);
        fps = dspin(0.1, 120.0, 1.0);
        fps->setToolTip(QStringLiteral("Frames per second (AnimatedTexture)"));
        scroll = new QCheckBox(QStringLiteral("Scroll ($basetexturetransform via TextureScroll)"));
        scrollRate = dspin(-20.0, 20.0, 0.05);
        scrollRate->setToolTip(QStringLiteral("Texture widths per second"));
        scrollAngle = dspin(-360.0, 360.0, 15.0);
        scrollAngle->setToolTip(QStringLiteral("Direction in degrees: 0 moves right, 90 up"));
        pulse = new QComboBox;
        pulse->addItems({QStringLiteral("Off"), QStringLiteral("Brightness"), QStringLiteral("Transparency (needs Translucent)")});
        pulse->setToolTip(QStringLiteral("Sine proxy: the value swings between Low and High"));
        pulseMin = dspin(0.0, 4.0, 0.05);
        pulseMax = dspin(0.0, 4.0, 0.05);
        pulsePeriod = dspin(0.05, 60.0, 0.1);
        pulsePeriod->setSuffix(QStringLiteral(" s"));
        af->addRow(framesLabel);
        af->addRow(QStringLiteral("Frame rate"), fps);
        af->addRow(scroll);
        af->addRow(QStringLiteral("Scroll speed"), scrollRate);
        af->addRow(QStringLiteral("Scroll direction"), scrollAngle);
        af->addRow(QStringLiteral("Pulse"), pulse);
        af->addRow(QStringLiteral("Low"), pulseMin);
        af->addRow(QStringLiteral("High"), pulseMax);
        af->addRow(QStringLiteral("Period"), pulsePeriod);
        af->addRow(hintLabel(QStringLiteral("Scrolling, pulsing and frames play in the 3D view, as in game.")));
        tabs->addTab(anim, QStringLiteral("Animation"));

        // --- Extra
        auto *ex = new QWidget;
        auto *exl = new QVBoxLayout(ex);
        extra = new QPlainTextEdit;
        extra->setPlaceholderText(QStringLiteral("\"$detailtint\" \"[1 0.9 0.8]\"   (any extra VMT lines)"));
        exl->addWidget(new QLabel(QStringLiteral("Extra lines, added inside the material:")));
        exl->addWidget(extra, 1);
        tabs->addTab(ex, QStringLiteral("Extra"));

        left->addWidget(tabs, 1);
        warn = new QLabel;
        warn->setWordWrap(true);
        warn->setStyleSheet(QStringLiteral("color:#e0823d"));
        left->addWidget(warn);
        overrideBox = new QCheckBox(QStringLiteral("Override for this model only"));
        overrideBox->setToolTip(QStringLiteral("Off: saved with the texture (assets/Textures/…/%1.bonkmat), shared by every model.\n"
                                               "On: saved in this model's .bpmdl; other models keep the shared settings.").arg(QFileInfo(imageFile).completeBaseName()));
        overrideBox->setChecked(!ov.isEmpty());
        overrideBox->setEnabled(canOverride);
        left->addWidget(overrideBox);
        auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults);
        left->addWidget(buttons);
        root->addLayout(left, 3);

        // --- the VMT, live and editable
        auto *right = new QVBoxLayout;
        auto *rh = new QHBoxLayout;
        rh->addWidget(new QLabel(QStringLiteral("<b>VMT</b> (edit it directly if you like)")), 1);
        backBtn = new QPushButton(QStringLiteral("Back to Generated"));
        backBtn->setToolTip(QStringLiteral("Forget your hand edits and write the VMT from the settings on the left again"));
        rh->addWidget(backBtn);
        right->addLayout(rh);
        customNote = new QLabel(QStringLiteral("Hand-edited: this text is written as is; the settings on the left no longer change it."));
        customNote->setWordWrap(true);
        customNote->setStyleSheet(QStringLiteral("color:#e0823d"));
        right->addWidget(customNote);
        preview = new QPlainTextEdit;
        preview->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        preview->setMinimumWidth(340);
        preview->setLineWrapMode(QPlainTextEdit::NoWrap);
        preview->setTabStopDistance(QFontMetricsF(preview->font()).horizontalAdvance(QLatin1Char(' ')) * 4);
        right->addWidget(preview, 1);
        right->addWidget(hintLabel(QStringLiteral("Written into each model's materials folder at compile time. "
                                                  "A VMT you've edited in the addon folder by hand is never overwritten.")));
        root->addLayout(right, 2);

        load(p);
        for (QComboBox *c : {preset, shader, transp, envMask, bump, detail, detailMode, envMaskTex, selfillumMask, phongExpTex, pulse})
            QObject::connect(c, &QComboBox::currentIndexChanged, this, [this] { changed(); });
        QObject::connect(surf, &QComboBox::currentTextChanged, this, [this] { changed(); });
        for (QCheckBox *c : {nocull, selfillum, envmap, phong, isNormal, scroll})
            QObject::connect(c, &QCheckBox::toggled, this, [this] { changed(); });
        for (QDoubleSpinBox *s : {alphaRef, envTint, boost, exponent, detailScale, detailBlend, fps, scrollRate, scrollAngle, pulseMin, pulseMax, pulsePeriod})
            QObject::connect(s, &QDoubleSpinBox::valueChanged, this, [this] { changed(); });
        QObject::connect(extra, &QPlainTextEdit::textChanged, this, [this] { changed(); });
        QObject::connect(preview, &QPlainTextEdit::textChanged, this, [this] {
            if (loading) return;
            custom = preview->toPlainText(); // typed into the VMT: it's yours now
            customNote->show();
            backBtn->setEnabled(true);
        });
        QObject::connect(backBtn, &QPushButton::clicked, this, [this] {
            custom.clear();
            changed();
        });
        QObject::connect(preset, &QComboBox::currentIndexChanged, this, [this] { applyPreset(); });
        QObject::connect(buttons, &QDialogButtonBox::accepted, this, [this] { save(); });
        QObject::connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);
        QObject::connect(buttons->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, [this] { custom.clear(); load(MatProps()); });
        resize(920, 600);
    }

private:
    QString key, imageFile, pool, custom;
    int framesInImage = 1;
    QComboBox *preset, *shader, *surf, *transp, *bump, *envMask, *envMaskTex, *selfillumMask, *phongExpTex, *detail, *detailMode, *pulse;
    QDoubleSpinBox *alphaRef, *envTint, *boost, *exponent, *detailScale, *detailBlend, *fps, *scrollRate, *scrollAngle, *pulseMin, *pulseMax, *pulsePeriod;
    QCheckBox *nocull, *selfillum, *envmap, *phong, *isNormal, *overrideBox, *scroll;
    QPlainTextEdit *extra, *preview;
    QLabel *warn, *framesLabel, *customNote;
    QPushButton *backBtn;
    bool loading = false;

    static void pick(QComboBox *c, const QString &k) {
        int i = c->findData(k);
        if (i < 0 && !k.isEmpty()) {
            c->addItem(k, k);
            i = c->count() - 1;
        }
        c->setCurrentIndex(i < 0 ? 0 : i);
    }

    MatProps gather() const {
        MatProps p;
        p.preset = preset->currentData().toString();
        p.shader = shader->currentData().toString();
        p.surfaceprop = surf->currentText().trimmed().toLower();
        p.transparency = transp->currentIndex();
        p.alphaRef = alphaRef->value();
        p.nocull = nocull->isChecked();
        p.selfillum = selfillum->isChecked();
        p.selfillumMask = selfillumMask->currentData().toString();
        p.bumpmap = bump->currentData().toString();
        p.isNormalMap = isNormal->isChecked();
        p.envmap = envmap->isChecked();
        p.envTint = envTint->value();
        p.envMask = envMask->currentIndex();
        p.envMaskTex = envMaskTex->currentData().toString();
        p.phong = phong->isChecked();
        p.phongBoost = boost->value();
        p.phongExp = exponent->value();
        p.phongExpTex = phongExpTex->currentData().toString();
        p.detail = detail->currentData().toString();
        p.detailScale = detailScale->value();
        p.detailBlend = detailBlend->value();
        p.detailMode = detailMode->currentIndex();
        p.frames = framesInImage;
        p.fps = fps->value();
        p.scroll = scroll->isChecked();
        p.scrollRate = scrollRate->value();
        p.scrollAngle = scrollAngle->value();
        p.pulse = pulse->currentIndex();
        p.pulseMin = pulseMin->value();
        p.pulseMax = pulseMax->value();
        p.pulsePeriod = pulsePeriod->value();
        p.extra = extra->toPlainText();
        p.custom = custom;
        return p;
    }

    void load(const MatProps &p) {
        loading = true;
        custom = p.custom;
        preset->setCurrentIndex(std::max(0, preset->findData(p.preset)));
        shader->setCurrentIndex(std::max(0, shader->findData(p.shader)));
        surf->setCurrentText(p.surfaceprop);
        transp->setCurrentIndex(std::clamp(p.transparency, 0, 2));
        alphaRef->setValue(p.alphaRef);
        nocull->setChecked(p.nocull);
        selfillum->setChecked(p.selfillum);
        pick(selfillumMask, p.selfillumMask);
        pick(bump, p.bumpmap);
        isNormal->setChecked(p.isNormalMap);
        envmap->setChecked(p.envmap);
        envTint->setValue(p.envTint);
        envMask->setCurrentIndex(std::clamp(p.envMask, 0, 3));
        pick(envMaskTex, p.envMaskTex);
        phong->setChecked(p.phong);
        boost->setValue(p.phongBoost);
        exponent->setValue(p.phongExp);
        pick(phongExpTex, p.phongExpTex);
        pick(detail, p.detail);
        detailScale->setValue(p.detailScale);
        detailBlend->setValue(p.detailBlend);
        detailMode->setCurrentIndex(std::clamp(p.detailMode, 0, 4));
        fps->setValue(p.fps);
        scroll->setChecked(p.scroll);
        scrollRate->setValue(p.scrollRate);
        scrollAngle->setValue(p.scrollAngle);
        pulse->setCurrentIndex(std::clamp(p.pulse, 0, 2));
        pulseMin->setValue(p.pulseMin);
        pulseMax->setValue(p.pulseMax);
        pulsePeriod->setValue(p.pulsePeriod);
        extra->setPlainText(p.extra);
        loading = false;
        changed();
    }

    void applyPreset() {
        if (loading) return;
        const QString pr = preset->currentData().toString();
        loading = true;
        if (pr == QStringLiteral("monitor") || pr == QStringLiteral("sign")) {
            shader->setCurrentIndex(shader->findData(QStringLiteral("UnlitGeneric")));
        } else if (pr == QStringLiteral("glass")) {
            shader->setCurrentIndex(shader->findData(QStringLiteral("VertexLitGeneric")));
            transp->setCurrentIndex(2);
            envmap->setChecked(true);
            envTint->setValue(0.4);
            surf->setCurrentText(QStringLiteral("glass"));
        }
        loading = false;
        changed();
    }

    void changed() {
        if (loading) return;
        MatProps p = gather();
        alphaRef->setEnabled(p.transparency == 1);
        envTint->setEnabled(p.envmap);
        envMask->setEnabled(p.envmap);
        envMaskTex->setEnabled(p.envmap && p.envMask == 3);
        boost->setEnabled(p.phong);
        exponent->setEnabled(p.phong && p.phongExpTex.isEmpty());
        phongExpTex->setEnabled(p.phong);
        selfillumMask->setEnabled(p.selfillum);
        for (QWidget *w : std::initializer_list<QWidget *>{detailScale, detailBlend, detailMode}) w->setEnabled(!p.detail.isEmpty());
        for (QWidget *w : std::initializer_list<QWidget *>{scrollRate, scrollAngle}) w->setEnabled(p.scroll);
        for (QWidget *w : std::initializer_list<QWidget *>{pulseMin, pulseMax, pulsePeriod}) w->setEnabled(p.pulse != 0);
        fps->setEnabled(framesInImage > 1);
        framesLabel->setText(framesInImage > 1
                                 ? QStringLiteral("<b>%1 frames</b> (an animation imported with Import Animation). They play with AnimatedTexture.").arg(framesInImage)
                                 : QStringLiteral("A still image. To animate frames, use <b>Import Animation…</b> in the Texture Picker."));
        {
            loading = true; // showing the VMT isn't an edit
            const int pos = preview->textCursor().position();
            const QString text = p.vmt(key, pool);
            if (preview->toPlainText() != text) preview->setPlainText(text);
            if (preview->hasFocus()) {
                QTextCursor c = preview->textCursor();
                c.setPosition(std::min(pos, int(text.size())));
                preview->setTextCursor(c);
            }
            loading = false;
        }
        customNote->setVisible(!custom.isEmpty());
        backBtn->setEnabled(!custom.isEmpty());
        QString w;
        if (p.phong && p.bumpmap.isEmpty()) w = QStringLiteral("Shine ($phong) generally needs a normal map in Source; pick one, or it may not show.");
        else if (p.envMask == 2 && p.envmap && p.bumpmap.isEmpty()) w = QStringLiteral("The normal-map reflection mask needs a normal map.");
        else if (p.envMask == 3 && p.envmap && p.envMaskTex.isEmpty()) w = QStringLiteral("Pick a mask texture for the reflections.");
        else if (p.selfillum && p.shader != QStringLiteral("VertexLitGeneric")) w = QStringLiteral("This shader is already fullbright, so Glows does nothing.");
        else if (p.pulse == 2 && p.transparency != 2) w = QStringLiteral("Pulsing transparency needs Transparency: Translucent.");
        warn->setText(w);
        warn->setVisible(!w.isEmpty());
    }

    void save() {
        MatProps p = gather();
        QByteArray k = key.toUtf8();
        if (overrideBox->isChecked()) {
            QByteArray s = p.serialize().toUtf8();
            bonk_set_mat_override(k.constData(), s.constData());
        } else {
            bonk_set_mat_override(k.constData(), "");
            if (!p.saveSidecar(imageFile)) {
                QMessageBox::warning(this, windowTitle(), QStringLiteral("Couldn't save %1").arg(MatProps::sidecarFor(imageFile)));
                return;
            }
        }
        accept();
    }
};

// ------------------------------------------------------------------ textures

/// Source's own limit; anything power-of-two up to this is left alone (0.12.1: was 1024)
constexpr int kMaxTexture = 4096;

bool isPow2(int v) { return v > 0 && (v & (v - 1)) == 0; }

int nearestPow2(double v) {
    int p = 1 << int(std::lround(std::log2(std::max(1.0, v))));
    return std::clamp(p, 4, kMaxTexture);
}

bool needsResize(QSize s) { return s.width() > kMaxTexture || s.height() > kMaxTexture || !isPow2(s.width()) || !isPow2(s.height()); }

/// Fit within 4096 keeping the aspect, then each side to the nearest power of two.
QSize resizeTarget(QSize s) {
    double k = std::min(1.0, double(kMaxTexture) / std::max(s.width(), s.height()));
    return QSize(nearestPow2(s.width() * k), nearestPow2(s.height() * k));
}

/// One image in assets/Textures.
struct TexEntry {
    QString key;   // material name: cleaned relative path without extension
    QString file;  // absolute path
    QString rel;   // relative to assets/Textures, as on disk
    QString group; // first folder, or empty
    QDateTime mtime;
    QSize size;
    QIcon icon;
    bool converted = false;
    bool props = false; // has a .bonkmat
    QString vtf;        // where its VTF goes
    int frames = 1;     // an animation strip: frames stacked top to bottom
    QSize frameSize() const { return QSize(size.width(), size.height() / std::max(1, frames)); }
};

/// The project's textures: scans assets/Textures (live, through a file
/// watcher), decodes the images for the renderer and makes swatches.
class TextureLibrary {
public:
    std::vector<TexEntry> entries;
    std::function<void()> changed;                       // the list or an image changed
    std::function<void(const QStringList &)> oversized;  // new images that need resizing (relative paths)
    std::function<void(const QStringList &)> stale;      // textures whose VTF was removed because the image changed

    TextureLibrary() {
        debounce.setSingleShot(true);
        debounce.setInterval(400);
        QObject::connect(&debounce, &QTimer::timeout, &debounce, [this] { scan(); });
        QObject::connect(&watcher, &QFileSystemWatcher::directoryChanged, &debounce, [this] { debounce.start(); });
        QObject::connect(&watcher, &QFileSystemWatcher::fileChanged, &debounce, [this] { debounce.start(); });
    }

    void setProject(const Project *p) {
        project = p;
        asked.clear();
        for (const TexEntry &e : entries) bonk_texture_remove(e.key.toUtf8().constData());
        entries.clear();
        if (!watcher.directories().isEmpty()) watcher.removePaths(watcher.directories());
        if (!watcher.files().isEmpty()) watcher.removePaths(watcher.files());
        scan();
    }

    const TexEntry *find(const QString &key) const {
        for (const TexEntry &e : entries)
            if (e.key == key) return &e;
        return nullptr;
    }

    QStringList groups() const {
        QStringList g;
        if (project && project->isOpen()) {
            QDirIterator it(project->texturesDir(), QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
            while (it.hasNext()) g << QDir(project->texturesDir()).relativeFilePath(it.next());
        }
        g.sort(Qt::CaseInsensitive);
        return g;
    }

    /// Rescan the folder; reload images that are new or changed.
    void scan() {
        if (!project || !project->isOpen()) {
            if (changed) changed();
            return;
        }
        const QString root = project->texturesDir();
        QDir().mkpath(root);
        std::vector<TexEntry> next;
        QStringList dirs{root}, files;
        QStringList fresh;
        QDirIterator it(root, {QStringLiteral("*.png"), QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"),
                               QStringLiteral("*.PNG"), QStringLiteral("*.JPG"), QStringLiteral("*.JPEG")},
                        QDir::Files, QDirIterator::Subdirectories);
        QStringList found;
        while (it.hasNext()) found << it.next();
        found.sort(Qt::CaseInsensitive);
        bool any = false, unreadable = false;
        QStringList staleNow;
        for (const QString &path : found) {
            QFileInfo fi(path);
            TexEntry e;
            e.file = fi.absoluteFilePath();
            e.rel = QDir(root).relativeFilePath(e.file);
            QString noext = e.rel.left(e.rel.size() - fi.suffix().size() - 1);
            e.key = cleanGamePath(noext);
            e.group = e.rel.contains(QLatin1Char('/')) ? e.rel.section(QLatin1Char('/'), 0, -2) : QString();
            e.mtime = fi.lastModified();
            bool dup = false;
            for (const TexEntry &n : next) dup |= n.key == e.key;
            if (dup) continue;
            const TexEntry *old = find(e.key);
            if (QFileInfo::exists(MatProps::sidecarFor(e.file))) e.frames = std::max(1, MatProps::loadSidecar(e.file).frames);
            if (old && old->file == e.file && old->mtime == e.mtime && old->frames == e.frames) {
                e.size = old->size;
                e.icon = old->icon;
            } else {
                QImage img(e.file);
                if (img.isNull()) {
                    unreadable = true; // probably still being copied: look again shortly
                    continue;
                }
                e.size = img.size();
                QImage rgba = img.convertToFormat(QImage::Format_RGBA8888);
                bonk_texture_set(e.key.toUtf8().constData(), rgba.width(), rgba.height(), rgba.constBits());
                // square swatch, the image centered in it with its aspect kept
                QImage sq(192, 192, QImage::Format_ARGB32_Premultiplied);
                sq.fill(Qt::transparent);
                {
                    // an animation shows its first frame, with a film-strip mark
                    const QImage first = e.frames > 1 ? img.copy(0, 0, img.width(), img.height() / e.frames) : img;
                    QImage t = first.scaled(192, 192, Qt::KeepAspectRatio, Qt::SmoothTransformation);
                    QPainter q(&sq);
                    q.drawImage((192 - t.width()) / 2, (192 - t.height()) / 2, t);
                    if (e.frames > 1) {
                        q.setRenderHint(QPainter::Antialiasing);
                        q.setPen(Qt::NoPen);
                        q.setBrush(QColor(0, 0, 0, 170));
                        // bottom right: the bottom-left corner is the material-properties dot
                        q.drawRoundedRect(QRectF(112, 150, 74, 36), 6, 6);
                        QFont f = smoothFont();
                        f.setPixelSize(22);
                        f.setBold(true);
                        q.setFont(f);
                        // a drawn play triangle (fonts don't all have ▶), then the frame count
                        q.setBrush(Qt::white);
                        q.drawPolygon(QPolygonF({QPointF(121, 159), QPointF(121, 177), QPointF(136, 168)}));
                        q.setPen(Qt::white);
                        q.drawText(QRectF(138, 150, 46, 36), Qt::AlignVCenter | Qt::AlignLeft, QString::number(e.frames));
                    }
                }
                e.icon = QIcon(QPixmap::fromImage(sq));
                any = true;
                if (needsResize(e.frames > 1 ? e.frameSize() : e.size) && !asked.contains(e.file)) fresh << e.rel;
            }
            e.vtf = vtfPath(e.key);
            QFileInfo vi(e.vtf);
            if (vi.exists() && vi.lastModified() < e.mtime) {
                // the image changed after it was converted: that VTF is stale
                QFile::remove(e.vtf);
                staleNow << e.key;
            }
            e.converted = QFileInfo::exists(e.vtf);
            e.props = QFileInfo::exists(MatProps::sidecarFor(e.file));
            files << e.file;
            next.push_back(e);
        }
        for (const TexEntry &o : entries) {
            bool kept = false;
            for (const TexEntry &n : next) kept |= n.key == o.key;
            if (!kept) {
                bonk_texture_remove(o.key.toUtf8().constData());
                any = true;
            }
        }
        if (next.size() != entries.size()) any = true;
        entries = std::move(next);
        QDirIterator di(root, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
        while (di.hasNext()) dirs << di.next();
        // keep the watcher in step with the folders and files
        QStringList wd = watcher.directories(), wf = watcher.files();
        for (const QString &d : wd) if (!dirs.contains(d)) watcher.removePath(d);
        for (const QString &f : wf) if (!files.contains(f)) watcher.removePath(f);
        for (const QString &d : dirs) if (!wd.contains(d)) watcher.addPath(d);
        for (const QString &f : files) if (!wf.contains(f)) watcher.addPath(f);
        // paused (an import is writing files) or the setting is off: no question now
        if (paused || !askResize) fresh.clear();
        for (const QString &r : fresh) asked.insert(root + QLatin1Char('/') + r);
        if (unreadable && retries < 5) {
            ++retries;
            debounce.start(1000);
        } else {
            retries = 0;
            if (!debounce.isActive()) debounce.setInterval(400);
        }
        if (changed) changed();
        (void)any;
        if (!fresh.isEmpty() && oversized) oversized(fresh);
        if (!staleNow.isEmpty() && stale) stale(staleNow);
    }

    /// Resize an image in place; the original moves to assets/Original Textures.
    bool resize(const QString &rel, QString *err) {
        const QString src = project->texturesDir() + QLatin1Char('/') + rel;
        QImage img(src);
        if (img.isNull()) {
            *err = QStringLiteral("Couldn't read %1").arg(rel);
            return false;
        }
        QSize t = resizeTarget(img.size());
        QImage out = img.scaled(t, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        QString dest = project->originalsDir() + QLatin1Char('/') + rel;
        QDir().mkpath(QFileInfo(dest).absolutePath());
        if (QFileInfo::exists(dest)) {
            QFileInfo di(dest);
            for (int n = 2;; ++n) {
                QString c = di.absolutePath() + QLatin1Char('/') + di.completeBaseName() + QStringLiteral(" (%1).").arg(n) + di.suffix();
                if (!QFileInfo::exists(c)) { dest = c; break; }
            }
        }
        if (!QFile::rename(src, dest)) {
            if (!QFile::copy(src, dest) || !QFile::remove(src)) {
                *err = QStringLiteral("Couldn't move %1 to Original Textures").arg(rel);
                return false;
            }
        }
        if (!out.save(src, nullptr, 95)) {
            *err = QStringLiteral("Couldn't save the resized %1").arg(rel);
            QFile::rename(dest, src);
            return false;
        }
        return true;
    }

    void rescanSoon() { debounce.start(); }

    QString vtfPath(const QString &key) const {
        return project->addonDir() + QLatin1Char('/') + project->texturePool + QLatin1Char('/') + key + QStringLiteral(".vtf");
    }

    /// Convert one texture to VTF (using its material properties for the
    /// normal-map / alpha-test flags). Returns an error message or "".
    QString convert(const TexEntry &e) {
        QImage img(e.file);
        if (img.isNull()) return QStringLiteral("Couldn't read %1").arg(e.rel);
        QImage rgba = img.convertToFormat(QImage::Format_RGBA8888);
        MatProps p = MatProps::loadSidecar(e.file);
        int flags = (p.isNormalMap ? 1 : 0) | (p.transparency == 1 ? 2 : 0);
        QByteArray path = QFile::encodeName(e.vtf.isEmpty() ? vtfPath(e.key) : e.vtf);
        if (p.frames > 1) {
            // an animation strip: one VTF frame per strip cell (AnimatedTexture)
            const int fh = rgba.height() / p.frames;
            if (fh < 1 || bonk_write_vtf_frames(path.constData(), rgba.width(), fh, p.frames, rgba.constBits(), flags) != 0)
                return fh < 1 ? QStringLiteral("the animation's frames are too small") : QString::fromUtf8(bonk_last_error());
            return QString();
        }
        if (bonk_write_vtf(path.constData(), rgba.width(), rgba.height(), rgba.constBits(), flags) != 0)
            return QString::fromUtf8(bonk_last_error());
        return QString();
    }

    bool hasProject() const { return project && project->isOpen(); }

    /// Ask about resizing these files again (they were just imported or replaced).
    void forget(const QStringList &files) {
        for (const QString &f : files) asked.remove(QFileInfo(f).absoluteFilePath());
    }

    /// Never ask about these (textures that came from a working Source game or addon).
    void markAsked(const QStringList &files) {
        for (const QString &f : files) asked.insert(QFileInfo(f).absoluteFilePath());
    }

    /// Hold the watcher's scans while an import writes many files; resuming
    /// scans once, so a batch gets (at most) one question.
    void pauseScans(bool on) {
        paused = on;
        watcher.blockSignals(on);
        if (on) debounce.stop();
        else scan();
    }

    bool askResize = true;

private:
    const Project *project = nullptr;
    QFileSystemWatcher watcher;
    QTimer debounce;
    QSet<QString> asked; // files already offered a resize this session
    bool paused = false;
    int retries = 0;
};

/// Swatch grid; Ctrl+wheel zooms. Swatches can be dragged onto folder tiles;
/// files dragged in from outside are handed to the picker.
class SwatchList : public QListWidget {
public:
    std::function<void(int)> zoomBy;
    std::function<bool(const QMimeData *)> extEnter;
    std::function<void()> extLeave;
    std::function<void(const QMimeData *)> extDrop;
    std::function<void(const QString &key, const QString &folder)> moveInto;
    std::function<bool(QKeyEvent *)> keys;

protected:
    void wheelEvent(QWheelEvent *e) override {
        if ((e->modifiers() & Qt::ControlModifier) && zoomBy) {
            int d = e->angleDelta().y();
            if (d) zoomBy(d > 0 ? 16 : -16);
            e->accept();
            return;
        }
        QListWidget::wheelEvent(e);
    }
    void keyPressEvent(QKeyEvent *e) override {
        if (keys && keys(e)) return;
        QListWidget::keyPressEvent(e);
    }
    /// With the swatch grid focused, its own Cut / Copy / Paste / Delete / F2
    /// (texture files) win over the Edit menu's (brushes, layers).
    bool event(QEvent *e) override {
        if (e->type() == QEvent::ShortcutOverride) {
            auto *k = static_cast<QKeyEvent *>(e);
            if (k->matches(QKeySequence::Cut) || k->matches(QKeySequence::Copy) || k->matches(QKeySequence::Paste) ||
                k->matches(QKeySequence::Delete) || k->key() == Qt::Key_F2) {
                e->accept();
                return true;
            }
        }
        return QListWidget::event(e);
    }
    QListWidgetItem *folderAt(const QPoint &pos) const {
        QListWidgetItem *it = itemAt(pos);
        return it && it->data(Qt::UserRole + 1).toInt() == 4 ? it : nullptr;
    }
    void dragEnterEvent(QDragEnterEvent *e) override {
        if (e->source() == this) {
            e->acceptProposedAction();
            return;
        }
        if (extEnter && extEnter(e->mimeData())) e->acceptProposedAction();
        else e->ignore();
    }
    void dragMoveEvent(QDragMoveEvent *e) override {
        if (e->source() == this) {
            QListWidgetItem *f = folderAt(e->position().toPoint());
            if (f) {
                setCurrentItem(f, QItemSelectionModel::NoUpdate);
                e->setDropAction(Qt::MoveAction);
                e->accept();
            } else {
                e->ignore();
            }
            return;
        }
        e->acceptProposedAction();
    }
    void dragLeaveEvent(QDragLeaveEvent *e) override {
        if (extLeave) extLeave();
        QListWidget::dragLeaveEvent(e);
    }
    void dropEvent(QDropEvent *e) override {
        if (e->source() == this) {
            QListWidgetItem *f = folderAt(e->position().toPoint());
            const QString key = dragKey;
            if (f && !key.isEmpty() && moveInto) {
                const QString folder = f->data(Qt::UserRole).toString();
                QTimer::singleShot(0, this, [this, key, folder] { moveInto(key, folder); });
            }
            e->setDropAction(Qt::IgnoreAction); // we move the file ourselves
            e->accept();
            return;
        }
        if (extDrop) extDrop(e->mimeData());
        e->acceptProposedAction();
    }
    void startDrag(Qt::DropActions actions) override {
        QListWidgetItem *it = currentItem();
        dragKey = it && it->data(Qt::UserRole + 1).toInt() == 1 ? it->data(Qt::UserRole).toString() : QString();
        if (dragKey.isEmpty()) return;
        QListWidget::startDrag(actions);
    }

private:
    QString dragKey;
};

/// Dockable texture picker: built-in materials plus the project's textures.
/// Clicking a swatch makes it the current texture (and, by default, switches
/// to Face Select & Paint so right-click paints it). Double-click opens the
/// texture's Material Properties. While Face Select & Paint is on, the Face
/// section below the swatches edits the selected faces' alignment.
class TexturePicker : public QWidget {
public:
    std::function<void(unsigned)> changed;  // editor flags to handle (repaint etc.)
    std::function<void()> importReq;        // Import… pressed
    std::function<void()> animReq;          // Import Animation… pressed
    std::function<void(const QStringList &, const QString &)> dropped; // paths, group ("" = ask)
    std::function<void()> switchToFace;     // swatch clicked outside Face Select & Paint
    std::function<void(const QStringList &)> convertReq; // keys to convert ([] = all)
    std::function<QSet<QString>()> otherModelKeys;        // textures only the project's other models use
    std::function<void(const QString &)> propertiesReq;   // double-clicked a project texture
    /// File operations for the main window: op is cut / copy / paste / move /
    /// rename / delete / newfolder / renamefolder / deletefolder / color;
    /// keys are material names, arg a folder (relative to assets/Textures)
    std::function<void(const QString &op, const QStringList &keys, const QString &arg)> fileReq;
    bool canPaste = false;
    bool applyOnClick = false;
    bool switchOnClick = true;

    explicit TexturePicker(TextureLibrary *library, QWidget *parent = nullptr) : QWidget(parent), lib(library) {
        auto *v = new QVBoxLayout(this);
        v->setContentsMargins(8, 6, 8, 8);
        v->setSpacing(6);

        // "Propper'd" banner for textures whose VTF went stale
        banner = new QFrame;
        banner->setStyleSheet(QStringLiteral("QFrame { background:#f2c94c; color:#3a2c00; border-radius:5px; } QLabel { background:transparent; color:#3a2c00; }"));
        auto *bv = new QVBoxLayout(banner);
        bv->setContentsMargins(8, 6, 6, 6);
        bv->setSpacing(4);
        bannerText = new QLabel;
        bannerText->setWordWrap(true);
        bv->addWidget(bannerText);
        auto *bl = new QHBoxLayout;
        bl->addStretch(1);
        auto *bconv = new QPushButton(QStringLiteral("Convert Now"));
        auto *bx = new QPushButton(QStringLiteral("Later"));
        bl->addWidget(bconv);
        bl->addWidget(bx);
        bv->addLayout(bl);
        banner->hide();
        v->addWidget(banner);
        QObject::connect(bx, &QPushButton::clicked, this, [this] { staleKeys.clear(); banner->hide(); });
        QObject::connect(bconv, &QPushButton::clicked, this, [this] {
            QStringList k = staleKeys;
            staleKeys.clear();
            banner->hide();
            if (convertReq) convertReq(k);
        });

        target = new QLabel;
        target->setWordWrap(true);
        v->addWidget(target);
        filter = new QLineEdit;
        filter->setPlaceholderText(QStringLiteral("Filter materials…"));
        filter->setClearButtonEnabled(true);
        group = new QComboBox;
        group->addItem(QStringLiteral("Folders"), QStringLiteral("#"));
        group->addItem(QStringLiteral("All textures"), QStringLiteral("*"));
        group->addItem(QStringLiteral("Built-in"), QStringLiteral("@"));
        group->setToolTip(QStringLiteral("Folders: browse assets/Textures like a file manager (folders become materials/ folders)\n"
                                         "All textures: everything in one grid"));
        auto *fr = new QHBoxLayout;
        fr->addWidget(filter, 3);
        fr->addWidget(group, 2);
        v->addLayout(fr);
        thisModelOnly = new QCheckBox(QStringLiteral("Hide other models' textures"));
        thisModelOnly->setToolTip(QStringLiteral("Leaves out textures that only the project's other models use (ones this model uses stay),\n"
                                                 "and textures in the models/ folder (Source's materials/models: other models' skins)."));
        thisModelOnly->setChecked(QSettings().value("picker/thisModelOnly", false).toBool());
        QObject::connect(thisModelOnly, &QCheckBox::toggled, this, [this](bool on) {
            QSettings().setValue("picker/thisModelOnly", on);
            rebuild();
        });
        v->addWidget(thisModelOnly);
        crumbs = new QLabel;
        crumbs->setTextFormat(Qt::RichText);
        crumbs->setTextInteractionFlags(Qt::LinksAccessibleByMouse);
        crumbs->setWordWrap(true);
        v->addWidget(crumbs);
        QObject::connect(crumbs, &QLabel::linkActivated, this, [this](const QString &link) { enter(link == QStringLiteral("/") ? QString() : link); });
        list = new SwatchList;
        list->setViewMode(QListView::IconMode);
        list->setResizeMode(QListView::Adjust);
        list->setMovement(QListView::Static);
        list->setWrapping(true);
        list->setUniformItemSizes(true);
        list->setWordWrap(true);
        list->setTextElideMode(Qt::ElideMiddle);
        list->setSpacing(4);
        list->setSelectionMode(QAbstractItemView::SingleSelection);
        list->setFocusPolicy(Qt::ClickFocus); // for Ctrl+X / C / V, Delete, F2, Backspace
        list->setDragEnabled(true);
        list->setDragDropMode(QAbstractItemView::DragDrop);
        list->setDefaultDropAction(Qt::MoveAction);
        list->setAcceptDrops(true);
        list->viewport()->setAcceptDrops(true);
        list->extEnter = [this](const QMimeData *m) { return beginDrop(m); };
        list->extLeave = [this] { endDrop(); };
        list->extDrop = [this](const QMimeData *m) { finishDrop(m); };
        list->moveInto = [this](const QString &key, const QString &folder) { if (fileReq) fileReq(QStringLiteral("move"), {key}, folder); };
        list->keys = [this](QKeyEvent *e) { return listKey(e); };
        list->setContextMenuPolicy(Qt::CustomContextMenu);
        list->setMinimumHeight(gCompact ? 120 : 170);
        setAcceptDrops(true);
        v->addWidget(list, 1);
        auto *zr = new QHBoxLayout;
        zr->addWidget(new QLabel(QStringLiteral("Zoom")));
        zoom = new QSlider(Qt::Horizontal);
        zoom->setRange(32, 192);
        zoom->setValue(QSettings().value("picker/zoom", gCompact ? lowOr(48, 40) : 64).toInt());
        zoom->setToolTip(QStringLiteral("Swatch size (Ctrl+wheel over the swatches)"));
        zr->addWidget(zoom, 1);
        v->addLayout(zr);
        current = new QLabel;
        current->setWordWrap(true);
        v->addWidget(current);
        auto *br = new QGridLayout;
        br->setSpacing(4);
        applyBtn = new QPushButton(QStringLiteral("Apply"));
        applyBtn->setToolTip(QStringLiteral("Put the current texture on the selected faces, or on every face of the selected brushes"));
        auto *importBtn = new QPushButton(QStringLiteral("Import…"));
        importBtn->setToolTip(QStringLiteral("Copy PNG / JPG files into the project's assets/Textures (or drop them here)"));
        convertBtn = new QPushButton(QStringLiteral("Convert All"));
        convertBtn->setToolTip(QStringLiteral("Convert every texture that isn't converted yet to VTF\n(into addon/<project>/<texture pool>)"));
        auto *animBtn = new QPushButton(QStringLiteral("Import Animation…"));
        animBtn->setToolTip(QStringLiteral("Make an animated texture: pick several images (its frames, in name order),\n"
                                           "one sprite sheet, or a GIF. It plays in the 3D view and in game (AnimatedTexture)."));
        QObject::connect(animBtn, &QPushButton::clicked, this, [this] { if (animReq) animReq(); });
        br->addWidget(applyBtn, 0, 0);
        br->addWidget(importBtn, 0, 1);
        br->addWidget(animBtn, 1, 0, 1, 2);
        br->addWidget(convertBtn, 2, 0, 1, 2);
        v->addLayout(br);

        buildFaceSection(v);

        hint = hintLabel(QString());
        v->addWidget(hint);
        if (gCompact) hint->hide(); // compact: the help lives in tooltips

        setZoom(zoom->value());
        QObject::connect(zoom, &QSlider::valueChanged, this, [this](int z) { setZoom(z); });
        list->zoomBy = [this](int d) { zoom->setValue(zoom->value() + d); };
        QObject::connect(filter, &QLineEdit::textChanged, this, [this] { rebuild(); });
        QObject::connect(group, &QComboBox::currentIndexChanged, this, [this] { rebuild(); });
        QObject::connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem *it) {
            const int kind = it->data(Qt::UserRole + 1).toInt();
            if (kind == 2) {
                if (importReq) importReq();
                return;
            }
            if (kind == 3) {
                if (fileReq) fileReq(QStringLiteral("colour"), {}, dir);
                return;
            }
            if (kind == 4) return; // folders open on double-click
            choose(it);
            if (switchOnClick && tool != 6 && switchToFace) switchToFace();
            if (applyOnClick) applyCurrent();
        });
        QObject::connect(list, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *it) {
            int kind = it->data(Qt::UserRole + 1).toInt();
            if (kind == 4) {
                enter(it->data(Qt::UserRole).toString());
            } else if (kind == 1) {
                if (propertiesReq) propertiesReq(it->data(Qt::UserRole).toString());
            } else if (kind == 0) {
                choose(it);
                applyCurrent();
            }
        });
        QObject::connect(list, &QWidget::customContextMenuRequested, this, [this](const QPoint &pos) {
            QListWidgetItem *it = list->itemAt(pos);
            const int kind = it ? it->data(Qt::UserRole + 1).toInt() : -1;
            QMenu m;
            auto file = [this](const QString &op, const QStringList &k, const QString &a) { if (fileReq) fileReq(op, k, a); };
            if (kind == 1) {
                const QString key = it->data(Qt::UserRole).toString();
                const TexEntry *e = lib->find(key);
                if (!e) return;
                m.addAction(QStringLiteral("Material Properties…"), [this, key] { if (propertiesReq) propertiesReq(key); });
                m.addAction(QStringLiteral("Select Faces Using This"), [this, key] { selectUsing(key); });
                m.addAction(QStringLiteral("Replace This Texture…"), [this, key] { replaceDialog(key); });
                m.addAction(e->converted ? QStringLiteral("Convert Again") : QStringLiteral("Convert to VTF"), [this, key] { if (convertReq) convertReq({key}); });
                m.addSeparator();
                m.addAction(QStringLiteral("Cut"), [file, key] { file(QStringLiteral("cut"), {key}, QString()); })->setShortcut(QKeySequence::Cut);
                m.addAction(QStringLiteral("Copy"), [file, key] { file(QStringLiteral("copy"), {key}, QString()); })->setShortcut(QKeySequence::Copy);
                m.addAction(QStringLiteral("Move to Folder…"), [file, key] { file(QStringLiteral("moveto"), {key}, QString()); });
                m.addAction(QStringLiteral("Rename…"), [file, key] { file(QStringLiteral("rename"), {key}, QString()); })->setShortcut(QKeySequence(Qt::Key_F2));
                m.addAction(QStringLiteral("Delete…"), [file, key] { file(QStringLiteral("delete"), {key}, QString()); })->setShortcut(QKeySequence::Delete);
                m.addSeparator();
                const QString path = e->file;
                m.addAction(QStringLiteral("Show in Folder"), [path] { QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(path).absolutePath())); });
            } else if (kind == 4) {
                const QString folder = it->data(Qt::UserRole).toString();
                m.addAction(QStringLiteral("Open"), [this, folder] { enter(folder); });
                QAction *pa = m.addAction(QStringLiteral("Paste Into"), [file, folder] { file(QStringLiteral("paste"), {}, folder); });
                pa->setEnabled(canPaste);
                m.addAction(QStringLiteral("Rename Folder…"), [file, folder] { file(QStringLiteral("renamefolder"), {}, folder); });
                m.addAction(QStringLiteral("Delete Folder…"), [file, folder] { file(QStringLiteral("deletefolder"), {}, folder); });
            } else {
                if (!lib->hasProject()) return;
                m.addAction(QStringLiteral("New Folder…"), [file, this] { file(QStringLiteral("newfolder"), {}, dir); });
                QAction *pa = m.addAction(QStringLiteral("Paste"), [file, this] { file(QStringLiteral("paste"), {}, dir); });
                pa->setEnabled(canPaste);
                pa->setShortcut(QKeySequence::Paste);
                m.addSeparator();
                m.addAction(QStringLiteral("New Color…"), [file, this] { file(QStringLiteral("colour"), {}, dir); });
                m.addAction(QStringLiteral("Import…"), [this] { if (importReq) importReq(); });
            }
            m.exec(list->viewport()->mapToGlobal(pos));
        });
        QObject::connect(applyBtn, &QPushButton::clicked, this, [this] { applyCurrent(); });
        QObject::connect(importBtn, &QPushButton::clicked, this, [this] { if (importReq) importReq(); });
        QObject::connect(convertBtn, &QPushButton::clicked, this, [this] { if (convertReq) convertReq({}); });
        rebuild();
    }

    /// The folder being browsed (relative to assets/Textures; "" = top).
    QString folder() const { return dir; }

    void enter(const QString &d) {
        dir = d;
        if (group->currentData().toString() != QStringLiteral("#")) {
            QSignalBlocker b(group);
            group->setCurrentIndex(0);
        }
        rebuild();
    }

    /// Rebuild the swatches from the library.
    void rebuild() {
        const QString mode = group->currentData().toString();
        const QString f = filter->text().trimmed().toLower();
        const bool browse = mode == QStringLiteral("#") && f.isEmpty();
        const QStringList folders = lib->groups();
        if (!dir.isEmpty() && !folders.contains(dir)) dir.clear(); // it went away
        // breadcrumb
        if (browse && lib->hasProject()) {
            QString html = QStringLiteral("<a href='/'>Textures</a>");
            QString acc;
            for (const QString &part : dir.split(QLatin1Char('/'), Qt::SkipEmptyParts)) {
                acc = acc.isEmpty() ? part : acc + QLatin1Char('/') + part;
                html += QStringLiteral(" › <a href='%1'>%2</a>").arg(acc.toHtmlEscaped(), part.toHtmlEscaped());
            }
            crumbs->setText(html);
            crumbs->show();
        } else {
            crumbs->hide();
        }

        list->clear();
        const QSet<QString> others = thisModelOnly->isChecked() && otherModelKeys ? otherModelKeys() : QSet<QString>();
        auto otherModels = [&](const QString &key) {
            return thisModelOnly->isChecked() && (others.contains(key) || key.startsWith(QStringLiteral("models/")));
        };
        const bool showBuiltins = mode == QStringLiteral("@") || mode == QStringLiteral("*") || (browse && dir.isEmpty()) ||
                                  (!f.isEmpty() && (QStringLiteral("nodraw").contains(f)));
        if (showBuiltins) {
            addItem(QStringLiteral("nodraw"), QStringLiteral("tools/toolsnodraw"), builtinIcon(true),
                    QStringLiteral("nodraw (tools/toolsnodraw)\nNever rendered: for faces nobody will see"), QStringLiteral("@"));
        }
        if (browse) {
            for (const QString &g : folders) {
                const QString parent = g.contains(QLatin1Char('/')) ? g.section(QLatin1Char('/'), 0, -2) : QString();
                if (parent != dir) continue;
                int n = 0;
                for (const TexEntry &e : lib->entries) n += (e.group == g || e.group.startsWith(g + QLatin1Char('/'))) && !otherModels(e.key);
                if (n == 0 && thisModelOnly->isChecked()) continue; // only other models' textures in there
                auto *it = new QListWidgetItem(folderIcon(), g.section(QLatin1Char('/'), -1));
                it->setData(Qt::UserRole, g);
                it->setData(Qt::UserRole + 1, 4);
                it->setData(Qt::UserRole + 2, QStringLiteral("@folder"));
                it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDropEnabled);
                it->setToolTip(QStringLiteral("%1/ — %2 texture%3\nDouble-click to open; drag swatches onto it to move them; right-click for more")
                                   .arg(g).arg(n).arg(n == 1 ? QString() : QStringLiteral("s")));
                list->addItem(it);
            }
        }
        int unconverted = 0;
        for (const TexEntry &e : lib->entries) {
            unconverted += !e.converted;
            if (mode == QStringLiteral("@") && f.isEmpty()) continue;
            if (otherModels(e.key)) continue;
            if (browse && e.group != dir) continue;
            if (!f.isEmpty() && !e.key.contains(f) && !QFileInfo(e.rel).completeBaseName().toLower().contains(f)) continue;
            const bool over = !QString::fromUtf8(bonk_mat_override(e.key.toUtf8().constData())).isEmpty();
            QString tip = QStringLiteral("%1\n%2 × %3 · assets/Textures/%4").arg(e.key).arg(e.size.width()).arg(e.size.height()).arg(e.rel);
            if (needsResize(e.size)) tip += QStringLiteral("\nNot a power of two (or over 4096): Source needs it resized");
            tip += e.converted ? QStringLiteral("\nConverted to VTF") : QStringLiteral("\nNot converted to VTF yet");
            if (e.props) tip += QStringLiteral("\nHas material properties");
            if (over) tip += QStringLiteral("\nThis model overrides its properties");
            tip += QStringLiteral("\nDouble-click: Material Properties · right-click: more · drag onto a folder to move");
            addItem(QFileInfo(e.rel).completeBaseName(), e.key, badged(e.icon, !e.converted, e.props || over), tip, e.group);
        }
        if (mode != QStringLiteral("@") && f.isEmpty()) {
            auto *col = new QListWidgetItem(colourIcon(), QStringLiteral("Color"));
            col->setData(Qt::UserRole + 1, 3);
            col->setData(Qt::UserRole + 2, QStringLiteral("@add"));
            col->setFlags(Qt::ItemIsEnabled);
            col->setToolTip(QStringLiteral("A solid color as a texture: a 16×16 image in colors/, named by its hex code"));
            list->addItem(col);
            auto *add = new QListWidgetItem(addIcon(), QStringLiteral("Import"));
            add->setData(Qt::UserRole + 1, 2);
            add->setData(Qt::UserRole + 2, QStringLiteral("@add"));
            add->setFlags(Qt::ItemIsEnabled);
            add->setToolTip(QStringLiteral("Import PNG / JPG files into this folder, or drop them onto the picker"));
            list->addItem(add);
        }
        convertBtn->setEnabled(lib->hasProject() && !lib->entries.empty());
        convertBtn->setText(unconverted ? QStringLiteral("Convert All (%1)").arg(unconverted) : QStringLiteral("Convert All"));
        syncCurrent();
    }

    /// Select the swatch for the editor's current material.
    void syncCurrent() {
        const QString cur = QString::fromUtf8(bonk_current_material());
        QSignalBlocker b(list);
        list->clearSelection();
        for (int i = 0; i < list->count(); ++i) {
            QListWidgetItem *it = list->item(i);
            if (it->data(Qt::UserRole + 1).toInt() != 2 && it->data(Qt::UserRole).toString() == cur) {
                list->setCurrentItem(it);
                it->setSelected(true);
                list->scrollToItem(it);
                break;
            }
        }
        QString name = displayName(cur);
        bool missing = !cur.isEmpty() && cur != QStringLiteral("tools/toolsnodraw") && !lib->find(cur);
        current->setText(QStringLiteral("Current: <b>%1</b>%2").arg(name.toHtmlEscaped(),
                         missing ? QStringLiteral(" <span style='color:#e0823d'>(not in this project)</span>") : QString()));
        updateHint();
    }

    /// Called when the tool changes: the Face section shows in Face Select & Paint.
    void setTool(int t) {
        if (t == tool) return;
        tool = t;
        faceBox->setVisible(tool == 6);
        updateHint();
        refreshFace();
    }

    void refresh() {
        if (!isVisible()) return;
        QString t = QString::fromUtf8(bonk_material_target());
        if (t != lastTarget) {
            lastTarget = t;
            target->setText(QStringLiteral("Applies to:<br><b>%1</b>").arg(t.toHtmlEscaped()));
        }
        refreshFace();
    }

    /// Show the "convert it before it gets Propper'd" banner.
    void showStale(const QStringList &keys) {
        for (const QString &k : keys)
            if (!staleKeys.contains(k)) staleKeys << k;
        if (staleKeys.isEmpty()) return;
        QString names = staleKeys.size() == 1 ? QStringLiteral("<b>%1</b> changed").arg(staleKeys.first().section(QLatin1Char('/'), -1).toHtmlEscaped())
                                              : QStringLiteral("<b>%1 textures</b> changed").arg(staleKeys.size());
        bannerText->setText(QStringLiteral("%1, so the old VTF is gone. Convert it before it gets <i>Propper'd</i>!").arg(names));
        banner->show();
    }

protected:
    void showEvent(QShowEvent *e) override {
        lastTarget.clear();
        refresh();
        QWidget::showEvent(e);
    }

    // ---- drag and drop from Dolphin (or anything that drags files)
    static QStringList localPaths(const QMimeData *m) {
        QStringList out;
        if (!m->hasUrls()) return out;
        for (const QUrl &u : m->urls()) {
            if (!u.isLocalFile()) continue;
            QFileInfo fi(u.toLocalFile());
            const QString suf = fi.suffix().toLower();
            if (fi.isDir() || suf == QStringLiteral("png") || suf == QStringLiteral("jpg") || suf == QStringLiteral("jpeg"))
                out << fi.absoluteFilePath();
        }
        return out;
    }
    /// Where dropped / imported files go: the folder being browsed ("" = ask).
    QString dropGroup() const {
        return group->currentData().toString() == QStringLiteral("#") ? dir : QString();
    }
    bool beginDrop(const QMimeData *m) {
        if (localPaths(m).isEmpty()) return false;
        dropping = true;
        const QString g = dropGroup();
        list->setStyleSheet(QStringLiteral("QListWidget { border: 2px solid palette(highlight); }"));
        hint->setText(g.isEmpty() ? QStringLiteral("<b>Drop to import</b> (you'll pick a group)") : QStringLiteral("<b>Drop to import into %1</b>").arg(g.toHtmlEscaped()));
        hint->show();
        return true;
    }
    void finishDrop(const QMimeData *m) {
        QStringList paths = localPaths(m);
        endDrop();
        if (paths.isEmpty()) return;
        const QString g = dropGroup();
        // let the drop finish (Dolphin waits for it) before any dialogs
        QTimer::singleShot(0, this, [this, paths, g] { if (dropped) dropped(paths, g); });
    }
    void dragEnterEvent(QDragEnterEvent *e) override {
        if (beginDrop(e->mimeData())) e->acceptProposedAction();
    }
    void dragMoveEvent(QDragMoveEvent *e) override { e->acceptProposedAction(); }
    void dragLeaveEvent(QDragLeaveEvent *) override { endDrop(); }
    void dropEvent(QDropEvent *e) override {
        e->acceptProposedAction();
        finishDrop(e->mimeData());
    }

    /// Keys in the swatch grid: Ctrl+X / C / V, Delete, F2, Backspace.
    bool listKey(QKeyEvent *e) {
        if (!fileReq) return false;
        QListWidgetItem *it = list->currentItem();
        const bool tex = it && it->data(Qt::UserRole + 1).toInt() == 1 && it->isSelected();
        const QString key = tex ? it->data(Qt::UserRole).toString() : QString();
        if (e->matches(QKeySequence::Cut) && tex) fileReq(QStringLiteral("cut"), {key}, QString());
        else if (e->matches(QKeySequence::Copy) && tex) fileReq(QStringLiteral("copy"), {key}, QString());
        else if (e->matches(QKeySequence::Paste)) fileReq(QStringLiteral("paste"), {}, dir);
        else if (e->matches(QKeySequence::Delete) && tex) fileReq(QStringLiteral("delete"), {key}, QString());
        else if (e->key() == Qt::Key_F2 && tex) fileReq(QStringLiteral("rename"), {key}, QString());
        else if (e->key() == Qt::Key_Backspace && !dir.isEmpty()) enter(dir.contains(QLatin1Char('/')) ? dir.section(QLatin1Char('/'), 0, -2) : QString());
        else return false;
        return true;
    }

private:
    TextureLibrary *lib;
    QFrame *banner;
    QLabel *bannerText;
    QStringList staleKeys;
    QLabel *target, *current, *hint, *crumbs;
    QString dir; // folder being browsed
    QLineEdit *filter;
    QCheckBox *thisModelOnly;
    QComboBox *group;
    SwatchList *list;
    QSlider *zoom;
    QPushButton *applyBtn, *convertBtn;
    QString lastTarget;
    bool dropping = false;
    int tool = -1;

    // Face section
    QGroupBox *faceBox;
    QLabel *faceCount, *facePreview, *faceTexName;
    QPushButton *selUsingBtn, *replaceBtn;

    QStringList selectedMats() const {
        return QString::fromUtf8(bonk_selected_face_materials()).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    }

    void selectUsing(const QString &key) {
        QByteArray k = key.toUtf8();
        const int n = bonk_select_faces_with(k.constData());
        if (switchToFace) switchToFace();
        if (changed) changed(F_R3D | F_R2D | F_TOOL);
        (void)n;
        refreshFace();
    }

    /// Replace a texture: on the selected faces, or everywhere in the model.
    void replaceDialog(const QString &find) {
        QDialog d(this);
        d.setWindowTitle(QStringLiteral("Replace texture"));
        auto *form = new QFormLayout(&d);
        auto *findBox = new QComboBox;
        auto *withBox = new QComboBox;
        findBox->setEditable(true);
        withBox->setEditable(true);
        QStringList keys{QStringLiteral("tools/toolsnodraw")};
        for (const TexEntry &e : lib->entries) keys << e.key;
        const QStringList sel = selectedMats();
        if (find.isEmpty() && sel.size() > 1) findBox->addItem(QStringLiteral("(any texture on the selected faces)"), QString());
        for (const QString &k : keys) findBox->addItem(displayName(k), k);
        if (!find.isEmpty()) {
            int i = findBox->findData(find);
            if (i < 0) { findBox->addItem(displayName(find), find); i = findBox->count() - 1; }
            findBox->setCurrentIndex(i);
        }
        for (const QString &k : keys) withBox->addItem(displayName(k), k);
        const QString cur = QString::fromUtf8(bonk_current_material());
        int wi = withBox->findData(cur);
        withBox->setCurrentIndex(wi < 0 ? 0 : wi);
        auto *everywhere = new QRadioButton(QStringLiteral("Everywhere in the model"));
        auto *selected = new QRadioButton(QStringLiteral("Only on the selected faces"));
        selected->setEnabled(!sel.isEmpty());
        (sel.isEmpty() || !find.isEmpty() ? everywhere : selected)->setChecked(true);
        form->addRow(QStringLiteral("Find"), findBox);
        form->addRow(QStringLiteral("Replace with"), withBox);
        form->addRow(everywhere);
        form->addRow(selected);
        form->addRow(hintLabel(QStringLiteral("Texture alignment stays as it is. Ctrl+Z undoes it.")));
        auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        bb->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Replace"));
        form->addRow(bb);
        QObject::connect(bb, &QDialogButtonBox::accepted, &d, &QDialog::accept);
        QObject::connect(bb, &QDialogButtonBox::rejected, &d, &QDialog::reject);
        if (d.exec() != QDialog::Accepted) return;
        auto keyOf = [](QComboBox *c) {
            const int i = c->findText(c->currentText());
            return i >= 0 ? c->itemData(i).toString() : cleanGamePath(c->currentText());
        };
        const QByteArray f = keyOf(findBox).toUtf8(), w = keyOf(withBox).toUtf8();
        bonk_replace_material(f.constData(), w.constData(), selected->isChecked());
        if (changed) changed(F_R3D | F_R2D | F_TOOL);
        refreshFace();
    }
    QDoubleSpinBox *fShiftU, *fShiftV, *fScaleU, *fScaleV, *fRot;
    QCheckBox *lockBox, *scaleBox, *groupBox;
    bool syncingFace = false;

    static QString displayName(const QString &m) {
        if (m.isEmpty() || m == QStringLiteral("tools/toolsnodraw")) return QStringLiteral("nodraw");
        return m;
    }

    void updateHint() {
        if (dropping) return;
        const QString cur = displayName(QString::fromUtf8(bonk_current_material()));
        if (gCompact) {
            list->setToolTip(tool == 6 ? QStringLiteral("Right-click a face in 3D to paint the current texture (Shift: whole brush, Alt: wrap on). "
                                                        "Drag a selected face to move its texture; Ctrl+drag rotates, Shift+drag scales. Ctrl+wheel zooms.")
                                       : QStringLiteral("Click a swatch to paint with it, double-click for Material Properties. Ctrl+wheel zooms. Drop images here to import."));
        }
        hint->setText(tool == 6 ? QStringLiteral("Right-click a face to paint <b>%1</b> (Shift: whole brush, Alt: wrap on). "
                                                 "Drag a selected face: move the texture; Ctrl+drag rotate, Shift+drag scale.").arg(cur.toHtmlEscaped())
                                : QStringLiteral("Click a swatch to paint with it. Drop PNG / JPG files or folders here to import them."));
    }

    void buildFaceSection(QVBoxLayout *v) {
        faceBox = new QGroupBox(QStringLiteral("Face"));
        auto *g = new QGridLayout(faceBox);
        g->setContentsMargins(8, 6, 8, 8);
        g->setHorizontalSpacing(6);
        g->setVerticalSpacing(4);
        // the selected face's texture: a thumbnail and its name
        auto *pv = new QHBoxLayout;
        facePreview = new QLabel;
        facePreview->setFixedSize(48, 48);
        facePreview->setScaledContents(true);
        facePreview->setFrameShape(QFrame::StyledPanel);
        faceTexName = new QLabel;
        faceTexName->setWordWrap(true);
        faceTexName->setTextInteractionFlags(Qt::TextSelectableByMouse);
        pv->addWidget(facePreview);
        pv->addWidget(faceTexName, 1);
        g->addLayout(pv, 0, 0, 1, 4);
        faceCount = new QLabel;
        g->addWidget(faceCount, 8, 0, 1, 4);
        auto *fr = new QHBoxLayout;
        selUsingBtn = new QPushButton(QStringLiteral("Select All Using"));
        selUsingBtn->setToolTip(QStringLiteral("Select every face in the model that uses this texture"));
        replaceBtn = new QPushButton(QStringLiteral("Replace…"));
        replaceBtn->setToolTip(QStringLiteral("Swap this texture for another, on the selected faces or everywhere in the model"));
        fr->addWidget(selUsingBtn);
        fr->addWidget(replaceBtn);
        g->addLayout(fr, 9, 0, 1, 4);
        QObject::connect(selUsingBtn, &QPushButton::clicked, this, [this] {
            const QStringList m = selectedMats();
            if (!m.isEmpty()) selectUsing(m.first());
        });
        QObject::connect(replaceBtn, &QPushButton::clicked, this, [this] {
            const QStringList m = selectedMats();
            replaceDialog(m.size() == 1 ? m.first() : QString());
        });
        auto spin = [](double lo, double hi, double step, int dec) {
            auto *s = new QDoubleSpinBox;
            s->setRange(lo, hi);
            s->setSingleStep(step);
            s->setDecimals(dec);
            s->setKeyboardTracking(false);
            s->setAccelerated(true);
            s->setMinimumWidth(60);
            s->setMinimumHeight(24);
            return s;
        };
        fShiftU = spin(-65536, 65536, 8, 1);
        fShiftV = spin(-65536, 65536, 8, 1);
        fScaleU = spin(-64, 64, 0.05, 3);
        fScaleV = spin(-64, 64, 0.05, 3);
        fRot = spin(-360, 360, 15, 1);
        fRot->setWrapping(true);
        fRot->setRange(-180, 180);
        g->addWidget(new QLabel(QStringLiteral("Shift")), 1, 0);
        g->addWidget(fShiftU, 1, 1);
        g->addWidget(fShiftV, 1, 2);
        g->addWidget(new QLabel(QStringLiteral("Scale")), 2, 0);
        g->addWidget(fScaleU, 2, 1);
        g->addWidget(fScaleV, 2, 2);
        g->addWidget(new QLabel(QStringLiteral("Rotate")), 3, 0);
        g->addWidget(fRot, 3, 1);
        fShiftU->setToolTip(QStringLiteral("Horizontal shift in texels"));
        fShiftV->setToolTip(QStringLiteral("Vertical shift in texels"));
        fScaleU->setToolTip(QStringLiteral("Horizontal scale: world units per texel (0.25 is Hammer's default)"));
        fScaleV->setToolTip(QStringLiteral("Vertical scale: world units per texel"));
        fRot->setToolTip(QStringLiteral("Rotation in degrees"));
        auto *just = new QGridLayout;
        just->setSpacing(2);
        auto jb = [this, just](const QString &t, const QString &tip, int mode, int r, int c, int span = 1) {
            auto *b = new QPushButton(t);
            b->setToolTip(tip);
            b->setMinimumWidth(10);
            b->setMinimumHeight(24);
            b->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
            QObject::connect(b, &QPushButton::clicked, this, [this, mode] { if (changed) changed(bonk_face_justify(mode)); refreshFace(); });
            just->addWidget(b, r, c, 1, span);
        };
        jb(QStringLiteral("L"), QStringLiteral("Justify left"), 0, 0, 0);
        jb(QStringLiteral("R"), QStringLiteral("Justify right"), 1, 0, 1);
        jb(QStringLiteral("T"), QStringLiteral("Justify top"), 2, 0, 2);
        jb(QStringLiteral("B"), QStringLiteral("Justify bottom"), 3, 0, 3);
        jb(QStringLiteral("C"), QStringLiteral("Center"), 4, 0, 4);
        jb(QStringLiteral("Stretch"), QStringLiteral("Fit one copy of the image across the face (or the whole sheet with Treat as one)"), 5, 1, 0, 3);
        jb(QStringLiteral("Reset"), QStringLiteral("Back to world-aligned, scale 0.25"), 6, 1, 3, 2);
        g->addLayout(just, 4, 0, 1, 4);
        groupBox = new QCheckBox(QStringLiteral("Treat as one"));
        groupBox->setToolTip(QStringLiteral("With several faces selected, the controls work on them as one surface:\n"
                                            "Stretch, justify, shift, scale and rotate use the group's edges and center,\n"
                                            "and faces around corners are unfolded from the source face so seams line up."));
        lockBox = new QCheckBox(QStringLiteral("Texture Lock"));
        lockBox->setToolTip(QStringLiteral("Textures travel with brushes you move or rotate"));
        scaleBox = new QCheckBox(QStringLiteral("Scale texture with brush"));
        scaleBox->setToolTip(QStringLiteral("With Texture Lock: resizing a brush stretches its textures too.\n"
                                            "Off: textures keep their size and stay anchored."));
        g->addWidget(groupBox, 5, 0, 1, 4);
        g->addWidget(lockBox, 6, 0, 1, 2);
        g->addWidget(scaleBox, 7, 0, 1, 4);
        v->addWidget(faceBox);
        faceBox->hide();

        QSettings st;
        lockBox->setChecked(st.value("texture/lock", true).toBool());
        scaleBox->setChecked(st.value("texture/scaleWithBrush", false).toBool());
        scaleBox->setEnabled(lockBox->isChecked());
        bonk_set_texlock(lockBox->isChecked(), scaleBox->isChecked());
        auto lockChanged = [this] {
            QSettings().setValue("texture/lock", lockBox->isChecked());
            QSettings().setValue("texture/scaleWithBrush", scaleBox->isChecked());
            scaleBox->setEnabled(lockBox->isChecked());
            bonk_set_texlock(lockBox->isChecked(), scaleBox->isChecked());
        };
        QObject::connect(lockBox, &QCheckBox::toggled, this, lockChanged);
        QObject::connect(scaleBox, &QCheckBox::toggled, this, lockChanged);
        QObject::connect(groupBox, &QCheckBox::toggled, this, [](bool on) { bonk_set_tex_group(on); });
        int field = 0;
        for (QDoubleSpinBox *s : {fShiftU, fShiftV, fScaleU, fScaleV, fRot}) {
            QObject::connect(s, &QDoubleSpinBox::valueChanged, this, [this, field](double val) {
                if (syncingFace) return;
                if (changed) changed(bonk_face_set(field, float(val)));
                refreshFace();
            });
            ++field;
        }
    }

    /// Show the source face's values in the Face section.
    void refreshFace() {
        if (tool != 6 || !faceBox->isVisible()) return;
        float info[8] = {0};
        int n = bonk_face_info(info);
        syncingFace = true;
        for (QDoubleSpinBox *s : {fShiftU, fShiftV, fScaleU, fScaleV, fRot}) s->setEnabled(n > 0);
        if (n > 0) {
            if (!fShiftU->hasFocus()) fShiftU->setValue(info[1]);
            if (!fShiftV->hasFocus()) fShiftV->setValue(info[2]);
            if (!fScaleU->hasFocus()) fScaleU->setValue(info[3]);
            if (!fScaleV->hasFocus()) fScaleV->setValue(info[4]);
            if (!fRot->hasFocus()) fRot->setValue(info[5]);
            faceCount->setText(n == 1 ? QStringLiteral("1 face · image %1 × %2").arg(int(info[6])).arg(int(info[7]))
                                      : QStringLiteral("%1 faces (values from the source face)").arg(n));
        } else {
            faceCount->setText(QStringLiteral("Click faces in the 3D view to edit their textures"));
        }
        const QStringList mats = n > 0 ? selectedMats() : QStringList();
        facePreview->setVisible(!mats.isEmpty());
        faceTexName->setVisible(!mats.isEmpty());
        selUsingBtn->setEnabled(mats.size() == 1);
        if (mats.size() == 1) {
            const QString k = mats.first();
            const TexEntry *e = lib->find(k);
            const bool nd = k.isEmpty() || k == QStringLiteral("tools/toolsnodraw");
            facePreview->setPixmap((e ? e->icon : builtinIcon(nd)).pixmap(96, 96));
            faceTexName->setText(QStringLiteral("<b>%1</b>%2").arg(displayName(k).toHtmlEscaped(),
                                 !e && !nd ? QStringLiteral("<br><span style='color:#e0823d'>not in this project</span>") : QString()));
        } else if (mats.size() > 1) {
            facePreview->setPixmap(QPixmap());
            faceTexName->setText(QStringLiteral("<b>%1 textures</b>").arg(mats.size()));
        }
        groupBox->setEnabled(n > 1);
        syncingFace = false;
    }

    void endDrop() {
        dropping = false;
        list->setStyleSheet(QString());
        if (gCompact) hint->hide();
        updateHint();
    }

    void addItem(const QString &label, const QString &key, const QIcon &icon, const QString &tip, const QString &grp) {
        auto *it = new QListWidgetItem(icon, label);
        it->setFlags(grp == QStringLiteral("@") ? (Qt::ItemIsEnabled | Qt::ItemIsSelectable) : (Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsDragEnabled));
        it->setData(Qt::UserRole, key);
        it->setData(Qt::UserRole + 1, grp == QStringLiteral("@") ? 0 : 1);
        it->setData(Qt::UserRole + 2, grp);
        it->setToolTip(tip);
        list->addItem(it);
    }

    void setZoom(int z) {
        list->setIconSize(QSize(z, z));
        QFontMetrics fm(list->font());
        list->setGridSize(QSize(z + 14, z + fm.height() + 10));
        QSettings().setValue("picker/zoom", z);
    }

    void choose(QListWidgetItem *it) {
        QByteArray k = it->data(Qt::UserRole).toString().toUtf8();
        bonk_set_current_material(k.constData());
        syncCurrent();
    }

    void applyCurrent() {
        bonk_apply_material(bonk_current_material());
        lastTarget.clear();
        refresh();
        if (changed) changed(F_R3D | F_R2D | F_TOOL);
    }

    static QIcon builtinIcon(bool nodraw) {
        QPixmap px(128, 128);
        px.fill(nodraw ? QColor(219, 189, 64) : QColor(90, 94, 104));
        QPainter p(&px);
        p.setRenderHint(QPainter::Antialiasing);
        if (nodraw) {
            p.setPen(QPen(QColor(120, 100, 30), 10));
            for (int k = -128; k < 256; k += 28) p.drawLine(k, 128, k + 128, 0);
        } else {
            p.setPen(QPen(QColor(200, 200, 200), 6));
            p.drawLine(24, 24, 104, 104);
            p.drawLine(104, 24, 24, 104);
        }
        return QIcon(px);
    }

    static QIcon folderIcon() {
        QPixmap px(128, 128);
        px.fill(Qt::transparent);
        QPainter p(&px);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(214, 160, 60));
        p.drawRoundedRect(QRectF(10, 26, 48, 22), 6, 6);
        p.setBrush(QColor(236, 186, 80));
        p.drawRoundedRect(QRectF(10, 38, 108, 72), 8, 8);
        p.setBrush(QColor(255, 255, 255, 40));
        p.drawRoundedRect(QRectF(16, 44, 96, 10), 4, 4);
        return QIcon(px);
    }

    static QIcon colourIcon() {
        QPixmap px(128, 128);
        px.fill(Qt::transparent);
        QPainter p(&px);
        p.setRenderHint(QPainter::Antialiasing);
        const QColor cs[4] = {QColor(230, 70, 70), QColor(240, 190, 50), QColor(70, 170, 90), QColor(70, 120, 230)};
        for (int i = 0; i < 4; ++i) {
            p.setPen(Qt::NoPen);
            p.setBrush(cs[i]);
            p.drawPie(QRectF(14, 14, 100, 100), i * 90 * 16, 90 * 16);
        }
        p.setBrush(QColor(255, 255, 255, 230));
        p.drawEllipse(QPointF(64, 64), 22, 22);
        p.setPen(QPen(QColor(80, 80, 80), 6));
        p.drawLine(64, 52, 64, 76);
        p.drawLine(52, 64, 76, 64);
        return QIcon(px);
    }

    static QIcon addIcon() {
        QPixmap px(128, 128);
        px.fill(Qt::transparent);
        QPainter p(&px);
        p.setRenderHint(QPainter::Antialiasing);
        QPen pen(QColor(150, 150, 150), 3, Qt::DashLine);
        p.setPen(pen);
        p.drawRoundedRect(QRectF(6, 6, 116, 116), 8, 8);
        p.setPen(QPen(QColor(170, 170, 170), 5));
        p.drawLine(64, 40, 64, 88);
        p.drawLine(40, 64, 88, 64);
        return QIcon(px);
    }

    /// Swatch badges: orange corner = not converted yet, blue dot = has
    /// material properties.
    static QIcon badged(const QIcon &icon, bool unconverted, bool props) {
        if (!unconverted && !props) return icon;
        QPixmap px = icon.pixmap(QSize(192, 192));
        if (px.isNull()) return icon;
        QPainter p(&px);
        p.setRenderHint(QPainter::Antialiasing);
        const qreal s = std::max(px.width(), px.height()) * 0.24;
        p.setPen(Qt::NoPen);
        if (unconverted) {
            QPolygonF tri;
            tri << QPointF(px.width() - s, 0) << QPointF(px.width(), 0) << QPointF(px.width(), s);
            p.setBrush(QColor(242, 140, 40));
            p.drawPolygon(tri);
        }
        if (props) {
            p.setBrush(QColor(255, 255, 255));
            p.drawEllipse(QPointF(s * 0.5, px.height() - s * 0.5), s * 0.32, s * 0.32);
            p.setBrush(QColor(60, 140, 240));
            p.drawEllipse(QPointF(s * 0.5, px.height() - s * 0.5), s * 0.24, s * 0.24);
        }
        return QIcon(px);
    }
};


// ------------------------------------------------------------------ budget bar

/// Limits the meters measure against (what GMod's studiomdl handles; editable).
struct BudgetLimits {
    int tris = 10922;   // per material: 32,768 indices / 3
    int verts = 65536;
    int materials = 64;
    int pieces = 1200;  // the meter turns yellow past this
    void load() {
        QSettings s;
        tris = s.value("budget/trianglesPerMaterial", 10922).toInt();
        verts = s.value("budget/vertices", 65536).toInt();
        materials = s.value("budget/materials", 64).toInt();
        pieces = s.value("budget/collisionPieces", 1200).toInt();
        if (pieces == 20) pieces = 1200;
    }
    void save() const {
        QSettings s;
        s.setValue("budget/trianglesPerMaterial", tris);
        s.setValue("budget/vertices", verts);
        s.setValue("budget/materials", materials);
        s.setValue("budget/collisionPieces", pieces);
    }
};

/// One meter: label, bar, "used / limit". Green, yellow from 75 %, red at the limit.
class BudgetMeter : public QWidget {
public:
    QString name;
    int used = 0, limit = 1;
    bool soft = false; // only a warning: yellow past the limit, never red
    std::function<void()> clicked;

    explicit BudgetMeter(const QString &n) : name(n) {
        setMinimumWidth(gCompact ? 150 : 190);
        setMaximumWidth(gCompact ? 300 : 360); // label, bar and numbers stay together
        setFixedHeight(gCompact ? lowOr(18, 14) : 22);
        setCursor(Qt::PointingHandCursor);
        setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
    }

protected:
    void mousePressEvent(QMouseEvent *e) override {
        if (e->button() == Qt::LeftButton && clicked) clicked();
    }
    void paintEvent(QPaintEvent *) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        const double frac = limit > 0 ? double(used) / limit : 0.0;
        QColor col = soft ? (frac >= 1.0 ? QColor(230, 175, 40) : QColor(70, 170, 90))
                          : frac >= 1.0 ? QColor(220, 60, 50) : frac >= 0.75 ? QColor(230, 175, 40) : QColor(70, 170, 90);
        QFont f = uiPx(font(), gCompact ? 10 : 11);
        p.setFont(f);
        QFontMetrics fm(f);
        const QString mark = soft ? (frac >= 1.0 ? QStringLiteral("▲ ") : QString())
                                  : frac >= 1.0 ? QStringLiteral("✖ ") : frac >= 0.75 ? QStringLiteral("▲ ") : QString();
        const QString left = mark + name;
        const QString right = soft ? (frac >= 1.0 ? QStringLiteral("%1 · Expensive collision mesh, model may still work fine").arg(QLocale().toString(used)) : QLocale().toString(used))
                                   : QStringLiteral("%1 / %2").arg(QLocale().toString(used), QLocale().toString(limit));
        const int lw = fm.horizontalAdvance(left) + 6;
        // a long note gives way: the bar keeps at least 40 px, the note is shortened with …
        const int rw = soft ? std::min(fm.horizontalAdvance(right) + 6, std::max(40, width() - lw - 46)) : fm.horizontalAdvance(right) + 6;
        const QString rightShown = soft ? fm.elidedText(right, Qt::ElideRight, rw - 6) : right;
        QRectF bar(lw, height() * 0.25, std::max(20, width() - lw - rw), height() * 0.5);
        p.setPen(palette().color(QPalette::WindowText));
        p.drawText(QRect(0, 0, lw, height()), Qt::AlignVCenter | Qt::AlignLeft, left);
        p.drawText(QRect(width() - rw, 0, rw, height()), Qt::AlignVCenter | Qt::AlignRight, rightShown);
        p.setPen(Qt::NoPen);
        p.setBrush(palette().color(QPalette::Mid));
        p.drawRoundedRect(bar, 3, 3);
        QRectF fill = bar;
        fill.setWidth(bar.width() * std::min(1.0, frac));
        p.setBrush(col);
        p.drawRoundedRect(fill, 3, 3);
    }
};

/// The bottom bar: what the model costs, live, against studiomdl's limits.
class BudgetBar : public QWidget {
public:
    BudgetLimits limits;
    std::function<void(unsigned)> changed;

    BudgetBar() {
        limits.load();
        auto *h = new QHBoxLayout(this);
        h->setContentsMargins(gCompact ? 6 : 10, 2, gCompact ? 6 : 10, 2);
        h->setSpacing(gCompact ? 10 : 18);
        meters[0] = new BudgetMeter(QStringLiteral("Triangles"));
        meters[1] = new BudgetMeter(QStringLiteral("Vertices"));
        meters[2] = new BudgetMeter(QStringLiteral("Materials"));
        meters[3] = new BudgetMeter(QStringLiteral("Collision pieces"));
        meters[3]->soft = true;
        meters[3]->setMinimumWidth(gCompact ? 200 : 260); // room for some of "Expensive collision mesh…"
        meters[3]->setMaximumWidth(gCompact ? 460 : 620);
        for (int i = 0; i < 4; ++i) {
            h->addWidget(meters[i], 1);
            if (i == 3) h->addStretch(1);
            meters[i]->clicked = [this, i] {
                bonk_budget_offenders(i, 1);
                if (changed) changed(F_R3D | F_R2D);
            };
        }
        timer.setSingleShot(true);
        timer.setInterval(150);
        QObject::connect(&timer, &QTimer::timeout, this, [this] { update_(); });
        update_();
    }

    /// Ask for a refresh (coalesced).
    void poke() {
        if (!timer.isActive()) timer.start();
    }

    /// Anything over its limit, for a warning before compiling.
    QStringList over() const {
        QStringList o;
        for (BudgetMeter *m : meters)
            if (!m->soft && m->used > m->limit) o << QStringLiteral("%1: %2 / %3").arg(m->name).arg(m->used).arg(m->limit);
        return o;
    }

    void update_() {
        int b[5] = {0};
        bonk_budget(b);
        const int used[4] = {b[1], b[2], b[3], b[4]};
        const int lim[4] = {limits.tris, limits.verts, limits.materials, limits.pieces};
        for (int i = 0; i < 4; ++i) {
            meters[i]->used = used[i];
            meters[i]->limit = std::max(1, lim[i]);
            meters[i]->update();
        }
        const QString top0 = QString::fromUtf8(bonk_budget_offenders(0, 0));
        meters[0]->setToolTip(QStringLiteral("Triangles of the busiest material: %1 (whole model: %2).\n"
                                             "studiomdl draws each material in one go, capped at 32,768 indices = %3 triangles.\n"
                                             "Nodraw faces don't count, so nodraw hidden faces to save budget.\n\nHeaviest:\n%4\n\nClick to select them.")
                                  .arg(b[1]).arg(b[0]).arg(limits.tris).arg(top0.isEmpty() ? QStringLiteral("(nothing yet)") : top0));
        meters[1]->setToolTip(QStringLiteral("Vertices as the compiler counts them (each face's corners separately).\n\nBiggest:\n%1\n\nClick to select them.")
                                  .arg(QString::fromUtf8(bonk_budget_offenders(1, 0))));
        meters[2]->setToolTip(QStringLiteral("Different materials on visible faces (nodraw doesn't count). Past 64, Source starts leaking memory.\n\nClick to select one brush per material."));
        meters[3]->setToolTip(QStringLiteral("Convex pieces in the collision layers: each collision brush is one.\n"
                                             "Bonk raises studiomdl's limit as needed; past %1 the meter turns yellow:\n"
                                             "an expensive collision mesh, but the model may still work fine.\n"
                                             "Simplified collision (or J, Join Brushes) brings it down.\n\nClick to select them.").arg(limits.pieces));
    }

private:
    BudgetMeter *meters[4];
    QTimer timer;
};

// ------------------------------------------------------------------ guide & about

#include "guide.inc"
#include "importer.inc"
#include "spawnlist.inc"
#include "prefabs.inc"
#include "workshop.inc"
#include "variants.inc"

// ------------------------------------------------------------------ main window

constexpr int kLayoutVersion = 10;

/// New collision layer: a copy of the visual model, or a simplified hull of it.
class CollisionDialog : public QDialog {
public:
    int mode = 1;
    double detail = 8.0;

    CollisionDialog(QWidget *parent, double startDetail, bool forCompile, bool allowSimplified = true) : QDialog(parent), detail(startDetail) {
        setWindowTitle(forCompile ? QStringLiteral("Generate a collision model") : QStringLiteral("New collision layer"));
        auto *v = new QVBoxLayout(this);
        if (forCompile)
            v->addWidget(hintLabel(QStringLiteral("This model has no collision layer, so it would have no physics: it can't be picked up "
                                                  "and things fall through it. Bonk can make one for this compile (it isn't saved into "
                                                  "the model; Layers → + Collision makes one you can edit).")));
        complexBtn = new QRadioButton(QStringLiteral("Complex"));
        auto *ct = hintLabel(QStringLiteral("A complex mesh has all of the geometric detail of your visual model: one convex piece per brush."));
        simpleBtn = new QRadioButton(QStringLiteral("Simplified"));
        auto *st = hintLabel(QStringLiteral("A simplified mesh removes as many details as possible, optimizing it automatically for you: "
                                            "walls, floors, roofs and stairs stay (stairs become ramps), windows are filled in, open "
                                            "doorways stay open, and pieces that line up are merged."));
        v->addWidget(complexBtn);
        v->addWidget(ct);
        v->addWidget(simpleBtn);
        v->addWidget(st);
        auto *rowW = new QWidget;
        auto *row = new QHBoxLayout(rowW);
        row->setContentsMargins(0, 0, 0, 0);
        row->addSpacing(22);
        row->addWidget(new QLabel(QStringLiteral("Detail size")));
        detailBox = new QComboBox;
        for (int g : {1, 2, 4, 8, 16, 32, 64}) detailBox->addItem(QStringLiteral("Grid %1").arg(g), g);
        detailBox->setToolTip(QStringLiteral("Brushes thinner than this on their second-longest side count as details"));
        int di = detailBox->findData(int(std::lround(startDetail)));
        detailBox->setCurrentIndex(di < 0 ? detailBox->findData(8) : di);
        row->addWidget(detailBox);
        row->addStretch(1);
        v->addWidget(rowW);
        count = new QLabel;
        v->addWidget(count);
        // Simplified collision is an Advanced feature (Settings → Advanced mode)
        for (QWidget *w : {static_cast<QWidget *>(simpleBtn), static_cast<QWidget *>(st), rowW, static_cast<QWidget *>(count)}) w->setVisible(allowSimplified);
        auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        bb->button(QDialogButtonBox::Ok)->setText(forCompile ? QStringLiteral("Generate") : QStringLiteral("Create"));
        if (forCompile) {
            QPushButton *without = bb->addButton(QStringLiteral("Compile Without"), QDialogButtonBox::ActionRole);
            QObject::connect(without, &QPushButton::clicked, this, [this] { mode = -1; accept(); });
        }
        v->addWidget(bb);
        (allowSimplified ? simpleBtn : complexBtn)->setChecked(true);
        timer.setSingleShot(true);
        timer.setInterval(250);
        QObject::connect(&timer, &QTimer::timeout, this, [this] { recount(); });
        QObject::connect(detailBox, &QComboBox::currentIndexChanged, this, [this] { count->setText(QStringLiteral("Counting…")); timer.start(); });
        QObject::connect(simpleBtn, &QRadioButton::toggled, this, [this](bool on) { detailBox->setEnabled(on); });
        QObject::connect(bb, &QDialogButtonBox::accepted, this, [this] {
            mode = simpleBtn->isChecked() ? 1 : 0;
            detail = detailBox->currentData().toDouble();
            accept();
        });
        QObject::connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
        resize(520, sizeHint().height());
        if (allowSimplified) QTimer::singleShot(0, this, [this] { recount(); });
    }

private:
    QRadioButton *complexBtn, *simpleBtn;
    QComboBox *detailBox;
    QLabel *count;
    QTimer timer;

    void recount() {
        int out[2] = {0, 0};
        const int n = bonk_simplify_count(float(detailBox->currentData().toDouble()), out);
        count->setText(QStringLiteral("Complex: <b>%1</b> pieces · Simplified: <b>%2</b> pieces").arg(QLocale().toString(out[1]), QLocale().toString(n)));
    }
};

class MainWindow : public QMainWindow {
public:
    MainWindow();

    Viewport *views[4];
    QLabel *status;
    QAction *toolSelect, *toolBlock, *toolVertex, *toolBisect, *toolMannequin, *toolOrigin, *toolFace;
    QAction *texAction = nullptr, *showMannequins = nullptr, *ignoreGroupsAct = nullptr, *nodraw2dAct = nullptr, *edges3dAct = nullptr;
    QAction *lookGhost = nullptr, *lookSolid = nullptr;
    QAction *importVmfAct = nullptr, *savePrefabAct = nullptr, *lookMenuAct = nullptr;
    bool modeApplying = false;     // applyMode() is running
    bool pickerHiddenForCollision = false; // the Texture Picker was put away while editing collision
    int appliedMode = -1;          // 0 Simple, 1 Advanced (as last applied)
    QSpinBox *fovSpin = nullptr;
    ToolPanel *panel = nullptr;
    LayersPane *layers = nullptr;
    TexturePicker *picker = nullptr;
    TextureLibrary library;
    Project project;
    QDockWidget *toolsDock = nullptr, *pickerDock = nullptr, *layersDock = nullptr, *budgetDock = nullptr, *prefabsDock = nullptr;
    PrefabsPane *prefabs = nullptr;
    VariantsPane *variants = nullptr;
    ModelPreview *previewWin = nullptr;
    QDockWidget *variantsDock = nullptr;
    BudgetBar *budget = nullptr;
    QByteArray defaultLayout;
    QMenu *recentMenu = nullptr, *recentModelsMenu = nullptr;
    QAction *projectSettingsAct = nullptr, *closeProjectAct = nullptr;
    bool captured = false;
    Prefs prefs;
    QElapsedTimer saveFlash; // "Project saved!" blink in every view
    QString filePath;
    // Cursor pinned during a hold-key action (S / Shift / R): the view, where
    // the cursor is frozen, and the virtual position the mouse has moved to.
    int pinView = -1;
    QPoint pinLocal;
    QPointF pinVirt;

    QSplitter *rowTop = nullptr, *rowBottom = nullptr;
    bool expanded3d = false, full3d = false;
    QList<QDockWidget *> fullHidDocks; // put away for F4
    bool fullWasMax = false;

    /// Shift+Z: the 3D view fills the views' area (press again for all four).
    void setExpand3d(bool on) {
        if (on == expanded3d) return;
        expanded3d = on;
        for (int i = 1; i < 4; ++i) views[i]->setVisible(!on);
        rowBottom->setVisible(!on);
        if (!on) rowBottom->setSizes(rowTop->sizes());
        views[0]->setFocus();
        statusBar()->showMessage(on ? QStringLiteral("3D view expanded (Shift+Z for all four views)") : QString(), 3000);
    }
    void toggleExpand3d() {
        if (full3d) toggleFull3d(); // F4 includes the expansion
        else setExpand3d(!expanded3d);
    }
    /// F4: the 3D view alone, full screen: no menus, panels or status bar.
    void toggleFull3d() {
        full3d = !full3d;
        if (full3d) {
            fullWasMax = isMaximized();
            fullHidDocks.clear();
            for (QDockWidget *d : findChildren<QDockWidget *>())
                if (d->isVisible() && !d->isFloating()) {
                    fullHidDocks << d;
                    d->hide();
                }
            menuBar()->hide();
            statusBar()->hide();
            setExpand3d(true);
            showFullScreen();
        } else {
            for (QDockWidget *d : fullHidDocks) d->show();
            fullHidDocks.clear();
            menuBar()->show();
            statusBar()->show();
            setExpand3d(false);
            if (fullWasMax) showMaximized();
            else showNormal();
        }
        views[0]->setFocus();
    }

    unsigned long long viewsRev = 0;
    bool lastColl = false; // the document edit all four views were last drawn for
    void handle(unsigned flags, Viewport *src = nullptr) {
        // any edit (deleting brushes too) redraws every view, so nothing it removed lingers in one
        if (const unsigned long long rev = bonk_layers_rev(); rev != viewsRev) {
            viewsRev = rev;
            flags |= F_R3D | F_R2D;
        }
        if (flags & F_CAP_ON) setCapture(true);
        if (flags & F_CAP_OFF) setCapture(false);
        if (flags & F_R3D) views[0]->update();
        if (flags & F_R2D)
            for (int i = 1; i < 4; ++i) views[i]->update();
        if (const bool coll = bonk_editing_collision(); coll != lastColl) {
            lastColl = coll; // into or out of collision editing: Paint and Texture come and go
            flags |= F_TOOL;
        }
        if (flags & F_TOOL) {
            syncTool();
            if (picker) picker->syncCurrent();
        }
        unsigned cur = (flags >> 8) & 0xff;
        if (cur && src && !captured) {
            src->setCursor(cursorOf(cur));
            src->cursorKind = -1;
        }
        if (src && !captured) decorateCursor(src);
        status->setText(QString::fromUtf8(bonk_status()));
        updateTitle();
        if (layers) {
            layers->refresh();
            layers->syncSelection();
        }
        if (picker) picker->refresh();
        if (budget) budget->poke();
        syncPin();
    }

    /// Pin or release the cursor to follow the editor's hold-key actions.
    void syncPin() {
        int v = (prefs.pinCursor && !captured) ? bonk_hold_view() : -1;
        if (v == pinView) return;
        if (pinView > 0) {
            Viewport *old = views[pinView];
            pinView = -1;
            old->releaseMouse();
            QCursor::setPos(old->mapToGlobal(pinLocal)); // back where it was
        }
        if (v > 0 && v < 4) {
            Viewport *w = views[v];
            pinLocal = w->mapFromGlobal(QCursor::pos());
            pinVirt = QPointF(pinLocal);
            w->rel.start(QPointF(pinLocal));
            pinView = v;
            w->grabMouse();
        }
    }

    /// Cursor badges: a paintbrush in Face Select & Paint, and the held key's
    /// action (G move, Shift copy, R rotate, S size) while one is held.
    void decorateCursor(Viewport *v) {
        int hint[2] = {-1, 0};
        bonk_hold_hint(hint);
        int kind = 0;
        if (hint[1] >= 1 && hint[1] <= 4 && hint[0] == v->id) kind = hint[1];
        else if (bonk_tool() == 6 && (v->cursor().shape() == Qt::ArrowCursor || v->cursorKind == 6)) kind = 6;
        if (kind == v->cursorKind) return;
        if (kind == 0) {
            if (v->cursorKind > 0) v->setCursor(Qt::ArrowCursor);
            v->cursorKind = 0;
            return;
        }
        v->cursorKind = kind;
        v->setCursor(badgeCursor(kind));
    }

    static QCursor badgeCursor(int kind) {
        static std::map<int, QCursor> cache;
        auto it = cache.find(kind);
        if (it != cache.end()) return it->second;
        const int S = 40;
        QPixmap pm(S, S);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        // a plain arrow
        QPolygonF arrow({QPointF(1, 1), QPointF(1, 17), QPointF(5.5, 13), QPointF(8.5, 19.5), QPointF(11, 18.5), QPointF(8, 12), QPointF(13.5, 12)});
        p.setPen(QPen(Qt::black, 1.2));
        p.setBrush(Qt::white);
        p.drawPolygon(arrow);
        const QRectF badge(17, 17, 21, 21);
        if (kind == 6) {
            QString ic = toolIconPath(QStringLiteral("paint-cursor"));
            if (!ic.isEmpty()) p.drawPixmap(badge.toRect(), QIcon(ic).pixmap(QSize(42, 42)));
        } else {
            static const char *letters[5] = {"", "G", "⧉", "R", "S"};
            static const QColor cols[5] = {QColor(), QColor(70, 140, 230), QColor(80, 180, 110), QColor(220, 140, 50), QColor(200, 90, 200)};
            p.setPen(QPen(QColor(0, 0, 0, 200), 1.2));
            p.setBrush(cols[kind]);
            p.drawRoundedRect(badge.adjusted(1, 1, -1, -1), 5, 5);
            QFont f = smoothFont();
            f.setBold(true);
            f.setPixelSize(13);
            p.setFont(f);
            p.setPen(Qt::white);
            p.drawText(badge, Qt::AlignCenter, QString::fromUtf8(letters[kind]));
        }
        p.end();
        return cache[kind] = QCursor(pm, 1, 1);
    }

    /// Mouse 4 / 5: Select / Face Select & Paint, with the tool's icon flashing by the cursor.
    void quickTool(Viewport *v, bool paint, QPointF at) {
        command(paint ? CMD_TOOL_FACE : CMD_TOOL_SELECT);
        v->toolFlash.start();
        v->toolFlashIcon = paint ? QStringLiteral("face") : QStringLiteral("select");
        v->toolFlashAt = at;
        v->update();
        for (int ms : {120, 300, 500, 720}) QTimer::singleShot(ms, v, [v] { v->update(); });
    }

    // ---------------------------------------------------------- physics

    void newCollisionLayer() {
        int mode = prefs.advanced ? 1 : 0; // Simple mode: a Complex copy, no question to answer
        double detail = prefs.simplifyDetail;
        if (prefs.advanced && !qEnvironmentVariableIsSet("BONK_ASSUME_YES")) {
            CollisionDialog d(this, prefs.simplifyDetail, false, prefs.advanced);
            if (d.exec() != QDialog::Accepted) return;
            mode = d.mode;
            detail = d.detail;
        }
        if (mode == 1 && detail != prefs.simplifyDetail) {
            prefs.simplifyDetail = detail;
            prefs.save();
            prefs.apply();
        }
        bonk_new_collision(mode, float(detail));
        handle(F_R3D | F_R2D | F_TOOL);
    }

    // ---------------------------------------------------------------- spawnlist

    /// A spawn icon: the saved model drawn off screen with a transparent
    /// background, at 2× and scaled down for smooth edges.
    QImage renderThumb(const QString &bpmdl, const ThumbCam &cam, int w, int h) {
        Viewport *v = views[0];
        if (qEnvironmentVariableIsSet("BONK_SHOT_THUMBS")) { // developer aid: no GL off screen, use thumbnails drawn elsewhere
            QImage img(qEnvironmentVariable("BONK_SHOT_THUMBS") + QLatin1Char('/') + QFileInfo(bpmdl).completeBaseName() + QStringLiteral(".png"));
            if (img.isNull()) return img;
            if (w != h) { // crop the square stand-in to the shape
                const int s = img.width();
                img = w > h ? img.copy(0, (s - s * h / w) / 2, s, s * h / w) : img.copy((s - s * w / h) / 2, 0, s * w / h, s);
            }
            return img.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
        }
        if (!v->context() || !v->isValid()) return QImage();
        v->makeCurrent();
        // drawn at twice the size (up to 1024 on the longer side) and scaled down, for smooth edges
        const double up = std::min(2.0, 1024.0 / std::max(w, h));
        const int sw = std::max(1, int(w * up)), sh = std::max(1, int(h * up));
        QOpenGLFramebufferObjectFormat fmt;
        fmt.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        fmt.setSamples(0);
        QImage out;
        {
            QOpenGLFramebufferObject fbo(sw, sh, fmt);
            if (fbo.isValid() && fbo.bind()) {
                QOpenGLFunctions *gl = v->context()->functions();
                gl->glViewport(0, 0, sw, sh);
                const QByteArray p = QFile::encodeName(bpmdl);
                ThumbCam c = cam;
                if (c.dist <= 0) c = fitCam(bpmdl, c, double(w) / h);
                if (bonk_render_thumb(0, p.constData(), float(c.yaw), float(c.pitch), float(std::max(1.0, c.dist)), float(c.tx), float(c.ty), float(c.tz),
                                      float(c.fov), sw, sh) == 0)
                    out = fbo.toImage(true).convertToFormat(QImage::Format_ARGB32);
                fbo.release();
            }
        }
        v->doneCurrent();
        if (out.isNull()) return out;
        return out.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    ThumbRenderer thumbRenderer() {
        return [this](const QString &f, const ThumbCam &c, int w, int h) { return renderThumb(f, c, w, h); };
    }

    /// View → Model Preview: the model as compiled (skin, bodygroups, LOD), off screen.
    QImage renderPreview(int skin, const QString &choices, int lod, const ThumbCam &cam, int w, int h, bool grid, int *tris) {
        Viewport *v = views[0];
        if (tris) *tris = 0;
        if (!v->context() || !v->isValid()) return QImage();
        v->makeCurrent();
        const double up = std::min(2.0, 2048.0 / std::max(w, h));
        const int sw = std::max(1, int(w * up)), sh = std::max(1, int(h * up));
        QOpenGLFramebufferObjectFormat fmt;
        fmt.setAttachment(QOpenGLFramebufferObject::CombinedDepthStencil);
        QImage out;
        {
            QOpenGLFramebufferObject fbo(sw, sh, fmt);
            if (fbo.isValid() && fbo.bind()) {
                v->context()->functions()->glViewport(0, 0, sw, sh);
                const QByteArray c = choices.toUtf8();
                const int n = bonk_render_preview(0, skin, c.constData(), lod, float(cam.yaw), float(cam.pitch), float(std::max(1.0, cam.dist)),
                                                  float(cam.tx), float(cam.ty), float(cam.tz), float(cam.fov), sw, sh, grid);
                if (n >= 0) {
                    out = fbo.toImage(true).convertToFormat(QImage::Format_ARGB32);
                    if (tris) *tris = n;
                }
                fbo.release();
            }
        }
        v->doneCurrent();
        return out.isNull() ? out : out.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    }

    /// View → Model Preview…
    void openPreview() {
        if (!previewWin) {
            previewWin = new ModelPreview(this);
            previewWin->render = [this](int skin, const QString &c, int lod, const ThumbCam &cam, int w, int h, bool grid, int *tris) {
                return renderPreview(skin, c, lod, cam, w, h, grid, tris);
            };
        }
        previewWin->show();
        previewWin->raise();
        previewWin->reload(true);
    }

    /// Tools → Spawnlist Editor…
    void openSpawnlists() {
        if (!project.isOpen()) {
            QMessageBox::information(this, QStringLiteral("Spawnlist Editor"), QStringLiteral("Open a project first: the spawnlist belongs to the project's addon."));
            return;
        }
        if (bonk_is_dirty() && !filePath.isEmpty() && !qEnvironmentVariableIsSet("BONK_SCREENSHOT")) save(); // spawn icons are drawn from the saved models
        SpawnlistDialog d(this, project, thumbRenderer(), [this](const QString &r) {
            if (!r.isEmpty()) statusBar()->showMessage(r.startsWith(QLatin1Char('!')) ? r.mid(1) : r, 8000);
        });
        if (qEnvironmentVariableIsSet("BONK_SPAWNLIST_SELECT")) {
            const QStringList a = qEnvironmentVariable("BONK_SPAWNLIST_SELECT").split(QLatin1Char(','));
            d.select(a.value(0).toInt(), a.value(1, QStringLiteral("-1")).toInt());
        }
        if (qEnvironmentVariableIsSet("BONK_SPAWNLIST_DROP")) d.devDrop(qEnvironmentVariable("BONK_SPAWNLIST_DROP"));
        if (qEnvironmentVariableIsSet("BONK_SPAWNLIST_ADDONS")) d.devPickModels(); // developer aid: Add Models → From Addons
        if (qEnvironmentVariableIsSet("BONK_SPAWNLIST_TREEMOVE")) {
            const QStringList m = qEnvironmentVariable("BONK_SPAWNLIST_TREEMOVE").split(QLatin1Char(','));
            d.devTreeMove(m.value(0).toInt(), m.value(1).toInt(), m.value(2).toInt());
        }
        if (qEnvironmentVariableIsSet("BONK_SPAWNLIST_ENTER")) { // developer aid: press Enter in the dialog
            d.show();
            QWidget *f = d.focusWidget() ? d.focusWidget() : &d;
            QKeyEvent dn(QEvent::KeyPress, Qt::Key_Return, Qt::NoModifier), up(QEvent::KeyRelease, Qt::Key_Return, Qt::NoModifier);
            QApplication::sendEvent(f, &dn);
            QApplication::sendEvent(f, &up);
        }
        if (qEnvironmentVariableIsSet("BONK_SCREENSHOT")) {
            d.show();
            for (int i = 0; i < 5; ++i) QApplication::processEvents(); // let deferred layouts run
            d.grab().save(qEnvironmentVariable("BONK_SCREENSHOT") + QStringLiteral(".dialog.png"));
            return;
        }
        d.exec();
    }

    /// After a compile (and before an export): the spawnlist into the addon, when there is one.
    void writeSpawnlistQuiet(CompileWindow *cw = nullptr) {
        if (!project.isOpen()) return;
        Spawnlists sl;
        if (!sl.load(project) || sl.empty()) return;
        const QString r = writeSpawnlists(sl, project, thumbRenderer());
        if (cw && !r.isEmpty()) cw->note(r.startsWith(QLatin1Char('!')) ? r.mid(1).toHtmlEscaped() : r.toHtmlEscaped());
    }

    /// Tools → Set Mass…: the model's weight in game.
    void setMass() {
        QDialog d(this);
        d.setWindowTitle(QStringLiteral("Set Mass"));
        auto *v = new QVBoxLayout(&d);
        auto *autoBox = new QCheckBox(QStringLiteral("Automatic (studiomdl works it out from the collision model's volume)"));
        auto *kg = new QDoubleSpinBox;
        kg->setRange(0.1, 50000.0);
        kg->setDecimals(1);
        kg->setSuffix(QStringLiteral(" kg"));
        kg->setGroupSeparatorShown(true);
        const float cur = bonk_mass();
        autoBox->setChecked(cur <= 0.0f);
        kg->setValue(cur > 0.0f ? cur : 500.0);
        kg->setEnabled(cur > 0.0f);
        auto *row = new QHBoxLayout;
        row->addWidget(new QLabel(QStringLiteral("Mass")));
        row->addWidget(kg, 1);
        v->addWidget(autoBox);
        v->addLayout(row);
        auto *grid = new QGridLayout;
        struct P { const char *name; double kg; const char *tip; };
        const P presets[] = {{"Feather-light", 1, "A paper cup: blows around"},
                             {"Light (ragdoll)", 90, "About a person"},
                             {"Moderate", 500, "A heavy crate or a car door"},
                             {"Heavy", 5000, "A car"},
                             {"Very Heavy (house)", 50000, "Source's maximum: a house barely budges"}};
        int i = 0;
        for (const P &p : presets) {
            auto *b = new QPushButton(QStringLiteral("%1\n%2 kg").arg(QString::fromUtf8(p.name), QLocale().toString(p.kg, 'f', 0)));
            b->setToolTip(QString::fromUtf8(p.tip));
            const double val = p.kg;
            QObject::connect(b, &QPushButton::clicked, &d, [autoBox, kg, val] { autoBox->setChecked(false); kg->setValue(val); });
            grid->addWidget(b, i / 3, i % 3);
            ++i;
        }
        v->addLayout(grid);
        v->addWidget(hintLabel(QStringLiteral("Written as $mass in the QC's collision model when you compile. Saved in the model.")));
        QObject::connect(autoBox, &QCheckBox::toggled, kg, [kg](bool a) { kg->setEnabled(!a); });
        auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        QObject::connect(bb, &QDialogButtonBox::accepted, &d, &QDialog::accept);
        QObject::connect(bb, &QDialogButtonBox::rejected, &d, &QDialog::reject);
        v->addWidget(bb);
        if (d.exec() != QDialog::Accepted) return;
        bonk_set_mass(autoBox->isChecked() ? 0.0f : float(kg->value()));
        updateTitle();
        statusBar()->showMessage(autoBox->isChecked() ? QStringLiteral("Mass: automatic")
                                                      : QStringLiteral("Mass: %1 kg").arg(QLocale().toString(kg->value(), 'f', 1)), 4000);
    }

    /// Start Garry's Mod through Steam (it never touches the game's files).
    void startGmod() {
        QDesktopServices::openUrl(QUrl(QStringLiteral("steam://rungameid/4000")));
        statusBar()->showMessage(QStringLiteral("Starting Garry's Mod… (already running? change map to load new models)"), 6000);
    }

    // ---------------------------------------------------------- clipboard

    /// Ctrl+C / Ctrl+X: the selection goes on the system clipboard, so it can
    /// be pasted into another model, project or Bonk window.
    void copySelection(bool cut) {
        const QByteArray src = project.isOpen() ? QFile::encodeName(project.texturesDir()) : QByteArray();
        const QString text = QString::fromUtf8(bonk_copy(src.constData(), cut));
        if (!text.isEmpty()) {
            auto *md = new QMimeData;
            md->setData(QStringLiteral("application/x-bonk-brushes"), text.toUtf8());
            md->setText(text);
            QApplication::clipboard()->setMimeData(md);
        }
        handle(F_R3D | F_R2D | F_TOOL);
    }

    /// Ctrl+V (same spot) / Ctrl+Shift+V (under the mouse). Textures the
    /// brushes use that this project doesn't have are copied in from the
    /// project they came from.
    void pasteClipboard(bool atMouse) {
        const QMimeData *md = QApplication::clipboard()->mimeData();
        QString text = md && md->hasFormat(QStringLiteral("application/x-bonk-brushes"))
                           ? QString::fromUtf8(md->data(QStringLiteral("application/x-bonk-brushes")))
                           : (md ? md->text() : QString());
        if (!text.contains(QStringLiteral("BonkModel"))) {
            statusBar()->showMessage(QStringLiteral("Nothing to paste: copy brushes in Bonk first (Ctrl+C)"), 4000);
            return;
        }
        const int fetched = fetchClipboardTextures(text);
        const QByteArray t = text.toUtf8();
        handle(bonk_paste(t.constData(), atMouse), views[1]);
        if (fetched)
            statusBar()->showMessage(QStringLiteral("Copied %1 texture%2 in from the project the brushes came from")
                                         .arg(fetched).arg(fetched == 1 ? QString() : QStringLiteral("s")), 6000);
    }

    // ---------------------------------------------------------------- prefabs

    bool prefabsShown = false;

    /// The prefab's thumbnail: drawn like a spawn icon, from a three-quarter view.
    QImage prefabThumb(const PrefabInfo &p) {
        ThumbCam cam;
        cam.yaw = -55;
        cam.pitch = 28;
        return renderThumb(p.file, fitCam(p.file, cam, 1.0), 128, 128);
    }

    /// Edit → Save Selection as Prefab… (and the Prefabs tab's button).
    void savePrefab() {
        const QByteArray src = project.isOpen() ? QFile::encodeName(project.texturesDir()) : QByteArray();
        QString text = QString::fromUtf8(bonk_prefab_text(src.constData()));
        if (text.isEmpty()) {
            statusBar()->showMessage(QStringLiteral("Select the brushes to save as a prefab first"), 4000);
            return;
        }
        const float lift = bonk_selection_lift();
        SavePrefabDialog d(this, project.isOpen(), QStringLiteral("Prefab"));
        d.rootFor = [this](bool shared) { return shared ? sharedPrefabDir() : projectPrefabDir(project); };
        d.fillFolders();
        if (qEnvironmentVariableIsSet("BONK_PREFAB_NAME")) { // developer aid: answer the dialog
            const QStringList a = qEnvironmentVariable("BONK_PREFAB_NAME").split(QLatin1Char(','));
            d.name->setText(a.value(0));
            d.where->setCurrentIndex(a.value(2) == QStringLiteral("project") ? std::min(1, d.where->count() - 1) : 0);
            d.folder->setCurrentText(a.value(1));
        } else if (d.exec() != QDialog::Accepted) {
            return;
        }
        const bool shared = d.where->currentData().toBool();
        const QString root = shared ? sharedPrefabDir() : projectPrefabDir(project);
        const QString folder = prefabFolderPath(d.folder->currentText());
        const QString dir = root + (folder.isEmpty() ? QString() : QLatin1Char('/') + folder);
        const QString base = dir + QLatin1Char('/') + prefabFileName(d.name->text());
        if (QFileInfo::exists(base + QStringLiteral(".bpfab")) && !qEnvironmentVariableIsSet("BONK_ASSUME_YES") &&
            QMessageBox::question(this, QStringLiteral("Save as Prefab"), QStringLiteral("There's already a prefab called \"%1\" there. Replace it?")
                                                                              .arg(QFileInfo(base).fileName())) != QMessageBox::Yes)
            return;
        QDir().mkpath(dir);
        PrefabInfo p;
        p.file = base + QStringLiteral(".bpfab");
        p.shared = shared;
        p.name = QFileInfo(base).fileName();
        // shared prefabs carry copies of their textures (and their material properties)
        int copied = 0;
        if (shared) {
            QDir(p.textures()).removeRecursively();
            static const QRegularExpression matRe(QStringLiteral("\"mat\"\\s+\"([^\"]*)\""));
            QSet<QString> used;
            for (auto it = matRe.globalMatch(text); it.hasNext();) used.insert(it.next().captured(1));
            for (const TexEntry &e : library.entries) {
                if (!used.contains(e.key)) continue;
                const QString dest = p.textures() + QLatin1Char('/') + e.rel;
                QDir().mkpath(QFileInfo(dest).absolutePath());
                if (QFile::copy(e.file, dest)) {
                    ++copied;
                    const QString side = MatProps::sidecarFor(e.file);
                    if (QFileInfo::exists(side)) QFile::copy(side, MatProps::sidecarFor(dest));
                }
            }
        }
        static const QRegularExpression srcRe(QStringLiteral("\"source_textures\"\\s+\"[^\"]*\""));
        text.replace(srcRe, QStringLiteral("\"source_textures\"\t\"\"")); // set again where it's placed
        QFile f(p.file);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            QMessageBox::warning(this, QStringLiteral("Save as Prefab"), QStringLiteral("Couldn't write %1").arg(p.file));
            return;
        }
        f.write(text.toUtf8());
        f.close();
        QFile m(p.meta());
        if (m.open(QIODevice::WriteOnly | QIODevice::Truncate))
            m.write(QJsonDocument(QJsonObject{{"lift", double(lift)}, {"format", 1}}).toJson());
        QFile::remove(p.thumb()); // drawn fresh by the refresh
        prefabs->refresh();
        prefabsDock->show();
        prefabsDock->raise();
        statusBar()->showMessage(QStringLiteral("Saved the prefab \"%1\"%2%3").arg(p.name)
                                     .arg(shared ? QStringLiteral(" (shared)") : QStringLiteral(" in this project"))
                                     .arg(copied ? QStringLiteral(" with %1 texture%2").arg(copied).arg(copied == 1 ? QString() : QStringLiteral("s")) : QString()),
                                 6000);
    }

    // ---------------------------------------------------------------- workshop

    /// File → Manage Workshop…: Steam check, then the Workshop window.
    void manageWorkshop() {
        if (!project.isOpen()) {
            QMessageBox::information(this, QStringLiteral("Manage Workshop"), QStringLiteral("Open a project first: it's the project's addon that gets published."));
            return;
        }
        const bool dev = qEnvironmentVariableIsSet("BONK_SCREENSHOT");
        SteamFind sf;
        sf.detect();
        SteamStatus st;
        st.detect(sf);
        if (dev && qEnvironmentVariableIsSet("BONK_WORKSHOP_PERSONA")) { // developer aid
            st.running = true;
            st.persona = qEnvironmentVariable("BONK_WORKSHOP_PERSONA");
        }
        if (!st.running && !st.unsure) {
            QMessageBox box(QMessageBox::Information, QStringLiteral("Manage Workshop"),
                            QStringLiteral("Steam isn't running. Start Steam and sign in, then open Manage Workshop again."), QMessageBox::Close, this);
            QPushButton *startBtn = box.addButton(QStringLiteral("Start Steam"), QMessageBox::AcceptRole);
            box.exec();
            if (box.clickedButton() == startBtn) QDesktopServices::openUrl(QUrl(QStringLiteral("steam://open/main")));
            return;
        }
        QSettings s;
        if (!s.value("workshop/agreed", false).toBool() || qEnvironmentVariableIsSet("BONK_WORKSHOP_ASK")) {
            QMessageBox box(this);
            box.setWindowTitle(QStringLiteral("Manage Workshop"));
            box.setIcon(QMessageBox::Question);
            box.setText(QStringLiteral("<b>Use your Steam profile for the Workshop?</b>"));
            box.setInformativeText(QStringLiteral("%1<br><br>Bonk only uses your Steam profile to list your Workshop addons and to upload the ones "
                                                  "you publish from here. It doesn't read anything else, and it never sees your password: "
                                                  "Garry's Mod's own gmpublish talks to Steam.")
                                       .arg(st.persona.isEmpty() ? QStringLiteral("Steam is running.")
                                                                 : QStringLiteral("Steam is running and signed in as <b>%1</b>.").arg(st.persona.toHtmlEscaped())));
            auto *dontAsk = new QCheckBox(QStringLiteral("Don't ask again"));
            box.setCheckBox(dontAsk);
            box.setStandardButtons(QMessageBox::Ok | QMessageBox::No);
            box.setDefaultButton(QMessageBox::Ok);
            if (dev) {
                box.show();
                QApplication::processEvents();
                box.grab().save(qEnvironmentVariable("BONK_SCREENSHOT") + QStringLiteral(".ask.png"));
                box.hide();
            } else {
                if (box.exec() != QMessageBox::Ok) return;
                if (dontAsk->isChecked()) s.setValue("workshop/agreed", true);
            }
        }
        WorkshopDialog d(this, project, sf, st, thumbRenderer(), [](bool on) { bonk_thumb_grid(on); }, [] { return true; });
        d.status = [this](const QString &m) { statusBar()->showMessage(m, 8000); };
        if (dev) {
            if (qEnvironmentVariableIsSet("BONK_WORKSHOP_FAKE")) { // "list output file,details json file"
                const QStringList f = qEnvironmentVariable("BONK_WORKSHOP_FAKE").split(QLatin1Char(','));
                QFile a(f.value(0)), b(f.value(1));
                a.open(QIODevice::ReadOnly);
                b.open(QIODevice::ReadOnly);
                d.fakeList(QString::fromUtf8(a.readAll()), b.readAll());
            }
            d.show();
            if (qEnvironmentVariableIsSet("BONK_WORKSHOP_REFRESH")) d.devRefresh();
            if (qEnvironmentVariableIsSet("BONK_WORKSHOP_SELECT")) d.devSelect(qEnvironmentVariable("BONK_WORKSHOP_SELECT"));
            if (qEnvironmentVariableIsSet("BONK_WORKSHOP_SNAP")) d.devSnap(qEnvironmentVariable("BONK_SCREENSHOT") + QStringLiteral(".snap.png"));
            for (int i = 0; i < 5; ++i) QApplication::processEvents();
            d.grab().save(qEnvironmentVariable("BONK_SCREENSHOT") + QStringLiteral(".dialog.png"));
            if (qEnvironmentVariableIsSet("BONK_WORKSHOP_PUBLISH")) {
                d.devPublish(qEnvironmentVariable("BONK_WORKSHOP_PUBLISH"));
                QElapsedTimer t;
                t.start();
                while (t.elapsed() < 6000) QApplication::processEvents(QEventLoop::AllEvents, 50);
                for (QWidget *top : QApplication::topLevelWidgets())
                    if (top->isVisible() && top->windowTitle().startsWith(QStringLiteral("Publishing"))) top->grab().save(qEnvironmentVariable("BONK_SCREENSHOT") + QStringLiteral(".publish.png"));
            }
            return;
        }
        d.exec();
    }

    /// A prefab file dropped on view `view` at `at`.
    void dropPrefab(const QString &file, int view, QPointF at) {
        PrefabInfo p;
        p.file = file;
        p.name = QFileInfo(file).completeBaseName();
        p.shared = QFileInfo(file).absoluteFilePath().startsWith(QFileInfo(sharedPrefabDir()).absoluteFilePath() + QLatin1Char('/'));
        placePrefab(p, view, at);
    }

    /// Place a prefab: dropped in a view (view, at), or under the mouse (view -1).
    void placePrefab(const PrefabInfo &p, int view, QPointF at) {
        const QString text = prefabText(p, project.isOpen() ? project.texturesDir() : QString());
        if (!text.contains(QStringLiteral("BonkModel"))) {
            statusBar()->showMessage(QStringLiteral("Couldn't read the prefab %1").arg(p.name), 5000);
            return;
        }
        const int fetched = fetchClipboardTextures(text);
        const QByteArray t = text.toUtf8();
        handle(bonk_place_prefab(t.constData(), view, float(at.x()), float(at.y()), prefabs->keepHeight(), float(p.lift())),
               view >= 0 ? views[view] : views[1]);
        if (fetched)
            statusBar()->showMessage(QStringLiteral("Placed %1 and copied in %2 of its texture%3").arg(p.name).arg(fetched).arg(fetched == 1 ? QString() : QStringLiteral("s")), 6000);
        if (view >= 0) views[view]->setFocus(Qt::OtherFocusReason);
    }

    int fetchClipboardTextures(const QString &text) {
        if (!project.isOpen()) return 0;
        static const QRegularExpression srcRe(QStringLiteral("\"source_textures\"\\s+\"([^\"]*)\""));
        static const QRegularExpression matRe(QStringLiteral("\"mat\"\\s+\"([^\"]*)\""));
        const QString src = srcRe.match(text).captured(1);
        if (src.isEmpty() || !QFileInfo(src).isDir() || QFileInfo(src).canonicalFilePath() == QFileInfo(project.texturesDir()).canonicalFilePath())
            return 0;
        QSet<QString> want;
        for (auto it = matRe.globalMatch(text); it.hasNext();) {
            const QString m = it.next().captured(1);
            if (!m.startsWith(QStringLiteral("tools/"))) want.insert(m);
        }
        for (const TexEntry &e : library.entries) want.remove(e.key);
        if (want.isEmpty()) return 0;
        QStringList written;
        library.pauseScans(true);
        QDirIterator it(src, {QStringLiteral("*.png"), QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"), QStringLiteral("*.PNG"), QStringLiteral("*.JPG"), QStringLiteral("*.JPEG")},
                        QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString f = it.next();
            const QString rel = QDir(src).relativeFilePath(f);
            const QString key = cleanGamePath(rel.left(rel.lastIndexOf(QLatin1Char('.'))));
            if (!want.contains(key)) continue;
            const QString dest = project.texturesDir() + QLatin1Char('/') + rel;
            if (QFileInfo::exists(dest)) continue;
            QDir().mkpath(QFileInfo(dest).absolutePath());
            if (QFile::copy(f, dest)) {
                written << dest;
                want.remove(key);
                const QString side = MatProps::sidecarFor(f);
                if (QFileInfo::exists(side)) QFile::copy(side, MatProps::sidecarFor(dest));
            }
        }
        library.markAsked(written);
        library.pauseScans(false);
        return int(written.size());
    }

    // ---------------------------------------------------------- overlaps

    /// Tools → Find Overlapping Brushes: a list; clicking one selects the pair.
    void findOverlaps() {
        const QString r = QString::fromUtf8(bonk_overlap_scan());
        const QStringList lines = r.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        if (lines.isEmpty()) {
            QMessageBox::information(this, QStringLiteral("Overlapping brushes"),
                                     QStringLiteral("No brushes overlap. (Brushes that only touch are fine.)"));
            return;
        }
        auto *d = new QDialog(this);
        d->setAttribute(Qt::WA_DeleteOnClose);
        d->setWindowTitle(QStringLiteral("Overlapping brushes"));
        auto *v = new QVBoxLayout(d);
        int dups = 0;
        for (const QString &l : lines) dups += l.section(QLatin1Char(' '), 3, 3) == QStringLiteral("1");
        auto *info = new QLabel(QStringLiteral("<b>%1</b> pair%2 of brushes share space%3. Click one to select the pair and center the views on it.<br>"
                                               "Overlaps inside the visual model only waste triangles; in a collision model they make "
                                               "extra pieces. Visual and collision brushes aren't compared with each other.")
                                    .arg(lines.size()).arg(lines.size() == 1 ? QString() : QStringLiteral("s"))
                                    .arg(dups == 1 ? QStringLiteral(", one of them an exact duplicate (usually a leftover to delete)")
                                         : dups ? QStringLiteral(", %1 of them exact duplicates (usually leftovers to delete)").arg(dups) : QString()));
        info->setWordWrap(true);
        v->addWidget(info);
        auto *list = new QListWidget;
        for (const QString &l : lines) {
            const QStringList f = l.split(QLatin1Char(' '));
            if (f.size() < 4) continue;
            const bool dup = f[3] == QStringLiteral("1");
            auto *it = new QListWidgetItem(dup ? QStringLiteral("Duplicate: brush %1 and %2 are the same shape in the same place").arg(f[0], f[1])
                                               : QStringLiteral("Brush %1 and %2 share %3 units³").arg(f[0], f[1], QLocale().toString(f[2].toDouble(), 'f', 0)));
            it->setData(Qt::UserRole, f[0] + QLatin1Char(' ') + f[1]);
            if (dup) it->setForeground(QColor(230, 120, 60));
            list->addItem(it);
        }
        v->addWidget(list, 1);
        QObject::connect(list, &QListWidget::currentItemChanged, d, [this](QListWidgetItem *it) {
            if (!it) return;
            const QByteArray ids = it->data(Qt::UserRole).toString().toUtf8();
            handle(bonk_select_ids(ids.constData()));
        });
        auto *bb = new QDialogButtonBox(QDialogButtonBox::Close);
        auto *again = bb->addButton(QStringLiteral("Scan Again"), QDialogButtonBox::ActionRole);
        QObject::connect(bb, &QDialogButtonBox::rejected, d, &QDialog::close);
        QObject::connect(again, &QPushButton::clicked, d, [this, d] { d->close(); QTimer::singleShot(0, this, [this] { findOverlaps(); }); });
        v->addWidget(bb);
        d->resize(560, 420);
        d->show();
    }

    void command(int c) {
        handle(bonk_command(c));
        if (c == CMD_TOGGLE_SEL_BOX_MODE && bonk_sel_box_mode() != 0) {
            prefs.selBoxWait = bonk_sel_box_mode() == 2; // the same choice as Settings → Editing
            prefs.save();
        }
        if (c == CMD_TOGGLE_BOX_TOUCH || c == CMD_TOGGLE_AXIS_LOCK || c == CMD_TOGGLE_COLLISION_SOLID) {
            // remembered between sessions
            int o[4] = {0, 1, 1, 0};
            bonk_selection_options(o);
            prefs.selInsideOnly = !o[1];
            prefs.axisLock = o[2];
            prefs.collisionSolid = o[3];
            prefs.save();
            syncTool();
        }
    }

    /// T: show the Texture Picker (or bring its tab to the front); hide it if it's already in view.
    void toggleTexture() {
        if (!pickerDock->isVisible()) {
            pickerDock->show();
            pickerDock->raise();
        } else if (pickerDock->visibleRegion().isEmpty()) {
            pickerDock->raise();
        } else {
            pickerDock->hide();
        }
        syncTool();
    }

    void repaintAll() {
        for (Viewport *v : views) v->update();
    }

    void updateTitle() {
        QString name = filePath.isEmpty() ? QStringLiteral("Untitled") : QFileInfo(filePath).fileName();
        if (project.isOpen()) name += QStringLiteral(" — ") + project.name;
        setWindowTitle(QStringLiteral("%1[*] — %2").arg(name, QString::fromUtf8(kAppName)));
        setWindowModified(bonk_is_dirty());
    }

    /// What's on offer right now: Simple mode hides the expert features, and
    /// collision editing hides painting and textures.
    void applyMode() {
        if (modeApplying || !panel) return;
        modeApplying = true;
        const bool adv = prefs.advanced, coll = bonk_editing_collision();
        QSet<QString> hide;
        if (coll) hide << QStringLiteral("face") << QStringLiteral("texture");
        if (!adv) hide << QStringLiteral("origin") << QStringLiteral("axis-lock") << QStringLiteral("box-touch") << QStringLiteral("vertex-pick");
        int t = bonk_tool();
        if ((coll && t == 6) || (!adv && t == 5)) handle(bonk_command(CMD_TOOL_SELECT)); // Paint / Origin → Select
        if (!adv && bonk_vertex_pick()) handle(bonk_command(CMD_TOGGLE_VERTEX_PICK));
        int o[4] = {0, 1, 1, 0};
        bonk_selection_options(o);
        if (!adv) {
            // what Simple leaves: the box takes what it touches, no axis lock, solid collision
            if (!o[1]) handle(bonk_command(CMD_TOGGLE_BOX_TOUCH));
            if (o[2]) handle(bonk_command(CMD_TOGGLE_AXIS_LOCK));
            if (!o[3]) handle(bonk_command(CMD_TOGGLE_COLLISION_SOLID));
        }
        if (pickerDock) {
            if (coll && pickerDock->isVisible()) {
                pickerDock->hide();
                pickerHiddenForCollision = true;
                if (layersDock) layersDock->raise();
            } else if (!coll && pickerHiddenForCollision) {
                pickerDock->show();
                pickerHiddenForCollision = false;
            }
            pickerDock->toggleViewAction()->setEnabled(!coll);
        }
        if (appliedMode != int(adv)) {
            if (adv && appliedMode == 0) {
                // back to Advanced: the choices you had there
                bonk_selection_options(o);
                if (o[1] == prefs.selInsideOnly) handle(bonk_command(CMD_TOGGLE_BOX_TOUCH));
                if (bool(o[2]) != prefs.axisLock) handle(bonk_command(CMD_TOGGLE_AXIS_LOCK));
                if (bool(o[3]) != prefs.collisionSolid) handle(bonk_command(CMD_TOGGLE_COLLISION_SOLID));
            }
            appliedMode = int(adv);
            for (QDockWidget *d : {prefabsDock, variantsDock}) {
                if (!d) continue;
                d->toggleViewAction()->setVisible(adv);
                if (!adv) d->hide();
            }
            if (adv && prefabsDock && !prefabsDock->isVisible() && QSettings().value("window/simpleHidPanels", false).toBool()) {
                prefabsDock->show();
                variantsDock->show();
                tabifyDockWidget(layersDock, prefabsDock);
                tabifyDockWidget(prefabsDock, variantsDock);
                QTimer::singleShot(0, this, [this] { (pickerDock->isVisible() ? pickerDock : layersDock)->raise(); });
            }
            QSettings().setValue("window/simpleHidPanels", !adv);
            for (QAction *a : {importVmfAct, savePrefabAct, lookMenuAct})
                if (a) a->setVisible(adv);
            if (layers) {
                layers->simple = !adv;
            }
        }
        if (layers) {
            layers->ghostRow->setVisible(coll && adv);
            if (layers->lookRowW) layers->lookRowW->setVisible(adv);
        }
        panel->setHidden(hide);
        modeApplying = false;
    }

    void syncTool() {
        applyMode();
        int t = bonk_tool();
        const int bmode = bonk_bisect_delete();
        toolSelect->setChecked(t == 0);
        toolBlock->setChecked(t == 1);
        toolVertex->setChecked(t == 2);
        toolBisect->setChecked(t == 3);
        toolMannequin->setChecked(t == 4 && !bonk_place_kind());
        toolOrigin->setChecked(t == 5);
        toolFace->setChecked(t == 6);
        toolBisect->setText(bmode == 0 ? QStringLiteral("B&isect Tool (Split)")
                            : bmode == 1 ? QStringLiteral("B&isect Tool (Split && Delete Smaller)") : QStringLiteral("B&isect Tool (Split && Delete Larger)"));
        if (showMannequins) showMannequins->setChecked(bonk_show_mannequins());
        if (ignoreGroupsAct) ignoreGroupsAct->setChecked(bonk_view_flags() & 1);
        if (nodraw2dAct) nodraw2dAct->setChecked(prefs.nd2d);
        if (edges3dAct) edges3dAct->setChecked(bonk_show_edges());
        if (lookGhost) {
            lookGhost->setChecked(!prefs.collisionSolid);
            lookSolid->setChecked(prefs.collisionSolid);
        }
        if (layers) layers->syncCollisionLook(prefs.collisionSolid);
        if (panel) panel->sync(t, bmode, prefs.hammerKeys, prefs.greyTools, pickerDock && pickerDock->isVisible());
        if (picker) picker->setTool(t);
        if (prefs.rememberBisect && bmode != prefs.bisectMode) {
            prefs.bisectMode = bmode;
            QSettings().setValue("tools/bisectMode", bmode);
        }
    }

    /// Tool shortcuts: 1–4 always; Shift+B/V/X too with Hammer-style keys
    /// (Shift+S is handled in the 2D views so it can share S with sizing).
    void applyToolKeys() {
        auto keys = [this](QAction *a, int digit, int hammer) {
            QList<QKeySequence> ks{QKeySequence(digit)};
            if (prefs.hammerKeys && hammer) ks << QKeySequence(Qt::SHIFT | hammer);
            a->setShortcuts(ks);
        };
        keys(toolSelect, Qt::Key_1, 0);
        keys(toolBlock, Qt::Key_2, Qt::Key_B);
        keys(toolVertex, Qt::Key_3, Qt::Key_V);
        keys(toolBisect, Qt::Key_4, Qt::Key_X);
        toolBisect->setShortcuts(toolBisect->shortcuts() << QKeySequence(Qt::Key_B)); // B for Bisect, same as 4
        keys(toolOrigin, Qt::Key_5, 0);
        keys(toolFace, Qt::Key_6, 0);
        keys(texAction, Qt::Key_T, Qt::Key_A);
    }

    void setCapture(bool on) {
        Viewport *v = views[0];
        if (on == captured) return;
        captured = on;
        if (on) {
            v->setFocus(Qt::OtherFocusReason);
            v->grabMouse(Qt::BlankCursor);
            v->grabKeyboard();
            v->center = v->rect().center();
            QCursor::setPos(v->mapToGlobal(v->center));
            v->rel.reset(); // the first move says where the cursor really is
        } else {
            v->releaseKeyboard();
            v->releaseMouse();
            v->setCursor(Qt::ArrowCursor);
        }
    }

    // ---------------------------------------------------------------- files

    QString lastDir() const {
        if (project.isOpen()) return project.modelsDir();
        QSettings s;
        QString d = s.value("files/lastDir").toString();
        return d.isEmpty() ? QDir::homePath() : d;
    }
    void rememberDir(const QString &path) {
        QSettings s;
        s.setValue("files/lastDir", QFileInfo(path).absolutePath());
    }

    bool loadFile(const QString &path) {
        if (path.endsWith(QStringLiteral(".bonkpj"), Qt::CaseInsensitive)) return openProjectFile(path);
        QByteArray p = QFile::encodeName(path);
        if (bonk_load(p.constData()) != 0) {
            QMessageBox::critical(this, QStringLiteral("Couldn't open model"), QString::fromUtf8(bonk_last_error()));
            return false;
        }
        filePath = QFileInfo(path).absoluteFilePath();
        rememberDir(filePath);
        addRecentModel(filePath);
        rebuildRecentModels();
        syncTextureAlpha(); // its overrides may differ
        // A model inside a project brings its project (and textures) along.
        QString pj = findProjectFor(filePath);
        if (!pj.isEmpty() && QFileInfo(pj).absoluteFilePath() != project.file) setProject(pj, false);
        handle(F_R3D | F_R2D | F_TOOL);
        return true;
    }

    bool saveTo(const QString &path) {
        // First save inside a project: give the model its default game path.
        if (QString::fromUtf8(bonk_model_path()).isEmpty() && project.isOpen()) {
            QByteArray mp = project.defaultModelPath(QFileInfo(path).completeBaseName()).toUtf8();
            bonk_set_model_path(mp.constData());
        }
        QByteArray p = QFile::encodeName(path);
        if (bonk_save(p.constData()) != 0) {
            QMessageBox::critical(this, QStringLiteral("Couldn't save model"), QString::fromUtf8(bonk_last_error()));
            return false;
        }
        filePath = QFileInfo(path).absoluteFilePath();
        rememberDir(filePath);
        addRecentModel(filePath);
        rebuildRecentModels();
        statusBar()->showMessage(QStringLiteral("Saved %1 (models/%2.mdl). Bonk!").arg(QFileInfo(path).fileName(),
                                 QString::fromUtf8(bonk_model_path())), 4000);
        saveFlash.start();
        handle(0);
        repaintAll();
        return true;
    }

    bool save() { return filePath.isEmpty() ? saveAs() : saveTo(filePath); }

    bool saveAs() {
        QString start = filePath.isEmpty() ? lastDir() + QStringLiteral("/untitled.bpmdl") : filePath;
        QString path = QFileDialog::getSaveFileName(this, QStringLiteral("Save model"), start,
                                                    QStringLiteral("Bonk! models (*.bpmdl)"));
        if (path.isEmpty()) return false;
        if (!path.endsWith(QStringLiteral(".bpmdl"), Qt::CaseInsensitive)) path += QStringLiteral(".bpmdl");
        return saveTo(path);
    }

    void open() {
        if (!maybeSave()) return;
        QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open model"), lastDir(),
                                                    QStringLiteral("Bonk! models (*.bpmdl);;Bonk! projects (*.bonkpj);;All files (*)"));
        if (!path.isEmpty()) loadFile(path);
    }

    void newModel() {
        if (!maybeSave()) return;
        filePath.clear();
        command(CMD_NEW);
    }

    void modelProperties() {
        QString stem = filePath.isEmpty() ? QStringLiteral("untitled") : QFileInfo(filePath).completeBaseName();
        ModelPropertiesDialog d(this, QString::fromUtf8(bonk_model_path()), project, stem);
        if (d.exec() == QDialog::Accepted) {
            QByteArray b = d.modelPath.toUtf8();
            bonk_set_model_path(b.constData());
            handle(0);
        }
    }

    // ---------------------------------------------------------------- projects

    /// Make `path` the open project. With `freshModel`, also start a new model
    /// (asking to save the current one first).
    bool setProject(const QString &path, bool freshModel) {
        Project p;
        QString err;
        if (!p.load(path, &err)) {
            QMessageBox::critical(this, QStringLiteral("Couldn't open project"), err);
            return false;
        }
        if (freshModel) {
            if (!maybeSave()) return false;
            filePath.clear();
            bonk_command(CMD_NEW);
        }
        p.createFolders(&err); // fill in anything missing (e.g. a fresh git checkout)
        project = p;
        addRecentProject(project.file);
        library.setProject(&project);
        if (prefabs) prefabs->refresh();
        rebuildRecentMenu();
        statusBar()->showMessage(QStringLiteral("Project %1: %2 texture%3").arg(project.name).arg(library.entries.size())
                                 .arg(library.entries.size() == 1 ? QString() : QStringLiteral("s")), 4000);
        handle(F_R3D | F_R2D | F_TOOL);
        return true;
    }

    bool openProjectFile(const QString &path) { return setProject(QFileInfo(path).absoluteFilePath(), true); }

    void openProject() {
        QString path = QFileDialog::getOpenFileName(this, QStringLiteral("Open project"), defaultProjectsDir(),
                                                    QStringLiteral("Bonk! projects (*.bonkpj)"));
        if (!path.isEmpty()) openProjectFile(path);
    }

    void newProject() {
        ProjectDialog d(this);
        if (d.exec() == QDialog::Accepted) openProjectFile(d.result.file);
    }

    void projectSettings() {
        if (!project.isOpen()) return;
        ProjectDialog d(this, &project);
        if (d.exec() == QDialog::Accepted) {
            project = d.result;
            library.setProject(&project);
            handle(0);
        }
    }

    void closeProject() {
        if (!project.isOpen() || !maybeSave()) return;
        project = Project();
        library.setProject(&project);
        if (prefabs) prefabs->refresh();
        filePath.clear();
        command(CMD_NEW);
        rebuildRecentMenu();
    }

    void rebuildRecentMenu() {
        if (!recentMenu) return;
        recentMenu->clear();
        const QStringList rec = recentProjects();
        for (const QString &r : rec) {
            QAction *a = recentMenu->addAction(QStringLiteral("%1   (%2)").arg(QFileInfo(r).completeBaseName(),
                                               tildePath(QFileInfo(r).absolutePath())));
            QObject::connect(a, &QAction::triggered, this, [this, r] { openProjectFile(r); });
        }
        if (rec.isEmpty()) recentMenu->addAction(QStringLiteral("(none yet)"))->setEnabled(false);
        if (projectSettingsAct) projectSettingsAct->setEnabled(project.isOpen());
        if (closeProjectAct) closeProjectAct->setEnabled(project.isOpen());
    }

    void rebuildRecentModels() {
        if (!recentModelsMenu) return;
        recentModelsMenu->clear();
        const QStringList rec = recentModels();
        QSettings().setValue("models/recent", rec); // prune missing files
        for (const QString &r : rec) {
            QAction *a = recentModelsMenu->addAction(modelLabel(r));
            a->setToolTip(tildePath(r));
            QObject::connect(a, &QAction::triggered, this, [this, r] { openRecentModel(r); });
        }
        if (rec.isEmpty()) recentModelsMenu->addAction(QStringLiteral("(none yet)"))->setEnabled(false);
        recentModelsMenu->addSeparator();
        QAction *clr = recentModelsMenu->addAction(QStringLiteral("Clear Recent Models"));
        clr->setEnabled(!rec.isEmpty());
        QObject::connect(clr, &QAction::triggered, this, [this] {
            QSettings().remove("models/recent");
            rebuildRecentModels();
        });
    }

    void openRecentModel(const QString &path) {
        if (!maybeSave()) return;
        loadFile(path);
    }

    /// Ctrl+Shift+O: the most recent model that isn't the one already open.
    void reopenLast() {
        for (const QString &r : recentModels())
            if (r != filePath) {
                openRecentModel(r);
                return;
            }
        statusBar()->showMessage(QStringLiteral("No other recent model to reopen."), 3000);
    }

    GuideWindow *guide = nullptr;
    bool guideHammer = false;

    /// The guide page for what you're doing right now.
    QString contextPage() const {
        QWidget *f = QApplication::focusWidget();
        if (f && picker && picker->isAncestorOf(f)) return QStringLiteral("textures");
        if (f && layers && layers->isAncestorOf(f)) return QStringLiteral("groups");
        if (f && budget && budget->isAncestorOf(f)) return QStringLiteral("budget");
        if (bonk_editing_collision()) return QStringLiteral("collision");
        switch (bonk_tool()) {
        case 1: return QStringLiteral("drawing");
        case 2: case 3: return QStringLiteral("vertices");
        case 6: return QStringLiteral("face");
        default: return QStringLiteral("selecting");
        }
    }

    void openGuide(const QString &page) {
        if (!guide) {
            guide = new GuideWindow(this);
            guide->build(prefs.hammerKeys);
            guideHammer = prefs.hammerKeys;
        } else if (guideHammer != prefs.hammerKeys) {
            guide->build(prefs.hammerKeys);
            guideHammer = prefs.hammerKeys;
        }
        guide->showPage(page);
    }

    void startScreen() {
        StartScreen st(this);
        st.exec();
        prefs.startScreen = QSettings().value("ui/startScreen", true).toBool();
        switch (st.choice) {
        case StartScreen::NewProject: newProject(); break;
        case StartScreen::OpenProject: openProject(); break;
        case StartScreen::OpenRecent: openProjectFile(st.recentPath); break;
        case StartScreen::OpenModel: loadFile(st.recentPath); break;
        default: break;
        }
    }

    /// Make sure a project is open (offering to create or open one).
    bool ensureProject() {
        if (project.isOpen()) return true;
        QMessageBox box(QMessageBox::Information, QStringLiteral("Textures live in a project"),
                        QStringLiteral("Textures are kept in a project's assets/Textures folder, so they can be "
                                       "shared by every model in it. Create or open a project first."),
                        QMessageBox::Cancel, this);
        QPushButton *nb = box.addButton(QStringLiteral("New Project…"), QMessageBox::AcceptRole);
        QPushButton *ob = box.addButton(QStringLiteral("Open Project…"), QMessageBox::AcceptRole);
        box.exec();
        if (box.clickedButton() == nb) newProject();
        else if (box.clickedButton() == ob) openProject();
        return project.isOpen();
    }

    /// Import button: pick images, then copy them in.
    void importTextures() {
        if (!ensureProject()) return;
        QStringList files = QFileDialog::getOpenFileNames(this, QStringLiteral("Import textures"),
                                                          QSettings().value("files/importDir", QDir::homePath()).toString(),
                                                          QStringLiteral("Images (*.png *.jpg *.jpeg *.PNG *.JPG *.JPEG)"));
        if (files.isEmpty()) return;
        QSettings().setValue("files/importDir", QFileInfo(files.first()).absolutePath());
        const QString here = picker ? picker->folder() : QString();
        importPaths(files, here, here.isEmpty());
    }

    /// Copy images (and folders of images, which become groups) into
    /// assets/Textures/<group>. With `ask`, asks which group first.
    void importPaths(const QStringList &paths, QString g, bool ask) {
        if (!ensureProject()) return;
        if (ask) {
            QStringList groups = library.groups();
            groups.prepend(QStringLiteral("(no group)"));
            int pre = std::max(0, int(groups.indexOf(QSettings().value("files/lastGroup").toString())));
            bool ok = false;
            g = QInputDialog::getItem(this, QStringLiteral("Import textures"),
                                      QStringLiteral("Put them in which group? (type a new name to make one)"), groups, pre, true, &ok);
            if (!ok) return;
            QSettings().setValue("files/lastGroup", g);
            g = g == QStringLiteral("(no group)") ? QString() : g;
        }
        g = cleanGamePath(g);
        const QString base = project.texturesDir() + (g.isEmpty() ? QString() : QLatin1Char('/') + g);
        // (source file, destination file) pairs; a folder keeps its own name and subfolders
        QList<QPair<QString, QString>> jobs;
        for (const QString &p : paths) {
            QFileInfo fi(p);
            if (fi.isDir()) {
                const QString top = base + QLatin1Char('/') + cleanGamePath(fi.fileName());
                QDirIterator it(p, {QStringLiteral("*.png"), QStringLiteral("*.jpg"), QStringLiteral("*.jpeg"), QStringLiteral("*.PNG"),
                                    QStringLiteral("*.JPG"), QStringLiteral("*.JPEG")}, QDir::Files, QDirIterator::Subdirectories);
                while (it.hasNext()) {
                    const QString f = it.next();
                    QString rel = QDir(p).relativeFilePath(QFileInfo(f).absolutePath());
                    jobs.append({f, (rel == QStringLiteral(".") ? top : top + QLatin1Char('/') + cleanGamePath(rel)) + QLatin1Char('/') + QFileInfo(f).fileName()});
                }
            } else {
                jobs.append({fi.absoluteFilePath(), base + QLatin1Char('/') + fi.fileName()});
            }
        }
        if (jobs.isEmpty()) {
            statusBar()->showMessage(QStringLiteral("No PNG or JPG files to import there."), 4000);
            return;
        }
        enum { Ask, ReplaceAll, KeepAll, SkipAll } clash = Ask;
        QStringList failed, done;
        library.pauseScans(true);
        for (auto &j : jobs) {
            QString dest = j.second;
            if (QFileInfo(j.first).absoluteFilePath() == QFileInfo(dest).absoluteFilePath()) continue; // already there
            QDir().mkpath(QFileInfo(dest).absolutePath());
            if (QFileInfo::exists(dest)) {
                int what = clash;
                if (clash == Ask) {
                    QMessageBox box(QMessageBox::Question, QStringLiteral("Texture already in the project"),
                                    QStringLiteral("<b>%1</b> is already in assets/Textures%2.")
                                        .arg(QFileInfo(dest).fileName().toHtmlEscaped(), g.isEmpty() ? QString() : QStringLiteral("/") + g.toHtmlEscaped()),
                                    QMessageBox::NoButton, this);
                    QPushButton *rep = box.addButton(QStringLiteral("Replace"), QMessageBox::AcceptRole);
                    QPushButton *keep = box.addButton(QStringLiteral("Keep Both"), QMessageBox::AcceptRole);
                    QPushButton *skip = box.addButton(QStringLiteral("Skip"), QMessageBox::RejectRole);
                    auto *all = new QCheckBox(QStringLiteral("Do this for the rest"));
                    if (jobs.size() > 1) box.setCheckBox(all);
                    box.exec();
                    what = box.clickedButton() == rep ? ReplaceAll : box.clickedButton() == keep ? KeepAll : SkipAll;
                    (void)skip;
                    if (all->isChecked()) clash = decltype(clash)(what);
                }
                if (what == SkipAll) continue;
                if (what == ReplaceAll) {
                    QFile::remove(dest);
                } else {
                    QFileInfo di(dest);
                    for (int n = 2;; ++n) {
                        QString c = di.absolutePath() + QLatin1Char('/') + di.completeBaseName() + QStringLiteral("_%1.").arg(n) + di.suffix();
                        if (!QFileInfo::exists(c)) { dest = c; break; }
                    }
                }
            }
            if (!QFile::copy(j.first, dest)) { failed << QFileInfo(j.first).fileName(); continue; }
            done << dest;
        }
        if (!failed.isEmpty()) QMessageBox::warning(this, QStringLiteral("Import"), QStringLiteral("Couldn't copy: %1").arg(failed.join(QStringLiteral(", "))));
        library.forget(done);       // replaced files get the resize offer again
        library.pauseScans(false);  // one scan: one question for the whole batch
        statusBar()->showMessage(QStringLiteral("Imported %1 texture%2%3").arg(done.size()).arg(done.size() == 1 ? QString() : QStringLiteral("s"))
                                 .arg(g.isEmpty() ? QString() : QStringLiteral(" into ") + g), 4000);
    }

    /// Convert textures to VTF: `keys`, or everything not converted yet.
    void convertTextures(const QStringList &keys) {
        if (!ensureProject()) return;
        std::vector<TexEntry> todo;
        for (const TexEntry &e : library.entries)
            if (keys.isEmpty() ? !e.converted : keys.contains(e.key)) todo.push_back(e);
        if (todo.empty()) {
            statusBar()->showMessage(library.entries.empty() ? QStringLiteral("No textures to convert yet.")
                                                             : QStringLiteral("Everything is already converted. Bonk!"), 4000);
            return;
        }
        QProgressDialog prog(QStringLiteral("Converting textures to VTF…"), QStringLiteral("Stop"), 0, int(todo.size()), this);
        prog.setWindowModality(Qt::WindowModal);
        prog.setMinimumDuration(400);
        QStringList errors;
        int done = 0;
        for (size_t i = 0; i < todo.size(); ++i) {
            prog.setValue(int(i));
            prog.setLabelText(QStringLiteral("Converting %1…").arg(todo[i].key));
            QCoreApplication::processEvents();
            if (prog.wasCanceled()) break;
            QString err = library.convert(todo[i]);
            if (err.isEmpty()) ++done;
            else errors << QStringLiteral("%1: %2").arg(todo[i].key, err);
        }
        prog.setValue(int(todo.size()));
        library.scan();
        if (!errors.isEmpty()) QMessageBox::warning(this, QStringLiteral("Convert"), errors.join(QLatin1Char('\n')));
        statusBar()->showMessage(QStringLiteral("Converted %1 texture%2 into addon/%3/%4. Bonk!").arg(done).arg(done == 1 ? QString() : QStringLiteral("s"))
                                 .arg(project.name, project.texturePool), 5000);
    }

    // ---------------------------------------------------------------- importing from Source

    /// Materials the open model uses that are real textures (not nodraw).
    static QStringList modelMaterials() {
        QStringList out;
        for (const QString &k : QString::fromUtf8(bonk_used_materials()).split(QLatin1Char('\n'), Qt::SkipEmptyParts))
            if (!k.startsWith(QStringLiteral("tools/"))) out << k;
        return out;
    }

    /// Decode a source texture to PNG at assets/Textures/<key>.png. Returns the
    /// key it ended up under ("" = skipped or failed; *why says which).
    QString importOneTexture(const MatSource &src, const QString &vtfKey, const QString &key, const MatProps &props,
                             int &clash, QString *why, QStringList &written) {
        const QByteArray data = src.read(vtfKey + QStringLiteral(".vtf"));
        if (data.isEmpty()) {
            *why = QStringLiteral("%1: no %2.vtf").arg(key, vtfKey);
            return QString();
        }
        int w = 0, h = 0;
        const uchar *px = bonk_vtf_decode(reinterpret_cast<const uchar *>(data.constData()), size_t(data.size()), &w, &h);
        if (!px) {
            *why = QStringLiteral("%1: %2").arg(key, QString::fromUtf8(bonk_last_error()));
            return QString();
        }
        const QImage img = QImage(px, w, h, w * 4, QImage::Format_RGBA8888).copy();
        QString k = key;
        QString dest = project.texturesDir() + QLatin1Char('/') + k + QStringLiteral(".png");
        if (QFileInfo::exists(dest)) {
            const QImage old = QImage(dest).convertToFormat(QImage::Format_RGBA8888);
            if (old == img) { // already here: just refresh its settings
                if (!QFileInfo::exists(MatProps::sidecarFor(dest))) props.saveSidecar(dest);
                *why = QString();
                return k;
            }
            enum { Ask = -1, Replace = 0, KeepBoth = 1, Skip = 2 };
            int what = clash;
            if (clash == Ask && !qEnvironmentVariableIsSet("BONK_ASSUME_YES")) {
                // on top of the progress window (a modal dialog of its own), not hidden behind it
                QWidget *over = importOver ? importOver : static_cast<QWidget *>(this);
                QMessageBox box(QMessageBox::Question, QStringLiteral("Texture already in the project"),
                                QStringLiteral("<b>%1</b> is already in assets/Textures, but it's a different image.").arg(k.toHtmlEscaped()),
                                QMessageBox::NoButton, over);
                QPushButton *rep = box.addButton(QStringLiteral("Replace"), QMessageBox::AcceptRole);
                QPushButton *keep = box.addButton(QStringLiteral("Keep Both"), QMessageBox::AcceptRole);
                box.addButton(QStringLiteral("Skip"), QMessageBox::RejectRole);
                auto *all = new QCheckBox(QStringLiteral("Do this for the rest"));
                box.setCheckBox(all);
                box.exec();
                what = box.clickedButton() == rep ? Replace : box.clickedButton() == keep ? KeepBoth : Skip;
                if (all->isChecked()) clash = what;
            } else if (clash == Ask) {
                what = KeepBoth;
            }
            if (what == Skip) {
                *why = QString();
                return k; // keep using the project's own
            }
            if (what == KeepBoth) {
                for (int n = 2;; ++n) {
                    const QString c = QStringLiteral("%1_%2").arg(key).arg(n);
                    if (!QFileInfo::exists(project.texturesDir() + QLatin1Char('/') + c + QStringLiteral(".png"))) {
                        k = c;
                        break;
                    }
                }
                dest = project.texturesDir() + QLatin1Char('/') + k + QStringLiteral(".png");
            }
        }
        QDir().mkpath(QFileInfo(dest).absolutePath());
        if (!img.save(dest, "PNG")) {
            *why = QStringLiteral("%1: couldn't write %2").arg(key, dest);
            return QString();
        }
        props.saveSidecar(dest);
        written << dest;
        return k;
    }

    /// Bring materials in from `src`: `keys` (material paths), or everything.
    /// Shows a summary at the end; faces of the open model follow any renames.
    void runSourceImport(const MatSource &src, QStringList keys, bool everything) {
        if (everything) keys = src.materials();
        keys.removeDuplicates();
        keys.erase(std::remove_if(keys.begin(), keys.end(), [](const QString &k) { return k.startsWith(QStringLiteral("tools/")) || k.isEmpty(); }),
                   keys.end());
        if (keys.isEmpty()) return;
        // big imports (stock HL2): say how big first
        if (everything && keys.size() > 250) {
            qint64 bytes = 0;
            for (const QString &k : keys) {
                const QByteArray head = src.read(k + QStringLiteral(".vtf"), 80);
                int info[3] = {0, 0, 0};
                if (head.size() >= 64 && bonk_vtf_info(reinterpret_cast<const uchar *>(head.constData()), size_t(head.size()), info) == 0)
                    bytes += qint64(info[0]) * info[1] * 2; // PNGs land around half the raw size
            }
            if (!qEnvironmentVariableIsSet("BONK_ASSUME_YES") &&
                QMessageBox::question(this, QStringLiteral("That's a lot of textures"),
                                      QStringLiteral("%1 holds <b>%2 materials</b>, about <b>%3</b> of PNGs once decoded into this project.<br><br>"
                                                     "Import them all?")
                                          .arg(src.describe().toHtmlEscaped(), QLocale().toString(keys.size()),
                                               QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat)),
                                      QMessageBox::Yes | QMessageBox::Cancel) != QMessageBox::Yes)
                return;
        }
        QProgressDialog prog(QStringLiteral("Importing textures…"), QStringLiteral("Stop"), 0, int(keys.size()), this);
        prog.setWindowModality(Qt::WindowModal);
        prog.setMinimumDuration(300);
        importOver = &prog;
        library.pauseScans(true);
        struct Reset { QWidget **p; ~Reset() { *p = nullptr; } } resetOver{&importOver};
        int clash = -1, imported = 0;
        QStringList missing, problems, written;
        QList<QPair<QString, QString>> renamed;
        for (int i = 0; i < keys.size(); ++i) {
            prog.setValue(i);
            prog.setLabelText(QStringLiteral("Importing %1…").arg(keys[i]));
            QCoreApplication::processEvents();
            if (prog.wasCanceled()) break;
            const QString key = keys[i];
            if (!src.has(key + QStringLiteral(".vmt"))) {
                missing << key;
                continue;
            }
            const SourceVmt vmt = SourceVmt::load(src, key);
            if (!vmt.ok) {
                problems << QStringLiteral("%1: couldn't read its VMT").arg(key);
                continue;
            }
            const QString base = SourceVmt::clean(vmt.params.value(QStringLiteral("$basetexture")));
            if (base.isEmpty()) {
                problems << QStringLiteral("%1: no $basetexture (a %2 material)").arg(key, vmt.shader);
                continue;
            }
            MatProps props = vmt.props();
            const QString clean = cleanGamePath(key);
            QString why;
            // the normal map first, so the material can point at it
            const QString bump = SourceVmt::clean(vmt.params.value(QStringLiteral("$bumpmap"), vmt.params.value(QStringLiteral("$normalmap"))));
            if (!bump.isEmpty()) {
                MatProps np;
                np.isNormalMap = true;
                const QString bk = importOneTexture(src, bump, cleanGamePath(bump), np, clash, &why, written);
                if (!bk.isEmpty()) props.bumpmap = bk;
                else if (!why.isEmpty()) problems << why;
            }
            const QString k = importOneTexture(src, base, clean, props, clash, &why, written);
            if (k.isEmpty()) {
                if (!why.isEmpty()) problems << why;
                continue;
            }
            ++imported;
            if (k != key) renamed.append({key, k});
        }
        prog.setValue(int(keys.size()));
        // faces of the open model follow renames (keep-both copies, cleaned names)
        int moved = 0;
        for (const auto &r : renamed) moved += bonk_rename_material(r.first.toUtf8().constData(), r.second.toUtf8().constData());
        library.markAsked(written); // valid Source textures: never offer a resize
        library.pauseScans(false);
        handle(F_R3D | F_R2D | F_TOOL);
        QString msg = QStringLiteral("Imported <b>%1</b> texture%2 from %3 into assets/Textures.")
                          .arg(imported).arg(imported == 1 ? QString() : QStringLiteral("s"), src.describe().toHtmlEscaped());
        if (moved) msg += QStringLiteral("<br>%1 face%2 now point at renamed copies.").arg(moved).arg(moved == 1 ? QString() : QStringLiteral("s"));
        if (!missing.isEmpty())
            msg += QStringLiteral("<br><br><b>Not in there</b> (%1): %2%3").arg(missing.size()).arg(missing.mid(0, 12).join(QStringLiteral(", ")).toHtmlEscaped(),
                                                                                  missing.size() > 12 ? QStringLiteral("…") : QString());
        if (!problems.isEmpty())
            msg += QStringLiteral("<br><br><b>Problems</b> (%1):<br>%2%3").arg(problems.size()).arg(problems.mid(0, 10).join(QStringLiteral("<br>")).toHtmlEscaped(),
                                                                                  problems.size() > 10 ? QStringLiteral("<br>…") : QString());
        statusBar()->showMessage(QStringLiteral("Imported %1 textures. Bonk!").arg(imported), 5000);
        if (!qEnvironmentVariableIsSet("BONK_ASSUME_YES")) QMessageBox::information(this, QStringLiteral("Import Source Textures"), msg);
        lastImportSummary = msg;
    }
    QString lastImportSummary;
    QWidget *importOver = nullptr; // the import's progress window: questions go on top of it

    /// File → Import Source Textures…
    void importSourceTextures() {
        if (!ensureProject()) return;
        MaterialSourceDialog d(this, modelMaterials(),
                               QStringLiteral("Bring in Source materials (VMT + VTF) as project textures. Point at a <b>materials</b> folder "
                                              "(e.g. sourcemods/hl2/materials), the addon folder above one, or a Workshop <b>.gma</b>."));
        if (d.exec() != QDialog::Accepted) return;
        runSourceImport(d.src, modelMaterials(), d.importEverything());
    }

    /// File → Import VMF…
    void importVmf(QString path = QString()) {
        if (path.isEmpty()) {
            path = QFileDialog::getOpenFileName(this, QStringLiteral("Import Hammer map"), QSettings().value("import/vmfDir", QDir::homePath()).toString(),
                                                QStringLiteral("Hammer maps (*.vmf);;All files (*)"));
            if (path.isEmpty()) return;
        }
        QSettings().setValue("import/vmfDir", QFileInfo(path).absolutePath());
        // what's in it
        const QStringList lines = QString::fromUtf8(bonk_vmf_scan(QFile::encodeName(path).constData())).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
        int solids = 0;
        QList<std::tuple<int, int, QString>> visgroups;
        int mats = 0;
        for (const QString &l : lines) {
            if (l.startsWith(QStringLiteral("solids "))) solids = l.mid(7).toInt();
            else if (l.startsWith(QStringLiteral("vg "))) {
                const QStringList f = l.mid(3).split(QLatin1Char('\t'));
                if (f.size() >= 3) visgroups.append({f[0].toInt(), f[1].toInt(), f[2]});
            } else if (l.startsWith(QStringLiteral("mat "))) ++mats;
        }
        if (solids == 0) {
            QMessageBox::warning(this, QStringLiteral("Import VMF"), lines.isEmpty() ? QString::fromUtf8(bonk_last_error())
                                                                                    : QStringLiteral("%1 has no brushes to import.").arg(QFileInfo(path).fileName()));
            return;
        }
        if (!maybeSave()) return;
        // how to bring it in
        enum { NewProject, AddToProject, JustOpen } how = JustOpen;
        if (!qEnvironmentVariableIsSet("BONK_ASSUME_YES")) {
            QMessageBox box(QMessageBox::Question, QStringLiteral("Import VMF"),
                            QStringLiteral("<b>%1</b>: %2 brush%3, %4 material%5.<br><br>How do you want to bring it in?")
                                .arg(QFileInfo(path).fileName().toHtmlEscaped()).arg(solids).arg(solids == 1 ? QString() : QStringLiteral("es"))
                                .arg(mats).arg(mats == 1 ? QString() : QStringLiteral("s")),
                            QMessageBox::Cancel, this);
            box.setInformativeText(QStringLiteral("A project keeps its textures, so it can bring the map's materials in too."));
            QPushButton *np = box.addButton(QStringLiteral("Create New Project…"), QMessageBox::AcceptRole);
            QPushButton *ap = box.addButton(project.isOpen() ? QStringLiteral("Add to %1").arg(project.name) : QStringLiteral("Add to Existing Project…"),
                                            QMessageBox::AcceptRole);
            QPushButton *jo = box.addButton(QStringLiteral("Just Open It"), QMessageBox::AcceptRole);
            box.setDefaultButton(np);
            box.exec();
            if (box.clickedButton() == np) how = NewProject;
            else if (box.clickedButton() == ap) how = AddToProject;
            else if (box.clickedButton() == jo) how = JustOpen;
            else return;
        } else {
            how = qEnvironmentVariable("BONK_VMF_HOW") == QStringLiteral("add") ? AddToProject : JustOpen;
        }
        QString skip;
        if (!visgroups.isEmpty() && !qEnvironmentVariableIsSet("BONK_ASSUME_YES")) {
            VisgroupDialog vd(this, visgroups, solids);
            if (vd.exec() != QDialog::Accepted) return;
            skip = vd.skipped();
        }
        if (bonk_vmf_import(QFile::encodeName(path).constData(), skip.toUtf8().constData()) != 0) {
            QMessageBox::critical(this, QStringLiteral("Import VMF"), QString::fromUtf8(bonk_last_error()));
            return;
        }
        filePath.clear();
        // what came in
        QMap<QString, int> rep;
        for (const QString &l : QString::fromUtf8(bonk_vmf_report()).split(QLatin1Char('\n'), Qt::SkipEmptyParts))
            rep[l.section(QLatin1Char(' '), 0, 0)] = l.section(QLatin1Char(' '), 1).toInt();
        QStringList notes;
        auto note = [&](const char *k, const QString &text) {
            if (rep.value(QString::fromLatin1(k))) notes << text.arg(rep.value(QString::fromLatin1(k)));
        };
        note("collision", QStringLiteral("%1 clip brush(es) became the collision layer (hidden for now: tick Show in Layers to see it)"));
        note("groups", QStringLiteral("%1 group(s)"));
        note("excluded", QStringLiteral("%1 brush(es) in unticked visgroups left out"));
        note("skipped_tools", QStringLiteral("%1 skybox / hint / trigger brush(es) skipped"));
        note("skipped_entities", QStringLiteral("%1 trigger / logic brush(es) skipped"));
        note("displacements", QStringLiteral("%1 displacement(s) skipped"));
        note("point_entities", QStringLiteral("%1 point entit(ies) skipped (lights, spawns, props…)"));
        note("broken", QStringLiteral("%1 broken brush(es) skipped"));
        handle(F_R3D | F_R2D | F_TOOL);
        const QString stem = cleanProjectName(QFileInfo(path).completeBaseName());

        if (how == NewProject) {
            ProjectDialog d(this);
            if (auto *n = d.findChild<QLineEdit *>()) n->setText(stem);
            if (d.exec() != QDialog::Accepted) {
                statusBar()->showMessage(QStringLiteral("No project made: the map is open as an unsaved model."), 6000);
                how = JustOpen;
            } else if (!setProject(d.result.file, false)) {
                how = JustOpen;
            }
        } else if (how == AddToProject && !project.isOpen()) {
            const QString pj = QFileDialog::getOpenFileName(this, QStringLiteral("Add to which project?"), defaultProjectsDir(),
                                                            QStringLiteral("Bonk! projects (*.bonkpj)"));
            if (pj.isEmpty() || !setProject(QFileInfo(pj).absoluteFilePath(), false)) how = JustOpen;
        }
        if (how != JustOpen) {
            // the materials step, then save it as a model in the project
            const QStringList used = modelMaterials();
            if (!used.isEmpty()) {
                if (qEnvironmentVariableIsSet("BONK_ASSUME_YES") && !qEnvironmentVariable("BONK_IMPORT_SOURCE").isEmpty()) {
                    MatSource src;
                    if (src.open(qEnvironmentVariable("BONK_IMPORT_SOURCE")))
                        runSourceImport(src, used, qEnvironmentVariable("BONK_IMPORT_ALL") == QStringLiteral("1"));
                } else if (!qEnvironmentVariableIsSet("BONK_ASSUME_YES")) {
                    MaterialSourceDialog d(this, used,
                                           QStringLiteral("Where are this map's materials? Point at the <b>materials</b> folder it used "
                                                          "(e.g. sourcemods/hl2/materials), the addon folder above one, or a Workshop <b>.gma</b>."));
                    if (d.exec() == QDialog::Accepted) runSourceImport(d.src, used, d.importEverything());
                }
            }
            QString dest = project.modelsDir() + QLatin1Char('/') + stem + QStringLiteral(".bpmdl");
            for (int n = 2; QFileInfo::exists(dest); ++n) dest = project.modelsDir() + QStringLiteral("/%1_%2.bpmdl").arg(stem).arg(n);
            saveTo(dest);
        }
        updateTitle();
        const QString summary = notes.isEmpty() ? QString() : QStringLiteral("<br><br>") + notes.join(QStringLiteral("<br>"));
        statusBar()->showMessage(QStringLiteral("Imported %1 brushes from %2. Bonk!").arg(rep.value(QStringLiteral("brushes"))).arg(QFileInfo(path).fileName()), 6000);
        lastImportSummary = QStringLiteral("Imported %1 brushes.%2").arg(rep.value(QStringLiteral("brushes"))).arg(summary);
        if (!notes.isEmpty() && !qEnvironmentVariableIsSet("BONK_ASSUME_YES"))
            QMessageBox::information(this, QStringLiteral("Import VMF"),
                                     QStringLiteral("Imported <b>%1</b> brush%2 from %3.%4")
                                         .arg(rep.value(QStringLiteral("brushes"))).arg(rep.value(QStringLiteral("brushes")) == 1 ? QString() : QStringLiteral("es"),
                                              QFileInfo(path).fileName().toHtmlEscaped(), summary));
    }

    // ---------------------------------------------------------------- compiling

    CompileWindow *compileWin = nullptr;
    QProcess *compileProc = nullptr;
    QByteArray compilePartial;
    struct CompileJob {
        QString mp, game, title;
        QElapsedTimer clock;
        QStringList errors;
        int tris = 0, pieces = 0;
        bool exportAfter = false;
    } job;

    QString colorPrefix; // the model being compiled: its color materials are named after it
    QString flatName(const QString &key) const {
        if (key.isEmpty()) return QStringLiteral("none");
        QString k = key;
        k.replace(QLatin1Char('/'), QLatin1Char('_'));
        if (key.startsWith(QStringLiteral("colors/")) && !colorPrefix.isEmpty()) k = colorPrefix + QLatin1Char('_') + k;
        return k;
    }

    /// Tell the 3D view which textures are cut-outs or see-through (from
    /// their Material Properties, the open model's overrides included).
    /// The 3D view's look for each texture: transparency, and how it moves
    /// (frames, scrolling, pulsing) as the game would show it.
    void syncTextureAlpha() {
        for (const TexEntry &e : library.entries) {
            const MatProps p = propsFor(e.key);
            const QByteArray k = e.key.toUtf8();
            bonk_texture_alpha(k.constData(), p.transparency, float(p.alphaRef));
            bonk_texture_anim(k.constData(), e.frames, float(p.fps), p.scroll ? float(p.scrollRate) : 0.0f, float(p.scrollAngle),
                              p.pulse, float(p.pulseMin), float(p.pulseMax), float(p.pulsePeriod));
        }
    }

    /// A material's properties: the model's override, else the texture's .bonkmat.
    MatProps propsFor(const QString &key) const {
        const QString ov = QString::fromUtf8(bonk_mat_override(key.toUtf8().constData()));
        if (!ov.isEmpty()) return MatProps::parse(ov);
        const TexEntry *e = library.find(key);
        return e ? MatProps::loadSidecar(e->file) : MatProps();
    }

    /// Write a VMT unless the one on disk was edited by hand since Bonk last
    /// wrote it. Returns false when it was kept.
    bool writeVmt(const QString &path, const QString &text, QHash<QString, QString> &written) {
        const QString rel = QDir(project.addonDir()).relativeFilePath(path);
        const QString mine = QString::fromLatin1(QCryptographicHash::hash(text.toUtf8(), QCryptographicHash::Sha1).toHex());
        QFile f(path);
        if (f.exists() && f.open(QIODevice::ReadOnly)) {
            const QString cur = QString::fromLatin1(QCryptographicHash::hash(f.readAll(), QCryptographicHash::Sha1).toHex());
            f.close();
            if (cur == mine) {
                written[rel] = mine;
                return true;
            }
            if (written.value(rel) != cur) return false; // not what Bonk wrote: hand-edited
        }
        QDir().mkpath(QFileInfo(path).absolutePath());
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return true;
        f.write(text.toUtf8());
        written[rel] = mine;
        return true;
    }

    /// Ask before compiling: needs a project and a model path.
    bool compileReady() {
        if (!project.isOpen()) {
            QMessageBox box(QMessageBox::Information, QStringLiteral("Compiling needs a project"),
                            QStringLiteral("A compiled model goes into a project's addon folder, next to its textures. "
                                           "Create or open a project first."), QMessageBox::Cancel, this);
            QPushButton *nb = box.addButton(QStringLiteral("New Project…"), QMessageBox::AcceptRole);
            QPushButton *ob = box.addButton(QStringLiteral("Open Project…"), QMessageBox::AcceptRole);
            box.exec();
            if (box.clickedButton() == nb) newProject();
            else if (box.clickedButton() == ob) openProject();
            if (!project.isOpen()) return false;
        }
        if (QString::fromUtf8(bonk_model_path()).isEmpty()) {
            if (!filePath.isEmpty()) {
                QByteArray mp = project.defaultModelPath(QFileInfo(filePath).completeBaseName()).toUtf8();
                bonk_set_model_path(mp.constData());
            } else {
                modelProperties();
            }
        }
        return !QString::fromUtf8(bonk_model_path()).isEmpty();
    }

    /// Show the "where's Garry's Mod" message with a way into Settings.
    void gmodMissing(const QString &what) {
        QMessageBox box(QMessageBox::Warning, QStringLiteral("Garry's Mod not found"), what, QMessageBox::Close, this);
        QPushButton *sb = box.addButton(QStringLiteral("Settings…"), QMessageBox::AcceptRole);
        box.exec();
        if (box.clickedButton() == sb) configure();
    }

    CompileWindow *compileWindow() {
        if (!compileWin) {
            compileWin = new CompileWindow(this);
            compileWin->exportReq = [this] { exportToGMod(false); };
            compileWin->openFolderReq = [this] {
                if (project.isOpen()) QDesktopServices::openUrl(QUrl::fromLocalFile(project.addonDir()));
            };
        }
        return compileWin;
    }

    /// File → Compile Model: SMDs, QC, VMTs and VTFs, then GMod's studiomdl through Proton.
    void compileModel() {
        if (compileProc) {
            compileWindow()->show();
            compileWin->raise();
            return;
        }
        if (!compileReady()) return;
        const QString mp = QString::fromUtf8(bonk_model_path());
        int b[5] = {0};
        bonk_budget(b);
        if (b[0] == 0) {
            QMessageBox::information(this, QStringLiteral("Nothing to compile"),
                                     QStringLiteral("Nothing to compile yet: every face is nodraw. Paint some faces first "
                                                    "(new brushes start out nodraw; collision brushes never render)."));
            return;
        }
        if (budget) {
            budget->update_();
            const QStringList over = budget->over();
            if (!over.isEmpty() && !qEnvironmentVariableIsSet("BONK_ASSUME_YES")) {
                QMessageBox box(QMessageBox::Warning, QStringLiteral("Over budget"),
                                QStringLiteral("This model is over Garry's Mod's studiomdl limits:<ul><li>%1</li></ul>"
                                               "studiomdl will probably refuse it or the model will misbehave in game. "
                                               "Click a meter in the Budget bar to select what's heaviest.")
                                    .arg(over.join(QStringLiteral("</li><li>"))),
                                QMessageBox::NoButton, this);
                QPushButton *go = box.addButton(QStringLiteral("Compile Anyway"), QMessageBox::AcceptRole);
                box.addButton(QMessageBox::Cancel);
                box.exec();
                if (box.clickedButton() != go) return;
            }
        }
        int physMode = 0;
        bool withPhys = true;
        if (bonk_collision_pieces() == 0) {
            // no collision layer: generate one for this compile (complex or simplified), or go without
            if (qEnvironmentVariableIsSet("BONK_ASSUME_YES")) {
                physMode = prefs.advanced ? 2 : 1;
            } else {
                CollisionDialog d(this, prefs.simplifyDetail, true, prefs.advanced);
                if (d.exec() != QDialog::Accepted) return;
                if (d.mode < 0) withPhys = false;
                else physMode = d.mode == 1 ? 2 : 1;
                if (d.mode == 1 && d.detail != prefs.simplifyDetail) {
                    prefs.simplifyDetail = d.detail;
                    prefs.save();
                }
            }
            bonk_set_simplify_detail(float(prefs.simplifyDetail));
        }
        SteamFind sf;
        sf.detect();
        if (!prefs.studiomdl.isEmpty()) {
            // Settings → Compiler: another studiomdl.exe (e.g. StudioMDL CE)
            if (!QFileInfo(prefs.studiomdl).isFile()) {
                QMessageBox::warning(this, QStringLiteral("Compiler not found"),
                                     QStringLiteral("Settings → Garry's Mod → Compiler points at %1, which isn't there. "
                                                    "Clear it to use Garry's Mod's own studiomdl.").arg(tildePath(prefs.studiomdl).toHtmlEscaped()));
                return;
            }
            sf.studiomdl = prefs.studiomdl;
        }
        if (sf.studiomdl.isEmpty()) {
            gmodMissing(sf.gmodDir.isEmpty()
                            ? QStringLiteral("Bonk compiles with Garry's Mod's own studiomdl, but couldn't find Garry's Mod in your Steam "
                                             "libraries. Install it, or choose its folder in Settings → Garry's Mod.")
                            : QStringLiteral("Garry's Mod is at %1, but its bin/studiomdl.exe is missing. Try Steam → Garry's Mod → "
                                             "Properties → Installed Files → Verify integrity.").arg(tildePath(sf.gmodDir)));
            return;
        }
        const QString proton = sf.proton();
        if (proton.isEmpty()) {
            gmodMissing(QStringLiteral("studiomdl is a Windows program, so Bonk runs it through Proton, but no Proton was found. "
                                       "Install one from Steam (search your library for \"Proton\"; the newest stable version is best)."));
            return;
        }

        // --- the build folder: SMDs, QC and a stand-in game folder for studiomdl's output
        const QString flat = flatName(mp);
        const QString work = project.buildDir() + QStringLiteral("/") + flat;
        const QString game = project.buildDir() + QStringLiteral("/game");
        QDir().mkpath(work);
        QDir().mkpath(game);
        QDir(game + QStringLiteral("/models")).removeRecursively(); // only fresh output gets copied
        {
            QFile gi(game + QStringLiteral("/gameinfo.txt"));
            if (gi.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text))
                gi.write("// Bonk! stand-in game folder: studiomdl writes its output here.\n"
                         "\"GameInfo\"\n{\n\tgame \"Bonk build\"\n\tFileSystem\n\t{\n\t\tSteamAppId 4000\n"
                         "\t\tSearchPaths\n\t\t{\n\t\t\tgame |gameinfo_path|.\n\t\t}\n\t}\n}\n");
        }
        const QString refSmd = work + QStringLiteral("/reference.smd"), phySmd = work + QStringLiteral("/physics.smd");
        QFile::remove(phySmd);
        int out[3] = {0, 0, 0};
        colorPrefix = QString::fromUtf8(bonk_set_color_prefix(mp.toUtf8().constData())); // house_colors_ffffff
        if (bonk_export_smd(QFile::encodeName(refSmd).constData(), withPhys ? QFile::encodeName(phySmd).constData() : nullptr,
                            physMode, out) != 0) {
            QMessageBox::critical(this, QStringLiteral("Compile"), QString::fromUtf8(bonk_last_error()));
            return;
        }
        CompileWindow *cw = compileWindow();
        cw->start(QStringLiteral("models/%1.mdl").arg(mp));
        cw->show();
        cw->raise();
        if (withPhys && out[1] >= 500)
            cw->note(QStringLiteral("%1 collision pieces is a lot: physics gets slow and studiomdl may refuse. "
                                    "A collision layer with a few simple boxes works much better for big models.").arg(out[1]));
        else if (withPhys && out[1] > 128)
            cw->note(QStringLiteral("%1 collision pieces is a lot, but based on testing it appears to be within a safe range and StudioMDL "
                                    "should accept it. If you're having trouble, consider stripping it down further.").arg(out[1]));
        if (out[2] > 0) cw->note(QStringLiteral("Left %1 collision piece%2 out: thinner than half a unit, which studiomdl can't use "
                                                "(it would give up and shrink-wrap the whole model).").arg(out[2]).arg(out[2] == 1 ? QString() : QStringLiteral("s")));
        if (physMode == 1) cw->note(QStringLiteral("Collision generated from the visual brushes (%1 piece%2).").arg(out[1]).arg(out[1] == 1 ? QString() : QStringLiteral("s")));
        if (physMode == 2) cw->note(QStringLiteral("Simplified collision generated from the visual brushes (%1 piece%2, detail size %3).")
                                        .arg(out[1]).arg(out[1] == 1 ? QString() : QStringLiteral("s")).arg(prefs.simplifyDetail));
        if (!withPhys) cw->note(QStringLiteral("No collision model: it won't have physics in game."));

        // --- textures (VTFs) and VMTs
        const QString matDir = project.materialsFor(mp);
        QStringList keys = QString::fromUtf8(bonk_used_materials()).split(QLatin1Char('\n'));
        QHash<QString, QString> written;
        const QString record = project.buildDir() + QStringLiteral("/vmt-written.txt");
        {
            QFile rf(record);
            if (rf.open(QIODevice::ReadOnly | QIODevice::Text))
                for (const QString &l : QString::fromUtf8(rf.readAll()).split(QLatin1Char('\n'), Qt::SkipEmptyParts))
                    written[l.section(QLatin1Char('\t'), 1)] = l.section(QLatin1Char('\t'), 0, 0);
        }
        QStringList kept, missing, convertErr;
        int converted = 0;
        QString surfaceprop;
        auto ensureVtf = [&](const QString &key) {
            const TexEntry *e = library.find(key);
            if (!e) {
                missing << key;
                return;
            }
            if (!e->converted) {
                const QString err = library.convert(*e);
                if (err.isEmpty()) ++converted;
                else convertErr << QStringLiteral("%1: %2").arg(key, err);
            }
        };
        for (const QString &key : keys) {
            QString text;
            if (key.isEmpty()) continue; // (old files: blank faces load as nodraw)
            {
                ensureVtf(key);
                const MatProps mp_ = propsFor(key);
                for (const QString &t : mp_.extraTextures()) ensureVtf(t);
                if (surfaceprop.isEmpty() && !mp_.surfaceprop.isEmpty()) surfaceprop = mp_.surfaceprop;
                text = mp_.vmt(key, project.texturePool);
            }
            const QString path = project.addonDir() + QLatin1Char('/') + matDir + QLatin1Char('/') + flatName(key) + QStringLiteral(".vmt");
            if (!writeVmt(path, text, written)) kept << QDir(project.addonDir()).relativeFilePath(path);
        }
        // variants: copies of materials (brushes a skin hides) and the invisible one
        QString variantQc, sequenceSmd = QStringLiteral("reference.smd");
        {
            const QString info = QString::fromUtf8(bonk_variant_export_info());
            const int qcAt = info.indexOf(QStringLiteral("qc\n"));
            variantQc = qcAt >= 0 ? info.mid(qcAt + 3) : QString();
            for (const QString &l : info.left(qcAt >= 0 ? qcAt : info.size()).split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
                const QStringList f = l.split(QLatin1Char('\t'));
                if (f.value(0) == QStringLiteral("sequence") && !f.value(1).isEmpty()) sequenceSmd = f.value(1);
                else if (f.value(0) == QStringLiteral("alias")) {
                    const QString copy = f.value(1), orig = f.value(2);
                    const QString path = project.addonDir() + QLatin1Char('/') + matDir + QLatin1Char('/') + flatName(copy) + QStringLiteral(".vmt");
                    if (!writeVmt(path, propsFor(orig).vmt(orig, project.texturePool), written)) kept << QDir(project.addonDir()).relativeFilePath(path);
                } else if (f.value(0) == QStringLiteral("hidden")) {
                    const QString path = project.addonDir() + QLatin1Char('/') + matDir + QStringLiteral("/bonk_hidden.vmt");
                    writeVmt(path, QStringLiteral("// Written by Bonk!: what a skin shows in place of a brush it hides.\n"
                                                  "\"VertexLitGeneric\"\n{\n\t\"$basetexture\" \"tools/toolsnodraw\"\n\t\"$no_draw\" \"1\"\n"
                                                  "\t\"$translucent\" \"1\"\n\t\"$alpha\" \"0\"\n}\n"),
                             written);
                }
            }
        }
        {
            QFile rf(record);
            if (rf.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                QString o;
                for (auto it = written.cbegin(); it != written.cend(); ++it) o += it.value() + QLatin1Char('\t') + it.key() + QLatin1Char('\n');
                rf.write(o.toUtf8());
            }
        }
        if (converted) library.scan();
        if (converted) cw->note(QStringLiteral("Converted %1 texture%2 to VTF.").arg(converted).arg(converted == 1 ? QString() : QStringLiteral("s")));
        if (!kept.isEmpty()) cw->note(QStringLiteral("Kept your hand-edited %1 (delete it to let Bonk write it again).")
                                          .arg(kept.join(QStringLiteral(", ")).toHtmlEscaped()));
        if (!missing.isEmpty()) cw->note(QStringLiteral("%1 material%2 not in the project's textures, so they'll show as missing: %3%4")
                                             .arg(missing.size()).arg(missing.size() == 1 ? QString() : QStringLiteral("s"),
                                                  missing.mid(0, 6).join(QStringLiteral(", ")).toHtmlEscaped(),
                                                  missing.size() > 6 ? QStringLiteral(" and %1 more").arg(missing.size() - 6) : QString()));
        for (const QString &e : convertErr) cw->note(QStringLiteral("Couldn't convert %1").arg(e.toHtmlEscaped()));
        writeAddonJson();

        // --- the QC
        QString cd = matDir;
        if (cd.startsWith(QStringLiteral("materials/"))) cd.remove(0, 10);
        QString qc = QStringLiteral("// Written by %1 on every compile: change the model, not this file.\n").arg(QString::fromUtf8(kAppName));
        qc += QStringLiteral("$modelname \"%1.mdl\"\n").arg(mp);
        qc += QStringLiteral("$staticprop\n");
        qc += QStringLiteral("$surfaceprop \"%1\"\n").arg(surfaceprop.isEmpty() ? QStringLiteral("default") : surfaceprop);
        qc += QStringLiteral("$cdmaterials \"%1/\"\n").arg(cd);
        qc += variantQc.isEmpty() ? QStringLiteral("$body \"body\" \"reference.smd\"\n") : variantQc; // bodygroups, skins, LODs
        qc += QStringLiteral("$sequence \"idle\" \"%1\" fps 30\n").arg(sequenceSmd);
        if (withPhys)
            qc += QStringLiteral("$collisionmodel \"physics.smd\"\n{\n\t$concave\n\t$maxconvexpieces %1\n\t%2\n}\n")
                      .arg(std::max(20, out[1]))
                      .arg(bonk_mass() > 0.0f ? QStringLiteral("$mass %1").arg(double(bonk_mass())) : QStringLiteral("$automass"));
        const QString qcPath = work + QStringLiteral("/model.qc");
        {
            QFile f(qcPath);
            if (f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) f.write(qc.toUtf8());
        }

        // --- run studiomdl through Proton
        job = CompileJob();
        job.mp = mp;
        job.game = game;
        job.title = QStringLiteral("models/%1.mdl").arg(mp);
        job.tris = out[0];
        job.pieces = withPhys ? out[1] : 0;
        job.exportAfter = prefs.exportAfterCompile;
        job.clock.start();
        const QString prefix = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/bonk/proton");
        const bool firstRun = !QFileInfo::exists(prefix + QStringLiteral("/pfx"));
        QDir().mkpath(prefix);
        const QStringList env = {QStringLiteral("STEAM_COMPAT_DATA_PATH=") + prefix,
                                 QStringLiteral("STEAM_COMPAT_CLIENT_INSTALL_PATH=") + sf.steamRoot,
                                 QStringLiteral("SteamAppId=4000"), QStringLiteral("SteamGameId=4000"),
                                 QStringLiteral("WINEDEBUG=-all")};
        const QStringList cmd = {proton, QStringLiteral("run"), sf.studiomdl, QStringLiteral("-nop4"), QStringLiteral("-game"),
                                 winPath(game), winPath(qcPath)};
        compilePartial.clear();
        compileProc = new QProcess(this);
        compileProc->setProcessChannelMode(QProcess::MergedChannels);
        compileProc->setWorkingDirectory(work);
        if (sf.inFlatpak) {
            QStringList args = {QStringLiteral("--host"), QStringLiteral("--directory=") + work};
            for (const QString &e : env) args << QStringLiteral("--env=") + e;
            compileProc->setProgram(QStringLiteral("flatpak-spawn"));
            compileProc->setArguments(args + cmd);
        } else {
            QProcessEnvironment pe = QProcessEnvironment::systemEnvironment();
            for (const QString &e : env) pe.insert(e.section(QLatin1Char('='), 0, 0), e.section(QLatin1Char('='), 1));
            compileProc->setProcessEnvironment(pe);
            compileProc->setProgram(cmd.first());
            compileProc->setArguments(cmd.mid(1));
        }
        cw->line(QStringLiteral("Bonk! %1 · %2 · %3").arg(QStringLiteral(BONK_VERSION), QFileInfo(proton).absoluteDir().dirName(), tildePath(sf.studiomdl)));
        if (firstRun) cw->line(QStringLiteral("First compile: Proton is setting up Bonk's prefix (%1). This takes a minute, once.").arg(tildePath(prefix)));
        QObject::connect(compileProc, &QProcess::readyRead, this, [this] {
            QByteArray &partial = compilePartial;
            partial += compileProc->readAll();
            int nl;
            while ((nl = partial.indexOf('\n')) >= 0) {
                const QString l = QString::fromLocal8Bit(partial.left(nl)).trimmed();
                partial.remove(0, nl + 1);
                if (protonChatter(l)) continue;
                // studiomdl writes its real errors in capitals ("ERROR: …")
                static const QRegularExpression errRe(QStringLiteral("\\bERROR\\b"));
                const bool err = errRe.match(l).hasMatch();
                if (err) job.errors << l;
                if (compileWin) compileWin->line(l, err);
            }
        });
        QObject::connect(compileProc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
            if (e == QProcess::FailedToStart) finishCompile(false, QStringLiteral("couldn't start %1").arg(compileProc->program()));
        });
        QObject::connect(compileProc, &QProcess::finished, this, [this](int code, QProcess::ExitStatus st) {
            finishCompile(st == QProcess::NormalExit && code == 0, QString());
        });
        statusBar()->showMessage(QStringLiteral("Compiling %1…").arg(job.title));
        compileProc->start();
    }

    /// studiomdl is done: copy its output into the addon folder and report.
    void finishCompile(bool exitOk, QString why) {
        if (!compileProc) return;
        compileProc->deleteLater();
        compileProc = nullptr;
        CompileWindow *cw = compileWindow();
        const QString outDir = job.game + QStringLiteral("/models/") + QFileInfo(job.mp).path();
        const QString stem = QFileInfo(job.mp).fileName();
        const bool haveMdl = QFileInfo::exists(outDir + QLatin1Char('/') + stem + QStringLiteral(".mdl"));
        bool ok = why.isEmpty() && haveMdl;
        if (ok) {
            const QString dest = project.addonDir() + QStringLiteral("/models/") + QFileInfo(job.mp).path();
            QDir().mkpath(dest);
            QStringList copied;
            QDir od(outDir);
            for (const QString &f : od.entryList({stem + QStringLiteral(".*")}, QDir::Files)) {
                QFile::remove(dest + QLatin1Char('/') + f);
                if (QFile::copy(od.absoluteFilePath(f), dest + QLatin1Char('/') + f)) copied << f;
            }
            const bool hasPhy = copied.contains(stem + QStringLiteral(".phy"));
            if (job.pieces > 0 && !hasPhy) cw->note(QStringLiteral("studiomdl didn't make a .phy: check the log for collision errors."));
            cw->note(QStringLiteral("%1 triangles%2 → <tt>addon/%3/models/%4.mdl</tt> (%5 files)")
                         .arg(QLocale().toString(job.tris))
                         .arg(hasPhy ? QStringLiteral(", %1 collision piece%2").arg(job.pieces).arg(job.pieces == 1 ? QString() : QStringLiteral("s")) : QString())
                         .arg(project.name.toHtmlEscaped(), job.mp.toHtmlEscaped()).arg(copied.size()));
        } else {
            if (why.isEmpty()) why = !job.errors.isEmpty() ? QStringLiteral("studiomdl reported %1 error%2").arg(job.errors.size()).arg(job.errors.size() == 1 ? QString() : QStringLiteral("s"))
                                     : exitOk ? QStringLiteral("studiomdl made no model") : QStringLiteral("studiomdl stopped with an error");
            for (const QString &e : job.errors.mid(0, 4)) {
                QString hint;
                if (e.contains(QStringLiteral("convex"), Qt::CaseInsensitive)) hint = QStringLiteral(" (too many collision pieces: merge collision brushes)");
                else if (e.contains(QStringLiteral("vert"), Qt::CaseInsensitive)) hint = QStringLiteral(" (too many vertices: see the Budget bar)");
                cw->note(e.toHtmlEscaped() + hint);
            }
        }
        cw->finish(ok, ok ? QStringLiteral("%1 in %2 s").arg(job.title).arg(job.clock.elapsed() / 1000.0, 0, 'f', 1) : why);
        statusBar()->showMessage(ok ? QStringLiteral("Compiled %1. Bonk!").arg(job.title) : QStringLiteral("Compile failed."), 6000);
        if (ok) writeSpawnlistQuiet(cw);
        if (ok && job.exportAfter) exportToGMod(true);
    }

    /// addon.json for the Workshop (gmad), if there isn't one.
    void writeAddonJson() {
        const QString path = project.addonDir() + QStringLiteral("/addon.json");
        if (QFileInfo::exists(path)) return;
        QFile f(path);
        if (!f.open(QIODevice::WriteOnly | QIODevice::Text)) return;
        QString t = project.name;
        t.replace(QLatin1Char('"'), QLatin1Char('\''));
        f.write(QStringLiteral("{\n\t\"title\": \"%1\",\n\t\"type\": \"model\",\n\t\"tags\": [\"build\"],\n"
                               "\t\"ignore\": [\"*.psd\", \"*.bonkmat\", \"*.txt\"]\n}\n").arg(t).toUtf8());
    }

    /// File → Export to GMod Addons Folder: copy addon/<project> to garrysmod/addons/<project>.
    /// `afterCompile`: the automatic export, which replaces Bonk's own earlier export without asking.
    void exportToGMod(bool afterCompile) {
        if (!project.isOpen()) {
            QMessageBox::information(this, QStringLiteral("Export"), QStringLiteral("Open a project first: it's the project's addon folder that gets exported."));
            return;
        }
        SteamFind sf;
        sf.detect();
        if (sf.addonsDir().isEmpty() || !QFileInfo(sf.addonsDir()).isDir()) {
            gmodMissing(QStringLiteral("Couldn't find Garry's Mod's garrysmod/addons folder. Choose the Garry's Mod folder in Settings → Garry's Mod."));
            return;
        }
        const QString src = project.addonDir();
        QStringList models;
        {
            QDirIterator it(src + QStringLiteral("/models"), {QStringLiteral("*.mdl")}, QDir::Files, QDirIterator::Subdirectories);
            while (it.hasNext()) models << QDir(src).relativeFilePath(it.next());
        }
        if (models.isEmpty()) {
            QMessageBox box(QMessageBox::Information, QStringLiteral("Nothing compiled yet"),
                            QStringLiteral("The project's addon folder has no compiled models yet."), QMessageBox::Cancel, this);
            QPushButton *cb = box.addButton(QStringLiteral("Compile Model"), QMessageBox::AcceptRole);
            box.exec();
            if (box.clickedButton() == cb) compileModel();
            return;
        }
        if (!afterCompile) writeSpawnlistQuiet(); // after a compile it's already written
        const QString name = project.name.trimmed();
        if (name.isEmpty() || name.contains(QLatin1Char('/')) || name == QStringLiteral(".") || name == QStringLiteral("..")) return;
        const QString dest = sf.addonsDir() + QLatin1Char('/') + name;
        const QString marker = dest + QStringLiteral("/.bonk-export");
        if (QFileInfo::exists(dest)) {
            const bool ours = QFileInfo::exists(marker);
            if (!(afterCompile && ours) && !qEnvironmentVariableIsSet("BONK_ASSUME_YES")) {
                QMessageBox box(QMessageBox::Question, QStringLiteral("Replace addon?"),
                                ours ? QStringLiteral("<b>garrysmod/addons/%1</b> is already there from an earlier export. Replace it with this one?")
                                           .arg(name.toHtmlEscaped())
                                     : QStringLiteral("<b>garrysmod/addons/%1</b> already exists, and Bonk didn't make it. Replacing it "
                                                      "deletes everything in that folder.").arg(name.toHtmlEscaped()),
                                QMessageBox::NoButton, this);
                QPushButton *rb = box.addButton(QStringLiteral("Replace"), QMessageBox::DestructiveRole);
                box.addButton(QMessageBox::Cancel);
                box.setDefaultButton(QMessageBox::Cancel);
                box.exec();
                if (box.clickedButton() != rb) return;
            }
            QDir(dest).removeRecursively();
        }
        QStringList errors;
        const int n = copyTree(src, dest, &errors);
        QFile mk(marker);
        if (mk.open(QIODevice::WriteOnly | QIODevice::Text))
            mk.write(QStringLiteral("Exported by %1 from %2\n").arg(QString::fromUtf8(kAppName), project.file).toUtf8());
        int vmts = 0, vtfs = 0;
        QDirIterator it(dest, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString f = it.next();
            vmts += f.endsWith(QStringLiteral(".vmt"));
            vtfs += f.endsWith(QStringLiteral(".vtf"));
        }
        const QString summary = QStringLiteral("Exported <b>%1</b> to <tt>%2</tt>:<ul><li>%3 model%4: %5</li><li>%6 material%7 (VMT), %8 texture%9 (VTF)</li><li>%10 files in all</li></ul>"
                                               "Restart Garry's Mod (or change map) to pick up new models; they're in the spawn menu under "
                                               "Browse.%11")
                                    .arg(name.toHtmlEscaped(), tildePath(dest).toHtmlEscaped())
                                    .arg(models.size()).arg(models.size() == 1 ? QString() : QStringLiteral("s"))
                                    .arg(models.join(QStringLiteral(", ")).toHtmlEscaped())
                                    .arg(vmts).arg(vmts == 1 ? QString() : QStringLiteral("s"))
                                    .arg(vtfs).arg(vtfs == 1 ? QString() : QStringLiteral("s"))
                                    .arg(n)
                                    .arg(errors.isEmpty() ? QString() : QStringLiteral("<br><br><b>Problems:</b> ") + errors.join(QStringLiteral("; ")).toHtmlEscaped());
        if (prefs.launchGmod && errors.isEmpty() && !qEnvironmentVariableIsSet("BONK_SCREENSHOT")) startGmod();
        if (afterCompile) {
            compileWindow()->note(QStringLiteral("Exported to <tt>%1</tt> (%2 files).%3").arg(tildePath(dest).toHtmlEscaped()).arg(n)
                                      .arg(prefs.launchGmod ? QStringLiteral(" Starting Garry's Mod.") : QString()));
            compileWin->finish(true, QStringLiteral("%1, exported to GMod").arg(job.title));
            return;
        }
        statusBar()->showMessage(QStringLiteral("Exported %1 to garrysmod/addons. Bonk!").arg(name), 6000);
        if (qEnvironmentVariableIsSet("BONK_ASSUME_YES")) return;
        QMessageBox box(errors.isEmpty() ? QMessageBox::Information : QMessageBox::Warning, QStringLiteral("Exported to Garry's Mod"), summary,
                        QMessageBox::Ok, this);
        QPushButton *ob = box.addButton(QStringLiteral("Open Folder"), QMessageBox::ActionRole);
        box.exec();
        if (box.clickedButton() == ob) QDesktopServices::openUrl(QUrl::fromLocalFile(dest));
    }

    // ---------------------------------------------------------------- texture files

    QStringList clipKeys; // texture clipboard (material names)
    bool clipCut = false;

    /// Textures the project's other models use and the open one doesn't
    /// (cached until a model file changes).
    QSet<QString> otherModelKeys() {
        static QString stamp;
        static QSet<QString> cachedOthers;
        QString now;
        const QStringList files = otherModels();
        for (const QString &f : files) now += f + QString::number(QFileInfo(f).lastModified().toMSecsSinceEpoch());
        if (now != stamp) {
            stamp = now;
            cachedOthers.clear();
            static const QRegularExpression re(QStringLiteral("\"mat\"\\s+\"([^\"]*)\""));
            for (const QString &f : files) {
                QFile fl(f);
                if (!fl.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
                for (auto it = re.globalMatch(QString::fromUtf8(fl.readAll())); it.hasNext();) cachedOthers.insert(it.next().captured(1));
            }
        }
        QSet<QString> out = cachedOthers;
        for (const QString &m : QString::fromUtf8(bonk_used_materials()).split(QLatin1Char('\n'), Qt::SkipEmptyParts)) out.remove(m);
        return out;
    }

    /// Model files in this project other than the open one.
    QStringList otherModels(bool includeOpen = false) const {
        QStringList out;
        if (!project.isOpen()) return out;
        QDirIterator it(project.modelsDir(), {QStringLiteral("*.bpmdl")}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString f = QFileInfo(it.next()).absoluteFilePath();
            if (includeOpen || f != filePath) out << f;
        }
        return out;
    }

    static QRegularExpression matLine(const QString &key) {
        return QRegularExpression(QStringLiteral("(\"mat\"\\s+\")%1(\")").arg(QRegularExpression::escape(key)));
    }

    /// Faces using a texture in the other models: (models, faces).
    QPair<int, int> otherUsage(const QString &key) const {
        int models = 0, faces = 0;
        const QRegularExpression re = matLine(key);
        for (const QString &f : otherModels()) {
            QFile fl(f);
            if (!fl.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
            const QString t = QString::fromUtf8(fl.readAll());
            int n = 0;
            auto it = re.globalMatch(t);
            while (it.hasNext()) {
                it.next();
                ++n;
            }
            if (n) {
                ++models;
                faces += n;
            }
        }
        return {models, faces};
    }

    /// A texture's name changed: update this model (one undo step) and the
    /// other models in the project. Returns how many other models changed.
    int renameRefs(const QString &oldKey, const QString &newKey) {
        QByteArray o = oldKey.toUtf8(), n = newKey.toUtf8();
        bonk_rename_material(o.constData(), n.constData());
        int changed = 0;
        const QRegularExpression re = matLine(oldKey);
        const QString ov = QStringLiteral("\"material_override\"\t\"") + oldKey + QLatin1Char('|');
        // the open model's file too, so it's right on disk even if you don't save
        for (const QString &f : otherModels(true)) {
            QFile fl(f);
            if (!fl.open(QIODevice::ReadOnly | QIODevice::Text)) continue;
            QString t = QString::fromUtf8(fl.readAll());
            fl.close();
            QString t2 = t;
            t2.replace(re, QStringLiteral("\\1") + newKey + QStringLiteral("\\2"));
            t2.replace(ov, QStringLiteral("\"material_override\"\t\"") + newKey + QLatin1Char('|'));
            if (t2 != t) {
                QFile w(f);
                if (w.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                    w.write(t2.toUtf8());
                    if (f != filePath) ++changed;
                }
            }
        }
        return changed;
    }

    static QString keyForRel(const QString &rel) {
        const QFileInfo fi(rel);
        const QString noext = rel.left(rel.size() - fi.suffix().size() - 1);
        return cleanGamePath(noext);
    }

    static bool moveFile(const QString &from, const QString &to) {
        if (!QFileInfo::exists(from)) return true;
        QDir().mkpath(QFileInfo(to).absolutePath());
        if (QFile::rename(from, to)) return true;
        return QFile::copy(from, to) && QFile::remove(from);
    }

    /// Move or rename one texture (image, its .bonkmat, its original, its VTF)
    /// to `newRel` under assets/Textures. Returns an error, or "".
    QString relocate(const TexEntry &e, const QString &newRel, int *otherModelsChanged) {
        const QString root = project.texturesDir();
        const QString dst = root + QLatin1Char('/') + newRel;
        if (QFileInfo(dst).absoluteFilePath() == QFileInfo(e.file).absoluteFilePath()) return QString();
        const QString newKey = keyForRel(newRel);
        if (QFileInfo::exists(dst) || (newKey != e.key && library.find(newKey)))
            return QStringLiteral("%1 already exists").arg(newRel);
        if (!moveFile(e.file, dst)) return QStringLiteral("Couldn't move %1").arg(e.rel);
        moveFile(MatProps::sidecarFor(e.file), MatProps::sidecarFor(dst));
        moveFile(project.originalsDir() + QLatin1Char('/') + e.rel, project.originalsDir() + QLatin1Char('/') + newRel);
        moveFile(library.vtfPath(e.key), library.vtfPath(newKey));
        if (newKey != e.key) *otherModelsChanged += renameRefs(e.key, newKey);
        return QString();
    }

    QString uniqueRel(const QString &rel) const {
        const QString root = project.texturesDir();
        if (!QFileInfo::exists(root + QLatin1Char('/') + rel) && !library.find(keyForRel(rel))) return rel;
        QFileInfo fi(rel);
        const QString dir = rel.contains(QLatin1Char('/')) ? rel.section(QLatin1Char('/'), 0, -2) + QLatin1Char('/') : QString();
        for (int n = 1;; ++n) {
            QString c = dir + fi.completeBaseName() + (n == 1 ? QStringLiteral("_copy") : QStringLiteral("_copy%1").arg(n)) + QLatin1Char('.') + fi.suffix();
            if (!QFileInfo::exists(root + QLatin1Char('/') + c) && !library.find(keyForRel(c))) return c;
        }
    }

    QString pickFolder(const QString &title) {
        QStringList groups = library.groups();
        groups.prepend(QStringLiteral("(top level)"));
        bool ok = false;
        QString g = QInputDialog::getItem(this, title, QStringLiteral("Folder (type a new name to make one):"), groups, 0, true, &ok);
        if (!ok) return QStringLiteral("\x01");
        return g == QStringLiteral("(top level)") ? QString() : cleanGamePath(g);
    }

    void textureFileOp(const QString &op, const QStringList &keys, const QString &arg) {
        if (!ensureProject()) return;
        const QString root = project.texturesDir();
        int others = 0;
        QStringList errors;
        auto finish = [&](const QString &msg) {
            library.scan();
            picker->canPaste = !clipKeys.isEmpty();
            if (!errors.isEmpty()) QMessageBox::warning(this, QStringLiteral("Textures"), errors.join(QLatin1Char('\n')));
            QString m = msg;
            if (others) m += QStringLiteral(" (updated %1 other model%2)").arg(others).arg(others == 1 ? QString() : QStringLiteral("s"));
            if (!m.isEmpty()) statusBar()->showMessage(m, 5000);
            handle(F_R3D | F_R2D | F_TOOL);
        };
        if (op == QStringLiteral("cut") || op == QStringLiteral("copy")) {
            clipKeys = keys;
            clipCut = op == QStringLiteral("cut");
            picker->canPaste = true;
            statusBar()->showMessage(QStringLiteral("%1 %2: open a folder and Paste (Ctrl+V)").arg(clipCut ? QStringLiteral("Cut") : QStringLiteral("Copied"), keys.join(QStringLiteral(", "))), 4000);
            return;
        }
        if (op == QStringLiteral("paste") || op == QStringLiteral("move") || op == QStringLiteral("moveto")) {
            QString dest = arg;
            QStringList list = op == QStringLiteral("paste") ? clipKeys : keys;
            bool copying = op == QStringLiteral("paste") && !clipCut;
            if (op == QStringLiteral("moveto")) {
                dest = pickFolder(QStringLiteral("Move to folder"));
                if (dest == QStringLiteral("\x01")) return;
            }
            QDir().mkpath(root + (dest.isEmpty() ? QString() : QLatin1Char('/') + dest));
            int n = 0;
            for (const QString &k : list) {
                const TexEntry *e = library.find(k);
                if (!e) continue;
                const TexEntry entry = *e;
                QString rel = (dest.isEmpty() ? QString() : dest + QLatin1Char('/')) + QFileInfo(entry.rel).fileName();
                if (copying) {
                    rel = uniqueRel(rel);
                    const QString dst = root + QLatin1Char('/') + rel;
                    if (!QFile::copy(entry.file, dst)) { errors << QStringLiteral("Couldn't copy %1").arg(entry.rel); continue; }
                    if (QFileInfo::exists(MatProps::sidecarFor(entry.file))) QFile::copy(MatProps::sidecarFor(entry.file), MatProps::sidecarFor(dst));
                    ++n;
                } else {
                    QString err = relocate(entry, rel, &others);
                    if (err.isEmpty()) ++n;
                    else errors << err;
                }
            }
            if (op == QStringLiteral("paste") && clipCut) clipKeys.clear();
            finish(QStringLiteral("%1 %2 texture%3 to %4").arg(copying ? QStringLiteral("Copied") : QStringLiteral("Moved")).arg(n)
                       .arg(n == 1 ? QString() : QStringLiteral("s")).arg(dest.isEmpty() ? QStringLiteral("the top level") : dest));
            return;
        }
        if (op == QStringLiteral("rename") && !keys.isEmpty()) {
            const TexEntry *e = library.find(keys.first());
            if (!e) return;
            const TexEntry entry = *e;
            bool ok = false;
            QString nm = QInputDialog::getText(this, QStringLiteral("Rename texture"), QStringLiteral("New name:"), QLineEdit::Normal,
                                               QFileInfo(entry.rel).completeBaseName(), &ok).trimmed();
            if (!ok || nm.isEmpty()) return;
            nm.replace(QLatin1Char('/'), QLatin1Char('_'));
            const QString dirPart = entry.rel.contains(QLatin1Char('/')) ? entry.rel.section(QLatin1Char('/'), 0, -2) + QLatin1Char('/') : QString();
            const QString err = relocate(entry, dirPart + nm + QLatin1Char('.') + QFileInfo(entry.rel).suffix(), &others);
            if (!err.isEmpty()) errors << err;
            finish(err.isEmpty() ? QStringLiteral("Renamed to %1").arg(nm) : QString());
            return;
        }
        if (op == QStringLiteral("delete")) {
            int here = 0;
            QPair<int, int> there{0, 0};
            for (const QString &k : keys) {
                here += bonk_material_usage(k.toUtf8().constData());
                auto u = otherUsage(k);
                there.first += u.first;
                there.second += u.second;
            }
            QString usage = here || there.second ? QStringLiteral("\n\nUsed on %1 face%2 in this model%3. Those faces will show it as missing.")
                                                       .arg(here).arg(here == 1 ? QString() : QStringLiteral("s"))
                                                       .arg(there.first ? QStringLiteral(" and %1 face(s) in %2 other model(s)").arg(there.second).arg(there.first) : QString())
                                                 : QString();
            if (QMessageBox::question(this, QStringLiteral("Delete texture"),
                    QStringLiteral("Move %1 to the Trash (with its properties and VTF)?%2").arg(keys.join(QStringLiteral(", ")), usage)) != QMessageBox::Yes)
                return;
            for (const QString &k : keys) {
                const TexEntry *e = library.find(k);
                if (!e) continue;
                const TexEntry entry = *e;
                if (!QFile::moveToTrash(entry.file)) errors << QStringLiteral("Couldn't move %1 to the Trash").arg(entry.rel);
                if (QFileInfo::exists(MatProps::sidecarFor(entry.file))) QFile::moveToTrash(MatProps::sidecarFor(entry.file));
                if (QFileInfo::exists(library.vtfPath(k))) QFile::moveToTrash(library.vtfPath(k));
            }
            finish(QStringLiteral("Moved to the Trash"));
            return;
        }
        if (op == QStringLiteral("newfolder")) {
            bool ok = false;
            QString nm = QInputDialog::getText(this, QStringLiteral("New folder"), QStringLiteral("Folder name:"), QLineEdit::Normal, QString(), &ok);
            nm = cleanGamePath(nm).replace(QLatin1Char('/'), QLatin1Char('_'));
            if (!ok || nm.isEmpty()) return;
            const QString rel = arg.isEmpty() ? nm : arg + QLatin1Char('/') + nm;
            QDir().mkpath(root + QLatin1Char('/') + rel);
            finish(QStringLiteral("Made folder %1").arg(rel));
            return;
        }
        if (op == QStringLiteral("renamefolder") || op == QStringLiteral("deletefolder")) {
            const QString folder = arg;
            std::vector<TexEntry> inside;
            for (const TexEntry &e : library.entries)
                if (e.group == folder || e.group.startsWith(folder + QLatin1Char('/'))) inside.push_back(e);
            if (op == QStringLiteral("deletefolder")) {
                int here = 0, there = 0;
                for (const TexEntry &e : inside) {
                    here += bonk_material_usage(e.key.toUtf8().constData());
                    there += otherUsage(e.key).second;
                }
                if (QMessageBox::question(this, QStringLiteral("Delete folder"),
                        QStringLiteral("Move %1/ and its %2 texture(s) to the Trash?%3").arg(folder).arg(inside.size())
                            .arg(here + there ? QStringLiteral("\n\nThey're used on %1 face(s) in your models.").arg(here + there) : QString())) != QMessageBox::Yes)
                    return;
                for (const TexEntry &e : inside)
                    if (QFileInfo::exists(library.vtfPath(e.key))) QFile::moveToTrash(library.vtfPath(e.key));
                if (!QFile::moveToTrash(root + QLatin1Char('/') + folder)) errors << QStringLiteral("Couldn't move %1 to the Trash").arg(folder);
                if (picker->folder() == folder || picker->folder().startsWith(folder + QLatin1Char('/'))) picker->enter(QString());
                finish(QStringLiteral("Moved %1/ to the Trash").arg(folder));
                return;
            }
            bool ok = false;
            QString nm = QInputDialog::getText(this, QStringLiteral("Rename folder"), QStringLiteral("New name:"), QLineEdit::Normal, folder.section(QLatin1Char('/'), -1), &ok);
            nm = cleanGamePath(nm).replace(QLatin1Char('/'), QLatin1Char('_'));
            if (!ok || nm.isEmpty()) return;
            const QString parent = folder.contains(QLatin1Char('/')) ? folder.section(QLatin1Char('/'), 0, -2) + QLatin1Char('/') : QString();
            const QString nf = parent + nm;
            if (QFileInfo::exists(root + QLatin1Char('/') + nf)) {
                QMessageBox::warning(this, QStringLiteral("Rename folder"), QStringLiteral("%1 already exists").arg(nf));
                return;
            }
            // move the whole folder (anything else you keep in it comes along)
            if (!QDir(root).rename(folder, nf)) {
                QMessageBox::warning(this, QStringLiteral("Rename folder"), QStringLiteral("Couldn't rename %1").arg(folder));
                return;
            }
            if (QFileInfo::exists(project.originalsDir() + QLatin1Char('/') + folder)) QDir(project.originalsDir()).rename(folder, nf);
            const QString vtfRoot = project.addonDir() + QLatin1Char('/') + project.texturePool;
            if (QFileInfo::exists(vtfRoot + QLatin1Char('/') + cleanGamePath(folder))) {
                QDir().mkpath(QFileInfo(vtfRoot + QLatin1Char('/') + nf).absolutePath());
                QDir(vtfRoot).rename(cleanGamePath(folder), cleanGamePath(nf));
            }
            for (const TexEntry &e : inside) {
                const QString newKey = keyForRel(nf + e.rel.mid(folder.size()));
                if (newKey != e.key) others += renameRefs(e.key, newKey);
            }
            if (picker->folder() == folder) picker->enter(nf);
            finish(QStringLiteral("Renamed %1/ to %2/").arg(folder, nf));
            return;
        }
        if (op == QStringLiteral("colour")) {
            QColor c = QColorDialog::getColor(QColor(QSettings().value("picker/lastColour", "#d06030").toString()), this, QStringLiteral("New color texture"));
            if (!c.isValid()) return;
            QSettings().setValue("picker/lastColour", c.name());
            const QString hex = c.name().mid(1).toLower();
            const QString rel = QStringLiteral("colors/") + hex + QStringLiteral(".png");
            const QString path = root + QLatin1Char('/') + rel;
            QString key = QStringLiteral("colors/") + hex;
            // an existing swatch of this color (even renamed) is reused
            for (const TexEntry &e : library.entries) {
                if (!e.group.startsWith(QStringLiteral("colors"))) continue;
                QImage im(e.file);
                if (im.width() == 16 && im.height() == 16 && im.pixelColor(0, 0).rgb() == c.rgb()) {
                    key = e.key;
                    break;
                }
            }
            if (!library.find(key)) {
                QDir().mkpath(QFileInfo(path).absolutePath());
                QImage img(16, 16, QImage::Format_RGB32);
                img.fill(c);
                if (!img.save(path)) {
                    QMessageBox::warning(this, QStringLiteral("Color"), QStringLiteral("Couldn't save %1").arg(rel));
                    return;
                }
                library.forget({path});
                library.scan();
            }
            QByteArray kb = key.toUtf8();
            bonk_set_current_material(kb.constData());
            if (prefs.swatchSwitchesTool && bonk_tool() != 6) command(CMD_TOOL_FACE);
            picker->enter(QStringLiteral("colors"));
            statusBar()->showMessage(QStringLiteral("Color %1 is ready: right-click faces to paint it").arg(key), 5000);
            handle(F_TOOL);
            return;
        }
    }

    /// Material Properties for a project texture.
    void materialProperties(const QString &key) {
        const TexEntry *e = library.find(key);
        if (!e) return;
        QStringList keys;
        for (const TexEntry &x : library.entries) keys << x.key;
        MaterialDialog d(this, key, e->file, project.texturePool, keys, true);
        if (d.exec() == QDialog::Accepted) {
            library.scan(); // badges
            syncTextureAlpha();
            handle(0);
        }
    }

    // ---------------------------------------------------------- animations

    /// Texture Picker → Import Animation…: frames from several images (name
    /// order), one sprite sheet, or a GIF, saved as one strip image (frames
    /// stacked top to bottom) plus its frame count. Compiling turns it into a
    /// multi-frame VTF with an AnimatedTexture proxy.
    void importAnimation(QStringList files = QStringList()) {
        if (!ensureProject()) return;
        if (files.isEmpty())
            files = QFileDialog::getOpenFileNames(this, QStringLiteral("Import animation: pick the frames, a sprite sheet or a GIF"),
                                                  QSettings().value("files/importDir", QDir::homePath()).toString(),
                                                  QStringLiteral("Images (*.png *.jpg *.jpeg *.gif *.PNG *.JPG *.JPEG *.GIF)"));
        if (files.isEmpty()) return;
        QSettings().setValue("files/importDir", QFileInfo(files.first()).absolutePath());
        QList<QImage> frames;
        double fps = 10.0;
        const bool assume = qEnvironmentVariableIsSet("BONK_ASSUME_YES");
        if (files.size() == 1 && files.first().endsWith(QStringLiteral(".gif"), Qt::CaseInsensitive)) {
            QImageReader r(files.first());
            int delays = 0, n = 0;
            while (r.canRead()) {
                QImage im = r.read();
                if (im.isNull()) break;
                frames << im;
                if (r.nextImageDelay() > 0) { delays += r.nextImageDelay(); ++n; }
            }
            if (n) fps = std::clamp(1000.0 * n / delays, 1.0, 60.0);
            if (frames.isEmpty()) {
                QMessageBox::warning(this, QStringLiteral("Import animation"), QStringLiteral("Couldn't read the GIF's frames: %1").arg(r.errorString()));
                return;
            }
        } else if (files.size() == 1) {
            // one image: a sprite sheet. Guess the grid from square frames, then ask.
            QImage sheet(files.first());
            if (sheet.isNull()) return;
            int cols = 1, rows = 1;
            if (sheet.width() > sheet.height() && sheet.width() % sheet.height() == 0) cols = sheet.width() / sheet.height();
            else if (sheet.height() > sheet.width() && sheet.height() % sheet.width() == 0) rows = sheet.height() / sheet.width();
            if (!assume) {
                QDialog d(this);
                d.setWindowTitle(QStringLiteral("Sprite sheet"));
                auto *f = new QFormLayout(&d);
                auto *c = new QSpinBox;
                auto *r = new QSpinBox;
                auto *cnt = new QSpinBox;
                c->setRange(1, 64);
                r->setRange(1, 64);
                cnt->setRange(1, 4096);
                c->setValue(cols);
                r->setValue(rows);
                cnt->setValue(cols * rows);
                f->addRow(new QLabel(QStringLiteral("%1 is %2 × %3. How is it divided? Frames go left to right, then top to bottom.")
                                         .arg(QFileInfo(files.first()).fileName()).arg(sheet.width()).arg(sheet.height())));
                f->addRow(QStringLiteral("Columns"), c);
                f->addRow(QStringLiteral("Rows"), r);
                f->addRow(QStringLiteral("Frames used"), cnt);
                QObject::connect(c, &QSpinBox::valueChanged, &d, [c, r, cnt] { cnt->setValue(c->value() * r->value()); });
                QObject::connect(r, &QSpinBox::valueChanged, &d, [c, r, cnt] { cnt->setValue(c->value() * r->value()); });
                auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
                f->addRow(bb);
                QObject::connect(bb, &QDialogButtonBox::accepted, &d, &QDialog::accept);
                QObject::connect(bb, &QDialogButtonBox::rejected, &d, &QDialog::reject);
                if (d.exec() != QDialog::Accepted) return;
                cols = c->value();
                rows = r->value();
                const int used = cnt->value();
                const int fw = sheet.width() / cols, fh = sheet.height() / rows;
                for (int i = 0; i < std::min(used, cols * rows); ++i) frames << sheet.copy((i % cols) * fw, (i / cols) * fh, fw, fh);
            } else {
                const int fw = sheet.width() / cols, fh = sheet.height() / rows;
                for (int i = 0; i < cols * rows; ++i) frames << sheet.copy((i % cols) * fw, (i / cols) * fh, fw, fh);
            }
        } else {
            // several images: their frames, in natural name order (frame2 before frame10)
            QCollator col;
            col.setNumericMode(true);
            std::sort(files.begin(), files.end(), [&col](const QString &a, const QString &b) { return col.compare(QFileInfo(a).fileName(), QFileInfo(b).fileName()) < 0; });
            for (const QString &f : files) {
                QImage im(f);
                if (im.isNull()) {
                    QMessageBox::warning(this, QStringLiteral("Import animation"), QStringLiteral("Couldn't read %1").arg(QFileInfo(f).fileName()));
                    return;
                }
                frames << im;
            }
        }
        if (frames.size() < 2) {
            QMessageBox::information(this, QStringLiteral("Import animation"), QStringLiteral("An animation needs at least two frames."));
            return;
        }
        // every frame the same power-of-two size, and the strip no taller than GL likes
        QSize fs = resizeTarget(frames.first().size());
        while (fs.height() * frames.size() > 16384 && fs.height() > 4) fs = QSize(std::max(4, fs.width() / 2), fs.height() / 2);
        QImage strip(fs.width(), fs.height() * int(frames.size()), QImage::Format_ARGB32);
        strip.fill(Qt::transparent);
        {
            QPainter q(&strip);
            for (int i = 0; i < frames.size(); ++i)
                q.drawImage(QRect(0, i * fs.height(), fs.width(), fs.height()), frames[i].scaled(fs, Qt::IgnoreAspectRatio, Qt::SmoothTransformation));
        }
        // its name
        QString base = QFileInfo(files.first()).completeBaseName();
        static const QRegularExpression trail(QStringLiteral("[_\\-\\s]*\\d+$"));
        if (files.size() > 1) base.remove(trail);
        if (base.isEmpty()) base = QStringLiteral("animation");
        QString name = base;
        if (!assume) {
            bool ok = false;
            name = QInputDialog::getText(this, QStringLiteral("Import animation"),
                                         QStringLiteral("%1 frames of %2 × %3. Name it:").arg(frames.size()).arg(fs.width()).arg(fs.height()),
                                         QLineEdit::Normal, base, &ok);
            if (!ok || name.trimmed().isEmpty()) return;
        }
        const QString folder = picker ? picker->folder() : QString();
        const QString dir = project.texturesDir() + (folder.isEmpty() ? QString() : QLatin1Char('/') + folder);
        QDir().mkpath(dir);
        QString path = dir + QLatin1Char('/') + cleanProjectName(name) + QStringLiteral(".png");
        for (int n = 2; QFileInfo::exists(path); ++n) path = dir + QLatin1Char('/') + cleanProjectName(name) + QStringLiteral("_%1.png").arg(n);
        library.pauseScans(true);
        MatProps mp;
        mp.frames = int(frames.size());
        mp.fps = std::round(fps * 10.0) / 10.0;
        if (!strip.save(path) || !mp.saveSidecar(path)) {
            library.pauseScans(false);
            QMessageBox::warning(this, QStringLiteral("Import animation"), QStringLiteral("Couldn't save %1").arg(path));
            return;
        }
        library.markAsked({path});
        library.pauseScans(false);
        syncTextureAlpha();
        handle(F_R3D | F_TOOL);
        statusBar()->showMessage(QStringLiteral("Imported a %1-frame animation (%2 fps): %3. Material Properties → Animation changes its speed.")
                                     .arg(frames.size()).arg(mp.fps).arg(QDir(project.texturesDir()).relativeFilePath(path)), 8000);
    }

    /// Offer to resize images that are over 4096 or not powers of two.
    void offerResize(const QStringList &rels) {
        QString list;
        for (const QString &r : rels) {
            QImageReader rd(project.texturesDir() + QLatin1Char('/') + r);
            QSize s = rd.size(), t = resizeTarget(s);
            list += QStringLiteral("<li><tt>%1</tt>: %2×%3 → %4×%5</li>").arg(r.toHtmlEscaped()).arg(s.width()).arg(s.height()).arg(t.width()).arg(t.height());
        }
        bool yes = !qEnvironmentVariableIsEmpty("BONK_ASSUME_YES");
        if (!yes) {
            QMessageBox box(QMessageBox::Question, QStringLiteral("Resize for Source?"),
                            QStringLiteral("Source textures need power-of-two sizes (up to 4096):"
                                           "<ul>%1</ul>The originals are moved to <b>assets/Original Textures</b>.").arg(list),
                            QMessageBox::NoButton, this);
            QPushButton *rb = box.addButton(QStringLiteral("Resize"), QMessageBox::AcceptRole);
            box.addButton(QStringLiteral("Not Now"), QMessageBox::RejectRole);
            box.setDefaultButton(rb);
            box.exec();
            yes = box.clickedButton() == rb;
        }
        if (!yes) return;
        QStringList errs;
        for (const QString &r : rels) {
            QString e;
            if (!library.resize(r, &e)) errs << e;
        }
        if (!errs.isEmpty()) QMessageBox::warning(this, QStringLiteral("Resize"), errs.join(QLatin1Char('\n')));
        library.scan();
        statusBar()->showMessage(QStringLiteral("Resized %1 texture%2; originals are in assets/Original Textures.")
                                 .arg(rels.size() - errs.size()).arg(rels.size() - errs.size() == 1 ? QString() : QStringLiteral("s")), 5000);
    }

    /// Returns false if the user canceled.
    bool maybeSave() {
        if (!bonk_is_dirty()) return true;
        if (qEnvironmentVariableIsSet("BONK_SCREENSHOT")) return true; // developer screenshots: never stop to ask
        auto r = QMessageBox::warning(this, QStringLiteral("Unsaved changes"),
                                      QStringLiteral("This model has unsaved changes. Save them first?"),
                                      QMessageBox::Save | QMessageBox::Discard | QMessageBox::Cancel, QMessageBox::Save);
        if (r == QMessageBox::Save) return save();
        return r == QMessageBox::Discard;
    }

    void configure() {
        auto refresh = [this] {
            prefs.apply();
            library.askResize = prefs.askResize;
            picker->applyOnClick = prefs.applyOnClick;
            picker->switchOnClick = prefs.swatchSwitchesTool;
            if (panel && panel->layoutMode() != prefs.toolLayout) panel->setLayoutMode(prefs.toolLayout);
            if (budget) {
                budget->limits = BudgetLimits{prefs.budTris, prefs.budVerts, prefs.budMats, prefs.budPieces};
                budget->update_();
            }
            applyToolKeys();
            syncTool();
            repaintAll();
        };
        SettingsDialog dlg(prefs, refresh, this);
        dlg.exec();
        refresh();
        if (prefs.rememberBisect) QSettings().setValue("tools/bisectMode", bonk_bisect_delete());
    }

protected:
    void changeEvent(QEvent *e) override {
        if (e->type() == QEvent::ActivationChange && !isActiveWindow() && pinView > 0) {
            handle(bonk_focus_lost(pinView)); // place what's being sized / cloned / rotated
        }
        if (e->type() == QEvent::ActivationChange && !isActiveWindow() && captured) {
            bonk_command(CMD_RELEASE_CAPTURE);
            setCapture(false);
            handle(F_R3D | F_R2D);
        }
        QMainWindow::changeEvent(e);
    }
    void closeEvent(QCloseEvent *e) override {
        if (captured) {
            bonk_command(CMD_RELEASE_CAPTURE);
            setCapture(false);
        }
        if (maybeSave()) {
            QSettings st;
            st.setValue("window/geometry", saveGeometry());
            st.setValue("window/layout", saveState(kLayoutVersion));
            e->accept();
        } else {
            e->ignore();
        }
    }

private:
    QTimer timer;
    QElapsedTimer clock;
};

Viewport::Viewport(int id_, MainWindow *mw_) : id(id_), mw(mw_) {
    setFocusPolicy(Qt::StrongFocus);
    setMouseTracking(true);
    setMinimumSize(120, 90);
    setAcceptDrops(true); // prefabs from the Prefabs tab
}

void Viewport::dragEnterEvent(QDragEnterEvent *e) {
    if (e->mimeData()->hasFormat(kPrefabMime)) e->acceptProposedAction();
    else e->ignore();
}

void Viewport::dragMoveEvent(QDragMoveEvent *e) {
    if (e->mimeData()->hasFormat(kPrefabMime)) e->acceptProposedAction();
    else e->ignore();
}

void Viewport::dropEvent(QDropEvent *e) {
    if (!e->mimeData()->hasFormat(kPrefabMime)) return e->ignore();
    e->acceptProposedAction();
    const QString file = QString::fromUtf8(e->mimeData()->data(kPrefabMime));
    const QPointF at = e->position();
    QTimer::singleShot(0, mw, [m = mw, file, v = id, at] { m->dropPrefab(file, v, at); });
}

void Viewport::paintGL() {
    labels.clear();
    bonk_view_paint(id, float(width()), float(height()), float(devicePixelRatioF()), &Viewport::textCb, this);
    QPainter p(this);
    paintOverlay(p);
}

/// Everything Qt draws over the GL picture: labels, flashes, the hold hint.
void Viewport::paintOverlay(QPainter &p) {
    p.setRenderHint(QPainter::TextAntialiasing);
    QFont f = uiPt(font(), 9.0);
    p.setFont(f);
    QFontMetrics fm(f);
    bool light = mw->prefs.lightBg();
    QColor strong = light ? QColor(0, 0, 0) : QColor(255, 255, 255);
    QColor soft = light ? QColor(60, 60, 60) : QColor(200, 200, 200);
    QColor shadow = light ? QColor(255, 255, 255, 160) : QColor(0, 0, 0, 200);
    // View name, top-left like Hammer. Highlight the focused view.
    p.setPen(hasFocus() ? strong : soft);
    p.drawText(6, 4 + fm.ascent(), QString::fromLatin1(kViewNames[id]));
    for (const Label &l : labels) {
        int w = fm.horizontalAdvance(l.s);
        float x = l.align == 1 ? l.x - w / 2.0f : (l.align == 2 ? l.x - w : l.x);
        p.setPen(shadow);
        p.drawText(QPointF(x + 1, l.y + 1), l.s);
        p.setPen(l.c);
        p.drawText(QPointF(x, l.y), l.s);
    }
    // big outlined label in the top-right corner (Alt+wheel grid size, "Project saved!")
    auto cornerLabel = [&](const QString &t, double alpha) {
        QFont gf = font();
        gf.setBold(true);
        gf = uiPt(gf, gCompact ? 13 : 16);
        p.setFont(gf);
        QFontMetrics gm(gf);
        QRect r(width() - gm.horizontalAdvance(t) - 26, 8, gm.horizontalAdvance(t) + 16, gm.height() + 8);
        QColor sh = shadow, fg = strong;
        sh.setAlphaF(sh.alphaF() * alpha);
        fg.setAlphaF(alpha);
        p.setPen(sh);
        for (QPoint o : {QPoint(1, 1), QPoint(-1, 1), QPoint(1, -1), QPoint(-1, -1), QPoint(2, 2)}) p.drawText(r.translated(o), Qt::AlignCenter, t);
        p.setPen(fg);
        p.drawText(r, Qt::AlignCenter, t);
        p.setFont(f);
    };
    if (gridFlash.isValid() && gridFlash.elapsed() < 900)
        cornerLabel(QStringLiteral("Grid %1").arg(double(bonk_grid())), 1.0); // no dimming: just the label
    // Saved: a dark tint blinks over every view and fades out
    if (mw->saveFlash.isValid() && mw->saveFlash.elapsed() < 1400) {
        const double t = mw->saveFlash.elapsed() / 1400.0;
        const double tint = t < 0.12 ? t / 0.12 : 1.0 - (t - 0.12) / 0.88;
        p.fillRect(rect(), QColor(0, 0, 0, int(110 * std::clamp(tint, 0.0, 1.0))));
        if (id == 0) cornerLabel(QStringLiteral("Project saved!"), std::clamp(1.0 - std::max(0.0, t - 0.6) / 0.4, 0.0, 1.0));
    }
    // Mouse 4 / 5 switched tools: the tool's icon by the cursor, fading out
    if (toolFlash.isValid() && toolFlash.elapsed() < 700) {
        const double t = toolFlash.elapsed() / 700.0;
        QString ic = toolIconPath(toolFlashIcon);
        if (!ic.isEmpty()) {
            const int sz = 30;
            QRectF r(toolFlashAt.x() + 14, toolFlashAt.y() - sz - 6 - 10 * t, sz, sz);
            p.setOpacity(1.0 - t);
            p.setPen(Qt::NoPen);
            p.setBrush(palette().color(QPalette::Highlight));
            p.setRenderHint(QPainter::Antialiasing);
            p.drawRoundedRect(r.adjusted(-4, -4, 4, 4), 6, 6);
            p.drawPixmap(r.toRect(), QIcon(ic).pixmap(QSize(sz, sz), devicePixelRatioF()));
            p.setOpacity(1.0);
        }
    }
    // What the held key is doing: bottom-left of this view, with a slim bar along the bottom
    {
        int hint[2] = {-1, 0};
        const QString h = QString::fromUtf8(bonk_hold_hint(hint));
        if (hint[0] == id && !h.isEmpty()) {
            static const QColor cols[6] = {QColor(), QColor(70, 140, 230), QColor(80, 180, 110), QColor(220, 140, 50), QColor(200, 90, 200), QColor(130, 130, 150)};
            const QColor accent = cols[std::clamp(hint[1], 0, 5)];
            QFont hf = font();
            hf = uiPt(hf, gCompact ? 8.5 : 9.5);
            p.setFont(hf);
            QFontMetrics hm(hf);
            const int bar = 3, pad = 6;
            QString text = hm.elidedText(h, Qt::ElideRight, width() - 24);
            QRect r(8, height() - bar - hm.height() - 2 * pad - 6, hm.horizontalAdvance(text) + 2 * pad, hm.height() + 2 * pad);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(0, 0, 0, 170));
            p.drawRoundedRect(r, 5, 5);
            p.fillRect(QRect(0, height() - bar, width(), bar), accent);
            p.setPen(Qt::white);
            p.drawText(r, Qt::AlignCenter, text);
            p.setFont(f);
            p.setBrush(Qt::NoBrush); // or the focus frame below fills the whole view with it
        }
    }
    if (id == 0 && bonk_editing_collision()) {
        // editing the physics hull: say so, loudly
        QFont bf = font();
        bf.setBold(true);
        bf = uiPt(bf, gCompact ? 8.5 : 9.5);
        p.setFont(bf);
        const int bh = QFontMetrics(bf).height() + 8;
        p.fillRect(QRect(0, 0, width(), bh), QColor(242, 140, 40, 225));
        p.setPen(QColor(40, 20, 0));
        p.drawText(QRect(8, 0, width() - 16, bh), Qt::AlignVCenter | Qt::AlignHCenter,
                   QStringLiteral("You are editing the collision model"));
        p.setFont(f);
    }
    if (hasFocus()) {
        p.setPen(light ? QColor(70, 70, 140) : QColor(90, 90, 120));
        p.setBrush(Qt::NoBrush);
        p.drawRect(0, 0, width() - 1, height() - 1);
    }
}

void Viewport::mousePressEvent(QMouseEvent *e) {
    if (!mw->captured) setFocus(Qt::MouseFocusReason);
    if (e->button() == Qt::BackButton || e->button() == Qt::ForwardButton) {
        mw->quickTool(this, e->button() == Qt::ForwardButton, e->position());
        e->accept();
        return;
    }
    QPointF p = mw->pinView == id ? mw->pinVirt : e->position();
    mw->handle(bonk_mouse(id, 0, p.x(), p.y(), int(e->button()), modsOf(e->modifiers())), this);
}

void Viewport::mouseReleaseEvent(QMouseEvent *e) {
    if (e->button() == Qt::BackButton || e->button() == Qt::ForwardButton) return;
    QPointF p = mw->pinView == id ? mw->pinVirt : e->position();
    mw->handle(bonk_mouse(id, 1, p.x(), p.y(), int(e->button()), modsOf(e->modifiers())), this);
}

void Viewport::mouseMoveEvent(QMouseEvent *e) {
    if (mw->captured && id == 0) {
        const QPointF d = relDelta(e->position(), center);
        if (d.isNull()) return;
        mw->handle(bonk_mouse_delta(float(d.x()), float(d.y())));
        return;
    }
    if (mw->pinView == id) {
        // Pinned: read the movement, feed the virtual position.
        const QPointF d = relDelta(e->position(), mw->pinLocal);
        if (d.isNull()) return;
        mw->pinVirt += d;
        mw->handle(bonk_mouse(id, 2, float(mw->pinVirt.x()), float(mw->pinVirt.y()), int(e->buttons()), modsOf(e->modifiers())), this);
        return;
    }
    QPointF p = e->position();
    mw->handle(bonk_mouse(id, 2, p.x(), p.y(), int(e->buttons()), modsOf(e->modifiers())), this);
}

void Viewport::wheelEvent(QWheelEvent *e) {
    QPointF p = e->position();
    if (e->modifiers() & Qt::AltModifier) {
        // Alt+wheel: grid size (wheel up = bigger). Qt turns Alt+wheel into a
        // sideways scroll, so read whichever axis moved.
        int d = e->angleDelta().y() ? e->angleDelta().y() : e->angleDelta().x();
        wheelAcc += d;
        while (std::abs(wheelAcc) >= 120) {
            const bool up = wheelAcc > 0;
            wheelAcc += up ? -120 : 120;
            mw->handle(bonk_command(up ? CMD_GRID_LARGER : CMD_GRID_SMALLER), this);
        }
        gridFlash.start();
        update();
        QTimer::singleShot(950, this, [this] { update(); });
        e->accept();
        return;
    }
    float delta = e->angleDelta().y() / 120.0f;
    if (delta != 0.0f) mw->handle(bonk_wheel(id, p.x(), p.y(), delta), this);
    e->accept();
}

void Viewport::keyPressEvent(QKeyEvent *e) {
    // Shift+Z: 3D view fills the area; F4: 3D view full screen (any view)
    if (!e->isAutoRepeat() && !mw->captured && ((e->key() == Qt::Key_Z && e->modifiers() == Qt::ShiftModifier) || e->key() == Qt::Key_F4)
        && !(e->modifiers() & (Qt::ControlModifier | Qt::AltModifier))) {
        if (e->key() == Qt::Key_F4) mw->toggleFull3d();
        else mw->toggleExpand3d();
        e->accept();
        return;
    }
    // A in a 2D view: Axis Lock on / off
    if (id != 0 && e->key() == Qt::Key_A && !e->isAutoRepeat() && (e->modifiers() & (Qt::ControlModifier | Qt::AltModifier | Qt::ShiftModifier)) == 0
        && !mw->captured && mw->prefs.advanced) {
        mw->command(CMD_TOGGLE_AXIS_LOCK);
        int o[4] = {0, 1, 1, 0};
        bonk_selection_options(o);
        mw->statusBar()->showMessage(o[2] ? QStringLiteral("Axis Lock on (A)") : QStringLiteral("Axis Lock off (A)"), 3000);
        e->accept();
        return;
    }
    // Auto-repeat presses are forwarded (arrow-key nudging repeats while held).
    mw->handle(bonk_key(id, e->key(), 1, modsOf(e->modifiers()), e->isAutoRepeat() ? 1 : 0), this);
    e->accept();
}

void Viewport::keyReleaseEvent(QKeyEvent *e) {
    if (e->isAutoRepeat()) { e->accept(); return; }
    mw->handle(bonk_key(id, e->key(), 0, modsOf(e->modifiers()), 0), this);
    e->accept();
}

void Viewport::focusOutEvent(QFocusEvent *e) {
    if (!mw->captured) mw->handle(bonk_focus_lost(id));
    update();
    QOpenGLWidget::focusOutEvent(e);
}

void Viewport::enterEvent(QEnterEvent *e) {
    // Hammer-style: the view under the mouse takes keyboard focus.
    if (!mw->captured && QApplication::activeWindow() == mw) {
        setFocus(Qt::MouseFocusReason);
        for (Viewport *v : mw->views) v->update();
    }
    QOpenGLWidget::enterEvent(e);
}

MainWindow::MainWindow() {
    resize(1400, 900);
    prefs.load();
    prefs.apply();

    for (int i = 0; i < 4; ++i) views[i] = new Viewport(i, this);

    // Hammer layout: camera | top / front | side
    auto *topRow = rowTop = new QSplitter(Qt::Horizontal);
    topRow->addWidget(views[0]);
    topRow->addWidget(views[1]);
    auto *bottomRow = rowBottom = new QSplitter(Qt::Horizontal);
    bottomRow->addWidget(views[2]);
    bottomRow->addWidget(views[3]);
    auto *cols = new QSplitter(Qt::Vertical);
    cols->addWidget(topRow);
    cols->addWidget(bottomRow);
    for (QSplitter *s : {topRow, bottomRow, cols}) {
        s->setHandleWidth(3);
        s->setChildrenCollapsible(false);
    }
    // Keep the two rows' columns aligned, like Hammer's linked splitters.
    QObject::connect(topRow, &QSplitter::splitterMoved, bottomRow, [topRow, bottomRow] { bottomRow->setSizes(topRow->sizes()); });
    QObject::connect(bottomRow, &QSplitter::splitterMoved, topRow, [topRow, bottomRow] { topRow->setSizes(bottomRow->sizes()); });
    setCentralWidget(cols);

    // Panels are docks: drag a title bar to another edge, stack them as tabs,
    // or float them. Window → Reset Layout puts them back.
    panel = new ToolPanel([this](int c) { command(c); }, [this] { toggleTexture(); }, prefs.toolLayout);
    panel->repaint = [this](unsigned f) { handle(f); };
    panel->layoutChosen = [this](int m) {
        prefs.toolLayout = m;
        QSettings().setValue("tools/layout", m);
        syncTool();
    };
    layers = new LayersPane;
    layers->changed = [this](unsigned f) { handle(f | F_R3D | F_R2D); };
    layers->toggleCollisionLook = [this] { command(CMD_TOGGLE_COLLISION_SOLID); };
    layers->newCollision = [this] { newCollisionLayer(); };
    picker = new TexturePicker(&library);
    picker->changed = [this](unsigned f) { handle(f | F_R3D | F_R2D); };
    picker->importReq = [this] { importTextures(); };
    picker->animReq = [this] { importAnimation(); };
    picker->dropped = [this](const QStringList &paths, const QString &g) { importPaths(paths, g, g.isEmpty()); };
    picker->switchToFace = [this] { command(CMD_TOOL_FACE); };
    picker->convertReq = [this](const QStringList &keys) { convertTextures(keys); };
    picker->otherModelKeys = [this] { return otherModelKeys(); };
    picker->propertiesReq = [this](const QString &key) { materialProperties(key); };
    picker->fileReq = [this](const QString &op, const QStringList &keys, const QString &arg) { textureFileOp(op, keys, arg); };
    picker->switchOnClick = prefs.swatchSwitchesTool;
    library.stale = [this](const QStringList &keys) { picker->showStale(keys); };
    picker->applyOnClick = prefs.applyOnClick;
    library.changed = [this] {
        picker->rebuild();
        syncTextureAlpha();
        repaintAll();
    };
    library.askResize = prefs.askResize;
    library.oversized = [this](const QStringList &r) { QTimer::singleShot(0, this, [this, r] { offerResize(r); }); };
    auto dock = [this](const QString &title, const char *name, QWidget *w) {
        auto *d = new QDockWidget(title, this);
        d->setObjectName(QString::fromLatin1(name));
        d->setWidget(w);
        d->setFeatures(QDockWidget::DockWidgetClosable | QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
        return d;
    };
    auto *toolScroll = new QWidget;
    {
        // same shade as the panel all the way down, however tall the dock is
        toolScroll->setAutoFillBackground(true);
        QPalette pal = toolScroll->palette();
        pal.setColor(QPalette::Window, pal.color(QPalette::Window).darker(112));
        toolScroll->setPalette(pal);
        auto *tl = new QVBoxLayout(toolScroll);
        tl->setContentsMargins(0, 0, 0, 0);
        tl->addWidget(panel);
        tl->addStretch(1);
    }
    toolsDock = dock(QStringLiteral("Tools"), "tools", toolScroll);
    // the picker scrolls when the dock is shorter than swatches + Face section
    auto *pickerScroll = new QScrollArea;
    pickerScroll->setWidget(picker);
    pickerScroll->setWidgetResizable(true);
    pickerScroll->setFrameShape(QFrame::NoFrame);
    pickerScroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    pickerScroll->setMinimumWidth(std::max(gCompact ? 200 : 236, picker->minimumSizeHint().width() + pickerScroll->verticalScrollBar()->sizeHint().width() + 2));
    pickerDock = dock(QStringLiteral("Texture Picker"), "texturePicker", pickerScroll);
    layersDock = dock(QStringLiteral("Layers"), "layers", layers);
    prefabs = new PrefabsPane;
    prefabs->saveRequested = [this] { savePrefab(); };
    prefabs->placeRequested = [this](const PrefabInfo &p, int view, QPointF at) { placePrefab(p, view, at); };
    prefabs->renderThumb = [this](const PrefabInfo &p) { return prefabThumb(p); };
    prefabs->projectRoot = [this] { return projectPrefabDir(project); };
    prefabs->projectName = [this] { return project.name; };
    prefabsDock = dock(QStringLiteral("Prefabs"), "prefabs", prefabs);
    variants = new VariantsPane;
    variants->changed = [this](unsigned f) { handle(f); };
    variants->openPreview = [this] { openPreview(); };
    variantsDock = dock(QStringLiteral("Variants"), "variants", variants);
    budget = new BudgetBar;
    budget->changed = [this](unsigned f) { handle(f); };
    budgetDock = dock(QStringLiteral("Budget"), "budget", budget);
    setDockOptions(QMainWindow::AnimatedDocks | QMainWindow::AllowTabbedDocks | QMainWindow::AllowNestedDocks);
    setCorner(Qt::BottomLeftCorner, Qt::LeftDockWidgetArea);
    setCorner(Qt::BottomRightCorner, Qt::RightDockWidgetArea);
    // Default: Tools alone on the left; Texture Picker and Layers tabbed on
    // the right, the picker in front.
    addDockWidget(Qt::LeftDockWidgetArea, toolsDock);
    addDockWidget(Qt::RightDockWidgetArea, layersDock);
    addDockWidget(Qt::RightDockWidgetArea, pickerDock);
    addDockWidget(Qt::BottomDockWidgetArea, budgetDock); // under the viewports
    addDockWidget(Qt::RightDockWidgetArea, prefabsDock);
    addDockWidget(Qt::RightDockWidgetArea, variantsDock);
    tabifyDockWidget(layersDock, pickerDock);
    tabifyDockWidget(pickerDock, prefabsDock);
    tabifyDockWidget(prefabsDock, variantsDock);
    pickerDock->raise();
    const int sideW = gCompact ? lowOr(150, 124) : 190, rightW = gCompact ? lowOr(220, 190) : 270;
    resizeDocks({toolsDock, pickerDock}, {sideW, rightW}, Qt::Horizontal);
    QObject::connect(prefabsDock, &QDockWidget::visibilityChanged, this, [this](bool vis) {
        if (vis && !prefabsShown) {
            prefabsShown = true; // thumbnails are drawn the first time the tab is seen
            prefabs->refresh();
        }
    });
    for (QDockWidget *d : {toolsDock, pickerDock, layersDock})
        QObject::connect(d, &QDockWidget::visibilityChanged, this, [this, d](bool vis) {
            syncTool();
            if (picker) picker->refresh();
        });

    status = new QLabel;
    statusBar()->addWidget(status, 1);

    auto act = [this](QMenu *m, const QString &text, QKeySequence ks, int c) {
        QAction *a = m->addAction(text);
        if (!ks.isEmpty()) a->setShortcut(ks);
        QObject::connect(a, &QAction::triggered, this, [this, c] { command(c); });
        return a;
    };
    auto fn = [this](QMenu *m, const QString &text, QKeySequence ks, std::function<void()> f) {
        QAction *a = m->addAction(text);
        if (!ks.isEmpty()) a->setShortcut(ks);
        QObject::connect(a, &QAction::triggered, this, [f] { f(); });
        return a;
    };

    QMenu *file = menuBar()->addMenu(QStringLiteral("&File"));
    fn(file, QStringLiteral("New &Project…"), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_N), [this] { newProject(); });
    fn(file, QStringLiteral("Open P&roject…"), QKeySequence(Qt::CTRL | Qt::ALT | Qt::Key_O), [this] { openProject(); });
    recentMenu = file->addMenu(QStringLiteral("Recent Projects"));
    projectSettingsAct = fn(file, QStringLiteral("Project Se&ttings…"), QKeySequence(), [this] { projectSettings(); });
    closeProjectAct = fn(file, QStringLiteral("&Close Project"), QKeySequence(), [this] { closeProject(); });
    fn(file, QStringLiteral("Start Screen…"), QKeySequence(), [this] { startScreen(); });
    file->addSeparator();
    fn(file, QStringLiteral("&New Model"), QKeySequence::New, [this] { newModel(); });
    fn(file, QStringLiteral("&Open Model…"), QKeySequence::Open, [this] { open(); });
    recentModelsMenu = file->addMenu(QStringLiteral("Recent &Models"));
    fn(file, QStringLiteral("Reopen &Last Model"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_O), [this] { reopenLast(); });
    rebuildRecentModels();
    fn(file, QStringLiteral("&Save"), QKeySequence::Save, [this] { save(); });
    fn(file, QStringLiteral("Save &As…"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_S), [this] { saveAs(); });
    fn(file, QStringLiteral("&Model Properties…"), QKeySequence(Qt::CTRL | Qt::Key_P), [this] { modelProperties(); });
    file->addSeparator();
    importVmfAct = fn(file, QStringLiteral("&Import VMF…"), QKeySequence(Qt::CTRL | Qt::Key_I), [this] { importVmf(); });
    fn(file, QStringLiteral("Import Source &Textures…"), QKeySequence(), [this] { importSourceTextures(); });
    file->addSeparator();
    fn(file, QStringLiteral("&Compile Model"), QKeySequence(Qt::Key_F9), [this] { compileModel(); });
    fn(file, QStringLiteral("&Export to GMod Addons Folder…"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_E), [this] { exportToGMod(false); });
    fn(file, QStringLiteral("Manage &Workshop…"), QKeySequence(), [this] { manageWorkshop(); });
    fn(file, QStringLiteral("Start &Garry's Mod"), QKeySequence(), [this] { startGmod(); })
        ->setToolTip(QStringLiteral("Through Steam. Settings can start it after every export."));
    fn(file, QStringLiteral("Open A&ddon Folder"), QKeySequence(), [this] {
        if (project.isOpen()) QDesktopServices::openUrl(QUrl::fromLocalFile(project.addonDir()));
    });
    file->addSeparator();
    fn(file, QStringLiteral("&Quit"), QKeySequence::Quit, [this] { close(); });

    QMenu *edit = menuBar()->addMenu(QStringLiteral("&Edit"));
    act(edit, QStringLiteral("&Undo"), QKeySequence::Undo, CMD_UNDO);
    QAction *redo = act(edit, QStringLiteral("&Redo"), QKeySequence(), CMD_REDO);
    redo->setShortcuts({QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Z), QKeySequence(Qt::CTRL | Qt::Key_Y)});
    edit->addSeparator();
    fn(edit, QStringLiteral("Cu&t"), QKeySequence::Cut, [this] { copySelection(true); });
    fn(edit, QStringLiteral("&Copy"), QKeySequence::Copy, [this] { copySelection(false); });
    fn(edit, QStringLiteral("&Paste"), QKeySequence::Paste, [this] { pasteClipboard(false); })
        ->setToolTip(QStringLiteral("Paste in the same spot it was copied from (works across models and projects)"));
    fn(edit, QStringLiteral("Paste Under the &Mouse"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_V), [this] { pasteClipboard(true); });
    savePrefabAct = fn(edit, QStringLiteral("Save Selection as Pre&fab…"), QKeySequence(), [this] { savePrefab(); });
    act(edit, QStringLiteral("&Delete"), QKeySequence::Delete, CMD_DELETE);
    act(edit, QStringLiteral("Select &All"), QKeySequence::SelectAll, CMD_SELECT_ALL);
    edit->addSeparator();
    act(edit, QStringLiteral("&Quantize to Grid (straightens rotation to 45°)"), QKeySequence(Qt::ALT | Qt::Key_Q), CMD_QUANTIZE);
    edit->addSeparator();
    act(edit, QStringLiteral("&Group"), QKeySequence(Qt::CTRL | Qt::Key_G), CMD_GROUP);
    act(edit, QStringLiteral("U&ngroup"), QKeySequence(Qt::CTRL | Qt::Key_U), CMD_UNGROUP);
    edit->addSeparator();
    act(edit, QStringLiteral("&Join Brushes"), QKeySequence(Qt::Key_J), CMD_JOIN)
        ->setToolTip(QStringLiteral("One convex brush from the selection (hull, or exact only: Settings → Editing)"));
    act(edit, QStringLiteral("Join Brushes (&Hull)"), QKeySequence(), CMD_JOIN_HULL);
    act(edit, QStringLiteral("Join Brushes (E&xact Only)"), QKeySequence(), CMD_JOIN_EXACT);
    ignoreGroupsAct = act(edit, QStringLiteral("&Ignore Groups"), QKeySequence(Qt::CTRL | Qt::Key_W), CMD_IGNORE_GROUPS);
    ignoreGroupsAct->setCheckable(true);
    edit->addSeparator();
    act(edit, QStringLiteral("&Deselect (D in a 2D view)"), QKeySequence(Qt::CTRL | Qt::Key_D), CMD_DESELECT);
    // single keys are handled by the views (they need to know which view the mouse is in)
    act(edit, QStringLiteral("&Hide Selection / Show Hidden\tH"), QKeySequence(), CMD_HIDE_TOGGLE);
    act(edit, QStringLiteral("&Mirror Horizontally\t,"), QKeySequence(), CMD_MIRROR_H);
    act(edit, QStringLiteral("Mirror &Vertically\t."), QKeySequence(), CMD_MIRROR_V);
    edit->addSeparator();
    fn(edit, QStringLiteral("Set &Layer Name…"), QKeySequence(Qt::Key_F2), [this] { if (layers) layers->renameCurrent(); });
    fn(edit, QStringLiteral("&Tidy Nested Groups"), QKeySequence(), [this] {
        const int n = bonk_tidy_groups();
        handle(F_R3D | F_R2D);
        statusBar()->showMessage(n ? QStringLiteral("Removed %1 group%2 that only wrapped another group (Ctrl+Z undoes it)").arg(n).arg(n == 1 ? QString() : QStringLiteral("s"))
                                   : QStringLiteral("No wrapper groups to remove."), 6000);
    })->setToolTip(QStringLiteral("Dissolve groups that hold nothing but one other group (common in Hammer maps)"));

    QMenu *view = menuBar()->addMenu(QStringLiteral("&View"));
    act(view, QStringLiteral("Smaller Grid"), QKeySequence(Qt::Key_BracketLeft), CMD_GRID_SMALLER);
    act(view, QStringLiteral("Larger Grid"), QKeySequence(Qt::Key_BracketRight), CMD_GRID_LARGER);
    view->addSeparator();
    act(view, QStringLiteral("Center 2D Views on Selection"), QKeySequence(Qt::CTRL | Qt::Key_E), CMD_CENTER_2D);
    view->addSeparator();
    showMannequins = act(view, QStringLiteral("Show &Measurement Buddies"), QKeySequence(), CMD_TOGGLE_MANNEQUINS);
    showMannequins->setCheckable(true);
    nodraw2dAct = fn(view, QStringLiteral("Show &Nodraw in 2D Views"), QKeySequence(), [this] {
        prefs.nd2d = !prefs.nd2d;
        QSettings().setValue("nodraw/show2d", prefs.nd2d);
        prefs.apply();
        syncTool();
        repaintAll();
    });
    nodraw2dAct->setCheckable(true);
    edges3dAct = fn(view, QStringLiteral("Show &Edges in 3D"), QKeySequence(), [this] {
        bonk_set_show_edges(!bonk_show_edges());
        QSettings().setValue("view/edges3d", bool(bonk_show_edges()));
        syncTool();
        repaintAll();
    });
    edges3dAct->setCheckable(true);
    bonk_set_show_edges(QSettings().value("view/edges3d", true).toBool());
    QMenu *look = view->addMenu(QStringLiteral("&Collision Look"));
    lookMenuAct = look->menuAction();
    auto *lookGroup = new QActionGroup(this);
    lookGhost = fn(look, QStringLiteral("&Ghosted (orange glass)"), QKeySequence(), [this] { if (prefs.collisionSolid) command(CMD_TOGGLE_COLLISION_SOLID); });
    lookSolid = fn(look, QStringLiteral("&Solid (each piece its own color)"), QKeySequence(), [this] { if (!prefs.collisionSolid) command(CMD_TOGGLE_COLLISION_SOLID); });
    for (QAction *a : {lookGhost, lookSolid}) {
        a->setCheckable(true);
        lookGroup->addAction(a);
    }
    view->addSeparator();
    {
        // 3D field of view: [75 ▲▼], up to 120 unless unlocked
        auto *w = new QWidget;
        auto *h = new QHBoxLayout(w);
        h->setContentsMargins(28, 2, 8, 2);
        h->addWidget(new QLabel(QStringLiteral("3D field of view")));
        fovSpin = new QSpinBox;
        fovSpin->setRange(30, prefs.fovUnlocked ? 170 : 120);
        fovSpin->setSuffix(QStringLiteral("°"));
        fovSpin->setValue(prefs.fov);
        fovSpin->setToolTip(QStringLiteral("How wide the 3D view sees (Hammer uses 75–90). Up to 120 unless unlocked below."));
        h->addWidget(fovSpin);
        auto *wa = new QWidgetAction(this);
        wa->setDefaultWidget(w);
        view->addAction(wa);
        QObject::connect(fovSpin, &QSpinBox::valueChanged, this, [this](int val) {
            prefs.fov = val;
            QSettings().setValue("view/fov", val);
            bonk_set_fov(float(val));
            views[0]->update();
        });
        QAction *unlock = fn(view, QStringLiteral("Unlock Field of View Above 120°"), QKeySequence(), [this] {
            prefs.fovUnlocked = !prefs.fovUnlocked;
            QSettings().setValue("view/fovUnlocked", prefs.fovUnlocked);
            fovSpin->setMaximum(prefs.fovUnlocked ? 170 : 120); // locking again pulls it back to 120
        });
        unlock->setCheckable(true);
        unlock->setChecked(prefs.fovUnlocked);
        fn(view, QStringLiteral("Reset Field of View"), QKeySequence(), [this] { fovSpin->setValue(75); });
        view->addSeparator();
        fn(view, QStringLiteral("&Model Preview…"), QKeySequence(Qt::Key_F7), [this] { openPreview(); });
        // also on the window itself, so they work with the menu bar hidden (F4)
        addAction(fn(view, QStringLiteral("E&xpand 3D View"), QKeySequence(Qt::SHIFT | Qt::Key_Z), [this] { toggleExpand3d(); }));
        addAction(fn(view, QStringLiteral("3D View &Full Screen"), QKeySequence(Qt::Key_F4), [this] { toggleFull3d(); }));
    }

    QMenu *tools = menuBar()->addMenu(QStringLiteral("&Tools"));
    toolSelect = act(tools, QStringLiteral("&Selection Tool"), QKeySequence(), CMD_TOOL_SELECT);
    toolBlock = act(tools, QStringLiteral("&Block Tool"), QKeySequence(), CMD_TOOL_BLOCK);
    toolVertex = act(tools, QStringLiteral("&Vertex Tool"), QKeySequence(), CMD_TOOL_VERTEX);
    toolBisect = act(tools, QStringLiteral("B&isect Tool"), QKeySequence(), CMD_TOOL_BISECT);
    tools->addSeparator();
    toolMannequin = act(tools, QStringLiteral("Place Scale &Mannequin"), QKeySequence(), CMD_TOOL_MANNEQUIN);
    act(tools, QStringLiteral("Place &Car (HL2 jeep size)"), QKeySequence(), CMD_TOOL_CAR);
    tools->addSeparator();
    toolOrigin = act(tools, QStringLiteral("&Origin Tool"), QKeySequence(), CMD_TOOL_ORIGIN);
    toolFace = act(tools, QStringLiteral("&Face Select && Paint Tool"), QKeySequence(), CMD_TOOL_FACE);
    texAction = fn(tools, QStringLiteral("&Texture Picker"), QKeySequence(), [this] { toggleTexture(); });
    act(tools, QStringLiteral("Origin to Selection &Bottom-Center"), QKeySequence(), CMD_ORIGIN_TO_SEL_BOTTOM);
    act(tools, QStringLiteral("Origin to Selection Ce&ntre"), QKeySequence(), CMD_ORIGIN_TO_SEL_CENTER);
    tools->addSeparator();
    act(tools, QStringLiteral("&Create Block"), QKeySequence(), CMD_CREATE_BLOCK);
    act(tools, QStringLiteral("&Apply Bisect Cut"), QKeySequence(), CMD_BISECT_APPLY);
    tools->addSeparator();
    fn(tools, QStringLiteral("New Collision &Layer…"), QKeySequence(), [this] { newCollisionLayer(); });
    fn(tools, QStringLiteral("Set &Mass…"), QKeySequence(), [this] { setMass(); });
    tools->addSeparator();
    fn(tools, QStringLiteral("Find &Overlapping Brushes…"), QKeySequence(), [this] { findOverlaps(); })
        ->setIcon(QIcon(toolIconPath(QStringLiteral("overlap"))));
    tools->addSeparator();
    fn(tools, QStringLiteral("S&pawnlist Editor…"), QKeySequence(), [this] { openSpawnlists(); });
    auto *group = new QActionGroup(this);
    for (QAction *a : {toolSelect, toolBlock, toolVertex, toolBisect, toolMannequin, toolOrigin, toolFace}) {
        a->setCheckable(true);
        group->addAction(a);
    }
    applyToolKeys();

    QMenu *window = menuBar()->addMenu(QStringLiteral("&Window"));
    for (QDockWidget *d : {toolsDock, pickerDock, layersDock, prefabsDock, variantsDock, budgetDock}) window->addAction(d->toggleViewAction());
    window->addSeparator();
    fn(window, QStringLiteral("&Reset Layout"), QKeySequence(), [this] {
        restoreState(defaultLayout, kLayoutVersion);
        for (QDockWidget *d : {toolsDock, pickerDock, layersDock, prefabsDock, variantsDock, budgetDock}) d->show();
        pickerDock->raise();
        resizeDocks({toolsDock, pickerDock}, {gCompact ? lowOr(150, 124) : 190, gCompact ? lowOr(220, 190) : 270}, Qt::Horizontal);
    });

    QMenu *settings = menuBar()->addMenu(QStringLiteral("&Settings"));
    fn(settings, QStringLiteral("&Configure Bonk!…"), QKeySequence(Qt::CTRL | Qt::SHIFT | Qt::Key_Comma), [this] { configure(); });

    QMenu *help = menuBar()->addMenu(QStringLiteral("&Help"));
    fn(help, QStringLiteral("Bonk! &Guide"), QKeySequence(Qt::Key_F1), [this] { openGuide(contextPage()); });
    fn(help, QStringLiteral("&Cheat Sheet"), QKeySequence(), [this] { openGuide(QStringLiteral("cheat")); });
    help->addSeparator();
    fn(help, QStringLiteral("&About Bonk!"), QKeySequence(), [this] { AboutDialog(this).exec(); });

    // Restore the Bisect mode if the user asked for it to be remembered.
    if (prefs.rememberBisect)
        bonk_command(prefs.bisectMode == 2 ? CMD_BISECT_MODE_DELETE_LARGER : prefs.bisectMode == 1 ? CMD_BISECT_MODE_DELETE : CMD_BISECT_MODE_SPLIT);

    // Camera fly timer
    clock.start();
    QObject::connect(&timer, &QTimer::timeout, this, [this] {
        float dt = clock.restart() / 1000.0f;
        unsigned f = bonk_tick(dt);
        if (f) handle(f);
        if (saveFlash.isValid() && saveFlash.elapsed() < 1500) repaintAll();
    });
    timer.start(8);

    // Remember the default layout for Reset Layout, then restore the user's.
    defaultLayout = saveState(kLayoutVersion);
    {
        QSettings st;
        restoreGeometry(st.value("window/geometry").toByteArray());
        const bool restored = restoreState(st.value("window/layout").toByteArray(), kLayoutVersion);
        // layouts saved before the Prefabs tab existed: put it beside Layers
        if (restored && !st.value("window/prefabsPlaced", false).toBool()) {
            tabifyDockWidget(layersDock, prefabsDock);
            prefabsDock->show();
            pickerDock->raise();
        }
        st.setValue("window/prefabsPlaced", true);
        if (restored && !st.value("window/variantsPlaced", false).toBool()) { // and the Variants tab beside it
            tabifyDockWidget(prefabsDock, variantsDock);
            variantsDock->show();
            pickerDock->raise();
        }
        st.setValue("window/variantsPlaced", true);
    }
    rebuildRecentMenu();
    library.setProject(&project);

    syncTool();
    status->setText(QString::fromUtf8(bonk_status()));
    updateTitle();
}

class NoAltMenuStyle : public QProxyStyle {
public:
    explicit NoAltMenuStyle(QStyle *base) : QProxyStyle(base) {}
    int styleHint(StyleHint hint, const QStyleOption *opt, const QWidget *w, QStyleHintReturn *ret) const override {
        if (hint == QStyle::SH_MenuBar_AltKeyNavigation) return 0;
        return QProxyStyle::styleHint(hint, opt, w, ret);
    }
};

} // namespace

extern "C" void bonk_crash_report(int signal);
extern "C" const char *bonk_crash_log_path();

namespace {
/// Fatal signals: write the crash log (with a backtrace), then die as usual.
void onFatalSignal(int sig) {
    static volatile sig_atomic_t inside = 0;
    if (!inside) {
        inside = 1;
        bonk_crash_report(sig);
    }
    signal(sig, SIG_DFL);
    raise(sig);
}
} // namespace

extern "C" int bonk_qt_run(int *argc, char **argv) {
    for (int sig : {SIGSEGV, SIGABRT, SIGBUS, SIGFPE, SIGILL}) signal(sig, onFatalSignal);
    // Mouselook needs cursor warping, which native Wayland does not allow for
    // ordinary windows. Under a Wayland session (e.g. Plasma 6) run through
    // XWayland unless the user picked a platform or set BONK_WAYLAND=1.
    if (qEnvironmentVariableIsEmpty("QT_QPA_PLATFORM") && !qEnvironmentVariableIsEmpty("WAYLAND_DISPLAY") &&
        !qEnvironmentVariableIsEmpty("DISPLAY") && qEnvironmentVariableIsEmpty("BONK_WAYLAND")) {
        qputenv("QT_QPA_PLATFORM", "xcb");
    }

    QSurfaceFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QSurfaceFormat::CoreProfile);
    fmt.setDepthBufferSize(24);
    fmt.setSamples(4);
    QSurfaceFormat::setDefaultFormat(fmt);

    QApplication app(*argc, argv);
    {
        // Interface size: compact on screens under 1440p unless Settings says otherwise.
        QSettings st(QStringLiteral("bonk"), QStringLiteral("bonk"));
        int d = st.value("view/density", 0).toInt();
        if (qEnvironmentVariable("BONK_DENSITY") == QStringLiteral("compact") || qEnvironmentVariable("BONK_DENSITY") == QStringLiteral("lowres")) d = 1;
        if (qEnvironmentVariable("BONK_DENSITY") == QStringLiteral("regular")) d = 2;
        QScreen *sc = QGuiApplication::primaryScreen();
        const int physH = sc ? int(sc->geometry().height() * sc->devicePixelRatio()) : 1080;
        gCompact = d == 1 || (d == 0 && physH < 1440);
        gLowRes = d == 1;
        gLowPx = st.value("view/lowPx", 6).toInt();
        if (qEnvironmentVariableIsSet("BONK_LOWPX")) gLowPx = qEnvironmentVariable("BONK_LOWPX").toInt();
        {
            int n = 0;
            const unsigned char *data = bonk_pixel_font(&n);
            const int id = QFontDatabase::addApplicationFontFromData(QByteArray(reinterpret_cast<const char *>(data), n));
            const QStringList fam = id >= 0 ? QFontDatabase::applicationFontFamilies(id) : QStringList();
            if (!fam.isEmpty()) gPixelFamily = fam.first();
        }
        if (gLowRes) {
            QFont f = QApplication::font();
            if (!gPixelFamily.isEmpty()) {
                gPixelFont = true;
                f = QFont(gPixelFamily);
                f.setHintingPreference(QFont::PreferNoHinting);
            }
            gLowPx = gPixelFont ? std::clamp((gLowPx + 3) / 6 * 6, 6, 18) : std::clamp(gLowPx, 6, 11);
            f.setPixelSize(gLowPx);
            f.setStyleStrategy(QFont::StyleStrategy(QFont::NoAntialias | QFont::PreferMatch));
            QApplication::setFont(f);
        } else if (gCompact) {
            QFont f = QApplication::font();
            if (f.pointSizeF() > 0) f.setPointSizeF(std::max(7.5, f.pointSizeF() - 1.0));
            QApplication::setFont(f);
        }
    }
    QApplication::setOrganizationName(QStringLiteral("bonk"));
    QApplication::setApplicationName(QStringLiteral("bonk"));
    QApplication::setApplicationDisplayName(QString::fromUtf8(kAppName));
    QGuiApplication::setDesktopFileName(QStringLiteral("bonk"));
    // Window icon: the SVG installed next to the binary, else the icon theme.
    QString iconPath = QCoreApplication::applicationDirPath() + QStringLiteral("/bonk.svg");
#ifdef BONK_ASSET_DIR
    if (!QFileInfo::exists(iconPath)) iconPath = QStringLiteral(BONK_ASSET_DIR "/bonk.svg");
#endif
    QIcon icon = QFileInfo::exists(iconPath) ? QIcon(iconPath) : QIcon::fromTheme(QStringLiteral("bonk"));
    if (!icon.isNull()) QApplication::setWindowIcon(icon);
    // Alt is the "no snapping" modifier, so it must not jump into the menu bar
    // when released after a drag.
    app.setStyle(new NoAltMenuStyle(QStyleFactory::create(QApplication::style()->name())));

    MainWindow w;
    w.show();

    // Crashed last time? Point at the log so it can be sent along.
    {
        const QString log = QString::fromUtf8(bonk_crash_log_path());
        const QFileInfo fi(log);
        QSettings st;
        const qint64 seen = st.value("crash/seen", 0).toLongLong();
        if (fi.exists() && fi.lastModified().toSecsSinceEpoch() > seen) {
            st.setValue("crash/seen", fi.lastModified().toSecsSinceEpoch());
            if (seen > 0 && qEnvironmentVariableIsEmpty("BONK_SCREENSHOT") && qEnvironmentVariableIsEmpty("BONK_DIALOG_SHOT"))
                QTimer::singleShot(600, &w, [&w, log] {
                    QMessageBox box(QMessageBox::Warning, QStringLiteral("Bonk! crashed last time"),
                                    QStringLiteral("Sorry about that. Bonk wrote down where it happened in <tt>%1</tt>.<br><br>"
                                                   "Sending that file along with what you were doing helps get it fixed.").arg(tildePath(log).toHtmlEscaped()),
                                    QMessageBox::Ok, &w);
                    QPushButton *open = box.addButton(QStringLiteral("Open Folder"), QMessageBox::ActionRole);
                    box.exec();
                    if (box.clickedButton() == open) QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(log).absolutePath()));
                });
        } else if (!fi.exists() && seen == 0) {
            st.setValue("crash/seen", 1); // armed: the next crash gets mentioned
        }
    }

    // Open a model or project given on the command line (e.g. double-clicked in Dolphin).
    const QStringList args = QCoreApplication::arguments();
    bool opened = false;
    for (int i = 1; i < args.size(); ++i) {
        if (!args[i].startsWith(QLatin1Char('-'))) {
            opened = w.loadFile(args[i]);
            break;
        }
    }
    const bool devShot = !qEnvironmentVariableIsEmpty("BONK_SCREENSHOT") || !qEnvironmentVariableIsEmpty("BONK_DIALOG_SHOT") ||
                         !qEnvironmentVariableIsEmpty("BONK_SETTINGS_SHOT") || !qEnvironmentVariableIsEmpty("BONK_NO_START");
    if (!opened && !devShot && w.prefs.startScreen) QTimer::singleShot(0, &w, [&w] { w.startScreen(); });

    // Developer aid: BONK_SCREENSHOT=out.png grabs the window after startup and quits.
    // BONK_SHOT_SCRIPT=commands runs first: "tool:N", "mat:name", "texture" (toggle picker)
    const QString shot = qEnvironmentVariable("BONK_SCREENSHOT");
    if (!shot.isEmpty() && qEnvironmentVariableIsSet("BONK_GRAB_MODALS")) { // grab and close each message box that pops up
        auto *mt = new QTimer(&w);
        QObject::connect(mt, &QTimer::timeout, &w, [shot] {
            static int n = 0;
            if (auto *m = qobject_cast<QMessageBox *>(QApplication::activeModalWidget())) {
                m->grab().save(shot + QStringLiteral(".modal%1.png").arg(++n));
                const QString pick = qEnvironmentVariable("BONK_GRAB_MODALS");
                for (QAbstractButton *b : m->buttons())
                    if (b->text().remove(QLatin1Char('&')) == pick) { b->click(); return; }
                m->reject();
            }
        });
        mt->start(300);
    }
    if (!shot.isEmpty()) {
        QTimer::singleShot(1500, &w, [&w, shot] {
            for (const QString &c : qEnvironmentVariable("BONK_SHOT_SCRIPT").split(QLatin1Char(';'), Qt::SkipEmptyParts)) {
                if (c.startsWith(QStringLiteral("cmd:"))) w.command(c.mid(4).toInt());
                else if (c.startsWith(QStringLiteral("hover:"))) { // hover:view,x,y
                    const QStringList a = c.mid(6).split(QLatin1Char(','));
                    const int v = a.value(0).toInt();
                    w.handle(bonk_mouse(v, 2, a.value(1).toFloat(), a.value(2).toFloat(), 0, 0), w.views[v]);
                } else if (c.startsWith(QStringLiteral("keydown:")) || c.startsWith(QStringLiteral("keyup:"))) { // keydown:view,qtkey
                    const bool down = c.startsWith(QStringLiteral("keydown:"));
                    const QStringList a = c.mid(down ? 8 : 6).split(QLatin1Char(','));
                    const int v = a.value(0).toInt();
                    w.handle(bonk_key(v, a.value(1).toInt(0, 0), down, 0, 0), w.views[v]);
                } else if (c.startsWith(QStringLiteral("move:"))) { // move:view,x,y — a real mouse move event (pinning included)
                    const QStringList a = c.mid(5).split(QLatin1Char(','));
                    Viewport *vp = w.views[a.value(0).toInt()];
                    const QPointF at(a.value(1).toDouble(), a.value(2).toDouble());
                    QMouseEvent me(QEvent::MouseMove, at, vp->mapToGlobal(at), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
                    QApplication::sendEvent(vp, &me);
                } else if (c == QStringLiteral("selbounds")) {
                    float b[6] = {0, 0, 0, 0, 0, 0};
                    if (bonk_debug_sel_bounds(b)) fprintf(stderr, "selbounds %.1f %.1f %.1f %.1f %.1f %.1f\n", b[0], b[1], b[2], b[3], b[4], b[5]);
                    else fprintf(stderr, "selbounds none\n");
                } else if (c == QStringLiteral("copy")) w.copySelection(false);
                else if (c == QStringLiteral("paste") || c == QStringLiteral("pastem")) w.pasteClipboard(c.endsWith(QLatin1Char('m')));
                else if (c == QStringLiteral("overlaps")) w.findOverlaps();
                else if (c.startsWith(QStringLiteral("mass:"))) bonk_set_mass(c.mid(5).toFloat());
                else if (c == QStringLiteral("savefab")) w.savePrefab();
                else if (c == QStringLiteral("workshop")) w.manageWorkshop();
                else if (c.startsWith(QStringLiteral("wsthumb:"))) { QString e; saveWorkshopJpg(QImage(c.mid(8)), WorkshopProject::thumbPath(w.project), &e); }
                else if (c == QStringLiteral("prefabs")) { w.prefabsDock->show(); w.prefabsDock->raise(); }
                else if (c.startsWith(QStringLiteral("skinop:"))) { // skinop:<op>:<index>:<text>
                    const QStringList a = c.split(QLatin1Char(':'));
                    const QByteArray t = a.value(3).toUtf8();
                    w.handle(bonk_skin_op(a.value(1).toInt(), a.value(2).toInt(), t.constData()));
                } else if (c.startsWith(QStringLiteral("bgop:"))) { // bgop:<op>:<group id>:<text>:<text2>
                    const QStringList a = c.split(QLatin1Char(':'));
                    const QByteArray t = a.value(3).toUtf8(), t2 = a.value(4).toUtf8();
                    w.handle(bonk_bodygroup_op(a.value(1).toInt(), a.value(2).toInt(), t.constData(), t2.constData()));
                } else if (c.startsWith(QStringLiteral("lods:"))) { const QByteArray t = c.mid(5).toUtf8(); w.handle(bonk_set_lods(t.constData())); }
                else if (c == QStringLiteral("variants")) { w.variantsDock->show(); w.variantsDock->raise(); w.variants->refresh(true); }
                else if (c.startsWith(QStringLiteral("placefab:"))) { // placefab:<name>:<view>:<x>:<y>
                    const QStringList a = c.split(QLatin1Char(':'));
                    for (const QString &root : {sharedPrefabDir(), projectPrefabDir(w.project)})
                        for (const PrefabInfo &pf : listPrefabs(root, root == sharedPrefabDir()))
                            if (pf.name == a.value(1)) w.dropPrefab(pf.file, a.value(2, QStringLiteral("-1")).toInt(), QPointF(a.value(3).toDouble(), a.value(4).toDouble()));
                }
                else if (c == QStringLiteral("spawnlist")) w.openSpawnlists();
                else if (c == QStringLiteral("spawnwrite")) w.writeSpawnlistQuiet();
                else if (c == QStringLiteral("coldlg")) { auto *d = new CollisionDialog(&w, w.prefs.simplifyDetail, false, w.prefs.advanced); d->setAttribute(Qt::WA_DeleteOnClose); d->show(); }
                else if (c.startsWith(QStringLiteral("anim:"))) w.importAnimation(c.mid(5).split(QLatin1Char('|')));
                else if (c.startsWith(QStringLiteral("selusing:"))) { QByteArray k = c.mid(9).toUtf8(); bonk_select_faces_with(k.constData()); w.handle(F_R3D | F_R2D | F_TOOL); }
                else if (c == QStringLiteral("convertall")) { if (w.picker->convertReq) w.picker->convertReq({}); }
                else if (c.startsWith(QStringLiteral("matdlg:"))) { // matdlg:key:tab
                    const QString key = c.section(QLatin1Char(':'), 1, 1);
                    const TexEntry *e = w.library.find(key);
                    if (e) {
                        QStringList keys;
                        for (const TexEntry &x : w.library.entries) keys << x.key;
                        auto *d = new MaterialDialog(&w, key, e->file, w.project.texturePool, keys, true);
                        d->setAttribute(Qt::WA_DeleteOnClose);
                        d->findChild<QTabWidget *>()->setCurrentIndex(c.section(QLatin1Char(':'), 2, 2).toInt());
                        d->show();
                    }
                }
                else if (c.startsWith(QStringLiteral("select:"))) { QByteArray b = c.mid(7).toUtf8(); w.handle(bonk_select_ids(b.constData())); }
                else if (c == QStringLiteral("current")) w.layers->debugCurrent();
                else if (c.startsWith(QStringLiteral("fold:"))) w.layers->debugFold(c.mid(5).toInt());
                else if (c.startsWith(QStringLiteral("clipfile:"))) { // clipfile:path: put a file's text on the clipboard
                    QFile cf(c.mid(9));
                    if (cf.open(QIODevice::ReadOnly)) QApplication::clipboard()->setText(QString::fromUtf8(cf.readAll()));
                } else if (c.startsWith(QStringLiteral("clipsave:"))) { // clipsave:path
                    QFile cf(c.mid(9));
                    if (cf.open(QIODevice::WriteOnly)) cf.write(QApplication::clipboard()->text().toUtf8());
                }
                else if (c.startsWith(QStringLiteral("shape:"))) bonk_set_block_shape(c.mid(6).section(QLatin1Char(','), 0, 0).toInt(), c.mid(6).section(QLatin1Char(','), 1, 1).toInt());
                else if (c == QStringLiteral("saveflash")) {
                    w.saveFlash.start();
                } else if (c.startsWith(QStringLiteral("toolflash:"))) w.quickTool(w.views[1], c.endsWith(QLatin1Char('5')), QPointF(220, 160));
                else if (c.startsWith(QStringLiteral("mat:"))) {
                    QByteArray m = c.mid(4).toUtf8();
                    bonk_set_current_material(m.constData());
                    w.handle(F_TOOL);
                } else if (c.startsWith(QStringLiteral("apply:"))) {
                    QByteArray m = c.mid(6).toUtf8();
                    bonk_apply_material(m.constData());
                    w.handle(F_R3D | F_R2D | F_TOOL);
                } else if (c == QStringLiteral("texture")) w.toggleTexture();
                else if (c == QStringLiteral("convert")) w.convertTextures({});
                else if (c == QStringLiteral("layers")) w.layersDock->raise();
                else if (c.startsWith(QStringLiteral("vmf:"))) {
                    w.importVmf(c.mid(4));
                    fprintf(stderr, "IMPORT %s\n", qPrintable(w.lastImportSummary));
                } else if (c.startsWith(QStringLiteral("srcimport:"))) { // srcimport:path|all
                    MatSource src;
                    if (src.open(c.mid(10).section(QLatin1Char('|'), 0, 0)))
                        w.runSourceImport(src, w.modelMaterials(), c.section(QLatin1Char('|'), 1) == QStringLiteral("all"));
                    else fprintf(stderr, "SRC ERROR %s\n", qPrintable(src.error));
                    fprintf(stderr, "IMPORT %s\n", qPrintable(w.lastImportSummary));
                } else if (c.startsWith(QStringLiteral("show:"))) { // show:<layer id>: click its Show box
                    QElapsedTimer t;
                    t.start();
                    w.layers->clickShow(c.mid(5).toInt());
                    for (int i = 0; i < 20; ++i) QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
                    fprintf(stderr, "SHOW toggle took %lld ms\n", (long long)t.elapsed());
                } else if (c.startsWith(QStringLiteral("groupclick:"))) {
                    w.layers->clickGroup(c.mid(11).toInt());
                    for (int i = 0; i < 20; ++i) QCoreApplication::processEvents(QEventLoop::AllEvents, 20);
                    fprintf(stderr, "status: %s\n", bonk_status());
                } else if (c == QStringLiteral("2d")) {
                    for (int i = 1; i < 4; ++i) bonk_view_resize(i, float(w.views[i]->width()), float(w.views[i]->height()));
                }
                else if (c.startsWith(QStringLiteral("guide:")) || c.startsWith(QStringLiteral("search:"))) {
                    w.openGuide(c.startsWith(QLatin1Char('g')) ? c.mid(6) : QStringLiteral("selecting"));
                    if (c.startsWith(QLatin1Char('s'))) w.guide->searchBox()->setText(c.mid(7));
                    for (int i = 0; i < 10; ++i) QCoreApplication::processEvents(QEventLoop::AllEvents, 30);
                    w.guide->grab().save(qEnvironmentVariable("BONK_SCREENSHOT") + QStringLiteral(".guide.png"));
                } else if (c == QStringLiteral("about")) {
                    auto *d = new AboutDialog(&w);
                    d->show();
                    for (int i = 0; i < 10; ++i) QCoreApplication::processEvents(QEventLoop::AllEvents, 30);
                    d->grab().save(qEnvironmentVariable("BONK_SCREENSHOT") + QStringLiteral(".about.png"));
                    fprintf(stderr, "%s\n", qPrintable(AboutDialog::systemInfoText()));
                }
                else if (c == QStringLiteral("compile") || c == QStringLiteral("export")) {
                    if (c == QStringLiteral("compile")) w.compileModel();
                    else w.exportToGMod(false);
                    QElapsedTimer t;
                    t.start();
                    while (w.compileProc && t.elapsed() < 20000) QCoreApplication::processEvents(QEventLoop::AllEvents, 50);
                    if (w.compileWin && w.compileWin->isVisible()) {
                        QCoreApplication::processEvents();
                        w.compileWin->grab().save(qEnvironmentVariable("BONK_SCREENSHOT") + QStringLiteral(".compile.png"));
                    }
                }
                else if (c.startsWith(QStringLiteral("fileop:"))) { // fileop:op|key,key|arg
                    const QStringList parts = c.mid(7).split(QLatin1Char('|'));
                    w.textureFileOp(parts.value(0), parts.value(1).split(QLatin1Char(','), Qt::SkipEmptyParts), parts.value(2));
                } else if (c.startsWith(QStringLiteral("enter:"))) w.picker->enter(c.mid(6));
                else if (c == QStringLiteral("addcol")) w.handle(bonk_layer_op(1, 0));
                else if (c == QStringLiteral("expand3d")) w.toggleExpand3d();
                else if (c == QStringLiteral("full3d")) w.toggleFull3d();
                else if (c.startsWith(QStringLiteral("selbox:"))) { // selbox:x0,y0,x1,y1 in the top view
                    const QStringList v = c.mid(7).split(QLatin1Char(','));
                    bonk_view_resize(1, float(w.views[1]->width()), float(w.views[1]->height()));
                    w.handle(bonk_mouse(1, 0, v[0].toFloat(), v[1].toFloat(), int(Qt::LeftButton), 0));
                    w.handle(bonk_mouse(1, 2, (v[0].toFloat() + v[2].toFloat()) / 2, (v[1].toFloat() + v[3].toFloat()) / 2, int(Qt::LeftButton), 0));
                    w.handle(bonk_mouse(1, 2, v[2].toFloat(), v[3].toFloat(), int(Qt::LeftButton), 0));
                    w.handle(bonk_mouse(1, 1, v[2].toFloat(), v[3].toFloat(), int(Qt::LeftButton), 0));
                }
                else if (c.startsWith(QStringLiteral("click3d:")) || c.startsWith(QStringLiteral("rclick3d:"))) {
                    const bool right = c.startsWith(QLatin1Char('r'));
                    const QStringList xy = c.section(QLatin1Char(':'), 1).split(QLatin1Char(','));
                    const int mods = xy.size() > 2 ? xy[2].toInt() : 0;
                    const int btn = right ? int(Qt::RightButton) : int(Qt::LeftButton);
                    bonk_view_resize(0, float(w.views[0]->width()), float(w.views[0]->height())); // no GL here: size it by hand
                    w.handle(bonk_mouse(0, 0, xy[0].toFloat(), xy[1].toFloat(), btn, mods));
                    w.handle(bonk_mouse(0, 1, xy[0].toFloat(), xy[1].toFloat(), btn, mods));
                }
                else if (c.startsWith(QStringLiteral("props:"))) {
                    const QString key = c.mid(6);
                    QTimer::singleShot(200, &w, [&w, key] { w.materialProperties(key); });
                    QTimer::singleShot(420, &w, [] {
                        for (QWidget *tl : QApplication::topLevelWidgets())
                            if (auto *d = dynamic_cast<MaterialDialog *>(tl)) {
                                d->grab().save(qEnvironmentVariable("BONK_SCREENSHOT") + QStringLiteral(".props.png"));
                                d->reject();
                            }
                    });
                }                else if (c.startsWith(QStringLiteral("drop:"))) // drop:path|group (same as a drop on the picker)
                    w.picker->dropped(c.mid(5).section(QLatin1Char('|'), 0, 0).split(QLatin1Char(',')), c.section(QLatin1Char('|'), 1));
                else if (c == QStringLiteral("tabright")) {
                    w.addDockWidget(Qt::RightDockWidgetArea, w.pickerDock);
                    w.tabifyDockWidget(w.layersDock, w.pickerDock);
                    w.pickerDock->raise();
                    w.resizeDocks({w.layersDock}, {300}, Qt::Horizontal);
                }
            }
            QTimer::singleShot(500, &w, [&w, shot] {
                for (Viewport *v : w.views) {
                    QRect r(v->mapTo(&w, QPoint(0, 0)), v->size());
                    fprintf(stderr, "view %d %d %d %d %d\n", v->id, r.x(), r.y(), r.width(), r.height());
                }
                QImage img = w.grab().toImage();
                if (qEnvironmentVariableIsSet("BONK_SHOT_OVERLAY")) { // offscreen has no GL: draw the Qt overlays on a dark view
                    QPainter q(&img);
                    for (Viewport *v : w.views) {
                        QRect r(v->mapTo(&w, QPoint(0, 0)), v->size());
                        q.save();
                        q.translate(r.topLeft());
                        q.setClipRect(QRect(QPoint(0, 0), r.size()));
                        q.fillRect(QRect(QPoint(0, 0), r.size()), QColor(18, 18, 22));
                        v->paintOverlay(q);
                        q.restore();
                    }
                }
                img.save(shot);
                for (QDialog *dlg : w.findChildren<QDialog *>())
                    if (dlg->isVisible()) dlg->grab().save(shot + QStringLiteral(".dialog.png"));
                QApplication::quit();
            });
        });
    }
    // Developer aid: BONK_DIALOG_SHOT=start|newproject|modelprops|settings:out.png
    const QString dshot = qEnvironmentVariable("BONK_DIALOG_SHOT");
    if (!dshot.isEmpty()) {
        QTimer::singleShot(800, &w, [&w, dshot] {
            const QString kind = dshot.section(QLatin1Char(':'), 0, 0), out = dshot.section(QLatin1Char(':'), 1);
            QDialog *d = nullptr;
            if (kind == QStringLiteral("start")) d = new StartScreen(&w);
            else if (kind == QStringLiteral("newproject")) {
                auto *pd = new ProjectDialog(&w);
                pd->findChild<QLineEdit *>()->setText(QStringLiteral("housepack"));
                d = pd;
            } else if (kind == QStringLiteral("modelprops")) d = new ModelPropertiesDialog(&w, QString::fromUtf8(bonk_model_path()), w.project, QStringLiteral("house_1"));
            else if (kind == QStringLiteral("matsource")) {
                QSettings().setValue("import/lastSource", qEnvironmentVariable("BONK_IMPORT_SOURCE"));
                d = new MaterialSourceDialog(&w, {QStringLiteral("custom/walls/brick_wall"), QStringLiteral("custom/floors/concrete"),
                                                  QStringLiteral("custom/metal/plate"), QStringLiteral("custom/missing/step")},
                                             QStringLiteral("Where are this map's materials? Point at the <b>materials</b> folder it used "
                                                            "(e.g. sourcemods/hl2/materials), the addon folder above one, or a Workshop <b>.gma</b>."));
            } else if (kind == QStringLiteral("visgroups"))
                d = new VisgroupDialog(&w, {{3, 2, QStringLiteral("Front wall")}, {4, 1, QStringLiteral("Roof")}, {5, 14, QStringLiteral("Props / Furniture")}}, 11);
            if (!d) { QApplication::quit(); return; }
            d->show();
            QTimer::singleShot(400, d, [d, out] {
                d->grab().save(out);
                QApplication::quit();
            });
        });
    }
    // Developer aid: BONK_SETTINGS_SHOT=out.png grabs the settings dialog.
    const QString sshot = qEnvironmentVariable("BONK_SETTINGS_SHOT");
    if (!sshot.isEmpty()) {
        QTimer::singleShot(800, &w, [&w, sshot] {
            Prefs p;
            p.load();
            SettingsDialog d(p, [] {}, &w);
            d.show();
            d.adjustSize();
            QTimer::singleShot(300, &d, [&d, sshot] {
                d.grab().save(sshot);
                if (auto *sa = d.findChild<QScrollArea *>()) sa->widget()->grab().save(sshot + QStringLiteral(".full.png"));
                QApplication::quit();
            });
            d.exec();
        });
    }
    return app.exec();
}
