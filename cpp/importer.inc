// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Steventechno and the Bonk! contributors

// Importing from Source: material sources (folders, Workshop .gma files,
// .vpk archives and installed Source games), the "where are the materials"
// dialog, the material picker and the visgroup picker used by
// File → Import VMF… and File → Import Source Textures…
// Included into host.cpp inside its namespace.

/// The Source games installed through Steam: (name, game folder), Garry's Mod
/// first. Looked up once per run (again with refresh).
QList<QPair<QString, QString>> sourceGames(bool refresh = false) {
    static QList<QPair<QString, QString>> games;
    static bool looked = false;
    if (looked && !refresh) return games;
    looked = true;
    games.clear();
    SteamFind sf;
    sf.detect();
    QStringList libs = sf.libraries;
    // a GMod folder chosen in Settings, outside the libraries Steam lists
    if (!sf.gmodDir.isEmpty()) libs << QDir::cleanPath(sf.gmodDir + QStringLiteral("/../../.."));
    for (const QString &l : QString::fromUtf8(bonk_source_games(rustPath(libs.join(QLatin1Char('\n'))).constData())).split(QLatin1Char('\n'), Qt::SkipEmptyParts))
        if (l.contains(QLatin1Char('\t'))) games << qMakePair(l.section(QLatin1Char('\t'), 0, 0), l.section(QLatin1Char('\t'), 1));
    return games;
}

