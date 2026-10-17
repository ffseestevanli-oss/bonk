// SPDX-License-Identifier: GPL-3.0-or-later
// Prefabs: saved sets of brushes (textures and all) to drop into any model.
// Shared ones live in ~/.local/share/bonk/prefabs, a project's own in
// <project>/Prefabs; each is <folder>/<name>.bpfab with a .png thumbnail,
// a .json (its height above the floor) and, for shared ones, a .textures
// folder with copies of the textures it uses.
//
// Included by host.cpp (inside its anonymous namespace).

const QString kPrefabMime = QStringLiteral("application/x-bonk-prefab");

QString sharedPrefabDir() { return QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/bonk/prefabs"); }
QString projectPrefabDir(const Project &p) { return p.isOpen() ? p.dir() + QStringLiteral("/Prefabs") : QString(); }

struct PrefabInfo {
    QString file;   // the .bpfab
    QString name;   // file name without .bpfab
    QString folder; // relative to its root, "" = top
    bool shared = true;
    QString base() const { return file.left(file.size() - 6); } // without ".bpfab"
    QString thumb() const { return base() + QStringLiteral(".png"); }
    QString meta() const { return base() + QStringLiteral(".json"); }
    QString textures() const { return base() + QStringLiteral(".textures"); }
    double lift() const {
        QFile f(meta());
        if (!f.open(QIODevice::ReadOnly)) return 0;
        return QJsonDocument::fromJson(f.readAll()).object().value(QStringLiteral("lift")).toDouble();
    }
};

QList<PrefabInfo> listPrefabs(const QString &root, bool shared) {
    QList<PrefabInfo> out;
    if (root.isEmpty() || !QFileInfo(root).isDir()) return out;
    QDirIterator it(root, {QStringLiteral("*.bpfab")}, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        PrefabInfo p;
        p.file = it.next();
        p.name = QFileInfo(p.file).completeBaseName();
        p.folder = QDir(root).relativeFilePath(QFileInfo(p.file).absolutePath());
        if (p.folder == QStringLiteral(".")) p.folder.clear();
        p.shared = shared;
        out << p;
    }
    QCollator col;
    col.setNumericMode(true);
    col.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(out.begin(), out.end(), [&](const PrefabInfo &a, const PrefabInfo &b) {
        return a.folder != b.folder ? col.compare(a.folder, b.folder) < 0 : col.compare(a.name, b.name) < 0;
    });
    return out;
}

QStringList prefabFolders(const QString &root) {
    QStringList out;
    if (root.isEmpty()) return out;
    QDirIterator it(root, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        const QString d = it.next();
        if (d.endsWith(QStringLiteral(".textures")) || d.contains(QStringLiteral(".textures/"))) continue;
        out << QDir(root).relativeFilePath(d);
    }
    out.sort(Qt::CaseInsensitive);
    return out;
}

/// A name that's safe as a file name.
QString prefabFileName(const QString &s) {
    QString o = s.trimmed();
    for (QChar &c : o)
        if (c == QLatin1Char('/') || c == QLatin1Char('\\') || c == QLatin1Char(':') || c == QLatin1Char('*') || c == QLatin1Char('?') ||
            c == QLatin1Char('"') || c == QLatin1Char('<') || c == QLatin1Char('>') || c == QLatin1Char('|'))
            c = QLatin1Char('-');
    while (o.startsWith(QLatin1Char('.'))) o.remove(0, 1);
    return o.isEmpty() ? QStringLiteral("Prefab") : o;
}

/// A folder path that stays inside its root ("Furniture/Kitchen").
QString prefabFolderPath(const QString &s) {
    QStringList parts;
    for (const QString &p : s.split(QRegularExpression(QStringLiteral("[/\\\\]")), Qt::SkipEmptyParts)) {
        const QString c = prefabFileName(p);
        if (c != QStringLiteral("..") && !c.isEmpty()) parts << c;
    }
    return parts.join(QLatin1Char('/'));
}

/// The prefab's text, with its textures' source pointed where they are now.
QString prefabText(const PrefabInfo &p, const QString &projectTextures) {
    QFile f(p.file);
    if (!f.open(QIODevice::ReadOnly)) return QString();
    QString t = QString::fromUtf8(f.readAll());
    const QString src = p.shared ? p.textures() : projectTextures;
    static const QRegularExpression srcRe(QStringLiteral("\"source_textures\"\\s+\"[^\"]*\""));
    QString v = src;
    v.replace(QLatin1Char('"'), QLatin1Char('\''));
    if (t.contains(srcRe)) t.replace(srcRe, QStringLiteral("\"source_textures\"\t\"%1\"").arg(v));
    return t;
}

// ------------------------------------------------------------------ the pane

/// The prefab grid: section and folder headers across the width, tiles with
/// a thumbnail and a name. Drag a tile into a view to place it.
class PrefabGrid : public QListWidget {
public:
    static constexpr int HeaderRole = Qt::UserRole + 1, FileRole = Qt::UserRole + 2, SharedRole = Qt::UserRole + 3;
    int tileSize = 72;

    PrefabGrid() {
        setViewMode(QListView::IconMode);
        setFlow(QListView::LeftToRight);
        setWrapping(true);
        setResizeMode(QListView::Adjust);
        setMovement(QListView::Static);
        setSpacing(3);
        setUniformItemSizes(false);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setDragEnabled(true);
        setDragDropMode(QAbstractItemView::DragOnly);
        setItemDelegate(new Delegate(this));
    }

    void fitHeaders() {
        const int w = std::max(60, viewport()->width() - 2 * spacing() - 2);
        bool any = false;
        for (int i = 0; i < count(); ++i)
            if (item(i)->data(HeaderRole).toInt() && item(i)->sizeHint().width() != w) {
                item(i)->setSizeHint(QSize(w, item(i)->data(HeaderRole).toInt() == 1 ? 24 : 18));
                any = true;
            }
        if (any) doItemsLayout();
    }

protected:
    void startDrag(Qt::DropActions) override {
        QListWidgetItem *it = currentItem();
        if (!it || it->data(HeaderRole).toInt()) return;
        auto *mime = new QMimeData;
        mime->setData(kPrefabMime, it->data(FileRole).toString().toUtf8());
        auto *drag = new QDrag(this);
        drag->setMimeData(mime);
        drag->setPixmap(it->icon().pixmap(QSize(tileSize, tileSize)));
        drag->setHotSpot(QPoint(tileSize / 2, tileSize / 2));
        drag->exec(Qt::CopyAction, Qt::CopyAction);
    }
    void resizeEvent(QResizeEvent *e) override {
        QListWidget::resizeEvent(e);
        QTimer::singleShot(0, this, [this] { fitHeaders(); });
    }
    void showEvent(QShowEvent *e) override {
        QListWidget::showEvent(e);
        QTimer::singleShot(0, this, [this] { fitHeaders(); });
    }

private:
    class Delegate : public QStyledItemDelegate {
    public:
        explicit Delegate(PrefabGrid *g) : QStyledItemDelegate(g), grid(g) {}
        QSize sizeHint(const QStyleOptionViewItem &o, const QModelIndex &i) const override {
            if (i.data(HeaderRole).toInt()) {
                const QSize h = i.data(Qt::SizeHintRole).toSize();
                return h.isValid() ? h : QSize(120, 22);
            }
            return QSize(grid->tileSize + 8, grid->tileSize + QFontMetrics(o.font).height() + 8);
        }
        void paint(QPainter *p, const QStyleOptionViewItem &o, const QModelIndex &i) const override {
            p->save();
            const QRect r = o.rect;
            const int kind = i.data(HeaderRole).toInt();
            const bool sel = o.state & QStyle::State_Selected;
            if (kind) {
                QFont f = o.font;
                f.setBold(kind == 1);
                p->setFont(f);
                p->setPen(o.palette.color(kind == 1 ? QPalette::WindowText : QPalette::PlaceholderText));
                p->drawText(r.adjusted(4, 0, -4, -3), Qt::AlignLeft | Qt::AlignBottom, i.data(Qt::DisplayRole).toString());
                if (kind == 1) {
                    p->setPen(o.palette.color(QPalette::Mid));
                    p->drawLine(r.left() + 2, r.bottom(), r.right() - 2, r.bottom());
                }
            } else {
                const QRect tile(r.left() + 4, r.top() + 3, grid->tileSize, grid->tileSize);
                p->setRenderHint(QPainter::Antialiasing);
                p->setPen(Qt::NoPen);
                p->setBrush(sel ? o.palette.highlight() : QBrush(o.palette.color(QPalette::Base).darker(108)));
                p->drawRoundedRect(QRectF(tile).adjusted(-2, -2, 2, 2), 5, 5);
                i.data(Qt::DecorationRole).value<QIcon>().paint(p, tile, Qt::AlignCenter);
                p->setPen(sel ? o.palette.color(QPalette::HighlightedText) : o.palette.color(QPalette::Text));
                const QRect tr(r.left() + 1, tile.bottom() + 3, r.width() - 2, r.bottom() - tile.bottom() - 3);
                p->drawText(tr, Qt::AlignHCenter | Qt::AlignTop, QFontMetrics(o.font).elidedText(i.data(Qt::DisplayRole).toString(), Qt::ElideRight, tr.width()));
            }
            p->restore();
        }

    private:
        PrefabGrid *grid;
    };
};

class PrefabsPane : public QWidget {
public:
    std::function<void()> saveRequested;
    /// place this prefab: in view `view` at (x, y), or -1 = under the last mouse position
    std::function<void(const PrefabInfo &, int view, QPointF at)> placeRequested;
    std::function<QImage(const PrefabInfo &)> renderThumb;
    std::function<QString()> projectRoot; // "" when no project is open
    std::function<QString()> projectName;

    PrefabsPane() {
        auto *v = new QVBoxLayout(this);
        v->setContentsMargins(gCompact ? 4 : 8, 4, gCompact ? 4 : 8, gCompact ? 4 : 8);
        v->setSpacing(gCompact ? 3 : 6);
        auto *saveBtn = new QPushButton(QStringLiteral("Save Selection as Prefab…"));
        saveBtn->setToolTip(QStringLiteral("The selected brushes, textures and all, as a prefab to drop into any model"));
        v->addWidget(saveBtn);
        filter = new QLineEdit;
        filter->setPlaceholderText(QStringLiteral("Filter prefabs…"));
        filter->setClearButtonEnabled(true);
        v->addWidget(filter);
        grid = new PrefabGrid;
        grid->setToolTip(QStringLiteral("Drag a prefab into a view to place it, or double-click to place it under the mouse.\n"
                                        "Right-click for more."));
        v->addWidget(grid, 1);
        keep = new QCheckBox(QStringLiteral("Keep height above floor"));
        keep->setToolTip(QStringLiteral("On: a prefab saved 32 units above the floor lands 32 units above where you drop it.\n"
                                        "Off: it sits right on the surface."));
        keep->setChecked(QSettings().value("prefabs/keepHeight", true).toBool());
        v->addWidget(keep);
        auto *zoomRow = new QHBoxLayout;
        zoomRow->addWidget(new QLabel(QStringLiteral("Size")));
        zoom = new QSlider(Qt::Horizontal);
        zoom->setRange(40, 160);
        zoom->setValue(QSettings().value("prefabs/zoom", gCompact ? lowOr(64, 48) : 80).toInt());
        grid->tileSize = zoom->value();
        zoomRow->addWidget(zoom, 1);
        v->addLayout(zoomRow);

        QObject::connect(saveBtn, &QPushButton::clicked, this, [this] { if (saveRequested) saveRequested(); });
        QObject::connect(filter, &QLineEdit::textChanged, this, [this] { refresh(); });
        QObject::connect(keep, &QCheckBox::toggled, this, [](bool on) { QSettings().setValue("prefabs/keepHeight", on); });
        QObject::connect(zoom, &QSlider::valueChanged, this, [this](int z) {
            grid->tileSize = z;
            QSettings().setValue("prefabs/zoom", z);
            grid->doItemsLayout();
        });
        QObject::connect(grid, &QListWidget::itemDoubleClicked, this, [this](QListWidgetItem *it) {
            if (const PrefabInfo *p = infoOf(it)) placeRequested(*p, -1, QPointF());
        });
        grid->setContextMenuPolicy(Qt::CustomContextMenu);
        QObject::connect(grid, &QWidget::customContextMenuRequested, this, [this](QPoint pos) { menu(pos); });
        QObject::connect(&watchTimer, &QTimer::timeout, this, [this] { refresh(); });
        watchTimer.setSingleShot(true);
        watchTimer.setInterval(300);
        QObject::connect(&watcher, &QFileSystemWatcher::directoryChanged, this, [this] { watchTimer.start(); });
    }

    bool keepHeight() const { return keep->isChecked(); }

    /// Rebuild the grid from disk (both sections).
    void refresh() {
        const QString cur = grid->currentItem() ? grid->currentItem()->data(PrefabGrid::FileRole).toString() : QString();
        grid->clear();
        items.clear();
        const QString f = filter->text().trimmed();
        auto header = [&](const QString &t, int kind) {
            auto *h = new QListWidgetItem(t);
            h->setData(PrefabGrid::HeaderRole, kind);
            h->setFlags(Qt::ItemIsEnabled);
            grid->addItem(h);
        };
        auto section = [&](const QString &title, const QString &root, bool shared, const QString &empty) {
            header(title, 1);
            const QList<PrefabInfo> all = listPrefabs(root, shared);
            int shown = 0;
            QString folder = QStringLiteral("\x01");
            for (const PrefabInfo &p : all) {
                if (!f.isEmpty() && !p.name.contains(f, Qt::CaseInsensitive) && !p.folder.contains(f, Qt::CaseInsensitive)) continue;
                if (p.folder != folder) {
                    folder = p.folder;
                    if (!folder.isEmpty()) header(folder, 2);
                }
                auto *it = new QListWidgetItem(p.name);
                it->setData(PrefabGrid::FileRole, p.file);
                it->setData(PrefabGrid::SharedRole, shared);
                it->setToolTip(QStringLiteral("%1%2\n%3").arg(p.folder.isEmpty() ? QString() : p.folder + QStringLiteral(" / "), p.name,
                                                              shared ? QStringLiteral("Shared: every project") : QStringLiteral("This project only")));
                QImage img(p.thumb());
                if (img.isNull() && renderThumb) {
                    img = renderThumb(p);
                    if (!img.isNull()) img.save(p.thumb());
                }
                it->setIcon(img.isNull() ? QIcon() : QIcon(QPixmap::fromImage(img)));
                grid->addItem(it);
                items.insert(it, p);
                if (p.file == cur) grid->setCurrentItem(it);
                ++shown;
            }
            if (!shown) {
                auto *e = new QListWidgetItem(f.isEmpty() ? empty : QStringLiteral("Nothing matches"));
                e->setData(PrefabGrid::HeaderRole, 2);
                e->setFlags(Qt::ItemIsEnabled);
                grid->addItem(e);
            }
        };
        section(QStringLiteral("Shared"), sharedPrefabDir(), true, QStringLiteral("None yet: select brushes, then Save Selection as Prefab…"));
        const QString pr = projectRoot ? projectRoot() : QString();
        if (!pr.isEmpty()) section(QStringLiteral("This project (%1)").arg(projectName ? projectName() : QString()), pr, false, QStringLiteral("None yet"));
        grid->fitHeaders();
        // watch the folders, so prefabs added in a file manager show up
        if (!watcher.directories().isEmpty()) watcher.removePaths(watcher.directories());
        QStringList dirs;
        for (const QString &root : {sharedPrefabDir(), pr}) {
            if (root.isEmpty() || !QFileInfo(root).isDir()) continue;
            dirs << root;
            for (const QString &d : prefabFolders(root)) dirs << root + QLatin1Char('/') + d;
        }
        if (!dirs.isEmpty()) watcher.addPaths(dirs);
    }

    /// Developer aid: the grid, for screenshots.
    PrefabGrid *gridWidget() { return grid; }

private:
    QLineEdit *filter;
    PrefabGrid *grid;
    QCheckBox *keep;
    QSlider *zoom;
    QHash<QListWidgetItem *, PrefabInfo> items;
    QFileSystemWatcher watcher;
    QTimer watchTimer;

    const PrefabInfo *infoOf(QListWidgetItem *it) const {
        auto f = items.find(it);
        return f == items.end() ? nullptr : &f.value();
    }

    static void removePrefabFiles(const PrefabInfo &p) {
        QFile::remove(p.file);
        QFile::remove(p.thumb());
        QFile::remove(p.meta());
        QDir(p.textures()).removeRecursively();
    }

    /// Move (rename) a prefab's files: .bpfab, .png, .json and .textures.
    static bool movePrefab(const PrefabInfo &p, const QString &newBase) {
        if (QFileInfo::exists(newBase + QStringLiteral(".bpfab"))) return false;
        QDir().mkpath(QFileInfo(newBase).absolutePath());
        bool ok = QFile::rename(p.file, newBase + QStringLiteral(".bpfab"));
        if (!ok) return false;
        QFile::rename(p.thumb(), newBase + QStringLiteral(".png"));
        QFile::rename(p.meta(), newBase + QStringLiteral(".json"));
        if (QFileInfo(p.textures()).isDir()) QDir().rename(p.textures(), newBase + QStringLiteral(".textures"));
        return true;
    }

    void menu(QPoint pos) {
        QListWidgetItem *it = grid->itemAt(pos);
        const PrefabInfo *pp = infoOf(it);
        if (!pp) return;
        const PrefabInfo p = *pp;
        QMenu m(this);
        m.addAction(QStringLiteral("Place Under the Mouse"), [this, p] { placeRequested(p, -1, QPointF()); });
        m.addSeparator();
        m.addAction(QStringLiteral("Rename…"), [this, p] {
            bool ok = false;
            const QString n = QInputDialog::getText(this, QStringLiteral("Rename prefab"), QStringLiteral("Name"), QLineEdit::Normal, p.name, &ok);
            if (!ok || n.trimmed().isEmpty() || n == p.name) return;
            if (!movePrefab(p, QFileInfo(p.file).absolutePath() + QLatin1Char('/') + prefabFileName(n)))
                QMessageBox::warning(this, QStringLiteral("Rename prefab"), QStringLiteral("There's already a prefab called \"%1\" there.").arg(n));
            refresh();
        });
        m.addAction(QStringLiteral("Move to Folder…"), [this, p] {
            const QString root = p.shared ? sharedPrefabDir() : (projectRoot ? projectRoot() : QString());
            QStringList folders = prefabFolders(root);
            folders.prepend(QString());
            bool ok = false;
            const QString f = QInputDialog::getItem(this, QStringLiteral("Move prefab"), QStringLiteral("Folder (type a new one, \"\" for none)"),
                                                    folders, std::max(0, int(folders.indexOf(p.folder))), true, &ok);
            if (!ok) return;
            const QString dir = prefabFolderPath(f);
            if (!movePrefab(p, root + (dir.isEmpty() ? QString() : QLatin1Char('/') + dir) + QLatin1Char('/') + p.name))
                QMessageBox::warning(this, QStringLiteral("Move prefab"), QStringLiteral("There's already a prefab called \"%1\" there.").arg(p.name));
            refresh();
        });
        m.addAction(QStringLiteral("Retake Thumbnail"), [this, p] {
            QFile::remove(p.thumb());
            refresh();
        });
        m.addAction(QStringLiteral("Show in File Manager"), [p] { QDesktopServices::openUrl(QUrl::fromLocalFile(QFileInfo(p.file).absolutePath())); });
        m.addSeparator();
        m.addAction(QStringLiteral("Delete…"), [this, p] {
            if (QMessageBox::question(this, QStringLiteral("Delete prefab"), QStringLiteral("Delete the prefab \"%1\"?").arg(p.name)) != QMessageBox::Yes) return;
            removePrefabFiles(p);
            refresh();
        });
        m.exec(grid->viewport()->mapToGlobal(pos));
    }
};

/// File → Save Selection as Prefab: name, where (shared / this project) and folder.
class SavePrefabDialog : public QDialog {
public:
    QLineEdit *name;
    QComboBox *where, *folder;

    SavePrefabDialog(QWidget *parent, bool hasProject, const QString &suggested) : QDialog(parent) {
        setWindowTitle(QStringLiteral("Save as Prefab"));
        auto *f = new QFormLayout(this);
        name = new QLineEdit(suggested);
        name->selectAll();
        where = new QComboBox;
        where->addItem(QStringLiteral("Shared (every project)"), true);
        if (hasProject) where->addItem(QStringLiteral("This project only"), false);
        folder = new QComboBox;
        folder->setEditable(true);
        folder->setToolTip(QStringLiteral("Pick a folder or type a new one (Furniture/Kitchen); empty for none"));
        f->addRow(QStringLiteral("Name"), name);
        f->addRow(QStringLiteral("Save in"), where);
        f->addRow(QStringLiteral("Folder"), folder);
        auto *note = hintLabel(QStringLiteral("Shared prefabs keep copies of their textures, so they work in any project."));
        f->addRow(note);
        auto *bb = new QDialogButtonBox(QDialogButtonBox::Save | QDialogButtonBox::Cancel);
        f->addRow(bb);
        QObject::connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
        QObject::connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
        QObject::connect(where, &QComboBox::currentIndexChanged, this, [this] { fillFolders(); });
        fillFolders();
        resize(420, sizeHint().height());
    }

    std::function<QString(bool shared)> rootFor;

    void fillFolders() {
        const QString cur = folder->currentText();
        folder->clear();
        folder->addItem(QString());
        if (rootFor) folder->addItems(prefabFolders(rootFor(where->currentData().toBool())));
        folder->setCurrentText(cur);
    }
};
