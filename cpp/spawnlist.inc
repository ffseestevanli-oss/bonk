// SPDX-License-Identifier: GPL-3.0-or-later
// Spawnlist Editor: GMod spawn menu categories for the project's models,
// written into the addon as Lua (spawnmenu.AddPropCategory), with category
// icons and spawn icons Bonk draws itself.
//
// Included by host.cpp (inside its anonymous namespace).

// ------------------------------------------------------------------ data

/// Where a spawn icon's camera is: orbiting `target` (yaw / pitch degrees,
/// distance), with a field of view.
struct ThumbCam {
    double yaw = -60.0, pitch = 20.0, dist = 0.0; // dist 0 = not framed yet (Fit)
    double tx = 0, ty = 0, tz = 0;
    double fov = 30.0;
    QJsonObject toJson() const {
        return {{"yaw", yaw}, {"pitch", pitch}, {"dist", dist}, {"tx", tx}, {"ty", ty}, {"tz", tz}, {"fov", fov}};
    }
    static ThumbCam fromJson(const QJsonObject &o) {
        ThumbCam c;
        c.yaw = o.value("yaw").toDouble(c.yaw);
        c.pitch = o.value("pitch").toDouble(c.pitch);
        c.dist = o.value("dist").toDouble(0);
        c.tx = o.value("tx").toDouble();
        c.ty = o.value("ty").toDouble();
        c.tz = o.value("tz").toDouble();
        c.fov = o.value("fov").toDouble(c.fov);
        return c;
    }
};

/// One entry on a category's page: a header line or a model.
struct SpawnItem {
    bool header = false;
    QString text;    // header text
    QString model;   // "models/…/x.mdl"
    QString source;  // the .bpmdl it's made from (relative to the project)
    int wide = 64, tall = 64; // the tile in the spawn menu (64 × 64 is GMod's usual)
    bool square() const { return wide == tall; }
    ThumbCam cam;
};

/// A category icon: a base (a built-in shape, a Silk icon or an image),
/// an optional tint, and an optional sub-icon in a corner.
struct IconSpec {
    QString base = QStringLiteral("shape:folder"); // shape:<name> | silk:<name> | file:<path>
    QColor tint;                                   // invalid = none
    QString sub;                                   // "" = none
    int corner = 3;                                // 0 top-left, 1 top-right, 2 bottom-left, 3 bottom-right
    QJsonObject toJson() const {
        return {{"base", base}, {"tint", tint.isValid() ? tint.name() : QString()}, {"sub", sub}, {"corner", corner}};
    }
    static IconSpec fromJson(const QJsonObject &o) {
        IconSpec s;
        s.base = o.value("base").toString(s.base);
        const QString t = o.value("tint").toString();
        if (!t.isEmpty()) s.tint = QColor(t);
        s.sub = o.value("sub").toString();
        s.corner = std::clamp(o.value("corner").toInt(3), 0, 3);
        return s;
    }
    /// A plain Silk icon is already in GMod (icon16/…): nothing to write.
    bool plainSilk() const { return base.startsWith(QStringLiteral("silk:")) && !tint.isValid() && sub.isEmpty(); }
};

struct SpawnCategory {
    QString name;
    IconSpec icon;
    QList<SpawnItem> items;
    QList<SpawnCategory> children;
};

QJsonObject catToJson(const SpawnCategory &c) {
    QJsonArray items, kids;
    for (const SpawnItem &it : c.items) {
        QJsonObject o;
        if (it.header) {
            o["type"] = QStringLiteral("header");
            o["text"] = it.text;
        } else {
            o["type"] = QStringLiteral("model");
            o["model"] = it.model;
            o["source"] = it.source;
            o["wide"] = it.wide;
            o["tall"] = it.tall;
            o["cam"] = it.cam.toJson();
        }
        items.append(o);
    }
    for (const SpawnCategory &k : c.children) kids.append(catToJson(k));
    return {{"name", c.name}, {"icon", c.icon.toJson()}, {"items", items}, {"children", kids}};
}

SpawnCategory catFromJson(const QJsonObject &o) {
    SpawnCategory c;
    c.name = o.value("name").toString();
    c.icon = IconSpec::fromJson(o.value("icon").toObject());
    for (const QJsonValue &v : o.value("items").toArray()) {
        const QJsonObject i = v.toObject();
        SpawnItem it;
        it.header = i.value("type").toString() == QStringLiteral("header");
        it.text = i.value("text").toString();
        it.model = i.value("model").toString();
        it.source = i.value("source").toString();
        const int size = i.value("size").toInt(64); // 0.16.0 kept one size
        it.wide = std::clamp(i.value("wide").toInt(size), 16, 1024);
        it.tall = std::clamp(i.value("tall").toInt(size), 16, 1024);
        it.cam = ThumbCam::fromJson(i.value("cam").toObject());
        c.items << it;
    }
    for (const QJsonValue &v : o.value("children").toArray()) c.children << catFromJson(v.toObject());
    return c;
}

/// The project's spawnlists, kept in <project>/spawnlist.json.
struct Spawnlists {
    QList<SpawnCategory> lists;

    static QString pathFor(const Project &p) { return p.dir() + QStringLiteral("/spawnlist.json"); }

    bool load(const Project &p) {
        lists.clear();
        QFile f(pathFor(p));
        if (!f.open(QIODevice::ReadOnly)) return false;
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        for (const QJsonValue &v : o.value("lists").toArray()) lists << catFromJson(v.toObject());
        return true;
    }
    bool save(const Project &p) const {
        QJsonArray a;
        for (const SpawnCategory &c : lists) a.append(catToJson(c));
        QFile f(pathFor(p));
        if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
        f.write(QJsonDocument(QJsonObject{{"format", 1}, {"lists", a}}).toJson());
        return true;
    }
    bool empty() const {
        std::function<bool(const SpawnCategory &)> any = [&](const SpawnCategory &c) {
            if (!c.items.isEmpty()) return true;
            for (const SpawnCategory &k : c.children)
                if (any(k)) return true;
            return false;
        };
        for (const SpawnCategory &c : lists)
            if (any(c)) return false;
        return true;
    }
};

// ------------------------------------------------------------------ icons