/// Where stock models picked from a game are copied (with their textures) so
/// their spawn icons can be drawn: one folder per game.
QString gameModelsRoot() { return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/bonk/game-models"); }
QString gameModelsDir(const QString &source) {
    QString n = QFileInfo(source).fileName().toLower();
    if (n.endsWith(QStringLiteral("_dir.vpk"))) n.chop(8);
    else if (n.endsWith(QStringLiteral(".vpk"))) n.chop(4);
    n.replace(QRegularExpression(QStringLiteral("[^a-z0-9_.-]+")), QStringLiteral("_"));
    return gameModelsRoot() + QLatin1Char('/') + (n.isEmpty() ? QStringLiteral("game") : n);
}
bool isStockModel(const QString &source) { return !source.isEmpty() && QDir::cleanPath(source).startsWith(gameModelsRoot() + QLatin1Char('/')); }

/// The game folder (the one with gameinfo.txt) for a folder that is one, or
/// holds one (a game's install folder: "Half-Life 2" → "Half-Life 2/hl2").
QString gameFolderIn(const QString &dir, QString *title = nullptr) {
    auto titleOf = [](const QString &d) { return QString::fromUtf8(bonk_game_title(rustPath(d).constData())); };
    QString t = titleOf(dir);
    QString found = t.isEmpty() ? QString() : dir;
    if (found.isEmpty() && QFileInfo(dir).isDir()) {
        QStringList subs = QDir(dir).entryList(QDir::Dirs | QDir::NoDotAndDotDot, QDir::Name);
        // the main game first when an install has several (hl2 before episodic, ep2…)
        std::stable_sort(subs.begin(), subs.end(), [](const QString &a, const QString &b) {
            auto main = [](const QString &n) { return n.compare(QStringLiteral("garrysmod"), Qt::CaseInsensitive) && n.compare(QStringLiteral("hl2"), Qt::CaseInsensitive); };
            return main(a) < main(b);
        });
        for (const QString &sub : subs) {
            if (sub.compare(QStringLiteral("materials"), Qt::CaseInsensitive) == 0) return QString(); // an addon or mod's content folder
            const QString d = dir + QLatin1Char('/') + sub;
            if (!(t = titleOf(d)).isEmpty()) {
                found = d;
                break;
            }
        }
    }
    if (title) *title = t;
    return found;
}

/// Where imported materials come from: a folder (an addon, a mod's materials
/// folder), a Workshop .gma, a .vpk, or a Source game (everything its
/// gameinfo.txt mounts). Paths are relative to materials/, lowercase, with "/"
/// separators, e.g. "brick/brickwall001a.vmt".
class MatSource {
public:
    QString path, error, title;
    bool isGma = false, isVpk = false, isGame = false;

    bool open(const QString &p) {
        path = p;
        error.clear();
        title.clear();
        index.clear();
        isGma = isVpk = isGame = false;
        QFileInfo fi(p);
        if (fi.isFile() && p.endsWith(QStringLiteral(".gma"), Qt::CaseInsensitive)) return openGma(p);
        if (fi.isFile() && p.endsWith(QStringLiteral(".vpk"), Qt::CaseInsensitive)) return openContent(p, false);
        if (!fi.isDir()) {
            error = p.isEmpty() ? QString() : QStringLiteral("That isn't a folder, a .gma or a .vpk file.");
            return false;
        }
        const QString game = gameFolderIn(p, &title);
        if (!game.isEmpty()) return openContent(game, true);
        QString root = p;
        for (const QString &d : QDir(p).entryList(QDir::Dirs | QDir::NoDotAndDotDot))
            if (d.compare(QStringLiteral("materials"), Qt::CaseInsensitive) == 0) root = QDir(p).absoluteFilePath(d);
        QDirIterator it(root, QDir::Files, QDirIterator::Subdirectories);
        const QDir rd(root);
        while (it.hasNext()) {
            const QString f = it.next();
            index.insert(rd.relativeFilePath(f).toLower(), Entry{f, 0, -1});
        }
        if (vmtCount() == 0) error = QStringLiteral("No materials (.vmt files) in that folder.");
        return error.isEmpty();
    }

    bool has(const QString &rel) const { return index.contains(rel.toLower()); }

    /// The first `max` bytes (all of it when max < 0).
    QByteArray read(const QString &rel, qint64 max = -1) const {
        auto it = index.constFind(rel.toLower());
        if (it == index.constEnd()) return QByteArray();
        QByteArray out = readPart(it->file, it->offset, it->size, max);
        if (!it->file2.isEmpty() && (max < 0 || out.size() < max)) out += readPart(it->file2, it->offset2, it->size2, max < 0 ? -1 : max - out.size());
        return out;
    }

    /// Every material in the source (without ".vmt").
    QStringList materials() const {
        QStringList out;
        for (auto it = index.constBegin(); it != index.constEnd(); ++it)
            if (it.key().endsWith(QStringLiteral(".vmt"))) out << it.key().chopped(4);
        out.sort();
        return out;
    }
    int vmtCount() const {
        int n = 0;
        for (auto it = index.constBegin(); it != index.constEnd(); ++it) n += it.key().endsWith(QStringLiteral(".vmt"));
        return n;
    }
    QString describe() const { return isGame ? title : (isGma || isVpk) ? QFileInfo(path).fileName() : tildePath(path); }

private:
    struct Entry {
        QString file;
        qint64 offset, size; // size < 0: a plain file
        QString file2;       // a VPK file split between its directory and an archive: the second piece
        qint64 offset2 = 0, size2 = 0;
    };
    QHash<QString, Entry> index;

    static QByteArray readPart(const QString &file, qint64 offset, qint64 size, qint64 max) {
        QFile f(file);
        if (!f.open(QIODevice::ReadOnly)) return QByteArray();
        if (size < 0) return max < 0 ? f.readAll() : f.read(max);
        f.seek(offset);
        return f.read(max < 0 ? size : std::min(max, size));
    }

    /// A .vpk, or a game folder: the Rust side indexes it (every VPK and loose
    /// folder the game mounts), this keeps where each material file is.
    bool openContent(const QString &p, bool game) {
        const char *list = bonk_content_list(rustPath(p).constData(), "materials/");
        if (!list) {
            error = QString::fromUtf8(bonk_last_error());
            return false;
        }
        const QString text = QString::fromUtf8(list);
        for (const QStringView line : QStringView(text).split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            const auto f = line.split(QLatin1Char('\t'));
            if (f.size() < 4) continue;
            Entry e{f[1].toString(), f[2].toLongLong(), f[3].toLongLong()};
            if (f.size() >= 7) {
                e.file2 = f[4].toString();
                e.offset2 = f[5].toLongLong();
                e.size2 = f[6].toLongLong();
            }
            index.insert(f[0].toString(), e);
        }
        path = p;
        isGame = game;
        isVpk = !game;
        if (vmtCount() == 0) error = game ? QStringLiteral("No materials in that game.") : QStringLiteral("No materials in that .vpk.");
        return error.isEmpty();
    }

    /// GMA: "GMAD", version, steam id, time, [required content], name,
    /// description, author, addon version, then the file table and the data.
    bool openGma(const QString &p) {
        QFile f(p);
        if (!f.open(QIODevice::ReadOnly)) {
            error = QStringLiteral("Couldn't open %1: %2").arg(QFileInfo(p).fileName(), f.errorString());
            return false;
        }
        auto bytes = [&](qint64 n) { return f.read(n); };
        auto cstr = [&]() {
            QByteArray s;
            char c;
            while (f.getChar(&c) && c != '\0') s += c;
            return s;
        };
        auto u32 = [&]() {
            const QByteArray b = bytes(4);
            if (b.size() < 4) return quint32(0);
            return quint32(uchar(b[0])) | quint32(uchar(b[1])) << 8 | quint32(uchar(b[2])) << 16 | quint32(uchar(b[3])) << 24;
        };
        auto u64 = [&]() { const quint64 lo = u32(); return lo | quint64(u32()) << 32; };
        if (bytes(4) != "GMAD") {
            error = QStringLiteral("That .gma isn't a Garry's Mod addon file.");
            return false;
        }
        char version = 0;
        f.getChar(&version);
        u64(); // steam id
        u64(); // timestamp
        if (version > 1)
            while (!cstr().isEmpty()) {} // required content
        cstr(); // name
        cstr(); // description
        cstr(); // author
        u32();  // addon version
        struct Item { QString name; qint64 size; };
        std::vector<Item> items;
        while (!f.atEnd()) {
            if (u32() == 0) break;
            const QString name = QString::fromUtf8(cstr());
            const qint64 size = qint64(u64());
            u32(); // crc
            items.push_back({name, size});
        }
        qint64 off = f.pos();
        for (const Item &it : items) {
            QString n = it.name.toLower();
            n.replace(QLatin1Char('\\'), QLatin1Char('/'));
            if (n.startsWith(QStringLiteral("materials/"))) index.insert(n.mid(10), Entry{p, off, it.size});
            off += it.size;
        }
        if (off > f.size()) {
            error = QStringLiteral("That .gma is cut short (an unfinished download?).");
            index.clear();
            return false;
        }
        isGma = true;
        if (vmtCount() == 0) error = QStringLiteral("No materials in that addon.");
        return error.isEmpty();
    }
};

/// A VMT, parsed: shader (lowercase) and parameters (lowercase keys). Patch
/// materials are resolved against the material they include.
struct SourceVmt {
    QString shader;
    QMap<QString, QString> params;
    bool ok = false;

    static SourceVmt parse(const QByteArray &text) {
        SourceVmt v;
        const QStringList lines = QString::fromUtf8(bonk_parse_vmt(text.constData())).split(QLatin1Char('\n'));
        if (lines.isEmpty() || lines.first().isEmpty()) return v;
        v.shader = lines.first();
        for (int i = 1; i < lines.size(); ++i)
            if (lines[i].contains(QLatin1Char('\t'))) v.params.insert(lines[i].section(QLatin1Char('\t'), 0, 0), lines[i].section(QLatin1Char('\t'), 1));
        v.ok = true;
        return v;
    }

    static SourceVmt load(const MatSource &src, const QString &key, int depth = 0) {
        SourceVmt v = parse(src.read(key + QStringLiteral(".vmt")));
        if (v.ok && v.shader == QStringLiteral("patch") && depth < 4) {
            const QString inc = clean(v.params.value(QStringLiteral("include")));
            SourceVmt base = load(src, inc, depth + 1);
            if (base.ok) {
                for (auto it = v.params.cbegin(); it != v.params.cend(); ++it)
                    if (it.key() != QStringLiteral("include")) base.params.insert(it.key(), it.value());
                return base;
            }
        }
        return v;
    }

    static QString clean(const QString &p) { return QString::fromUtf8(bonk_clean_texture_path(p.toUtf8().constData())); }

    /// Bonk's Material Properties for this material. Map-only parameters are
    /// dropped; anything Bonk has no control for goes into the extra lines.
    MatProps props() const {
        MatProps m;
        m.shader = shader == QStringLiteral("unlitgeneric") ? QStringLiteral("UnlitGeneric") : QStringLiteral("VertexLitGeneric");
        auto on = [&](const char *k) { return params.value(QString::fromLatin1(k)).trimmed() == QStringLiteral("1"); };
        auto num = [&](const char *k, double d) {
            const QString s = params.value(QString::fromLatin1(k));
            static const QRegularExpression n(QStringLiteral("-?[0-9]*\\.?[0-9]+"));
            QList<double> v;
            auto it = n.globalMatch(s);
            while (it.hasNext()) v << it.next().captured(0).toDouble();
            if (v.isEmpty()) return d;
            double sum = 0;
            for (double x : v) sum += x;
            return sum / v.size();
        };
        m.surfaceprop = params.value(QStringLiteral("$surfaceprop")).toLower();
        if (on("$translucent")) m.transparency = 2;
        else if (on("$alphatest")) {
            m.transparency = 1;
            m.alphaRef = num("$alphatestreference", 0.5);
        }
        m.nocull = on("$nocull");
        m.selfillum = on("$selfillum");
        m.envmap = !params.value(QStringLiteral("$envmap")).isEmpty();
        if (m.envmap) m.envTint = std::clamp(num("$envmaptint", 1.0), 0.0, 1.0);
        m.envMask = on("$basealphaenvmapmask") ? 1 : on("$normalmapalphaenvmapmask") ? 2 : 0;
        m.phong = on("$phong");
        if (m.phong) {
            m.phongBoost = num("$phongboost", 1.0);
            m.phongExp = num("$phongexponent", 20.0);
        }
        static const QSet<QString> handled = {
            QStringLiteral("$basetexture"), QStringLiteral("$bumpmap"), QStringLiteral("$normalmap"), QStringLiteral("$surfaceprop"),
            QStringLiteral("$translucent"), QStringLiteral("$alphatest"), QStringLiteral("$alphatestreference"), QStringLiteral("$nocull"),
            QStringLiteral("$selfillum"), QStringLiteral("$envmap"), QStringLiteral("$envmaptint"), QStringLiteral("$basealphaenvmapmask"),
            QStringLiteral("$normalmapalphaenvmapmask"), QStringLiteral("$phong"), QStringLiteral("$phongboost"), QStringLiteral("$phongexponent"),
            QStringLiteral("$phongfresnelranges"), QStringLiteral("include"), QStringLiteral("$model"),
            // map-only: blend textures, lightmap and brush tricks that don't apply to models
            QStringLiteral("$basetexture2"), QStringLiteral("$bumpmap2"), QStringLiteral("$blendmodulatetexture"), QStringLiteral("$surfaceprop2"),
            QStringLiteral("$basetexturetransform2"), QStringLiteral("$seamless_scale"), QStringLiteral("$seamless_base"),
            QStringLiteral("$seamless_detail"), QStringLiteral("$ssbump"), QStringLiteral("$ssbumpmathfix"), QStringLiteral("$basetexturenoenvmap"),
            QStringLiteral("$basetexture2noenvmap"), QStringLiteral("$lightmapwaterfog"), QStringLiteral("$reflectivity"),
            QStringLiteral("$hdrbasetexture"), QStringLiteral("$hdrcompressedtexture"), QStringLiteral("$blendtintbybasealpha")};
        QStringList extra;
        for (auto it = params.cbegin(); it != params.cend(); ++it) {
            if (handled.contains(it.key()) || it.key().startsWith(QLatin1Char('%')) || !it.key().startsWith(QLatin1Char('$'))) continue;
            QString v = it.value();
            v.replace(QLatin1Char('"'), QLatin1Char('\''));
            extra << QStringLiteral("\"%1\" \"%2\"").arg(it.key(), v);
        }
        m.extra = extra.join(QLatin1Char('\n'));
        return m;
    }
};

/// A texture preview for a material in a source: its $basetexture, decoded.
/// Null when it has none or it can't be read.
QImage sourcePreview(const MatSource &src, const QString &key, QString *info = nullptr) {
    const SourceVmt vmt = SourceVmt::load(src, key);
    const QString base = vmt.ok ? SourceVmt::clean(vmt.params.value(QStringLiteral("$basetexture"))) : QString();
    if (info) *info = vmt.ok ? QStringLiteral("%1 material").arg(vmt.shader) : QStringLiteral("couldn't read its VMT");
    if (base.isEmpty()) return QImage();
    const QByteArray data = src.read(base + QStringLiteral(".vtf"));
    int w = 0, h = 0;
    const uchar *px = data.isEmpty() ? nullptr : bonk_vtf_decode(reinterpret_cast<const uchar *>(data.constData()), size_t(data.size()), &w, &h);
    if (!px) {
        if (info) *info += QStringLiteral(", no texture (%1.vtf)").arg(base);
        return QImage();
    }
    if (info) *info += QStringLiteral(", %1 × %2").arg(w).arg(h);
    return QImage(px, w, h, w * 4, QImage::Format_RGBA8888).copy();
}

/// Tick the materials to bring in from a big source (a whole game): a search
/// box, a folder filter and a preview of the one under the cursor.
class MaterialPickerDialog : public QDialog {
public:
    MaterialPickerDialog(QWidget *parent, const MatSource &src, const QStringList &already) : QDialog(parent), src(src) {
        setWindowTitle(QStringLiteral("Choose Materials: %1").arg(src.describe()));
        auto *v = new QVBoxLayout(this);
        auto *top = new QHBoxLayout;
        search = new QLineEdit;
        search->setPlaceholderText(QStringLiteral("Search, e.g. brick wall"));
        search->setClearButtonEnabled(true);
        folder = new QComboBox;
        top->addWidget(search, 1);
        top->addWidget(folder);
        v->addLayout(top);
        auto *mid = new QHBoxLayout;
        list = new QListWidget;
        list->setUniformItemSizes(true);
        mid->addWidget(list, 1);
        auto *side = new QVBoxLayout;
        preview = new QLabel;
        preview->setFixedSize(gCompact ? 160 : 224, gCompact ? 160 : 224);
        preview->setAlignment(Qt::AlignCenter);
        preview->setStyleSheet(QStringLiteral("background:#202024;border:1px solid #3a3a40;"));
        info = new QLabel;
        info->setWordWrap(true);
        info->setFixedWidth(preview->width());
        info->setTextInteractionFlags(Qt::TextSelectableByMouse);
        side->addWidget(preview);
        side->addWidget(info);
        side->addStretch(1);
        mid->addLayout(side);
        v->addLayout(mid, 1);
        auto *row = new QHBoxLayout;
        auto *tickShown = new QPushButton(QStringLiteral("Tick Shown"));
        auto *untick = new QPushButton(QStringLiteral("Untick All"));
        count = new QLabel;
        row->addWidget(tickShown);
        row->addWidget(untick);
        row->addWidget(count, 1);
        v->addLayout(row);
        auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        bb->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Use These"));
        v->addWidget(bb);

        // the list: every material but the tool textures (nodraw, clip…)
        const QSet<QString> was(already.begin(), already.end());
        QSet<QString> folders;
        list->setUpdatesEnabled(false);
        for (const QString &m : src.materials()) {
            if (m.startsWith(QStringLiteral("tools/"))) continue;
            auto *it = new QListWidgetItem(m, list);
            it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable);
            it->setCheckState(was.contains(m) ? Qt::Checked : Qt::Unchecked);
            folders.insert(m.contains(QLatin1Char('/')) ? m.section(QLatin1Char('/'), 0, 0) : QString());
        }
        list->setUpdatesEnabled(true);
        QStringList fl(folders.begin(), folders.end());
        fl.removeAll(QString());
        fl.sort();
        folder->addItem(QStringLiteral("All folders"), QString());
        for (const QString &f : fl) folder->addItem(f, f);
        chosen = was.size();

        QObject::connect(search, &QLineEdit::textChanged, this, [this] { filter(); });
        QObject::connect(folder, &QComboBox::currentIndexChanged, this, [this] { filter(); });
        QObject::connect(list, &QListWidget::currentItemChanged, this, [this](QListWidgetItem *it) { showPreview(it); });
        QObject::connect(list, &QListWidget::itemChanged, this, [this] { recount(); });
        QObject::connect(tickShown, &QPushButton::clicked, this, [this] { setAll(true); });
        QObject::connect(untick, &QPushButton::clicked, this, [this] { setAll(false); });
        QObject::connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
        QObject::connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
        filter();
        resize(gCompact ? 640 : 820, gCompact ? 480 : 600);
    }

    QStringList picked() const {
        QStringList out;
        for (int i = 0; i < list->count(); ++i)
            if (list->item(i)->checkState() == Qt::Checked) out << list->item(i)->text();
        return out;
    }

    /// Developer aid: type a search and tick what it shows.
    void devPick(const QString &text) {
        search->setText(text);
        setAll(true);
        if (list->count()) {
            for (int i = 0; i < list->count(); ++i)
                if (!list->item(i)->isHidden()) {
                    list->setCurrentRow(i);
                    break;
                }
        }
    }

private:
    const MatSource &src;
    QLineEdit *search;
    QComboBox *folder;
    QListWidget *list;
    QLabel *preview, *info, *count;
    int chosen = 0, shown = 0;

    void filter() {
        const QStringList words = search->text().toLower().split(QLatin1Char(' '), Qt::SkipEmptyParts);
        const QString f = folder->currentData().toString();
        shown = 0;
        list->setUpdatesEnabled(false);
        for (int i = 0; i < list->count(); ++i) {
            QListWidgetItem *it = list->item(i);
            const QString t = it->text();
            bool ok = f.isEmpty() || t.startsWith(f + QLatin1Char('/'));
            for (const QString &w : words)
                if (ok && !t.contains(w)) ok = false;
            it->setHidden(!ok);
            shown += ok;
        }
        list->setUpdatesEnabled(true);
        recount();
    }
    void setAll(bool on) {
        if (on && shown > 2000 &&
            QMessageBox::question(this, windowTitle(), QStringLiteral("Tick all %1 materials shown?").arg(QLocale().toString(shown))) != QMessageBox::Yes)
            return;
        list->blockSignals(true);
        for (int i = 0; i < list->count(); ++i)
            if (!on || !list->item(i)->isHidden()) list->item(i)->setCheckState(on ? Qt::Checked : Qt::Unchecked);
        list->blockSignals(false);
        recount();
    }
    void recount() {
        chosen = 0;
        for (int i = 0; i < list->count(); ++i) chosen += list->item(i)->checkState() == Qt::Checked;
        count->setText(QStringLiteral("%1 shown, <b>%2 ticked</b>").arg(QLocale().toString(shown), QLocale().toString(chosen)));
    }
    void showPreview(QListWidgetItem *it) {
        if (!it) {
            preview->clear();
            info->clear();
            return;
        }
        QString text;
        const QImage img = sourcePreview(src, it->text(), &text);
        if (img.isNull()) preview->setText(QStringLiteral("No preview"));
        else preview->setPixmap(QPixmap::fromImage(img.scaled(preview->size() - QSize(4, 4), Qt::KeepAspectRatio, Qt::SmoothTransformation)));
        info->setText(QStringLiteral("<b>%1</b><br>%2").arg(it->text().toHtmlEscaped(), text.toHtmlEscaped()));
    }
};

