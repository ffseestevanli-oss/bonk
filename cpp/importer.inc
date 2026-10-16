// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Steventechno and the Bonk! contributors

// Importing from Source: material sources (folders and Workshop .gma files),
// the "where are the materials" dialog and the visgroup picker used by
// File → Import VMF… and File → Import Source Textures…
// Included into host.cpp inside its namespace.

/// Where imported materials come from: a folder (an addon, a mod's materials
/// folder) or a Workshop .gma. Paths are relative to materials/, lowercase,
/// with "/" separators, e.g. "brick/brickwall001a.vmt".
class MatSource {
public:
    QString path, error;
    bool isGma = false;

    bool open(const QString &p) {
        path = p;
        error.clear();
        index.clear();
        isGma = false;
        QFileInfo fi(p);
        if (fi.isFile() && p.endsWith(QStringLiteral(".gma"), Qt::CaseInsensitive)) return openGma(p);
        if (!fi.isDir()) {
            error = p.isEmpty() ? QString() : QStringLiteral("That isn't a folder or a .gma file.");
            return false;
        }
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
        QFile f(it->file);
        if (!f.open(QIODevice::ReadOnly)) return QByteArray();
        if (it->size < 0) return max < 0 ? f.readAll() : f.read(max);
        f.seek(it->offset);
        return f.read(max < 0 ? it->size : std::min(max, it->size));
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
    QString describe() const { return isGma ? QFileInfo(path).fileName() : tildePath(path); }

private:
    struct Entry {
        QString file;
        qint64 offset, size; // size < 0: a plain file
    };
    QHash<QString, Entry> index;

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

/// "Where are this model's materials?": a folder or .gma, and how much to bring in.
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
        pathEdit->setPlaceholderText(QStringLiteral("…/materials, an addon folder, or a .gma"));
        auto *folderBtn = new QPushButton(QStringLiteral("Folder…"));
        auto *gmaBtn = new QPushButton(QStringLiteral(".gma…"));
        row->addWidget(pathEdit, 1);
        row->addWidget(folderBtn);
        row->addWidget(gmaBtn);
        v->addLayout(row);
        status = new QLabel;
        status->setWordWrap(true);
        v->addWidget(status);
        onlyUsed = new QRadioButton;
        everything = new QRadioButton;
        v->addWidget(onlyUsed);
        v->addWidget(everything);
        v->addWidget(hintLabel(QStringLiteral("Textures go into the project's assets/Textures in the same folders as the originals, "
                                              "with each material's settings as its Material Properties.")));
        auto *bb = new QDialogButtonBox;
        importBtn = bb->addButton(QStringLiteral("Import Textures"), QDialogButtonBox::AcceptRole);
        bb->addButton(QStringLiteral("Skip"), QDialogButtonBox::RejectRole);
        v->addWidget(bb);
        QObject::connect(bb, &QDialogButtonBox::accepted, this, [this] {
            QSettings().setValue("import/lastSource", src.path);
            accept();
        });
        QObject::connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
        QObject::connect(pathEdit, &QLineEdit::editingFinished, this, [this] { check(); });
        QObject::connect(folderBtn, &QPushButton::clicked, this, [this] {
            const QString d = QFileDialog::getExistingDirectory(this, QStringLiteral("Materials folder (or the addon folder above it)"), startDir());
            if (!d.isEmpty()) { pathEdit->setText(d); check(); }
        });
        QObject::connect(gmaBtn, &QPushButton::clicked, this, [this] {
            const QString f = QFileDialog::getOpenFileName(this, QStringLiteral("Workshop addon"), startDir(), QStringLiteral("Garry's Mod addons (*.gma)"));
            if (!f.isEmpty()) { pathEdit->setText(f); check(); }
        });
        onlyUsed->setChecked(!used.isEmpty());
        everything->setChecked(used.isEmpty());
        QObject::connect(onlyUsed, &QRadioButton::toggled, this, [this] { syncButton(); });
        check();
        resize(gCompact ? 520 : 620, sizeHint().height());
    }

    bool importEverything() const { return everything->isChecked(); }

private:
    QStringList used;
    QLineEdit *pathEdit;
    QLabel *status;
    QRadioButton *onlyUsed, *everything;
    QPushButton *importBtn;

    QString startDir() const {
        const QString t = pathEdit->text();
        return t.isEmpty() ? QDir::homePath() : QFileInfo(t).isDir() ? t : QFileInfo(t).absolutePath();
    }

    void check() {
        const QString p = pathEdit->text().trimmed();
        const bool ok = src.open(p);
        int found = 0;
        QStringList miss;
        for (const QString &u : used) {
            if (src.has(u + QStringLiteral(".vmt"))) ++found;
            else miss << u;
        }
        if (p.isEmpty()) status->setText(QStringLiteral("Choose where the materials are."));
        else if (!ok) status->setText(QStringLiteral("<span style='color:#e0603a'>%1</span>").arg(src.error.toHtmlEscaped()));
        else if (used.isEmpty()) status->setText(QStringLiteral("✔ %1 material%2 in %3.").arg(src.vmtCount()).arg(src.vmtCount() == 1 ? QString() : QStringLiteral("s"), src.describe().toHtmlEscaped()));
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
        everything->setText(QStringLiteral("Everything in it (%1)").arg(ok ? src.vmtCount() : 0));
        onlyUsed->setEnabled(ok && !used.isEmpty());
        everything->setEnabled(ok);
        if (used.isEmpty()) everything->setChecked(true);
        lastOk = ok;
        lastFound = found;
        syncButton();
    }
    void syncButton() { importBtn->setEnabled(lastOk && (everything->isChecked() ? src.vmtCount() > 0 : lastFound > 0)); }
    bool lastOk = false;
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
