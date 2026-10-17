// SPDX-License-Identifier: GPL-3.0-or-later
// The Variants tab: skins, bodygroups and LODs.
//
// Included by host.cpp (inside its anonymous namespace).

class VariantsPane : public QWidget {
public:
    std::function<void(unsigned)> changed; // flags from the core, to redraw
    std::function<void()> openPreview;

    VariantsPane() {
        auto *scroll = new QScrollArea;
        scroll->setWidgetResizable(true);
        scroll->setFrameShape(QFrame::NoFrame);
        scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
        auto *inner = new QWidget;
        scroll->setWidget(inner);
        auto *outer = new QVBoxLayout(this);
        outer->setContentsMargins(0, 0, 0, 0);
        outer->addWidget(scroll);
        auto *v = new QVBoxLayout(inner);
        v->setContentsMargins(gCompact ? 4 : 8, 4, gCompact ? 4 : 8, gCompact ? 4 : 8);
        v->setSpacing(gCompact ? 3 : 6);

        // --- skins
        v->addWidget(new SectionHeader(QStringLiteral("Skins")));
        skins = new QListWidget;
        skins->setMaximumHeight(120);
        skins->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed);
        skins->setToolTip(QStringLiteral("Click a skin to see and paint it. Double-click to rename.\nSkin 0 is the model as painted."));
        v->addWidget(skins);
        auto *sb = new QHBoxLayout;
        auto *addSkin = new QPushButton(QStringLiteral("+ Skin"));
        addSkin->setToolTip(QStringLiteral("A new skin, a copy of the one picked"));
        delSkin = new QPushButton(QStringLiteral("Delete"));
        sb->addWidget(addSkin);
        sb->addWidget(delSkin);
        v->addLayout(sb);
        skinHint = hintLabel(QString());
        v->addWidget(skinHint);
        swaps = new QListWidget;
        swaps->setMaximumHeight(96);
        swaps->setToolTip(QStringLiteral("What this skin swaps. Select one and press Delete to put the original back."));
        v->addWidget(swaps);
        auto *hb = new QGridLayout;
        hideBtn = new QPushButton(QStringLiteral("Hide Selection"));
        hideBtn->setToolTip(QStringLiteral("The selected brushes don't show in this skin\n(exported as an invisible material swap)"));
        showBtn = new QPushButton(QStringLiteral("Show Selection"));
        showAll = new QPushButton(QStringLiteral("Show All"));
        hb->addWidget(hideBtn, 0, 0);
        hb->addWidget(showBtn, 0, 1);
        hb->addWidget(showAll, 1, 0, 1, 2);
        v->addLayout(hb);
        v->addSpacing(gCompact ? 4 : 8);

        // --- bodygroups
        v->addWidget(new SectionHeader(QStringLiteral("Bodygroups")));
        bgs = new QTreeWidget;
        bgs->setHeaderHidden(true);
        bgs->setRootIsDecorated(true);
        bgs->setMaximumHeight(170);
        bgs->setToolTip(QStringLiteral("Click an option to show it in the views. Players switch between a bodygroup's options in game."));
        v->addWidget(bgs);
        auto *gb = new QGridLayout;
        markBtn = new QPushButton(QStringLiteral("Make Group an Option…"));
        markBtn->setToolTip(QStringLiteral("The selected group becomes an option of a bodygroup (its name is the option's name)"));
        unmarkBtn = new QPushButton(QStringLiteral("Not an Option"));
        noneBtn = new QPushButton(QStringLiteral("+ None Option"));
        noneBtn->setToolTip(QStringLiteral("Let players show nothing for this bodygroup"));
        gb->addWidget(markBtn, 0, 0, 1, 2);
        gb->addWidget(unmarkBtn, 1, 0);
        gb->addWidget(noneBtn, 1, 1);
        v->addLayout(gb);
        bgHint = hintLabel(QString());
        v->addWidget(bgHint);
        v->addSpacing(gCompact ? 4 : 8);