/// "Where are this model's materials?": a folder, .gma, .vpk or installed
/// Source game, and how much to bring in.
class MaterialSourceDialog : public QDialog {
public:
    MatSource src;

    MaterialSourceDialog(QWidget *parent, const QStringList &used, const QString &intro) : QDialog(parent), used(used) {
        setWindowTitle(QStringLiteral("Import Source Textures"));
        auto *v = new QVBoxLayout(this);
        auto *lead = new QLabel(intro);
        lead->setWordWrap(true);
        v->addWidget(lead);
        auto *row = new QHBoxLayout;
        pathEdit = new QLineEdit(QSettings().value("import/lastSource").toString());
        pathEdit->setPlaceholderText(QStringLiteral("A Steam game, …/materials, an addon folder, a .gma or a .vpk"));
        auto *gameBtn = new QToolButton;
        gameBtn->setText(QStringLiteral("Steam Game"));
        gameBtn->setToolTip(QStringLiteral("The Source games installed through Steam: their stock materials, read from the game's .vpk files"));
        gameBtn->setPopupMode(QToolButton::InstantPopup);
        auto *games = new QMenu(gameBtn);
        gameBtn->setMenu(games);
        auto *folderBtn = new QPushButton(QStringLiteral("Folder…"));
        auto *fileBtn = new QPushButton(QStringLiteral("File…"));
        fileBtn->setToolTip(QStringLiteral("A Workshop addon (.gma) or a Valve pak (.vpk; for a split one, any of its parts)"));
        row->addWidget(pathEdit, 1);
        row->addWidget(gameBtn);
        row->addWidget(folderBtn);
        row->addWidget(fileBtn);
        v->addLayout(row);
        status = new QLabel;
        status->setWordWrap(true);
        v->addWidget(status);
        onlyUsed = new QRadioButton;
        everything = new QRadioButton;
        choose = new QRadioButton;
        auto *chooseBtn = new QPushButton(QStringLiteral("Choose…"));
        v->addWidget(onlyUsed);
        v->addWidget(everything);
        auto *cr = new QHBoxLayout;
        cr->addWidget(choose);
        cr->addWidget(chooseBtn);
        cr->addStretch(1);
        v->addLayout(cr);
        v->addWidget(hintLabel(QStringLiteral("Textures go into the project's assets/Textures in the same folders as the originals, "
                                              "with each material's settings as its Material Properties. Games are only read, never changed.")));
        auto *bb = new QDialogButtonBox;
        importBtn = bb->addButton(QStringLiteral("Import Textures"), QDialogButtonBox::AcceptRole);
        bb->addButton(QStringLiteral("Skip"), QDialogButtonBox::RejectRole);
        v->addWidget(bb);
        QObject::connect(bb, &QDialogButtonBox::accepted, this, [this] {
            QSettings().setValue("import/lastSource", src.path);
            accept();
        });
        QObject::connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
        QObject::connect(pathEdit, &QLineEdit::editingFinished, this, [this] {
            if (pathEdit->text().trimmed() != src.path) check();
        });
        QObject::connect(games, &QMenu::aboutToShow, this, [this, games] {
            games->clear();
            const auto found = sourceGames();
            for (const auto &g : found)
                games->addAction(g.first, this, [this, g] {
                    pathEdit->setText(g.second);
                    check();
                })->setToolTip(tildePath(g.second));
            if (found.isEmpty()) games->addAction(QStringLiteral("No Source games found in Steam"))->setEnabled(false);
            games->addSeparator();
            games->addAction(QStringLiteral("Look Again"), this, [] { sourceGames(true); });
        });
        QObject::connect(folderBtn, &QPushButton::clicked, this, [this] {
            const QString d = QFileDialog::getExistingDirectory(this, QStringLiteral("Materials folder, the addon folder above it, or a game's folder"), startDir());
            if (!d.isEmpty()) { pathEdit->setText(d); check(); }
        });
        QObject::connect(fileBtn, &QPushButton::clicked, this, [this] {
            const QString f = QFileDialog::getOpenFileName(this, QStringLiteral("Workshop addon or Valve pak"), startDir(),
                                                           QStringLiteral("Addons and paks (*.gma *.vpk);;Garry's Mod addons (*.gma);;Valve paks (*.vpk)"));
            if (!f.isEmpty()) { pathEdit->setText(f); check(); }
        });
        QObject::connect(chooseBtn, &QPushButton::clicked, this, [this] { pickMaterials(); });
        onlyUsed->setChecked(!used.isEmpty());
        everything->setChecked(used.isEmpty());
        QObject::connect(onlyUsed, &QRadioButton::toggled, this, [this] { syncButton(); });
        QObject::connect(choose, &QRadioButton::toggled, this, [this](bool on) {
            if (on && picked.isEmpty() && isVisible() && !checking) pickMaterials();
            syncButton();
        });
        check();
        resize(gCompact ? 560 : 680, sizeHint().height());
    }