QString silkDir() { return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/bonk/silk"); }

QStringList silkNames() {
    QStringList n;
    for (const QFileInfo &fi : QDir(silkDir()).entryInfoList({QStringLiteral("*.png")}, QDir::Files, QDir::Name)) n << fi.completeBaseName();
    return n;
}

const char *kIconShapes[] = {"folder", "house", "box", "star", "tv", "chair", "bed", "door", "brush", "car", "tree", "lamp", "circle", "square"};

/// A built-in icon shape, drawn on a 16-unit grid scaled to `s` pixels.
QImage drawShape(const QString &name, int s, const QColor &fill) {
    QImage img(s, s, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    QPainter p(&img);
    p.setRenderHint(QPainter::Antialiasing, s > 16);
    p.scale(s / 16.0, s / 16.0);
    const QColor c = fill.isValid() ? fill : QColor(232, 180, 70);
    const QColor dark = c.darker(160), light = c.lighter(140);
    p.setPen(QPen(dark, 1.0));
    p.setBrush(c);
    auto poly = [&](std::initializer_list<QPointF> pts) { p.drawPolygon(QPolygonF(pts)); };
    if (name == QStringLiteral("house")) {
        poly({{1.5, 8}, {8, 1.5}, {14.5, 8}, {13, 8}, {13, 14.5}, {3, 14.5}, {3, 8}});
        p.setBrush(QColor(250, 250, 250));
        p.drawRect(QRectF(6.5, 9.5, 3, 5));
    } else if (name == QStringLiteral("folder")) {
        poly({{1, 3}, {6, 3}, {7.5, 4.5}, {15, 4.5}, {15, 14}, {1, 14}});
        p.setBrush(light);
        poly({{1, 6.5}, {15, 6.5}, {15, 14}, {1, 14}});
    } else if (name == QStringLiteral("box")) {
        poly({{2, 5}, {8, 2}, {14, 5}, {14, 12}, {8, 15}, {2, 12}});
        p.setBrush(light);
        poly({{2, 5}, {8, 2}, {14, 5}, {8, 8}});
        p.drawLine(QPointF(8, 8), QPointF(8, 15));
    } else if (name == QStringLiteral("star")) {
        QPolygonF st;
        for (int i = 0; i < 10; ++i) {
            const double a = -kPi / 2 + i * kPi / 5, r = i % 2 ? 3.2 : 7.2;
            st << QPointF(8 + r * std::cos(a), 8.5 + r * std::sin(a));
        }
        p.drawPolygon(st);
    } else if (name == QStringLiteral("tv")) {
        p.drawRoundedRect(QRectF(1, 2.5, 14, 9.5), 1, 1);
        p.setBrush(QColor(40, 60, 90));
        p.drawRect(QRectF(2.5, 4, 11, 6.5));
        p.setBrush(c);
        poly({{6, 12}, {10, 12}, {11, 14.5}, {5, 14.5}});
    } else if (name == QStringLiteral("chair")) {
        p.drawRect(QRectF(4, 1.5, 2, 12.5));
        p.drawRect(QRectF(4, 8, 8, 2));
        p.drawRect(QRectF(10, 10, 2, 4.5));
    } else if (name == QStringLiteral("bed")) {
        p.drawRect(QRectF(1, 8, 14, 4));
        p.setBrush(light);
        p.drawRoundedRect(QRectF(2, 5.5, 4, 2.5), 1, 1);
        p.setBrush(c);
        p.drawRect(QRectF(1, 4, 1.5, 10.5));
        p.drawRect(QRectF(13.5, 8, 1.5, 6.5));
    } else if (name == QStringLiteral("door")) {
        p.drawRect(QRectF(3.5, 1, 9, 14));
        p.setBrush(dark);
        p.drawEllipse(QPointF(10.5, 8.5), 0.9, 0.9);
    } else if (name == QStringLiteral("brush")) {
        p.setBrush(QColor(160, 110, 60));
        poly({{9, 1}, {11, 1}, {11, 8}, {9, 8}});
        p.setBrush(QColor(200, 200, 205));
        p.drawRect(QRectF(8, 8, 4, 2));
        p.setBrush(c);
        poly({{8, 10}, {12, 10}, {13, 15}, {7, 15}});
    } else if (name == QStringLiteral("car")) {
        poly({{1, 9}, {3, 5.5}, {11, 5.5}, {14.5, 9}, {15, 12}, {1, 12}});
        p.setBrush(QColor(40, 40, 45));
        p.drawEllipse(QPointF(4.5, 12.5), 2, 2);
        p.drawEllipse(QPointF(11.5, 12.5), 2, 2);
    } else if (name == QStringLiteral("tree")) {
        p.setBrush(QColor(120, 80, 40));
        p.drawRect(QRectF(7, 10, 2, 5));
        p.setBrush(c);
        p.drawEllipse(QRectF(2.5, 1, 11, 10.5));
    } else if (name == QStringLiteral("lamp")) {
        poly({{4, 7}, {6, 1.5}, {10, 1.5}, {12, 7}});
        p.drawRect(QRectF(7.25, 7, 1.5, 6));
        p.drawRect(QRectF(4.5, 13, 7, 2));
    } else if (name == QStringLiteral("circle")) {
        p.drawEllipse(QRectF(1.5, 1.5, 13, 13));
    } else {
        p.drawRoundedRect(QRectF(2, 2, 12, 12), 2, 2);
    }
    return img;
}

/// Any icon base (shape:, silk:, file:) at `s` pixels; images get the tint over them.
QImage iconPart(const QString &spec, int s, const QColor &tint, const QString &projectDir) {
    if (spec.startsWith(QStringLiteral("shape:"))) return drawShape(spec.mid(6), s, tint);
    QImage src;
    if (spec.startsWith(QStringLiteral("silk:"))) src = QImage(silkDir() + QLatin1Char('/') + spec.mid(5) + QStringLiteral(".png"));
    else if (spec.startsWith(QStringLiteral("file:"))) {
        QString f = spec.mid(5);
        if (QFileInfo(f).isRelative()) f = projectDir + QLatin1Char('/') + f;
        src = QImage(f);
    }
    QImage img(s, s, QImage::Format_ARGB32_Premultiplied);
    img.fill(Qt::transparent);
    if (src.isNull()) return drawShape(QStringLiteral("square"), s, QColor(200, 70, 70));
    QPainter p(&img);
    p.setRenderHint(QPainter::SmoothPixmapTransform, s != 16 || src.width() != 16);
    p.drawImage(QRect(0, 0, s, s), src);
    if (tint.isValid()) {
        // recolor, keeping the shading: tint the image's own pixels only
        p.setCompositionMode(QPainter::CompositionMode_SourceAtop);
        QColor t = tint;
        t.setAlphaF(0.55);
        p.fillRect(img.rect(), t);
    }
    return img;
}

/// The finished category icon at `s` pixels (16 for the game).
QImage renderIcon(const IconSpec &spec, int s, const QString &projectDir) {
    QImage img = iconPart(spec.base, s, spec.tint, projectDir);
    if (!spec.sub.isEmpty()) {
        const int ss = std::max(6, s * 9 / 16);
        const QImage sub = iconPart(spec.sub, ss, QColor(), projectDir);
        const int x = (spec.corner % 2) ? s - ss : 0, y = (spec.corner / 2) ? s - ss : 0;
        QPainter p(&img);
        p.drawImage(x, y, sub);
    }
    return img;
}

/// A name usable in file names and Lua ids.
QString spawnSlug(const QString &s) {
    QString o;
    for (QChar c : s.toLower()) o += (c.isLetterOrNumber() && c.unicode() < 128) ? c : QLatin1Char('_');
    while (o.contains(QStringLiteral("__"))) o.replace(QStringLiteral("__"), QStringLiteral("_"));
    return o.isEmpty() ? QStringLiteral("list") : o;
}

/// A Lua string literal.
QString luaStr(const QString &s) {
    QString o = s;
    o.replace(QLatin1Char('\\'), QStringLiteral("\\\\"));
    o.replace(QLatin1Char('"'), QStringLiteral("\\\""));
    o.replace(QLatin1Char('\n'), QStringLiteral("\\n"));
    return QLatin1Char('"') + o + QLatin1Char('"');
}

// ------------------------------------------------------------------ writing

/// Renders a spawn icon of a saved model (the main window supplies it: it needs GL).
using ThumbRenderer = std::function<QImage(const QString &bpmdl, const ThumbCam &cam, int w, int h)>;

/// `cam` re-framed snugly for a view `aspect` wide per unit tall, keeping its angle.
ThumbCam fitCam(const QString &bpmdl, ThumbCam cam, double aspect) {
    float f[4];
    const QByteArray p = QFile::encodeName(bpmdl);
    if (bonk_model_fit(p.constData(), float(cam.yaw), float(cam.pitch), float(cam.fov), float(aspect), f) == 0) {
        cam.tx = f[0];
        cam.ty = f[1];
        cam.tz = f[2];
        cam.dist = f[3];
    }
    return cam;
}

/// Write the spawnlist into the addon: the Lua, category icons and spawn icons.
/// Returns a summary (or an error starting with "!").
QString writeSpawnlists(const Spawnlists &sl, const Project &pj, const ThumbRenderer &render) {
    if (sl.empty()) return QString();
    const QString addon = pj.addonDir();
    const QString slug = spawnSlug(pj.name);
    const QString iconDir = QStringLiteral("materials/%1/spawnlist").arg(slug);
    QDir().mkpath(addon + QLatin1Char('/') + iconDir);
    QDir().mkpath(addon + QStringLiteral("/lua/autorun/client"));
    // ids: a block of numbers of its own, so it doesn't collide with the player's spawnlists
    const int base = 700000 + int(qHash(slug) % 9000u) * 100;
    int next = base;
    int icons = 0, thumbs = 0;
    QStringList errors;
    QString lua = QStringLiteral("-- Written by Bonk! Model Prop-inator! for %1.\n"
                                 "-- Edit the spawnlist in Bonk (Tools > Spawnlist Editor), not this file: it's rewritten on compile.\n"
                                 "hook.Add( \"PopulatePropMenu\", %2, function()\n")
                      .arg(pj.name, luaStr(QStringLiteral("bonk_spawnlist_") + slug));
    QSet<QString> doneThumbs;
    std::function<void(const SpawnCategory &, int, const QString &)> walk = [&](const SpawnCategory &c, int parent, const QString &path) {
        const int id = next++;
        const QString cslug = path + spawnSlug(c.name);
        QString icon;
        if (c.icon.plainSilk()) {
            icon = QStringLiteral("icon16/%1.png").arg(c.icon.base.mid(5));
        } else {
            const QString rel = iconDir + QLatin1Char('/') + cslug + QStringLiteral(".png");
            if (renderIcon(c.icon, 16, pj.dir()).save(addon + QLatin1Char('/') + rel)) ++icons;
            else errors << QStringLiteral("couldn't write %1").arg(rel);
            icon = rel.mid(10); // without "materials/"
        }
        lua += QStringLiteral("\tspawnmenu.AddPropCategory( %1, %2, {\n").arg(luaStr(QStringLiteral("bonk_%1_%2").arg(slug).arg(id)), luaStr(c.name));
        for (const SpawnItem &it : c.items) {
            if (it.header) {
                lua += QStringLiteral("\t\t{ type = \"header\", text = %1 },\n").arg(luaStr(it.text));
                continue;
            }
            if (it.wide != 64 || it.tall != 64)
                lua += QStringLiteral("\t\t{ type = \"model\", model = %1, wide = %2, tall = %3 },\n").arg(luaStr(it.model)).arg(it.wide).arg(it.tall);
            else lua += QStringLiteral("\t\t{ type = \"model\", model = %1 },\n").arg(luaStr(it.model));
            // the spawn icon: GMod's 64 × 64 one, plus one named for the tile's size
            const QString key = QStringLiteral("%1@%2x%3").arg(it.model).arg(it.wide).arg(it.tall);
            if (doneThumbs.contains(key) || it.source.isEmpty() || !render) continue;
            doneThumbs.insert(key);
            QString stem = it.model;
            if (stem.endsWith(QStringLiteral(".mdl"))) stem.chop(4);
            const QString src = QFileInfo(it.source).isRelative() ? pj.dir() + QLatin1Char('/') + it.source : it.source;
            const QImage big = render(src, it.cam, it.wide, it.tall);
            if (big.isNull()) {
                errors << QStringLiteral("couldn't draw %1").arg(it.model);
                continue;
            }
            const QString file = addon + QStringLiteral("/materials/spawnicons/") + stem;
            QDir().mkpath(QFileInfo(file).absolutePath());
            // a square tile's own picture scales down; a wide or tall one is framed again square
            const QImage base = it.square() ? big : render(src, fitCam(src, it.cam, 1.0), 64, 64);
            if (!base.isNull()) base.scaled(64, 64, Qt::IgnoreAspectRatio, Qt::SmoothTransformation).save(file + QStringLiteral(".png"));
            if (it.square() && it.wide != 64) big.save(file + QStringLiteral("_%1.png").arg(it.wide));
            else if (!it.square()) big.save(file + QStringLiteral("_%1x%2.png").arg(it.wide).arg(it.tall));
            ++thumbs;
        }
        lua += QStringLiteral("\t}, %1, %2, %3 )\n").arg(luaStr(icon)).arg(id).arg(parent);
        for (const SpawnCategory &k : c.children) walk(k, id, cslug + QStringLiteral("__"));
    };
    for (const SpawnCategory &c : sl.lists) walk(c, 0, QString());
    lua += QStringLiteral("end )\n");
    QFile f(addon + QStringLiteral("/lua/autorun/client/bonk_spawnlist_%1.lua").arg(slug));
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) return QStringLiteral("!couldn't write %1").arg(f.fileName());
    f.write(lua.toUtf8());
    QString sum = QStringLiteral("Spawnlist written: %1 categor%2, %3 icon%4, %5 spawn icon%6.")
                      .arg(next - base).arg(next - base == 1 ? QStringLiteral("y") : QStringLiteral("ies"))
                      .arg(icons).arg(icons == 1 ? QString() : QStringLiteral("s"))
                      .arg(thumbs).arg(thumbs == 1 ? QString() : QStringLiteral("s"));
    if (!errors.isEmpty()) sum += QStringLiteral(" Problems: ") + errors.join(QStringLiteral("; "));
    return sum;
}

/// The project's models: (model path "models/….mdl", .bpmdl relative to the project).
QList<QPair<QString, QString>> projectModels(const Project &pj) {
    QList<QPair<QString, QString>> out;
    QDirIterator it(pj.modelsDir(), {QStringLiteral("*.bpmdl")}, QDir::Files, QDirIterator::Subdirectories);
    static const QRegularExpression mp(QStringLiteral("\"model_path\"\\s+\"([^\"]*)\""));
    while (it.hasNext()) {
        const QString f = it.next();
        QFile file(f);
        QString path;
        if (file.open(QIODevice::ReadOnly | QIODevice::Text)) path = mp.match(QString::fromUtf8(file.read(4096))).captured(1);
        if (path.isEmpty()) path = pj.defaultModelPath(QFileInfo(f).completeBaseName());
        out << qMakePair(QStringLiteral("models/") + path + QStringLiteral(".mdl"), QDir(pj.dir()).relativeFilePath(f));
    }
    std::sort(out.begin(), out.end());
    return out;
}

// ------------------------------------------------------------------ editor window

/// The spawn icon preview: drag to turn, right-drag (or Shift+drag) to move,
/// wheel to zoom. It renders through the main window's GL.
class ThumbView : public QLabel {
public:
    std::function<void()> moved;
    ThumbCam *cam = nullptr;
    QSize frame{256, 256}; // the picture's size inside the view

    ThumbView() {
        setFixedSize(256, 256);
        setAlignment(Qt::AlignCenter);
        setCursor(Qt::OpenHandCursor);
        setToolTip(QStringLiteral("Drag: turn · Right-drag or Shift+drag: move · Wheel: zoom"));
    }

protected:
    void paintEvent(QPaintEvent *e) override {
        // a checkerboard where the picture is shows what's transparent
        QPainter p(this);
        p.fillRect(rect(), palette().window());
        const QRect fr(QPoint((width() - frame.width()) / 2, (height() - frame.height()) / 2), frame);
        const int t = 12;
        p.setClipRect(fr);
        for (int y = fr.top(); y < fr.bottom(); y += t)
            for (int x = fr.left(); x < fr.right(); x += t)
                p.fillRect(x, y, t, t, (((x - fr.left()) + (y - fr.top())) / t) % 2 ? QColor(70, 70, 76) : QColor(90, 90, 98));
        p.end();
        QLabel::paintEvent(e);
    }
    void mousePressEvent(QMouseEvent *e) override { last = e->position(); }
    void mouseMoveEvent(QMouseEvent *e) override {
        if (!cam || !(e->buttons() & (Qt::LeftButton | Qt::RightButton | Qt::MiddleButton))) return;
        const QPointF d = e->position() - last;
        last = e->position();
        const bool pan = (e->buttons() & (Qt::RightButton | Qt::MiddleButton)) || (e->modifiers() & Qt::ShiftModifier);
        if (pan) {
            // move the target across the view plane
            const double y = qDegreesToRadians(cam->yaw), pt = qDegreesToRadians(cam->pitch);
            const double k = cam->dist * std::tan(qDegreesToRadians(cam->fov) * 0.5) * 2.0 / std::max(1, frame.height());
            const double rx = -std::sin(y), ry = std::cos(y);                                     // right
            const double ux = -std::cos(y) * std::sin(pt), uy = -std::sin(y) * std::sin(pt), uz = std::cos(pt); // up
            cam->tx += (-d.x() * rx + d.y() * ux) * k;
            cam->ty += (-d.x() * ry + d.y() * uy) * k;
            cam->tz += d.y() * uz * k;
        } else {
            cam->yaw -= d.x() * 0.5;
            cam->pitch = std::clamp(cam->pitch + d.y() * 0.5, -89.0, 89.0);
        }
        if (moved) moved();
    }
    void wheelEvent(QWheelEvent *e) override {
        if (!cam) return;
        cam->dist *= std::pow(0.9, e->angleDelta().y() / 120.0);
        cam->dist = std::max(1.0, cam->dist);
        if (moved) moved();
    }

private:
    QPointF last;
};

/// The category's page, laid out like GMod's spawn menu: tiles flowing left to
/// right, headers on lines of their own. Tiles are drawn at 3/4 of their size
/// in the spawn menu (smaller still past 256).
/// What a dragged set of page entries carries: their rows on the page.
const QString kSpawnRowsMime = QStringLiteral("application/x-bonk-spawnlist-rows");

QList<int> rowsFromMime(const QMimeData *m) {
    QList<int> rows;
    for (const QString &r : QString::fromLatin1(m->data(kSpawnRowsMime)).split(QLatin1Char(','), Qt::SkipEmptyParts)) rows << r.toInt();
    return rows;
}

class SpawnPage : public QListWidget {
public:
    static constexpr int HeaderRole = Qt::UserRole + 1, SizeRole = Qt::UserRole + 2;
    /// Rows dropped back on the page before `index` (Ctrl = copies).
    std::function<void(const QList<int> &rows, int index, bool copy)> dropped;

    SpawnPage() {
        setViewMode(QListView::IconMode);
        setFlow(QListView::LeftToRight);
        setWrapping(true);
        setResizeMode(QListView::Adjust);
        setMovement(QListView::Static);
        setSpacing(3);
        setUniformItemSizes(false);
        setSelectionMode(QAbstractItemView::ExtendedSelection);
        setDragEnabled(true);
        setAcceptDrops(true);
        viewport()->setAcceptDrops(true);
        setDragDropMode(QAbstractItemView::DragDrop);
        setDefaultDropAction(Qt::MoveAction);
        setItemDelegate(new Delegate(this));
        setStyleSheet(QStringLiteral("QListWidget { background: #3b3b40; }"));
    }

    static QSize tile(QSize s) {
        const double k = std::min(0.75, 256.0 / std::max(s.width(), s.height()));
        return QSize(std::max(12, int(s.width() * k)), std::max(12, int(s.height() * k)));
    }

    /// Headers span the width (that's what puts them on their own line).
    void fitHeaders() {
        const int w = std::max(60, viewport()->width() - 2 * spacing() - 2);
        bool any = false;
        for (int i = 0; i < count(); ++i)
            if (item(i)->data(HeaderRole).toBool() && item(i)->sizeHint().width() != w) {
                item(i)->setSizeHint(QSize(w, 30));
                any = true;
            }
        if (any) doItemsLayout();
    }

    /// Where a drop at `pos` would go: the row it would be inserted before.
    int insertionAt(QPoint pos) const {
        int at = 0;
        for (int i = 0; i < count(); ++i) {
            const QRect r = visualItemRect(item(i));
            if (r.contains(pos)) {
                const bool header = item(i)->data(HeaderRole).toBool();
                const bool after = header ? pos.y() > r.center().y() : pos.x() > r.center().x();
                return after ? i + 1 : i;
            }
            // reading order: rows above, or earlier on the same row
            if (r.bottom() < pos.y() || (r.top() <= pos.y() && r.right() < pos.x())) at = i + 1;
        }
        return at;
    }

protected:
    void startDrag(Qt::DropActions) override {
        QList<int> rows;
        for (QListWidgetItem *it : selectedItems()) rows << row(it);
        if (rows.isEmpty()) return;
        std::sort(rows.begin(), rows.end());
        QStringList parts;
        for (int r : rows) parts << QString::number(r);
        auto *mime = new QMimeData;
        mime->setData(kSpawnRowsMime, parts.join(QLatin1Char(',')).toLatin1());
        auto *drag = new QDrag(this);
        drag->setMimeData(mime);
        // the picture: the first tile (or the header's text), with a count for more
        QListWidgetItem *first = item(rows.first());
        QPixmap pm;
        if (first->data(HeaderRole).toBool()) {
            pm = QPixmap(QSize(160, 26) * devicePixelRatioF());
            pm.setDevicePixelRatio(devicePixelRatioF());
            pm.fill(QColor(59, 59, 64, 220));
            QPainter p(&pm);
            p.setPen(Qt::white);
            QFont f = font();
            f.setBold(true);
            p.setFont(f);
            p.drawText(QRect(6, 0, 154, 26), Qt::AlignVCenter, first->text());
        } else {
            pm = first->icon().pixmap(tile(first->data(SizeRole).toSize()));
        }
        if (rows.size() > 1) {
            QPainter p(&pm);
            p.setRenderHint(QPainter::Antialiasing);
            p.setBrush(palette().highlight());
            p.setPen(Qt::NoPen);
            const QRectF b(pm.width() / pm.devicePixelRatio() - 24, 2, 22, 18);
            p.drawRoundedRect(b, 9, 9);
            p.setPen(Qt::white);
            p.drawText(b, Qt::AlignCenter, QString::number(rows.size()));
        }
        drag->setPixmap(pm);
        drag->setHotSpot(QPoint(12, 12));
        drag->exec(Qt::MoveAction | Qt::CopyAction, Qt::MoveAction);
        dropLine = -1;
        viewport()->update();
    }
    void dragEnterEvent(QDragEnterEvent *e) override {
        if (e->source() == this && e->mimeData()->hasFormat(kSpawnRowsMime)) e->acceptProposedAction();
        else e->ignore();
    }
    void dragMoveEvent(QDragMoveEvent *e) override {
        if (e->source() != this || !e->mimeData()->hasFormat(kSpawnRowsMime)) return e->ignore();
        e->setDropAction((e->modifiers() & Qt::ControlModifier) ? Qt::CopyAction : Qt::MoveAction);
        e->accept();
        dropLine = insertionAt(e->position().toPoint());
        viewport()->update();
    }
    void dragLeaveEvent(QDragLeaveEvent *e) override {
        dropLine = -1;
        viewport()->update();
        QListWidget::dragLeaveEvent(e);
    }
    void dropEvent(QDropEvent *e) override {
        dropLine = -1;
        viewport()->update();
        if (e->source() != this || !e->mimeData()->hasFormat(kSpawnRowsMime)) return e->ignore();
        const bool copy = e->modifiers() & Qt::ControlModifier;
        const int at = insertionAt(e->position().toPoint());
        const QList<int> rows = rowsFromMime(e->mimeData());
        // the page is rebuilt by the callback: Qt must not remove the rows itself
        e->setDropAction(Qt::CopyAction);
        e->accept();
        if (dropped) QTimer::singleShot(0, this, [this, rows, at, copy] { dropped(rows, at, copy); });
    }
    void paintEvent(QPaintEvent *e) override {
        QListWidget::paintEvent(e);
        if (dropLine < 0) return;
        // a bar where the drop goes: before the tile at dropLine, or after the last one
        QRect r;
        bool vertical = true;
        if (dropLine < count()) {
            r = visualItemRect(item(dropLine));
            if (item(dropLine)->data(HeaderRole).toBool()) vertical = false;
        } else if (count() > 0) {
            r = visualItemRect(item(count() - 1));
            vertical = !item(count() - 1)->data(HeaderRole).toBool();
            r.moveLeft(vertical ? r.right() + 4 : r.left());
            if (!vertical) r.moveTop(r.bottom() + 4);
        }
        QPainter p(viewport());
        p.setPen(Qt::NoPen);
        p.setBrush(palette().highlight());
        if (vertical) p.drawRoundedRect(QRect(r.left() - 3, r.top(), 3, r.height()), 1, 1);
        else p.drawRoundedRect(QRect(r.left(), r.top() - 3, r.width(), 3), 1, 1);
    }
    void resizeEvent(QResizeEvent *e) override {
        QListWidget::resizeEvent(e);
        QTimer::singleShot(0, this, [this] { fitHeaders(); }); // once the viewport has its new size
    }
    void showEvent(QShowEvent *e) override {
        QListWidget::showEvent(e);
        QTimer::singleShot(0, this, [this] { fitHeaders(); });
    }

private:
    int dropLine = -1;

    class Delegate : public QStyledItemDelegate {
    public:
        using QStyledItemDelegate::QStyledItemDelegate;
        QSize sizeHint(const QStyleOptionViewItem &o, const QModelIndex &i) const override {
            if (i.data(HeaderRole).toBool()) {
                const QSize h = i.data(Qt::SizeHintRole).toSize();
                return h.isValid() ? h : QSize(200, 30);
            }
            return tile(i.data(SizeRole).toSize());
        }
        void paint(QPainter *p, const QStyleOptionViewItem &o, const QModelIndex &i) const override {
            p->save();
            const QRect r = o.rect;
            const bool sel = o.state & QStyle::State_Selected;
            if (i.data(HeaderRole).toBool()) {
                QFont f = o.font;
                f.setBold(true);
                f = uiScaled(f, 1.5);
                p->setFont(f);
                p->setPen(sel ? o.palette.highlight().color().lighter(150) : QColor(235, 235, 240));
                p->drawText(r.adjusted(6, 0, -6, -4), Qt::AlignLeft | Qt::AlignBottom, i.data(Qt::DisplayRole).toString());
                p->setPen(QColor(255, 255, 255, 60));
                p->drawLine(r.left() + 4, r.bottom(), r.right() - 4, r.bottom());
            } else {
                // GMod's tile: a soft panel, the icon over it
                p->setRenderHint(QPainter::Antialiasing);
                p->setPen(Qt::NoPen);
                p->setBrush(QColor(255, 255, 255, 28));
                p->drawRoundedRect(QRectF(r).adjusted(0.5, 0.5, -0.5, -0.5), 4, 4);
                const QIcon icon = i.data(Qt::DecorationRole).value<QIcon>();
                icon.paint(p, r.adjusted(2, 2, -2, -2), Qt::AlignCenter);
                if (sel) {
                    p->setBrush(Qt::NoBrush);
                    p->setPen(QPen(o.palette.highlight().color(), 2.5));
                    p->drawRoundedRect(QRectF(r).adjusted(1.5, 1.5, -1.5, -1.5), 4, 4);
                }
            }
            p->restore();
        }
    };
};

/// The category tree: categories drag about inside it, and page entries
/// dropped on a category move there (Ctrl copies).
class CategoryTree : public QTreeWidget {
public:
    std::function<void(const QList<int> &rows, QTreeWidgetItem *to, bool copy)> itemsDropped;
    /// After a category was dragged to a new place in the tree.
    std::function<void()> categoriesMoved;

protected:
    void dragEnterEvent(QDragEnterEvent *e) override {
        if (e->mimeData()->hasFormat(kSpawnRowsMime)) e->acceptProposedAction();
        else QTreeWidget::dragEnterEvent(e);
    }
    void dragMoveEvent(QDragMoveEvent *e) override {
        if (!e->mimeData()->hasFormat(kSpawnRowsMime)) return QTreeWidget::dragMoveEvent(e);
        QTreeWidgetItem *it = itemAt(e->position().toPoint());
        setHover(it);
        if (!it) return e->ignore();
        e->setDropAction((e->modifiers() & Qt::ControlModifier) ? Qt::CopyAction : Qt::MoveAction);
        e->accept();
    }
    void dragLeaveEvent(QDragLeaveEvent *e) override {
        setHover(nullptr);
        QTreeWidget::dragLeaveEvent(e);
    }
    void dropEvent(QDropEvent *e) override {
        if (!e->mimeData()->hasFormat(kSpawnRowsMime)) {
            QTreeWidget::dropEvent(e);
            if (categoriesMoved) QTimer::singleShot(0, this, [this] { categoriesMoved(); });
            return;
        }
        QTreeWidgetItem *it = itemAt(e->position().toPoint());
        setHover(nullptr);
        if (!it) return e->ignore();
        const bool copy = e->modifiers() & Qt::ControlModifier;
        const QList<int> rows = rowsFromMime(e->mimeData());
        e->setDropAction(Qt::CopyAction); // the dialog moves the data itself
        e->accept();
        if (itemsDropped) QTimer::singleShot(0, this, [this, rows, it, copy] { itemsDropped(rows, it, copy); });
    }

private:
    QTreeWidgetItem *hover = nullptr;
    void setHover(QTreeWidgetItem *it) {
        if (it == hover) return;
        if (hover) hover->setBackground(0, QBrush());
        hover = it;
        if (hover) hover->setBackground(0, palette().highlight().color().lighter(160));
    }
};

class SpawnlistDialog : public QDialog {
public:
    SpawnlistDialog(QWidget *parent, const Project &pj, ThumbRenderer render, std::function<void(const QString &)> wrote)
        : QDialog(parent), pj(pj), render(std::move(render)), wrote(std::move(wrote)) {
        setWindowTitle(QStringLiteral("Spawnlist Editor — %1").arg(pj.name));
        if (!data.load(pj) || data.lists.isEmpty()) {
            SpawnCategory root;
            root.name = pj.name;
            root.icon.base = QStringLiteral("shape:house");
            data.lists << root;
        }
        auto *root = new QHBoxLayout(this);

        // --- categories
        auto *left = new QVBoxLayout;
        left->addWidget(new QLabel(QStringLiteral("<b>Categories</b>")));
        tree = new CategoryTree;
        tree->setHeaderHidden(true);
        tree->setIconSize(QSize(16, 16));
        tree->setDragDropMode(QAbstractItemView::InternalMove);
        tree->setToolTip(QStringLiteral("Drag to reorder or nest. Double-click to rename.\nDrop tiles from the page on a category to move them there (Ctrl copies)."));
        left->addWidget(tree, 1);
        auto *cb = new QGridLayout;
        auto *addCat = new QPushButton(QStringLiteral("+ Spawnlist"));
        addCat->setToolTip(QStringLiteral("Another top-level spawnlist (most projects need just one)"));
        auto *addSub = new QPushButton(QStringLiteral("+ Subcategory"));
        auto *delCat = new QPushButton(QStringLiteral("Delete"));
        cb->addWidget(addSub, 0, 0);
        cb->addWidget(addCat, 0, 1);
        cb->addWidget(delCat, 1, 0, 1, 2);
        left->addLayout(cb);
        root->addLayout(left, 2);

        // --- the page
        auto *mid = new QVBoxLayout;
        pageTitle = new QLabel;
        mid->addWidget(pageTitle);
        page = new SpawnPage;
        page->setToolTip(QStringLiteral("As in the spawn menu. Drag to rearrange, or onto a category on the left to move there (Ctrl copies).\n"
                                        "Ctrl+←/→ (or the arrows below) also move the selection; Delete removes."));
        page->dropped = [this](const QList<int> &rows, int at, bool copy) { dropOnPage(rows, at, copy); };
        tree->itemsDropped = [this](const QList<int> &rows, QTreeWidgetItem *to, bool copy) { dropOnCategory(rows, to, copy); };
        mid->addWidget(page, 1);
        auto *pb = new QHBoxLayout;
        auto *earlier = new QPushButton(QStringLiteral("◀"));
        earlier->setToolTip(QStringLiteral("Move earlier (Ctrl+←)"));
        auto *later = new QPushButton(QStringLiteral("▶"));
        later->setToolTip(QStringLiteral("Move later (Ctrl+→)"));
        for (QPushButton *b : {earlier, later}) b->setFixedWidth(34);
        pb->addWidget(earlier);
        pb->addWidget(later);
        auto *addModels = new QPushButton(QStringLiteral("Add Models…"));
        auto *addHeader = new QPushButton(QStringLiteral("Add Header"));
        auto *removeIt = new QPushButton(QStringLiteral("Remove"));
        pb->addWidget(addModels);
        pb->addWidget(addHeader);
        pb->addWidget(removeIt);
        mid->addLayout(pb);
        root->addLayout(mid, 3);

        // --- the inspector (category icon / header / spawn icon)
        auto *right = new QVBoxLayout;
        stack = new QStackedWidget;
        stack->addWidget(buildIconPane());
        stack->addWidget(buildHeaderPane());
        stack->addWidget(buildThumbPane());
        right->addWidget(stack, 1);
        auto *bb = new QHBoxLayout;
        auto *silk = new QPushButton(QStringLiteral("Import Silk Icons…"));
        silk->setToolTip(QStringLiteral("GMod's icon16 set (famfamfam Silk, CC BY 2.5). Point at the folder you unzipped;\n"
                                        "Bonk keeps a copy in ~/.local/share/bonk/silk so every project can use it."));
        auto *writeBtn = new QPushButton(QStringLiteral("Write to Addon"));
        writeBtn->setToolTip(QStringLiteral("Write the Lua, icons and spawn icons into the addon folder now\n(compiling and exporting do it too)"));
        auto *closeBtn = new QPushButton(QStringLiteral("Close"));
        bb->addWidget(silk);
        bb->addStretch(1);
        bb->addWidget(writeBtn);
        bb->addWidget(closeBtn);
        right->addLayout(bb);
        root->addLayout(right, 3);

        QObject::connect(tree, &QTreeWidget::currentItemChanged, this, [this] { showCategory(); });
        QObject::connect(tree, &QTreeWidget::itemChanged, this, [this](QTreeWidgetItem *it) {
            if (syncing) return;
            if (SpawnCategory *c = catOf(it)) c->name = it->text(0);
            changed();
        });
        // a category dragged in the tree: Qt moves the row as remove + insert (never "rowsMoved"),
        // so the tree tells us after its own drop, and the data follows the tree
        tree->categoriesMoved = [this] { rebuildFromTree(); };
        QObject::connect(earlier, &QPushButton::clicked, this, [this] { moveItems(-1); });
        QObject::connect(later, &QPushButton::clicked, this, [this] { moveItems(1); });
        QObject::connect(page, &QListWidget::currentRowChanged, this, [this] { showItem(); });
        QObject::connect(addCat, &QPushButton::clicked, this, [this] { addCategory(false); });
        QObject::connect(addSub, &QPushButton::clicked, this, [this] { addCategory(true); });
        QObject::connect(delCat, &QPushButton::clicked, this, [this] { deleteCategory(); });
        QObject::connect(addModels, &QPushButton::clicked, this, [this] { pickModels(); });
        QObject::connect(addHeader, &QPushButton::clicked, this, [this] {
            SpawnCategory *c = current();
            if (!c) return;
            SpawnItem h;
            h.header = true;
            h.text = QStringLiteral("New header");
            c->items << h;
            changed();
            showCategory();
            page->setCurrentRow(page->count() - 1);
        });
        QObject::connect(removeIt, &QPushButton::clicked, this, [this] { removeItems(); });
        QObject::connect(silk, &QPushButton::clicked, this, [this] { importSilk(); });
        QObject::connect(writeBtn, &QPushButton::clicked, this, [this] { writeNow(); });
        QObject::connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
        rebuildTree();
        // no default button: Enter in a text field (renaming, a header) mustn't press "+ Subcategory"
        for (QPushButton *b : findChildren<QPushButton *>()) {
            b->setAutoDefault(false);
            b->setDefault(false);
        }
        resize(1180, 700);
    }

    /// Developer aid: move the tree's item number `from` under item number `to` (-1 = top level) at `at`,
    /// the way a drag does, then let the dialog catch up.
    void devTreeMove(int from, int to, int at) {
        QList<QTreeWidgetItem *> all;
        for (QTreeWidgetItemIterator it(tree); *it; ++it) all << *it;
        QTreeWidgetItem *f = all.value(from), *t = to < 0 ? nullptr : all.value(to);
        if (!f) return;
        QTreeWidgetItem *taken = f->parent() ? f->parent()->takeChild(f->parent()->indexOfChild(f)) : tree->takeTopLevelItem(tree->indexOfTopLevelItem(f));
        if (t) t->insertChild(std::min(at, t->childCount()), taken);
        else tree->insertTopLevelItem(std::min(at, tree->topLevelItemCount()), taken);
        if (tree->categoriesMoved) tree->categoriesMoved();
    }

    /// Developer aid: "page:<rows>:<at>" or "cat:<rows>:<tree index>", rows like 0+2; a trailing ":c" copies.
    void devDrop(const QString &spec) {
        const QStringList a = spec.split(QLatin1Char(':'));
        QList<int> rows;
        for (const QString &r : a.value(1).split(QLatin1Char('+'), Qt::SkipEmptyParts)) rows << r.toInt();
        const bool copy = a.value(3) == QStringLiteral("c");
        if (a.value(0) == QStringLiteral("page")) dropOnPage(rows, a.value(2).toInt(), copy);
        else {
            QTreeWidgetItemIterator it(tree);
            for (int i = 0; *it && i < a.value(2).toInt(); ++it, ++i) {}
            if (*it) dropOnCategory(rows, *it, copy);
        }
    }

    /// Developer aid: open on a category / item.
    void select(int cat, int item) {
        QTreeWidgetItemIterator it(tree);
        for (int i = 0; *it && i < cat; ++it, ++i) {}
        if (*it) tree->setCurrentItem(*it);
        if (item >= 0) page->setCurrentRow(item);
    }

protected:
    void keyPressEvent(QKeyEvent *e) override {
        if (e->key() == Qt::Key_Delete && page->hasFocus()) {
            removeItems();
            return;
        }
        if ((e->modifiers() & Qt::ControlModifier) && (e->key() == Qt::Key_Left || e->key() == Qt::Key_Right) && page->hasFocus()) {
            moveItems(e->key() == Qt::Key_Left ? -1 : 1);
            return;
        }
        if (e->key() == Qt::Key_Return || e->key() == Qt::Key_Enter) return; // Enter finishes typing; it never presses a button
        QDialog::keyPressEvent(e);
    }

private:
    Project pj;
    ThumbRenderer render;
    std::function<void(const QString &)> wrote;
    Spawnlists data;
    CategoryTree *tree;
    SpawnPage *page;
    QLabel *pageTitle;
    QStackedWidget *stack;
    bool syncing = false;
    QHash<QString, QImage> thumbCache;
    // icon pane
    QComboBox *baseKind, *baseName, *subKind, *subName, *corner;
    QPushButton *tintBtn, *clearTint, *fileBtn, *subFileBtn;
    QLabel *iconBig, *iconReal;
    // header pane
    QLineEdit *headerText;
    // thumb pane
    ThumbView *thumb;
    QComboBox *sizeBox;
    QSpinBox *wideSpin, *tallSpin;
    QLabel *thumbInfo;
    QTimer thumbTimer;

    // ---- tree ↔ data

    void rebuildTree() {
        syncing = true;
        tree->clear();
        std::function<void(QList<SpawnCategory> &, QTreeWidgetItem *)> add = [&](QList<SpawnCategory> &cats, QTreeWidgetItem *parent) {
            for (SpawnCategory &c : cats) {
                auto *it = parent ? new QTreeWidgetItem(parent) : new QTreeWidgetItem(tree);
                it->setText(0, c.name);
                it->setIcon(0, QIcon(QPixmap::fromImage(renderIcon(c.icon, 32, pj.dir()))));
                it->setFlags(it->flags() | Qt::ItemIsEditable | Qt::ItemIsDragEnabled | Qt::ItemIsDropEnabled);
                it->setData(0, Qt::UserRole, QVariant::fromValue<quintptr>(reinterpret_cast<quintptr>(&c)));
                add(c.children, it);
            }
        };
        add(data.lists, nullptr);
        tree->expandAll();
        syncing = false;
        if (!tree->currentItem() && tree->topLevelItemCount()) tree->setCurrentItem(tree->topLevelItem(0));
    }

    SpawnCategory *catOf(QTreeWidgetItem *it) const {
        return it ? reinterpret_cast<SpawnCategory *>(it->data(0, Qt::UserRole).value<quintptr>()) : nullptr;
    }
    SpawnCategory *current() const { return catOf(tree->currentItem()); }

    /// After a drag in the tree: rebuild the data in the tree's new order.
    void rebuildFromTree() {
        std::function<SpawnCategory(QTreeWidgetItem *)> take = [&](QTreeWidgetItem *it) {
            SpawnCategory c = *catOf(it);
            c.children.clear();
            for (int i = 0; i < it->childCount(); ++i) c.children << take(it->child(i));
            return c;
        };
        QList<SpawnCategory> lists;
        for (int i = 0; i < tree->topLevelItemCount(); ++i) lists << take(tree->topLevelItem(i));
        data.lists = lists;
        changed();
        rebuildTree();
    }

    void addCategory(bool sub) {
        SpawnCategory c;
        c.name = sub ? QStringLiteral("New category") : QStringLiteral("New spawnlist");
        SpawnCategory *cur = current();
        if (sub && cur) cur->children << c;
        else data.lists << c;
        changed();
        rebuildTree();
    }

    void deleteCategory() {
        QTreeWidgetItem *it = tree->currentItem();
        if (!it) return;
        if (!it->parent() && tree->topLevelItemCount() <= 1) {
            QMessageBox::information(this, windowTitle(), QStringLiteral("A project keeps at least one spawnlist."));
            return;
        }
        if (QMessageBox::question(this, windowTitle(), QStringLiteral("Delete \"%1\" and what's in it?").arg(it->text(0))) != QMessageBox::Yes) return;
        delete it;
        rebuildFromTree();
    }

    // ---- the page

    QString displayName(const SpawnItem &it) const {
        return it.header ? it.text : QFileInfo(it.model).completeBaseName();
    }

    static QString tileTip(const SpawnItem &it) {
        return QStringLiteral("%1\n%2 × %3 in the spawn menu").arg(it.model).arg(it.wide).arg(it.tall);
    }

    QListWidgetItem *makeTile(const SpawnItem &it) {
        auto *li = new QListWidgetItem(displayName(it));
        li->setData(SpawnPage::HeaderRole, it.header);
        li->setData(SpawnPage::SizeRole, QSize(it.wide, it.tall));
        if (!it.header) {
            li->setIcon(QIcon(QPixmap::fromImage(thumbFor(it, SpawnPage::tile(QSize(it.wide, it.tall))))));
            li->setToolTip(tileTip(it));
        }
        return li;
    }

    void showCategory() {
        SpawnCategory *c = current();
        syncing = true;
        page->clear();
        if (c) {
            pageTitle->setText(QStringLiteral("<b>%1</b>").arg(c->name.toHtmlEscaped()));
            for (const SpawnItem &it : c->items) page->addItem(makeTile(it));
            page->fitHeaders();
        }
        syncing = false;
        stack->setCurrentIndex(0);
        loadIconPane();
    }

    /// Select these rows on the page (after a rebuild).
    void selectRows(const QList<int> &rows) {
        syncing = true;
        page->clearSelection();
        for (int r : rows)
            if (r >= 0 && r < page->count()) page->item(r)->setSelected(true);
        if (!rows.isEmpty()) page->setCurrentRow(rows.first(), QItemSelectionModel::Current);
        syncing = false;
        showItem();
    }

    /// Page entries dragged to before row `at` of the same page.
    void dropOnPage(QList<int> rows, int at, bool copy) {
        SpawnCategory *c = current();
        if (!c || rows.isEmpty()) return;
        std::sort(rows.begin(), rows.end());
        QList<SpawnItem> moving;
        for (int r : rows)
            if (r >= 0 && r < c->items.size()) moving << c->items[r];
        if (!copy) {
            for (int i = rows.size() - 1; i >= 0; --i) {
                if (rows[i] < 0 || rows[i] >= c->items.size()) continue;
                if (rows[i] < at) --at;
                c->items.removeAt(rows[i]);
            }
        }
        at = std::clamp(at, 0, int(c->items.size()));
        QList<int> placed;
        for (int i = 0; i < moving.size(); ++i) {
            c->items.insert(at + i, moving[i]);
            placed << at + i;
        }
        changed();
        showCategory();
        selectRows(placed);
    }

    /// Page entries dropped on a category in the tree: moved (or copied) to the end of its page.
    void dropOnCategory(QList<int> rows, QTreeWidgetItem *to, bool copy) {
        SpawnCategory *from = current(), *dest = catOf(to);
        if (!from || !dest || rows.isEmpty()) return;
        if (dest == from) {
            if (copy) dropOnPage(rows, from->items.size(), true);
            return;
        }
        std::sort(rows.begin(), rows.end());
        for (int r : rows)
            if (r >= 0 && r < from->items.size()) dest->items << from->items[r];
        if (!copy)
            for (int i = rows.size() - 1; i >= 0; --i)
                if (rows[i] >= 0 && rows[i] < from->items.size()) from->items.removeAt(rows[i]);
        changed();
        showCategory();
        pageTitle->setText(QStringLiteral("<b>%1</b> &nbsp;<span style='color:gray'>%2 %3 to %4</span>")
                               .arg(from->name.toHtmlEscaped(), copy ? QStringLiteral("Copied") : QStringLiteral("Moved"))
                               .arg(rows.size() == 1 ? QStringLiteral("1 entry") : QStringLiteral("%1 entries").arg(rows.size()))
                               .arg(dest->name.toHtmlEscaped()));
    }

    /// Move the selected entries one place earlier (-1) or later (+1).
    void moveItems(int dir) {
        SpawnCategory *c = current();
        if (!c) return;
        QList<int> rows;
        for (QListWidgetItem *it : page->selectedItems()) rows << page->row(it);
        if (rows.isEmpty()) return;
        std::sort(rows.begin(), rows.end());
        if ((dir < 0 && rows.first() == 0) || (dir > 0 && rows.last() >= c->items.size() - 1)) return;
        if (dir > 0) std::reverse(rows.begin(), rows.end());
        for (int r : rows) c->items.swapItemsAt(r, r + dir);
        changed();
        const int cur = page->currentRow() + dir;
        showCategory();
        syncing = true;
        for (int r : rows) page->item(r + dir)->setSelected(true);
        page->setCurrentRow(cur, QItemSelectionModel::Current);
        syncing = false;
        showItem();
    }

    void removeItems() {
        SpawnCategory *c = current();
        if (!c) return;
        QList<int> rows;
        for (QListWidgetItem *it : page->selectedItems()) rows << page->row(it);
        std::sort(rows.rbegin(), rows.rend());
        for (int r : rows) c->items.removeAt(r);
        changed();
        showCategory();
    }

    void pickModels() {
        SpawnCategory *c = current();
        if (!c) return;
        const auto models = projectModels(pj);
        if (models.isEmpty()) {
            QMessageBox::information(this, windowTitle(), QStringLiteral("This project has no models yet (Model Projects/*.bpmdl)."));
            return;
        }
        QDialog d(this);
        d.setWindowTitle(QStringLiteral("Add models"));
        auto *v = new QVBoxLayout(&d);
        auto *list = new QListWidget;
        QSet<QString> already;
        for (const SpawnItem &it : c->items) already.insert(it.model);
        for (const auto &m : models) {
            auto *it = new QListWidgetItem(m.first);
            it->setFlags(it->flags() | Qt::ItemIsUserCheckable);
            it->setCheckState(Qt::Unchecked);
            it->setData(Qt::UserRole, m.second);
            if (already.contains(m.first)) it->setText(m.first + QStringLiteral("   (already here)"));
            list->addItem(it);
        }
        v->addWidget(new QLabel(QStringLiteral("Tick the models to add to <b>%1</b>:").arg(c->name.toHtmlEscaped())));
        v->addWidget(list, 1);
        auto *all = new QCheckBox(QStringLiteral("All"));
        QObject::connect(all, &QCheckBox::toggled, list, [list](bool on) {
            for (int i = 0; i < list->count(); ++i) list->item(i)->setCheckState(on ? Qt::Checked : Qt::Unchecked);
        });
        v->addWidget(all);
        auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        QObject::connect(bb, &QDialogButtonBox::accepted, &d, &QDialog::accept);
        QObject::connect(bb, &QDialogButtonBox::rejected, &d, &QDialog::reject);
        v->addWidget(bb);
        d.resize(520, 480);
        if (d.exec() != QDialog::Accepted) return;
        for (int i = 0; i < list->count(); ++i) {
            QListWidgetItem *li = list->item(i);
            if (li->checkState() != Qt::Checked) continue;
            SpawnItem it;
            it.model = models[i].first;
            it.source = models[i].second;
            fit(it);
            c->items << it;
        }
        changed();
        showCategory();
    }

    // ---- inspector: icon

    QWidget *buildIconPane() {
        auto *w = new QWidget;
        auto *v = new QVBoxLayout(w);
        v->addWidget(new QLabel(QStringLiteral("<b>Category icon</b>")));
        auto *f = new QFormLayout;
        baseKind = new QComboBox;
        baseKind->addItems({QStringLiteral("Built-in shape"), QStringLiteral("Silk icon (icon16)"), QStringLiteral("Image file")});
        baseName = new QComboBox;
        baseName->setEditable(true);
        fileBtn = new QPushButton(QStringLiteral("Choose Image…"));
        tintBtn = new QPushButton(QStringLiteral("Tint…"));
        clearTint = new QPushButton(QStringLiteral("No Tint"));
        auto *tr = new QHBoxLayout;
        tr->addWidget(tintBtn);
        tr->addWidget(clearTint);
        subKind = new QComboBox;
        subKind->addItems({QStringLiteral("None"), QStringLiteral("Built-in shape"), QStringLiteral("Silk icon (icon16)"), QStringLiteral("Image file")});
        subName = new QComboBox;
        subName->setEditable(true);
        subFileBtn = new QPushButton(QStringLiteral("Choose Image…"));
        corner = new QComboBox;
        corner->addItems({QStringLiteral("Top left"), QStringLiteral("Top right"), QStringLiteral("Bottom left"), QStringLiteral("Bottom right")});
        f->addRow(QStringLiteral("Base"), baseKind);
        f->addRow(QString(), baseName);
        f->addRow(QString(), fileBtn);
        f->addRow(QStringLiteral("Color"), tr);
        f->addRow(QStringLiteral("Sub-icon"), subKind);
        f->addRow(QString(), subName);
        f->addRow(QString(), subFileBtn);
        f->addRow(QStringLiteral("Corner"), corner);
        v->addLayout(f);
        auto *pr = new QHBoxLayout;
        iconBig = new QLabel;
        iconBig->setFixedSize(128, 128);
        iconReal = new QLabel;
        iconReal->setFixedSize(16, 16);
        pr->addWidget(iconBig);
        auto *rv = new QVBoxLayout;
        rv->addWidget(new QLabel(QStringLiteral("In game:")));
        rv->addWidget(iconReal);
        rv->addStretch(1);
        pr->addLayout(rv);
        pr->addStretch(1);
        v->addLayout(pr);
        v->addWidget(hintLabel(QStringLiteral("The spawn menu shows it at 16 × 16. A plain Silk icon is used straight from GMod; anything "
                                              "tinted or with a sub-icon is written into the addon as a PNG.")));
        v->addStretch(1);
        auto names = [this](QComboBox *kind, QComboBox *name, bool sub) {
            const int k = kind->currentIndex() - (sub ? 1 : 0);
            name->blockSignals(true);
            name->clear();
            if (k == 0) for (const char *s : kIconShapes) name->addItem(QString::fromLatin1(s));
            else if (k == 1) name->addItems(silkNames());
            name->blockSignals(false);
            name->setVisible(k == 0 || k == 1);
        };
        QObject::connect(baseKind, &QComboBox::currentIndexChanged, this, [this, names] {
            if (syncing) return;
            names(baseKind, baseName, false);
            fileBtn->setVisible(baseKind->currentIndex() == 2);
            if (baseKind->currentIndex() == 1 && baseName->count() == 0)
                QMessageBox::information(this, windowTitle(), QStringLiteral("No Silk icons yet: use Import Silk Icons… first."));
            iconChanged();
        });
        QObject::connect(subKind, &QComboBox::currentIndexChanged, this, [this, names] {
            if (syncing) return;
            names(subKind, subName, true);
            subFileBtn->setVisible(subKind->currentIndex() == 3);
            corner->setEnabled(subKind->currentIndex() != 0);
            iconChanged();
        });
        for (QComboBox *c : {baseName, subName}) QObject::connect(c, &QComboBox::currentTextChanged, this, [this] { if (!syncing) iconChanged(); });
        QObject::connect(corner, &QComboBox::currentIndexChanged, this, [this] { if (!syncing) iconChanged(); });
        QObject::connect(fileBtn, &QPushButton::clicked, this, [this] { pickImage(false); });
        QObject::connect(subFileBtn, &QPushButton::clicked, this, [this] { pickImage(true); });
        QObject::connect(tintBtn, &QPushButton::clicked, this, [this] {
            SpawnCategory *c = current();
            if (!c) return;
            const QColor col = QColorDialog::getColor(c->icon.tint.isValid() ? c->icon.tint : QColor(232, 180, 70), this, QStringLiteral("Icon color"));
            if (!col.isValid()) return;
            c->icon.tint = col;
            refreshIcon();
        });
        QObject::connect(clearTint, &QPushButton::clicked, this, [this] {
            if (SpawnCategory *c = current()) {
                c->icon.tint = QColor();
                refreshIcon();
            }
        });
        return w;
    }

    void pickImage(bool sub) {
        SpawnCategory *c = current();
        if (!c) return;
        const QString f = QFileDialog::getOpenFileName(this, QStringLiteral("Icon image"), pj.dir(), QStringLiteral("Images (*.png *.jpg *.jpeg)"));
        if (f.isEmpty()) return;
        QString rel = QDir(pj.dir()).relativeFilePath(f);
        if (rel.startsWith(QStringLiteral(".."))) rel = f; // outside the project: keep the full path
        (sub ? c->icon.sub : c->icon.base) = QStringLiteral("file:") + rel;
        refreshIcon();
    }

    void loadIconPane() {
        SpawnCategory *c = current();
        if (!c) return;
        syncing = true;
        const IconSpec &s = c->icon;
        auto load = [&](const QString &spec, QComboBox *kind, QComboBox *name, QPushButton *file, bool sub) {
            int k = spec.startsWith(QStringLiteral("shape:")) ? 0 : spec.startsWith(QStringLiteral("silk:")) ? 1 : spec.startsWith(QStringLiteral("file:")) ? 2 : -1;
            kind->setCurrentIndex(sub ? k + 1 : std::max(0, k));
            name->clear();
            if (k == 0) for (const char *n : kIconShapes) name->addItem(QString::fromLatin1(n));
            else if (k == 1) name->addItems(silkNames());
            name->setCurrentText(spec.section(QLatin1Char(':'), 1));
            name->setVisible(k == 0 || k == 1);
            file->setVisible(k == 2);
        };
        load(s.base, baseKind, baseName, fileBtn, false);
        load(s.sub, subKind, subName, subFileBtn, true);
        corner->setCurrentIndex(s.corner);
        corner->setEnabled(!s.sub.isEmpty());
        syncing = false;
        refreshIcon(false);
    }

    void iconChanged() {
        SpawnCategory *c = current();
        if (!c) return;
        auto spec = [](QComboBox *kind, QComboBox *name, int offset, const QString &old) -> QString {
            const int k = kind->currentIndex() - offset;
            if (k == 0) return QStringLiteral("shape:") + name->currentText();
            if (k == 1) return name->currentText().isEmpty() ? old : QStringLiteral("silk:") + name->currentText();
            if (k == 2) return old.startsWith(QStringLiteral("file:")) ? old : QString();
            return QString();
        };
        c->icon.base = spec(baseKind, baseName, 0, c->icon.base);
        if (c->icon.base.isEmpty()) c->icon.base = QStringLiteral("shape:folder");
        c->icon.sub = spec(subKind, subName, 1, c->icon.sub);
        c->icon.corner = corner->currentIndex();
        refreshIcon();
    }

    void refreshIcon(bool save = true) {
        SpawnCategory *c = current();
        if (!c) return;
        QImage big = renderIcon(c->icon, 16, pj.dir()).scaled(128, 128, Qt::IgnoreAspectRatio, Qt::FastTransformation); // pixels, as in game
        iconBig->setPixmap(QPixmap::fromImage(big));
        iconReal->setPixmap(QPixmap::fromImage(renderIcon(c->icon, 16, pj.dir())));
        if (QTreeWidgetItem *it = tree->currentItem()) it->setIcon(0, QIcon(QPixmap::fromImage(renderIcon(c->icon, 32, pj.dir()))));
        if (save) changed();
    }

    // ---- inspector: header

    QWidget *buildHeaderPane() {
        auto *w = new QWidget;
        auto *v = new QVBoxLayout(w);
        v->addWidget(new QLabel(QStringLiteral("<b>Header</b>")));
        headerText = new QLineEdit;
        v->addWidget(headerText);
        v->addWidget(hintLabel(QStringLiteral("A line of big text across the page, like \"Dressers\" or \"Beds - Full\".")));
        v->addStretch(1);
        QObject::connect(headerText, &QLineEdit::textEdited, this, [this](const QString &t) {
            SpawnCategory *c = current();
            const int r = page->currentRow();
            if (!c || r < 0 || r >= c->items.size()) return;
            c->items[r].text = t;
            page->item(r)->setText(t);
            changed();
        });
        return w;
    }

    // ---- inspector: spawn icon

    QWidget *buildThumbPane() {
        auto *w = new QWidget;
        auto *v = new QVBoxLayout(w);
        v->addWidget(new QLabel(QStringLiteral("<b>Spawn icon</b>")));
        thumbInfo = new QLabel;
        thumbInfo->setWordWrap(true);
        v->addWidget(thumbInfo);
        thumb = new ThumbView;
        v->addWidget(thumb, 0, Qt::AlignHCenter);
        auto *r1 = new QHBoxLayout;
        auto *fitBtn = new QPushButton(QStringLiteral("Fit"));
        fitBtn->setToolTip(QStringLiteral("Frame the whole model, keeping the angle"));
        auto *resetBtn = new QPushButton(QStringLiteral("Reset Angle"));
        auto *allBtn = new QPushButton(QStringLiteral("Use for Whole Category"));
        allBtn->setToolTip(QStringLiteral("Every model in this category gets this angle (each one framed to fit)"));
        r1->addWidget(fitBtn);
        r1->addWidget(resetBtn);
        r1->addWidget(allBtn);
        v->addLayout(r1);
        auto *r2 = new QHBoxLayout;
        r2->addWidget(new QLabel(QStringLiteral("Tile size")));
        sizeBox = new QComboBox;
        sizeBox->setToolTip(QStringLiteral("The icon's size in the spawn menu, wide × tall"));
        for (QSize z : {QSize(64, 64), QSize(128, 64), QSize(64, 128), QSize(128, 128), QSize(256, 128), QSize(128, 256),
                        QSize(256, 256), QSize(512, 256), QSize(256, 512), QSize(512, 512)})
            sizeBox->addItem(QStringLiteral("%1 × %2").arg(z.width()).arg(z.height()), z);
        sizeBox->addItem(QStringLiteral("Custom"));
        wideSpin = new QSpinBox;
        tallSpin = new QSpinBox;
        for (QSpinBox *b : {wideSpin, tallSpin}) {
            b->setRange(16, 1024);
            b->setSingleStep(16);
            b->setSuffix(QStringLiteral(" px"));
        }
        wideSpin->setToolTip(QStringLiteral("Wide"));
        tallSpin->setToolTip(QStringLiteral("Tall"));
        r2->addWidget(sizeBox);
        r2->addWidget(wideSpin);
        r2->addWidget(new QLabel(QStringLiteral("×")));
        r2->addWidget(tallSpin);
        r2->addStretch(1);
        v->addLayout(r2);
        v->addWidget(hintLabel(QStringLiteral("Drag to turn the model, right-drag (or Shift+drag) to move it in the frame, wheel to zoom. "
                                              "The snapshot has a transparent background and is written as the model's spawn icon "
                                              "(materials/spawnicons/…) in the addon.")));
        v->addStretch(1);
        thumbTimer.setSingleShot(true);
        thumbTimer.setInterval(30);
        QObject::connect(&thumbTimer, &QTimer::timeout, this, [this] { drawThumb(); });
        thumb->moved = [this] {
            thumbTimer.start();
            changed();
        };
        QObject::connect(fitBtn, &QPushButton::clicked, this, [this] {
            if (SpawnItem *it = currentItem()) {
                fit(*it, true);
                drawThumb();
                changed();
            }
        });
        QObject::connect(resetBtn, &QPushButton::clicked, this, [this] {
            if (SpawnItem *it = currentItem()) {
                it->cam = ThumbCam();
                fit(*it);
                drawThumb();
                changed();
            }
        });
        QObject::connect(allBtn, &QPushButton::clicked, this, [this] {
            SpawnCategory *c = current();
            SpawnItem *src = currentItem();
            if (!c || !src) return;
            for (SpawnItem &it : c->items) {
                if (it.header || &it == src) continue;
                it.cam.yaw = src->cam.yaw;
                it.cam.pitch = src->cam.pitch;
                it.cam.fov = src->cam.fov;
                it.wide = src->wide;
                it.tall = src->tall;
                fit(it, true);
            }
            changed();
            const int r = page->currentRow();
            showCategory();
            page->setCurrentRow(r);
        });
        QObject::connect(sizeBox, &QComboBox::currentIndexChanged, this, [this] {
            const QSize z = sizeBox->currentData().toSize();
            if (syncing || !z.isValid()) return;
            setTileSize(z.width(), z.height());
        });
        for (QSpinBox *b : {wideSpin, tallSpin})
            QObject::connect(b, &QSpinBox::valueChanged, this, [this] {
                if (!syncing) setTileSize(wideSpin->value(), tallSpin->value());
            });
        return w;
    }

    /// Show the tile size in the preset box and the spin boxes.
    void showTileSize(const SpawnItem &it) {
        syncing = true;
        const int k = sizeBox->findData(QSize(it.wide, it.tall));
        sizeBox->setCurrentIndex(k >= 0 ? k : sizeBox->count() - 1);
        wideSpin->setValue(it.wide);
        tallSpin->setValue(it.tall);
        syncing = false;
    }

    void setTileSize(int w, int h) {
        SpawnItem *it = currentItem();
        if (!it) return;
        const bool reshaped = w * it->tall != h * it->wide;
        it->wide = w;
        it->tall = h;
        if (reshaped) fit(*it, true); // a new shape: frame it again from the same angle
        showTileSize(*it);
        const int r = page->currentRow();
        page->item(r)->setData(SpawnPage::SizeRole, QSize(w, h));
        page->item(r)->setToolTip(tileTip(*it));
        drawThumb();
        page->doItemsLayout();
        changed();
    }

    SpawnItem *currentItem() {
        SpawnCategory *c = current();
        const int r = page->currentRow();
        if (!c || r < 0 || r >= c->items.size()) return nullptr;
        return &c->items[r];
    }

    QString sourcePath(const SpawnItem &it) const {
        return QFileInfo(it.source).isRelative() ? pj.dir() + QLatin1Char('/') + it.source : it.source;
    }

    /// Frame the model snugly from the current angle.
    void fit(SpawnItem &it, bool keepAngle = false) {
        if (!keepAngle) it.cam = ThumbCam();
        it.cam = fitCam(sourcePath(it), it.cam, double(it.wide) / it.tall);
        for (const QString &k : thumbCache.keys())
            if (k.startsWith(it.model + QLatin1Char('@'))) thumbCache.remove(k);
    }

    QImage thumbFor(const SpawnItem &it, QSize size) {
        const QString key = QStringLiteral("%1@%2x%3").arg(it.model).arg(size.width()).arg(size.height());
        auto f = thumbCache.find(key);
        if (f != thumbCache.end()) return *f;
        QImage img = render ? render(sourcePath(it), it.cam, size.width(), size.height()) : QImage();
        if (img.isNull()) {
            img = QImage(size, QImage::Format_ARGB32_Premultiplied);
            img.fill(QColor(80, 80, 88));
        }
        thumbCache.insert(key, img);
        return img;
    }

    void showItem() {
        if (syncing) return;
        SpawnItem *it = currentItem();
        if (!it) {
            stack->setCurrentIndex(0);
            return;
        }
        if (it->header) {
            stack->setCurrentIndex(1);
            headerText->setText(it->text);
            return;
        }
        stack->setCurrentIndex(2);
        if (it->cam.dist <= 0) fit(*it);
        showTileSize(*it);
        thumbInfo->setText(QStringLiteral("<tt>%1</tt><br>from %2").arg(it->model.toHtmlEscaped(), it->source.toHtmlEscaped()));
        thumb->cam = &it->cam;
        drawThumb();
    }

    void drawThumb() {
        SpawnItem *it = currentItem();
        if (!it || it->header) return;
        // the preview: the tile's shape, its longer side 256
        const double k = 256.0 / std::max(it->wide, it->tall);
        const QSize ps(std::max(8, int(it->wide * k)), std::max(8, int(it->tall * k)));
        QImage img = render ? render(sourcePath(*it), it->cam, ps.width(), ps.height()) : QImage();
        thumb->frame = ps;
        thumb->setPixmap(QPixmap::fromImage(img));
        // the page's tile follows
        for (const QString &k : thumbCache.keys())
            if (k.startsWith(it->model + QLatin1Char('@'))) thumbCache.remove(k);
        if (QListWidgetItem *li = page->currentItem(); li && !img.isNull()) {
            li->setIcon(QIcon(QPixmap::fromImage(img.scaled(SpawnPage::tile(QSize(it->wide, it->tall)), Qt::KeepAspectRatio, Qt::SmoothTransformation))));
        }
    }

    // ---- silk, saving, writing

    void importSilk() {
        QString src = QFileDialog::getExistingDirectory(this, QStringLiteral("Folder with the Silk icons (the \"icons\" folder of famfamfam_silk_icons)"), QDir::homePath());
        if (src.isEmpty()) return;
        QDir().mkpath(silkDir());
        int n = 0;
        QDirIterator it(src, {QStringLiteral("*.png")}, QDir::Files, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString f = it.next();
            const QImage img(f);
            if (img.isNull() || img.width() > 32 || img.height() > 32) continue; // icons only
            const QString dest = silkDir() + QLatin1Char('/') + QFileInfo(f).fileName();
            QFile::remove(dest);
            n += QFile::copy(f, dest);
        }
        QMessageBox::information(this, windowTitle(),
                                 n ? QStringLiteral("Imported %1 Silk icons. Pick them under Base or Sub-icon → Silk icon. "
                                                    "(Silk is by Mark James, famfamfam.com, CC BY 2.5.)").arg(n)
                                   : QStringLiteral("No small PNG icons found in that folder."));
        loadIconPane();
    }

    void changed() {
        if (syncing) return;
        data.save(pj);
    }

    void writeNow() {
        data.save(pj);
        const QString r = writeSpawnlists(data, pj, render);
        if (wrote) wrote(r);
        if (!qEnvironmentVariableIsSet("BONK_ASSUME_YES"))
            QMessageBox::information(this, windowTitle(), r.startsWith(QLatin1Char('!')) ? r.mid(1) : (r.isEmpty() ? QStringLiteral("Nothing to write yet.") : r));
    }
};