        // --- LODs
        v->addWidget(new SectionHeader(QStringLiteral("Levels of detail")));
        lods = new QTableWidget(0, 3);
        lods->setHorizontalHeaderLabels({QStringLiteral("Switch at"), QStringLiteral("Drop under"), QStringLiteral("Triangles")});
        lods->verticalHeader()->setVisible(false);
        lods->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
        lods->setMaximumHeight(130);
        lods->setToolTip(QStringLiteral("Switch at: studiomdl's $lod distance (bigger = farther away; 30 and 60 are usual).\n"
                                        "Drop under: brushes smaller than this many units across are left out of that LOD."));
        v->addWidget(lods);
        auto *lb = new QHBoxLayout;
        auto *addLod = new QPushButton(QStringLiteral("+ LOD"));
        delLod = new QPushButton(QStringLiteral("Remove"));
        lb->addWidget(addLod);
        lb->addWidget(delLod);
        v->addLayout(lb);
        auto *prevBtn = new QPushButton(QStringLiteral("Model Preview (F7)"));
        prevBtn->setToolTip(QStringLiteral("The model as it compiles: switch skins, bodygroup options and LODs"));
        v->addWidget(prevBtn);
        QObject::connect(prevBtn, &QPushButton::clicked, this, [this] { if (openPreview) openPreview(); });
        v->addStretch(1);

        QObject::connect(skins, &QListWidget::currentRowChanged, this, [this](int r) {
            if (!syncing && r >= 0) emitFlags(bonk_skin_op(2, r, nullptr));
        });
        QObject::connect(skins, &QListWidget::itemChanged, this, [this](QListWidgetItem *it) {
            if (syncing) return;
            const QByteArray n = it->text().toUtf8();
            emitFlags(bonk_skin_op(3, skins->row(it), n.constData()));
        });
        QObject::connect(addSkin, &QPushButton::clicked, this, [this] { emitFlags(bonk_skin_op(0, std::max(0, skins->currentRow()), nullptr)); });
        QObject::connect(delSkin, &QPushButton::clicked, this, [this] {
            const int r = skins->currentRow();
            if (r <= 0) return;
            if (QMessageBox::question(this, QStringLiteral("Delete skin"), QStringLiteral("Delete %1?").arg(skins->item(r)->text())) != QMessageBox::Yes) return;
            emitFlags(bonk_skin_op(1, r, nullptr));
        });
        QObject::connect(hideBtn, &QPushButton::clicked, this, [this] { emitFlags(bonk_skin_op(4, 0, nullptr)); });
        QObject::connect(showBtn, &QPushButton::clicked, this, [this] { emitFlags(bonk_skin_op(5, 0, nullptr)); });
        QObject::connect(showAll, &QPushButton::clicked, this, [this] { emitFlags(bonk_skin_op(7, 0, nullptr)); });
        swaps->installEventFilter(this);
        QObject::connect(bgs, &QTreeWidget::itemClicked, this, [this](QTreeWidgetItem *it) {
            if (!it->parent()) return;
            const QByteArray b = it->parent()->text(0).toUtf8();
            const QByteArray o = it->data(0, Qt::UserRole + 1).toString().toUtf8();
            emitFlags(bonk_bodygroup_op(3, 0, b.constData(), o.constData()));
        });
        QObject::connect(bgs, &QTreeWidget::currentItemChanged, this, [this] { syncButtons(); });
        QObject::connect(markBtn, &QPushButton::clicked, this, [this] { markGroup(); });
        QObject::connect(unmarkBtn, &QPushButton::clicked, this, [this] {
            int gid = selGroup;
            if (QTreeWidgetItem *it = bgs->currentItem(); it && it->parent() && it->data(0, Qt::UserRole).toInt() > 0) gid = it->data(0, Qt::UserRole).toInt();
            if (gid > 0) emitFlags(bonk_bodygroup_op(0, gid, "", nullptr));
        });
        QObject::connect(noneBtn, &QPushButton::clicked, this, [this] {
            QTreeWidgetItem *it = bgs->currentItem();
            if (!it) return;
            if (it->parent()) it = it->parent();
            const QByteArray b = it->text(0).toUtf8();
            const bool has = it->data(0, Qt::UserRole + 2).toBool();
            emitFlags(bonk_bodygroup_op(has ? 2 : 1, 0, b.constData(), nullptr));
        });
        QObject::connect(addLod, &QPushButton::clicked, this, [this] {
            QStringList parts = lodText();
            const int n = parts.size();
            parts << QStringLiteral("%1 %2").arg(n == 0 ? 30 : 30 * (n + 1)).arg(n == 0 ? 16 : 16 * (1 << n));
            setLods(parts);
        });
        QObject::connect(delLod, &QPushButton::clicked, this, [this] {
            const int r = lods->currentRow();
            QStringList parts = lodText();
            if (r >= 0 && r < parts.size()) parts.removeAt(r);
            else if (!parts.isEmpty()) parts.removeLast();
            setLods(parts);
        });
        QObject::connect(lods, &QTableWidget::cellChanged, this, [this](int, int c) {
            if (!syncing && c < 2) setLods(lodText());
        });
        // the dock can be narrow: buttons shrink with it rather than spill out
        for (QPushButton *b : inner->findChildren<QPushButton *>()) {
            b->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
            b->setMinimumWidth(40);
        }
        lods->setMinimumWidth(60);
        poll.setInterval(250);
        QObject::connect(&poll, &QTimer::timeout, this, [this] { refresh(); });
        poll.start();
    }

    void refresh(bool force = false) {
        if (!isVisible() && !force) return;
        const QString info = QString::fromUtf8(bonk_variants_info());
        int tri[2] = {0, 0};
        bonk_lod_tris(0, tri);
        if (info == last && tri[1] == lastTris && !force) return;
        last = info;
        lastTris = tri[1];
        rebuild(info);
    }