    bool importEverything() const { return everything->isChecked(); }
    /// The materials to bring in: the ones chosen by hand, or those the model uses.
    QStringList keys() const { return choose->isChecked() ? picked : used; }

    /// Developer aid: pick a source and tick the materials a search finds.
    void devChoose(const QString &path, const QString &search) {
        pathEdit->setText(path);
        check();
        MaterialPickerDialog d(this, src, picked);
        d.devPick(search);
        picked = d.picked();
        choose->setChecked(true);
        updateChooseText();
        syncButton();
    }

private:
    QStringList used, picked;
    QLineEdit *pathEdit;
    QLabel *status;
    QRadioButton *onlyUsed, *everything, *choose;
    QPushButton *importBtn;

    QString startDir() const {
        const QString t = pathEdit->text();
        return t.isEmpty() ? QDir::homePath() : QFileInfo(t).isDir() ? t : QFileInfo(t).absolutePath();
    }

    void pickMaterials() {
        if (!lastOk) return;
        MaterialPickerDialog d(this, src, picked);
        if (d.exec() == QDialog::Accepted) picked = d.picked();
        choose->setChecked(!picked.isEmpty() || choose->isChecked());
        updateChooseText();
        syncButton();
    }
    void updateChooseText() { choose->setText(QStringLiteral("Only the ones I choose (%1)").arg(picked.size())); }

    void check() {
        const QString p = pathEdit->text().trimmed();
        checking = true;
        struct Done { bool *b; ~Done() { *b = false; } } done{&checking};
        QApplication::setOverrideCursor(Qt::WaitCursor); // a whole game takes a moment
        const bool ok = src.open(p);
        QApplication::restoreOverrideCursor();
        if (ok && src.path != p) pathEdit->setText(src.path); // a game's install folder → its game folder
        picked.clear();
        int found = 0;
        QStringList miss;
        for (const QString &u : used) {
            if (src.has(u + QStringLiteral(".vmt"))) ++found;
            else miss << u;
        }
        if (p.isEmpty()) status->setText(QStringLiteral("Choose where the materials are."));
        else if (!ok) status->setText(QStringLiteral("<span style='color:#e0603a'>%1</span>").arg(src.error.toHtmlEscaped()));
        else if (used.isEmpty()) status->setText(QStringLiteral("✔ %1 material%2 in %3.").arg(QLocale().toString(src.vmtCount())).arg(src.vmtCount() == 1 ? QString() : QStringLiteral("s"), src.describe().toHtmlEscaped()));
        else {
            QString t = QStringLiteral("✔ Found <b>%1 of %2</b> materials this model uses.").arg(found).arg(used.size());
            if (!miss.isEmpty()) {
                QStringList shown = miss.mid(0, 6);
                t += QStringLiteral("<br><span style='color:gray'>Not in there: %1%2</span>").arg(shown.join(QStringLiteral(", ")).toHtmlEscaped(),
                                                                                                 miss.size() > 6 ? QStringLiteral(" and %1 more").arg(miss.size() - 6) : QString());
            }
            status->setText(t);
        }
        onlyUsed->setText(QStringLiteral("Only what this model uses (%1)").arg(ok ? found : 0));
        everything->setText(QStringLiteral("Everything in it (%1)").arg(ok ? QLocale().toString(src.vmtCount()) : QStringLiteral("0")));
        updateChooseText();
        onlyUsed->setEnabled(ok && !used.isEmpty());
        everything->setEnabled(ok);
        choose->setEnabled(ok);
        // a whole game is too much to take everything from by default
        if (used.isEmpty() || (!onlyUsed->isEnabled() && onlyUsed->isChecked())) (src.vmtCount() > 500 ? choose : everything)->setChecked(true);
        else if (choose->isChecked()) onlyUsed->setChecked(true);
        lastOk = ok;
        lastFound = found;
        syncButton();
    }
    void syncButton() {
        importBtn->setEnabled(lastOk && (everything->isChecked() ? src.vmtCount() > 0 : choose->isChecked() ? !picked.isEmpty() : lastFound > 0));
    }
    bool lastOk = false, checking = false;
    int lastFound = 0;
};