protected:
    bool eventFilter(QObject *o, QEvent *e) override {
        if (o == swaps && e->type() == QEvent::KeyPress) {
            auto *k = static_cast<QKeyEvent *>(e);
            if ((k->key() == Qt::Key_Delete || k->key() == Qt::Key_Backspace) && swaps->currentItem()) {
                const QByteArray from = swaps->currentItem()->data(Qt::UserRole).toString().toUtf8();
                emitFlags(bonk_skin_op(6, skins->currentRow(), from.constData()));
                return true;
            }
        }
        return QWidget::eventFilter(o, e);
    }
    void showEvent(QShowEvent *e) override {
        QWidget::showEvent(e);
        refresh(true);
    }

private:
    QListWidget *skins, *swaps;
    QPushButton *delSkin, *hideBtn, *showBtn, *showAll, *markBtn, *unmarkBtn, *noneBtn, *delLod;
    QLabel *skinHint, *bgHint;
    QTreeWidget *bgs;
    QTableWidget *lods;
    QTimer poll;
    QString last;
    int lastTris = -1;
    bool syncing = false;
    int selGroup = 0;
    QString selGroupName, selGroupBg;
    QStringList bodygroupNames;

    void emitFlags(unsigned f) {
        if (changed) changed(f);
        refresh(true);
    }

    QStringList lodText() const {
        QStringList parts;
        for (int r = 0; r < lods->rowCount(); ++r) {
            const QString a = lods->item(r, 0) ? lods->item(r, 0)->text() : QString();
            const QString b = lods->item(r, 1) ? lods->item(r, 1)->text() : QString();
            bool ok1 = false, ok2 = false;
            const double d = a.toDouble(&ok1), t = b.toDouble(&ok2);
            if (ok1 && ok2) parts << QStringLiteral("%1 %2").arg(d).arg(t);
        }
        return parts;
    }
    void setLods(const QStringList &parts) {
        const QByteArray t = parts.join(QLatin1Char(';')).toUtf8();
        emitFlags(bonk_set_lods(t.constData()));
    }

    void markGroup() {
        if (selGroup <= 0) {
            QMessageBox::information(this, QStringLiteral("Bodygroup option"), QStringLiteral("Select a group first (Ctrl+G groups the selection)."));
            return;
        }
        QStringList names = bodygroupNames;
        if (names.isEmpty()) names << QStringLiteral("Door");
        bool ok = false;
        const QString n = QInputDialog::getItem(this, QStringLiteral("Bodygroup option"),
                                                QStringLiteral("\"%1\" becomes an option of the bodygroup:").arg(selGroupName),
                                                names, std::max(0, int(names.indexOf(selGroupBg))), true, &ok);
        if (!ok || n.trimmed().isEmpty()) return;
        const QByteArray b = n.trimmed().toUtf8();
        emitFlags(bonk_bodygroup_op(0, selGroup, b.constData(), nullptr));
    }

    void syncButtons() {
        const bool skinPicked = skins->currentRow() > 0;
        delSkin->setEnabled(skinPicked);
        hideBtn->setEnabled(skinPicked);
        showBtn->setEnabled(skinPicked);
        showAll->setEnabled(skinPicked);
        swaps->setEnabled(skinPicked);
        markBtn->setEnabled(selGroup > 0);
        markBtn->setText(QStringLiteral("Make Group an Option…"));
        markBtn->setToolTip(selGroup > 0 ? QStringLiteral("\"%1\" becomes an option of a bodygroup").arg(selGroupName) : QStringLiteral("Select a group first"));
        QTreeWidgetItem *it = bgs->currentItem();
        unmarkBtn->setEnabled(!selGroupBg.isEmpty() || (it && it->parent() && it->data(0, Qt::UserRole).toInt() > 0));
        noneBtn->setEnabled(it != nullptr);
        if (it) {
            QTreeWidgetItem *top = it->parent() ? it->parent() : it;
            noneBtn->setText(top->data(0, Qt::UserRole + 2).toBool() ? QStringLiteral("− None Option") : QStringLiteral("+ None Option"));
        }
    }

    void rebuild(const QString &info) {
        syncing = true;
        struct SkinRow { int index; QString name; int swaps, hidden; };
        QList<SkinRow> skinRows;
        QList<QPair<int, QPair<QString, QString>>> swapRows;
        int active = 0;
        struct Bg { QString name; bool blank; QString shown; QList<QPair<int, QString>> opts; };
        QList<Bg> bgList;
        QList<QPair<double, double>> lodRows;
        selGroup = 0;
        selGroupName.clear();
        selGroupBg.clear();
        for (const QString &l : info.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            const QStringList f = l.split(QLatin1Char('\t'));
            const QString k = f.value(0);
            if (k == QStringLiteral("skin")) skinRows << SkinRow{f.value(1).toInt(), f.value(2), f.value(3).toInt(), f.value(4).toInt()};
            else if (k == QStringLiteral("swap")) swapRows << qMakePair(f.value(1).toInt(), qMakePair(f.value(2), f.value(3)));
            else if (k == QStringLiteral("active")) active = f.value(1).toInt();
            else if (k == QStringLiteral("bg")) bgList << Bg{f.value(1), f.value(2) == QStringLiteral("1"), f.value(3), {}};
            else if (k == QStringLiteral("opt")) {
                for (Bg &b : bgList)
                    if (b.name == f.value(1)) b.opts << qMakePair(f.value(2).toInt(), f.value(3));
            } else if (k == QStringLiteral("lod")) lodRows << qMakePair(f.value(1).toDouble(), f.value(2).toDouble());
            else if (k == QStringLiteral("group")) {
                selGroup = f.value(1).toInt();
                selGroupName = f.value(2);
                selGroupBg = f.value(3);
            }
        }
        // skins
        skins->clear();
        auto *zero = new QListWidgetItem(QStringLiteral("Skin 0 (as painted)"));
        skins->addItem(zero);
        for (const SkinRow &s : skinRows) {
            auto *it = new QListWidgetItem(s.name);
            it->setFlags(it->flags() | Qt::ItemIsEditable);
            it->setToolTip(QStringLiteral("%1 material swap%2%3").arg(s.swaps).arg(s.swaps == 1 ? QString() : QStringLiteral("s"))
                               .arg(s.hidden ? QStringLiteral(", %1 brush%2 hidden").arg(s.hidden).arg(s.hidden == 1 ? QString() : QStringLiteral("es")) : QString()));
            skins->addItem(it);
        }
        skins->setCurrentRow(std::clamp(active, 0, skins->count() - 1));
        swaps->clear();
        for (const auto &sw : swapRows) {
            if (sw.first != active) continue;
            auto *it = new QListWidgetItem(QStringLiteral("%1  →  %2").arg(sw.second.first.section(QLatin1Char('/'), -1), sw.second.second.section(QLatin1Char('/'), -1)));
            it->setToolTip(QStringLiteral("%1 → %2").arg(sw.second.first, sw.second.second));
            it->setData(Qt::UserRole, sw.second.first);
            swaps->addItem(it);
        }
        int hiddenCount = 0;
        for (const SkinRow &s : skinRows)
            if (s.index == active) hiddenCount = s.hidden;
        skinHint->setText(active == 0 ? QStringLiteral("Add a skin, then paint faces while it's picked: their material is swapped in that skin only.")
                                      : QStringLiteral("Painting now swaps materials in this skin (every face with that material). %1")
                                            .arg(hiddenCount ? QStringLiteral("%1 brush%2 hidden.").arg(hiddenCount).arg(hiddenCount == 1 ? QString() : QStringLiteral("es")) : QString()));
        // bodygroups
        const QString keep = bgs->currentItem() ? bgs->currentItem()->text(0) : QString();
        bgs->clear();
        bodygroupNames.clear();
        for (const Bg &b : bgList) {
            bodygroupNames << b.name;
            auto *top = new QTreeWidgetItem(bgs, {b.name});
            QFont f = top->font(0);
            f.setBold(true);
            top->setFont(0, f);
            top->setData(0, Qt::UserRole + 2, b.blank);
            auto opt = [&](const QString &label, const QString &name, int gid) {
                auto *it = new QTreeWidgetItem(top, {label});
                it->setData(0, Qt::UserRole, gid);
                it->setData(0, Qt::UserRole + 1, name);
                const bool on = name == b.shown;
                it->setIcon(0, QIcon(radioPixmap(on)));
                if (it->text(0) == keep) bgs->setCurrentItem(it);
            };
            for (const auto &o : b.opts) opt(o.second, o.second, o.first);
            if (b.blank) opt(QStringLiteral("None"), QString(), 0);
            if (top->text(0) == keep) bgs->setCurrentItem(top);
        }
        bgs->expandAll();
        bgHint->setText(bgList.isEmpty() ? QStringLiteral("Group the brushes for each choice (Ctrl+G), then make each group an option of the same bodygroup.")
                                         : selGroup > 0 && !selGroupBg.isEmpty() ? QStringLiteral("\"%1\" is an option of %2.").arg(selGroupName, selGroupBg) : QString());
        bgHint->setVisible(!bgHint->text().isEmpty());
        // LODs
        lods->setRowCount(int(lodRows.size()));
        for (int r = 0; r < lodRows.size(); ++r) {
            lods->setItem(r, 0, new QTableWidgetItem(QString::number(lodRows[r].first)));
            lods->setItem(r, 1, new QTableWidgetItem(QString::number(lodRows[r].second)));
            int t[2] = {0, 0};
            bonk_lod_tris(float(lodRows[r].second), t);
            auto *tr = new QTableWidgetItem(t[1] ? QStringLiteral("%1 (%2%)").arg(t[0]).arg(int(100.0 * t[0] / t[1] + 0.5)) : QStringLiteral("0"));
            tr->setFlags(tr->flags() & ~Qt::ItemIsEditable);
            lods->setItem(r, 2, tr);
        }
        delLod->setEnabled(!lodRows.isEmpty());
        syncing = false;
        syncButtons();
    }

    QPixmap radioPixmap(bool on) const {
        const qreal dpr = devicePixelRatioF();
        QPixmap pm(QSize(14, 14) * dpr);
        pm.setDevicePixelRatio(dpr);
        pm.fill(Qt::transparent);
        QPainter p(&pm);
        p.setRenderHint(QPainter::Antialiasing);
        p.setPen(QPen(palette().color(QPalette::Text), 1.2));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(QRectF(1.5, 1.5, 11, 11));
        if (on) {
            p.setPen(Qt::NoPen);
            p.setBrush(palette().highlight());
            p.drawEllipse(QRectF(4, 4, 6, 6));
        }
        return pm;
    }
};

// ------------------------------------------------------------------ preview

/// View → Model Preview: the model the way it compiles, with its skins,
/// bodygroup options and LODs to switch between (Bonk's own renderer).
class ModelPreview : public QDialog {
public:
    std::function<QImage(int skin, const QString &choices, int lod, const ThumbCam &cam, int w, int h, bool grid, int *tris)> render;

    explicit ModelPreview(QWidget *parent) : QDialog(parent) {
        setWindowTitle(QStringLiteral("Model Preview"));
        auto *h = new QHBoxLayout(this);
        view = new ThumbView;
        view->setMinimumSize(360, 360);
        view->setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);
        view->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
        view->cam = &cam;
        view->moved = [this] { timer.start(); };
        h->addWidget(view, 1);
        auto *side = new QVBoxLayout;
        side->addWidget(new QLabel(QStringLiteral("<b>Skin</b>")));
        skinBox = new QComboBox;
        side->addWidget(skinBox);
        bgBox = new QVBoxLayout;
        side->addLayout(bgBox);
        side->addWidget(new QLabel(QStringLiteral("<b>Detail</b>")));
        lodBox = new QComboBox;
        side->addWidget(lodBox);
        grid = new QCheckBox(QStringLiteral("Floor grid"));
        grid->setChecked(true);
        side->addWidget(grid);
        auto *fit = new QPushButton(QStringLiteral("Fit"));
        side->addWidget(fit);
        stats = new QLabel;
        stats->setWordWrap(true);
        side->addWidget(stats);
        side->addStretch(1);
        side->addWidget(hintLabel(QStringLiteral("Drag: turn · Right-drag: move · Wheel: zoom.<br>As it compiles: visual brushes only, "
                                                 "nodraw left out. The game's own lighting differs.")));
        auto *sw = new QWidget;
        sw->setLayout(side);
        sw->setFixedWidth(230);
        h->addWidget(sw);
        timer.setSingleShot(true);
        timer.setInterval(16);
        QObject::connect(&timer, &QTimer::timeout, this, [this] { draw(); });
        QObject::connect(skinBox, &QComboBox::currentIndexChanged, this, [this] { if (!syncing) draw(); });
        QObject::connect(lodBox, &QComboBox::currentIndexChanged, this, [this] { if (!syncing) draw(); });
        QObject::connect(grid, &QCheckBox::toggled, this, [this] { draw(); });
        QObject::connect(fit, &QPushButton::clicked, this, [this] { refit(); });
        poll.setInterval(400);
        QObject::connect(&poll, &QTimer::timeout, this, [this] { reload(false); });
        poll.start();
        cam.yaw = -55;
        cam.pitch = 22;
        resize(860, 560);
    }

    /// Pick up changes to the model (and its variants).
    void reload(bool force) {
        if (!isVisible() && !force) return;
        const QString info = QString::fromUtf8(bonk_variants_info());
        const quint64 rev = bonk_layers_rev();
        if (!force && info == lastInfo && rev == lastRev) return;
        const bool first = lastInfo.isNull();
        lastInfo = info;
        lastRev = rev;
        syncing = true;
        const int skin = skinBox->currentIndex(), lod = lodBox->currentIndex();
        QHash<QString, QString> picked;
        for (auto it = bgCombos.cbegin(); it != bgCombos.cend(); ++it) picked[it.key()] = it.value()->currentData().toString();
        skinBox->clear();
        skinBox->addItem(QStringLiteral("Skin 0 (as painted)"));
        lodBox->clear();
        lodBox->addItem(QStringLiteral("Full (LOD 0)"));
        qDeleteAll(bgCombos);
        bgCombos.clear();
        while (QLayoutItem *i = bgBox->takeAt(0)) {
            delete i->widget();
            delete i;
        }
        QList<QPair<double, double>> lods;
        for (const QString &l : info.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            const QStringList f = l.split(QLatin1Char('\t'));
            if (f.value(0) == QStringLiteral("skin")) skinBox->addItem(f.value(2));
            else if (f.value(0) == QStringLiteral("bg")) {
                bgBox->addWidget(new QLabel(QStringLiteral("<b>%1</b>").arg(f.value(1).toHtmlEscaped())));
                auto *c = new QComboBox;
                c->setProperty("blank", f.value(2) == QStringLiteral("1"));
                bgBox->addWidget(c);
                bgCombos.insert(f.value(1), c);
                QObject::connect(c, &QComboBox::currentIndexChanged, this, [this] { if (!syncing) draw(); });
            } else if (f.value(0) == QStringLiteral("opt")) {
                if (QComboBox *c = bgCombos.value(f.value(1))) c->addItem(f.value(3), f.value(3));
            } else if (f.value(0) == QStringLiteral("lod")) lods << qMakePair(f.value(1).toDouble(), f.value(2).toDouble());
        }
        for (QComboBox *c : std::as_const(bgCombos))
            if (c->property("blank").toBool()) c->addItem(QStringLiteral("None"), QString());
        std::sort(lods.begin(), lods.end());
        for (int i = 0; i < lods.size(); ++i)
            lodBox->addItem(QStringLiteral("LOD %1 (switch at %2, drops under %3)").arg(i + 1).arg(lods[i].first).arg(lods[i].second));
        skinBox->setCurrentIndex(std::clamp(skin, 0, skinBox->count() - 1));
        lodBox->setCurrentIndex(std::clamp(lod, 0, lodBox->count() - 1));
        for (auto it = bgCombos.cbegin(); it != bgCombos.cend(); ++it) {
            const int k = it.value()->findData(picked.value(it.key(), QStringLiteral("\x01")));
            if (k >= 0) it.value()->setCurrentIndex(k);
        }
        syncing = false;
        if (first || force) refit();
        else draw();
    }

    /// Developer aid: pick options by text.
    void pick(int skin, const QString &choices, int lod) {
        syncing = true;
        skinBox->setCurrentIndex(std::clamp(skin, 0, skinBox->count() - 1));
        lodBox->setCurrentIndex(std::clamp(lod, 0, lodBox->count() - 1));
        for (const QString &p : choices.split(QLatin1Char(';'), Qt::SkipEmptyParts))
            if (QComboBox *c = bgCombos.value(p.section(QLatin1Char('='), 0, 0))) {
                const int k = c->findData(p.section(QLatin1Char('='), 1));
                if (k >= 0) c->setCurrentIndex(k);
            }
        syncing = false;
        refit();
    }