/// Pick which of a VMF's visgroups come in.
class VisgroupDialog : public QDialog {
public:
    VisgroupDialog(QWidget *parent, const QList<std::tuple<int, int, QString>> &groups, int total) : QDialog(parent) {
        setWindowTitle(QStringLiteral("Import VMF: Visgroups"));
        auto *v = new QVBoxLayout(this);
        auto *l = new QLabel(QStringLiteral("This map has %1 brush%2. Untick the visgroups you don't want; brushes in no visgroup always come in. "
                                            "Each visgroup becomes a layer.").arg(total).arg(total == 1 ? QString() : QStringLiteral("es")));
        l->setWordWrap(true);
        v->addWidget(l);
        list = new QListWidget;
        for (const auto &[id, n, name] : groups) {
            auto *it = new QListWidgetItem(QStringLiteral("%1   (%2 brush%3)").arg(name).arg(n).arg(n == 1 ? QString() : QStringLiteral("es")), list);
            it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
            it->setCheckState(Qt::Checked);
            it->setData(Qt::UserRole, id);
        }
        v->addWidget(list, 1);
        auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
        bb->button(QDialogButtonBox::Ok)->setText(QStringLiteral("Import"));
        v->addWidget(bb);
        QObject::connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
        QObject::connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
        resize(gCompact ? 420 : 480, gCompact ? 360 : 420);
    }
    QString skipped() const {
        QStringList s;
        for (int i = 0; i < list->count(); ++i)
            if (list->item(i)->checkState() != Qt::Checked) s << QString::number(list->item(i)->data(Qt::UserRole).toInt());
        return s.join(QLatin1Char(','));
    }

private:
    QListWidget *list;
};