protected:
    void resizeEvent(QResizeEvent *e) override {
        QDialog::resizeEvent(e);
        timer.start();
    }

private:
    ThumbView *view;
    QComboBox *skinBox, *lodBox;
    QVBoxLayout *bgBox;
    QMap<QString, QComboBox *> bgCombos;
    QCheckBox *grid;
    QLabel *stats;
    ThumbCam cam;
    QTimer timer, poll;
    QString lastInfo;
    quint64 lastRev = 0;
    bool syncing = false;

    QString choices() const {
        QStringList c;
        for (auto it = bgCombos.cbegin(); it != bgCombos.cend(); ++it) c << it.key() + QLatin1Char('=') + it.value()->currentData().toString();
        return c.join(QLatin1Char(';'));
    }

    void refit() {
        float f[4];
        const QByteArray c = choices().toUtf8();
        const QSize s = view->size();
        if (bonk_preview_fit(skinBox->currentIndex(), c.constData(), lodBox->currentIndex(), float(cam.yaw), float(cam.pitch), float(cam.fov),
                             float(s.width()) / std::max(1, s.height()), f) == 0) {
            cam.tx = f[0];
            cam.ty = f[1];
            cam.tz = f[2];
            cam.dist = f[3];
        }
        draw();
    }

    void draw() {
        if (!render) return;
        const QSize s = view->size();
        view->frame = s;
        int tris = 0;
        const QImage img = render(skinBox->currentIndex(), choices(), lodBox->currentIndex(), cam, s.width(), s.height(), grid->isChecked(), &tris);
        view->setPixmap(QPixmap::fromImage(img));
        stats->setText(QStringLiteral("%1 triangle%2").arg(QLocale().toString(tris)).arg(tris == 1 ? QString() : QStringLiteral("s")));
    }
};
