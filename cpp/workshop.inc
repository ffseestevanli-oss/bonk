// SPDX-License-Identifier: GPL-3.0-or-later
// File → Manage Workshop…: publish the project's addon to the Steam Workshop
// with Garry's Mod's own gmad and gmpublish (Linux builds when Garry's Mod
// has them, else the Windows ones through Proton), list your addons, and
// snap a Workshop thumbnail from a model.
//
// Bonk uses the Garry's Mod folder for three things only: studiomdl when
// compiling, garrysmod/addons when you export, and gmad / gmpublish here.
// Your Steam profile is only used by gmpublish, to list and upload addons.
//
// Included by host.cpp (inside its anonymous namespace).

// ------------------------------------------------------------------ Steam

/// Is Steam running, and who's signed in (the most recent login)?
struct SteamStatus {
    bool running = false;
    bool unsure = false; // can't tell from inside a Flatpak sandbox
    QString persona;     // display name
    QString account;     // login name
    QString steamId;     // SteamID64 (for the public Workshop page)

    void detect(const SteamFind &sf) {
        running = false;
        unsure = false;
        const QString home = QDir::homePath();
        for (const QString &pidFile : {home + QStringLiteral("/.steam/steam.pid"), home + QStringLiteral("/.var/app/com.valvesoftware.Steam/.steam/steam.pid")}) {
            QFile f(pidFile);
            if (!f.open(QIODevice::ReadOnly)) continue;
            const QString pid = QString::fromLatin1(f.readAll()).trimmed();
            if (!pid.isEmpty() && QFileInfo::exists(QStringLiteral("/proc/") + pid)) {
                QFile comm(QStringLiteral("/proc/") + pid + QStringLiteral("/comm"));
                if (comm.open(QIODevice::ReadOnly) && QString::fromLatin1(comm.readAll()).trimmed().startsWith(QStringLiteral("steam"))) running = true;
            }
        }
        if (!running && sf.inFlatpak) unsure = true; // the sandbox doesn't see Steam's process
        // who: config/loginusers.vdf, the "MostRecent" account
        persona.clear();
        account.clear();
        QFile f(sf.steamRoot + QStringLiteral("/config/loginusers.vdf"));
        if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) return;
        const QString t = QString::fromUtf8(f.readAll());
        static const QRegularExpression block(QStringLiteral("\"(\\d{10,})\"\\s*\\{([^}]*)\\}"));
        static const QRegularExpression kv(QStringLiteral("\"(\\w+)\"\\s+\"([^\"]*)\""));
        QString firstPersona, firstAccount, firstId;
        for (auto it = block.globalMatch(t); it.hasNext();) {
            const auto bm = it.next();
            const QString id = bm.captured(1);
            const QString b = bm.captured(2);
            QString p, a;
            bool recent = false;
            for (auto k = kv.globalMatch(b); k.hasNext();) {
                const auto m = k.next();
                if (m.captured(1) == QStringLiteral("PersonaName")) p = m.captured(2);
                else if (m.captured(1) == QStringLiteral("AccountName")) a = m.captured(2);
                else if (m.captured(1).compare(QStringLiteral("MostRecent"), Qt::CaseInsensitive) == 0) recent = m.captured(2) == QStringLiteral("1");
            }
            if (firstPersona.isEmpty()) {
                firstPersona = p;
                firstAccount = a;
                firstId = id;
            }
            if (recent) {
                persona = p;
                account = a;
                steamId = id;
            }
        }
        if (persona.isEmpty()) {
            persona = firstPersona;
            account = firstAccount;
            steamId = firstId;
        }
    }
};

// ------------------------------------------------------------------ the tools

/// gmad and gmpublish: Garry's Mod's Linux builds if it has them, else the
/// Windows ones through Proton (like studiomdl).
/// Text from a program run in a terminal (`script`), minus the terminal's
/// escape codes: cursor moves become line breaks, colours and titles go.
QString terminalText(const QByteArray &raw) {
    QString t = QString::fromLocal8Bit(raw);
    static const QRegularExpression osc(QStringLiteral("\x1b\\][^\x07\x1b]*(\x07|\x1b\\\\)"));
    static const QRegularExpression cup(QStringLiteral("\x1b\\[[0-9;]*[Hf]"));
    static const QRegularExpression csi(QStringLiteral("\x1b\\[[0-?]*[ -/]*[@-~]"));
    static const QRegularExpression other(QStringLiteral("\x1b[()][0-9A-Za-z]|\x1b[=>78DEM]"));
    t.remove(osc);
    t.replace(cup, QStringLiteral("\n"));
    t.remove(csi);
    t.remove(other);
    t.remove(QLatin1Char('\r'));
    t.remove(QChar(0x1b));
    static const QRegularExpression blank(QStringLiteral("\n[ \t]*\n+"));
    t.replace(blank, QStringLiteral("\n"));
    return t.trimmed().isEmpty() ? QString() : t;
}

/// Show a Workshop item's page: in Steam when it's running, else the browser.
/// Returns the URL it opened.
QString openWorkshopPage(const QString &id, bool steamRunning) {
    const QString web = QStringLiteral("https://steamcommunity.com/sharedfiles/filedetails/?id=") + id;
    const QString url = steamRunning ? QStringLiteral("steam://url/CommunityFilePage/") + id : web;
    if (qEnvironmentVariableIsSet("BONK_SCREENSHOT")) { // developer aid: log it instead
        qInfo("open %s", qPrintable(url));
        return url;
    }
    if (!QDesktopServices::openUrl(QUrl(url)) && steamRunning) QDesktopServices::openUrl(QUrl(web));
    return url;
}

struct WorkshopTools {
    QString gmad, gmpublish; // paths
    bool gmadNative = false, gmpublishNative = false;
    QString proton, prefix, steamRoot;
    bool inFlatpak = false;

    void find(const SteamFind &sf) {
        inFlatpak = sf.inFlatpak;
        steamRoot = sf.steamRoot;
        proton = sf.proton();
        prefix = QStandardPaths::writableLocation(QStandardPaths::GenericDataLocation) + QStringLiteral("/bonk/proton");
        gmad = gmpublish = QString();
        gmadNative = gmpublishNative = false;
        if (sf.gmodDir.isEmpty()) return;
        const QString bin = sf.gmodDir + QStringLiteral("/bin");
        auto first = [&](const QStringList &rels, bool exec) {
            for (const QString &r : rels) {
                const QFileInfo fi(bin + QLatin1Char('/') + r);
                if (fi.isFile() && (!exec || fi.isExecutable())) return fi.absoluteFilePath();
            }
            return QString();
        };
        gmad = first({QStringLiteral("linux64/gmad"), QStringLiteral("linux64/gmad_linux"), QStringLiteral("gmad_linux"), QStringLiteral("linux32/gmad"),
                      QStringLiteral("linux32/gmad_linux")}, true);
        gmadNative = !gmad.isEmpty();
        if (gmad.isEmpty()) gmad = first({QStringLiteral("gmad.exe"), QStringLiteral("win64/gmad.exe")}, false);
        gmpublish = first({QStringLiteral("linux64/gmpublish"), QStringLiteral("linux64/gmpublish_linux"), QStringLiteral("gmpublish_linux"),
                           QStringLiteral("linux32/gmpublish"), QStringLiteral("linux32/gmpublish_linux")}, true);
        gmpublishNative = !gmpublish.isEmpty();
        if (gmpublish.isEmpty()) gmpublish = first({QStringLiteral("gmpublish.exe"), QStringLiteral("win64/gmpublish.exe")}, false);
    }

    bool ready(QString *why) const {
        if (gmad.isEmpty() || gmpublish.isEmpty()) {
            if (why) *why = QStringLiteral("Couldn't find gmad and gmpublish in Garry's Mod's bin folder. Check Settings → Garry's Mod.");
            return false;
        }
        if ((!gmadNative || !gmpublishNative) && proton.isEmpty()) {
            if (why) *why = QStringLiteral("Garry's Mod's gmad / gmpublish are Windows programs here, and no Proton was found to run them.");
            return false;
        }
        return true;
    }

    /// A path as the tool sees it.
    QString path(bool publish, const QString &p) const { return (publish ? gmpublishNative : gmadNative) ? p : winPath(p); }

    /// Start a tool; the caller connects its output and finish. args are already path()-converted.
    QProcess *start(QObject *parent, bool publish, const QStringList &args, const QString &workdir) const {
        const bool native = publish ? gmpublishNative : gmadNative;
        const QString tool = publish ? gmpublish : gmad;
        QStringList env = {QStringLiteral("SteamAppId=4000"), QStringLiteral("SteamGameId=4000"), QStringLiteral("STEAM_COMPAT_APP_ID=4000")};
        QStringList cmd;
        if (native) {
            env << QStringLiteral("LD_LIBRARY_PATH=") + QFileInfo(tool).absolutePath() + QLatin1Char(':') + QFileInfo(tool).absolutePath() + QStringLiteral("/..");
            cmd << tool << args;
        } else {
            QDir().mkpath(prefix);
            env << QStringLiteral("STEAM_COMPAT_DATA_PATH=") + prefix << QStringLiteral("STEAM_COMPAT_CLIENT_INSTALL_PATH=") + steamRoot
                << QStringLiteral("WINEDEBUG=-all");
            cmd << proton << QStringLiteral("run") << tool << args;
            // Windows console programs only print to a terminal under Wine: give them one
            if (inFlatpak || !QStandardPaths::findExecutable(QStringLiteral("script")).isEmpty()) {
                QStringList q;
                for (QString a : cmd) q << QLatin1Char('\'') + a.replace(QLatin1Char('\''), QStringLiteral("'\\''")) + QLatin1Char('\'');
                cmd = {QStringLiteral("script"), QStringLiteral("-qefc"), q.join(QLatin1Char(' ')), QStringLiteral("/dev/null")};
            }
        }
        auto *p = new QProcess(parent);
        p->setProcessChannelMode(QProcess::MergedChannels);
        p->setWorkingDirectory(workdir);
        if (inFlatpak) {
            QStringList a = {QStringLiteral("--host"), QStringLiteral("--directory=") + workdir};
            for (const QString &e : env) a << QStringLiteral("--env=") + e;
            p->setProgram(QStringLiteral("flatpak-spawn"));
            p->setArguments(a + cmd);
        } else {
            QProcessEnvironment pe = QProcessEnvironment::systemEnvironment();
            for (const QString &e : env) pe.insert(e.section(QLatin1Char('='), 0, 0), e.section(QLatin1Char('='), 1));
            p->setProcessEnvironment(pe);
            p->setProgram(cmd.first());
            p->setArguments(cmd.mid(1));
        }
        return p;
    }
};

// ------------------------------------------------------------------ data

struct WorkshopItem {
    QString id, title, preview; // preview: the thumbnail's URL
    qint64 updated = 0;         // unix time, 0 = unknown
};

QString workshopCacheDir() { return QStandardPaths::writableLocation(QStandardPaths::GenericCacheLocation) + QStringLiteral("/bonk/workshop"); }

/// `gmpublish list` output: one addon per line, its ID (a long number) then its title.
QList<WorkshopItem> parseGmpublishList(const QString &out, int *duplicates = nullptr) {
    QList<WorkshopItem> items;
    QSet<QString> seen;
    if (duplicates) *duplicates = 0;
    static const QRegularExpression re(QStringLiteral("^\\s*(\\d{6,})\\s*[\\t ,:|-]*\\s*\"?(.*?)\"?\\s*$"));
    for (const QString &l : out.split(QLatin1Char('\n'))) {
        const auto m = re.match(l.trimmed());
        if (!m.hasMatch()) continue;
        WorkshopItem it;
        it.id = m.captured(1);
        it.title = m.captured(2).trimmed();
        if (seen.contains(it.id)) {
            if (duplicates) ++*duplicates;
            continue;
        }
        seen.insert(it.id);
        items << it;
    }
    return items;
}

/// Your Workshop items from your public Steam profile's Workshop page
/// (steamcommunity.com/profiles/<id>/myworkshopfiles): each item's ID and title.
QList<WorkshopItem> parseWorkshopPage(const QString &html) {
    QList<WorkshopItem> out;
    static const QRegularExpression title(QStringLiteral("class=\"workshopItemTitle[^\"]*\"[^>]*>([^<]*)<"));
    static const QRegularExpression idRe(QStringLiteral("filedetails/\\?id=(\\d{6,})"));
    for (auto it = title.globalMatch(html); it.hasNext();) {
        const auto m = it.next();
        // the item's link comes just before its title
        const QString before = html.mid(std::max<qsizetype>(0, m.capturedStart() - 3000), std::min<qsizetype>(3000, m.capturedStart()));
        QString id;
        for (auto i = idRe.globalMatch(before); i.hasNext();) id = i.next().captured(1);
        if (id.isEmpty() || std::any_of(out.begin(), out.end(), [&](const WorkshopItem &w) { return w.id == id; })) continue;
        QString t = m.captured(1).trimmed();
        t.replace(QStringLiteral("&amp;"), QStringLiteral("&")).replace(QStringLiteral("&#39;"), QStringLiteral("'")).replace(QStringLiteral("&quot;"), QStringLiteral("\""));
        t.replace(QStringLiteral("&lt;"), QStringLiteral("<")).replace(QStringLiteral("&gt;"), QStringLiteral(">"));
        out << WorkshopItem{id, t, QString(), 0};
    }
    return out;
}

/// Read your Workshop items from your public profile page with curl (a few pages).
/// Works without gmpublish, as long as the profile's Workshop items are public.
QList<WorkshopItem> fetchPublicWorkshopList(const QString &steamId, QString *why) {
    QList<WorkshopItem> all;
    if (steamId.isEmpty()) {
        if (why) *why = QStringLiteral("no Steam account found in loginusers.vdf");
        return all;
    }
    if (QStandardPaths::findExecutable(QStringLiteral("curl")).isEmpty()) {
        if (why) *why = QStringLiteral("curl isn't installed");
        return all;
    }
    if (qEnvironmentVariableIsSet("BONK_WORKSHOP_PAGE")) { // developer aid: a saved page instead of Steam's site
        QFile f(qEnvironmentVariable("BONK_WORKSHOP_PAGE"));
        if (f.open(QIODevice::ReadOnly)) return parseWorkshopPage(QString::fromUtf8(f.readAll()));
    }
    for (int page = 1; page <= 10; ++page) {
        QProcess p;
        p.start(QStringLiteral("curl"), {QStringLiteral("-s"), QStringLiteral("-L"), QStringLiteral("-m"), QStringLiteral("20"),
                                         QStringLiteral("https://steamcommunity.com/profiles/%1/myworkshopfiles/?appid=4000&browsefilter=myfiles&numperpage=30&p=%2")
                                             .arg(steamId).arg(page)});
        if (!p.waitForFinished(25000)) {
            if (why) *why = QStringLiteral("Steam's site didn't answer");
            break;
        }
        const QList<WorkshopItem> got = parseWorkshopPage(QString::fromUtf8(p.readAllStandardOutput()));
        int added = 0;
        for (const WorkshopItem &w : got)
            if (!std::any_of(all.begin(), all.end(), [&](const WorkshopItem &x) { return x.id == w.id; })) {
                all << w;
                ++added;
            }
        if (added < 30) break; // the last page
    }
    if (all.isEmpty() && why && why->isEmpty()) *why = QStringLiteral("no public Workshop items on your profile (or it's private)");
    return all;
}

/// Steam's public Workshop lookup (GetPublishedFileDetails) → titles, thumbnails, dates.
void applyWorkshopDetails(QList<WorkshopItem> &items, const QByteArray &json) {
    const QJsonArray arr = QJsonDocument::fromJson(json).object().value(QStringLiteral("response")).toObject().value(QStringLiteral("publishedfiledetails")).toArray();
    for (const QJsonValue &v : arr) {
        const QJsonObject o = v.toObject();
        if (o.value(QStringLiteral("result")).toInt() != 1) continue;
        const QString id = o.value(QStringLiteral("publishedfileid")).toString();
        for (WorkshopItem &it : items) {
            if (it.id != id) continue;
            const QString t = o.value(QStringLiteral("title")).toString();
            if (!t.isEmpty()) it.title = t;
            it.preview = o.value(QStringLiteral("preview_url")).toString();
            it.updated = qint64(o.value(QStringLiteral("time_updated")).toDouble());
        }
    }
}

QJsonArray itemsToJson(const QList<WorkshopItem> &items) {
    QJsonArray a;
    for (const WorkshopItem &it : items) a.append(QJsonObject{{"id", it.id}, {"title", it.title}, {"preview", it.preview}, {"updated", double(it.updated)}});
    return a;
}
QList<WorkshopItem> itemsFromJson(const QJsonArray &a) {
    QList<WorkshopItem> out;
    for (const QJsonValue &v : a) {
        const QJsonObject o = v.toObject();
        out << WorkshopItem{o.value("id").toString(), o.value("title").toString(), o.value("preview").toString(), qint64(o.value("updated").toDouble())};
    }
    return out;
}

/// The project's Workshop settings: <project>/Workshop/workshop.json and thumbnail.jpg.
struct WorkshopProject {
    QString id; // the Workshop item this project updates ("" = never published)
    QString title, description, type = QStringLiteral("model");
    QStringList tags;

    static QString dir(const Project &p) { return p.dir() + QStringLiteral("/Workshop"); }
    static QString thumbPath(const Project &p) { return dir(p) + QStringLiteral("/thumbnail.jpg"); }

    void load(const Project &p) {
        title = p.name;
        QFile f(dir(p) + QStringLiteral("/workshop.json"));
        if (!f.open(QIODevice::ReadOnly)) return;
        const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
        id = o.value("id").toString();
        title = o.value("title").toString(title);
        description = o.value("description").toString();
        type = o.value("type").toString(type);
        tags.clear();
        for (const QJsonValue &t : o.value("tags").toArray()) tags << t.toString();
    }
    void save(const Project &p) const {
        QDir().mkpath(dir(p));
        QJsonArray t;
        for (const QString &s : tags) t.append(s);
        QFile f(dir(p) + QStringLiteral("/workshop.json"));
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
            f.write(QJsonDocument(QJsonObject{{"id", id}, {"title", title}, {"description", description}, {"type", type}, {"tags", t}}).toJson());
    }
};

const char *kWorkshopTypes[] = {"model", "ServerContent", "gamemode", "map", "weapon", "vehicle", "npc", "tool", "effects", "entity"};
const char *kWorkshopTags[] = {"fun", "roleplay", "scenic", "movie", "realism", "cartoon", "water", "comic", "build"};

/// Files gmad accepts (Garry's Mod's addon whitelist, the parts props use).
bool gmadAllows(const QString &rel) {
    static const QStringList pats = {
        QStringLiteral("lua/*.lua"), QStringLiteral("scenes/*.vcd"), QStringLiteral("particles/*.pcf"), QStringLiteral("resource/fonts/*.ttf"),
        QStringLiteral("scripts/vehicles/*.txt"), QStringLiteral("resource/localization/*/*.properties"), QStringLiteral("maps/*.bsp"),
        QStringLiteral("maps/*.lmp"), QStringLiteral("maps/*.nav"), QStringLiteral("maps/*.ain"), QStringLiteral("maps/thumb/*.png"),
        QStringLiteral("sound/*.wav"), QStringLiteral("sound/*.mp3"), QStringLiteral("sound/*.ogg"), QStringLiteral("materials/*.vmt"),
        QStringLiteral("materials/*.vtf"), QStringLiteral("materials/*.png"), QStringLiteral("materials/*.jpg"), QStringLiteral("materials/*.jpeg"),
        QStringLiteral("materials/colorcorrection/*.raw"), QStringLiteral("models/*.mdl"), QStringLiteral("models/*.vtx"), QStringLiteral("models/*.phy"),
        QStringLiteral("models/*.ani"), QStringLiteral("models/*.vvd"), QStringLiteral("gamemodes/*/*.txt"), QStringLiteral("gamemodes/*/*.fgd"),
        QStringLiteral("gamemodes/*/logo.png"), QStringLiteral("gamemodes/*/icon24.png"), QStringLiteral("gamemodes/*/gamemode/*.lua"),
        QStringLiteral("gamemodes/*/entities/*.lua"), QStringLiteral("gamemodes/*/backgrounds/*.jpg"), QStringLiteral("gamemodes/*/backgrounds/*.png"),
        QStringLiteral("data_static/*.txt"), QStringLiteral("data_static/*.json"), QStringLiteral("data_static/*.dat"), QStringLiteral("data_static/*.xml"),
        QStringLiteral("data_static/*.csv"), QStringLiteral("shaders/fxc/*.vcs")};
    static QList<QRegularExpression> res;
    if (res.isEmpty())
        for (const QString &p : pats) {
            QString r = QRegularExpression::escape(p);
            r.replace(QStringLiteral("\\*"), QStringLiteral(".*"));
            res << QRegularExpression(QStringLiteral("^") + r + QStringLiteral("$"), QRegularExpression::CaseInsensitiveOption);
        }
    if (rel == QStringLiteral("addon.json")) return true;
    for (const QRegularExpression &re : res)
        if (re.match(rel).hasMatch()) return true;
    return false;
}

/// What's in the addon folder before publishing.
struct AddonCheck {
    int models = 0;
    bool addonJson = false, spawnlist = false;
    QStringList skipped; // files the Workshop won't take
    QDateTime newestModel;

    void run(const QString &addon) {
        *this = AddonCheck();
        QDirIterator it(addon, QDir::Files | QDir::Hidden, QDirIterator::Subdirectories);
        while (it.hasNext()) {
            const QString f = it.next();
            const QString rel = QDir(addon).relativeFilePath(f);
            if (rel.endsWith(QStringLiteral(".mdl"), Qt::CaseInsensitive) && rel.startsWith(QStringLiteral("models/"))) {
                ++models;
                const QDateTime t = QFileInfo(f).lastModified();
                if (!newestModel.isValid() || t > newestModel) newestModel = t;
            }
            if (rel == QStringLiteral("addon.json")) addonJson = true;
            if (rel.startsWith(QStringLiteral("lua/autorun/client/bonk_spawnlist_"))) spawnlist = true;
            if (!gmadAllows(rel)) skipped << rel;
        }
    }
};

/// gmpublish takes change notes in plain ASCII only.
QString asciiNotes(QString s) {
    s.replace(QChar(0x2018), QLatin1Char('\'')).replace(QChar(0x2019), QLatin1Char('\''));
    s.replace(QChar(0x201c), QLatin1Char('"')).replace(QChar(0x201d), QLatin1Char('"'));
    s.replace(QChar(0x2013), QLatin1Char('-')).replace(QChar(0x2014), QLatin1Char('-')).replace(QChar(0x2026), QStringLiteral("..."));
    QString o;
    for (QChar c : s) o += (c.unicode() < 128 && (c.isPrint() || c == QLatin1Char('\n'))) ? c : QLatin1Char('?');
    o.replace(QLatin1Char('"'), QLatin1Char('\''));
    return o.trimmed();
}

/// A JPG the Workshop takes: 512 × 512, baseline, 4:2:0.
bool saveWorkshopJpg(const QImage &src, const QString &path, QString *err) {
    QImage img = src.convertToFormat(QImage::Format_RGB32);
    if (img.size() != QSize(512, 512)) {
        const QImage scaled = img.scaled(512, 512, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation);
        img = scaled.copy((scaled.width() - 512) / 2, (scaled.height() - 512) / 2, 512, 512);
    }
    QDir().mkpath(QFileInfo(path).absolutePath());
    QImageWriter w(path, "jpeg");
    w.setQuality(92);
    w.setProgressiveScanWrite(false);
    w.setOptimizedWrite(false);
    w.setSubType("4:2:0"); // Qt 6.? honours it; libjpeg's default is 4:2:0 anyway
    if (!w.write(img)) {
        if (err) *err = w.errorString();
        return false;
    }
    return true;
}

// ------------------------------------------------------------------ snap a thumbnail

/// Snap Thumbnail from Model: the spawn icon camera at 512 × 512, on a background.
class SnapThumbDialog : public QDialog {
public:
    QImage result;

    SnapThumbDialog(QWidget *parent, const Project &pj, ThumbRenderer render, std::function<void(bool)> setGrid)
        : QDialog(parent), pj(pj), render(std::move(render)), setGrid(std::move(setGrid)) {
        setWindowTitle(QStringLiteral("Snap Thumbnail from Model"));
        models = projectModels(pj);
        spawn.load(pj);
        auto *root = new QVBoxLayout(this);
        auto *h = new QHBoxLayout;
        view = new ThumbView;
        view->setFixedSize(384, 384);
        view->frame = QSize(384, 384);
        h->addWidget(view);
        auto *side = new QVBoxLayout;
        side->addWidget(new QLabel(QStringLiteral("<b>Model</b>")));
        modelBox = new QComboBox;
        for (const auto &m : models) modelBox->addItem(QFileInfo(m.second).completeBaseName(), m.second);
        side->addWidget(modelBox);
        side->addSpacing(6);
        side->addWidget(new QLabel(QStringLiteral("<b>Background</b>")));
        auto *bgRow = new QHBoxLayout;
        for (const QColor &c : {QColor(0x3d6b8f), QColor(0x2b2f33), QColor(0xe9e4d8), QColor(0x6b8f3d)}) {
            auto *b = new QPushButton;
            b->setFixedSize(34, 34);
            b->setToolTip(c.name());
            b->setStyleSheet(QStringLiteral("QPushButton{background:%1;border:2px solid #4d5359;border-radius:6px}").arg(c.name()));
            QObject::connect(b, &QPushButton::clicked, this, [this, c] {
                bg = c;
                draw();
            });
            bgRow->addWidget(b);
        }
        auto *pick = new QPushButton(QStringLiteral("Pick Color…"));
        QObject::connect(pick, &QPushButton::clicked, this, [this] {
            const QColor c = QColorDialog::getColor(bg, this, QStringLiteral("Background"));
            if (c.isValid()) {
                bg = c;
                draw();
            }
        });
        bgRow->addWidget(pick);
        bgRow->addStretch(1);
        side->addLayout(bgRow);
        side->addWidget(hintLabel(QStringLiteral("The Workshop thumbnail is a JPG, so it gets a solid background.")));
        auto *btns = new QHBoxLayout;
        auto *fit = new QPushButton(QStringLiteral("Fit"));
        auto *reset = new QPushButton(QStringLiteral("Reset Angle"));
        spawnAngle = new QPushButton(QStringLiteral("Use Spawn Icon Angle"));
        spawnAngle->setToolTip(QStringLiteral("The angle you gave this model's spawn icon in the Spawnlist Editor"));
        btns->addWidget(fit);
        btns->addWidget(reset);
        btns->addWidget(spawnAngle);
        side->addLayout(btns);
        grid = new QCheckBox(QStringLiteral("Show the floor grid under the model"));
        grid->setChecked(QSettings().value("workshop/snapGrid", true).toBool());
        side->addWidget(grid);
        side->addStretch(1);
        side->addWidget(hintLabel(QStringLiteral("Drag: turn · Right-drag: move · Wheel: zoom. Saved as a 512 × 512 JPG in the project "
                                                 "(Workshop/thumbnail.jpg).")));
        h->addLayout(side, 1);
        root->addLayout(h);
        auto *bb = new QDialogButtonBox(QDialogButtonBox::Cancel);
        auto *use = bb->addButton(QStringLiteral("Use This Picture"), QDialogButtonBox::AcceptRole);
        use->setDefault(true);
        root->addWidget(bb);
        QObject::connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
        QObject::connect(use, &QPushButton::clicked, this, [this] {
            result = snap(512);
            if (!result.isNull()) accept();
        });
        QObject::connect(modelBox, &QComboBox::currentIndexChanged, this, [this] {
            cam = ThumbCam();
            cam.yaw = -55;
            cam.pitch = 22;
            refit();
        });
        QObject::connect(fit, &QPushButton::clicked, this, [this] { refit(); });
        QObject::connect(reset, &QPushButton::clicked, this, [this] {
            cam = ThumbCam();
            cam.yaw = -55;
            cam.pitch = 22;
            refit();
        });
        QObject::connect(spawnAngle, &QPushButton::clicked, this, [this] {
            if (const SpawnItem *it = spawnItem()) {
                cam = it->cam;
                draw();
            }
        });
        QObject::connect(grid, &QCheckBox::toggled, this, [this](bool on) {
            QSettings().setValue("workshop/snapGrid", on);
            draw();
        });
        timer.setSingleShot(true);
        timer.setInterval(30);
        QObject::connect(&timer, &QTimer::timeout, this, [this] { draw(); });
        view->cam = &cam;
        view->moved = [this] { timer.start(); };
        cam.yaw = -55;
        cam.pitch = 22;
        refit();
    }

private:
    Project pj;
    ThumbRenderer render;
    std::function<void(bool)> setGrid;
    QList<QPair<QString, QString>> models;
    Spawnlists spawn;
    ThumbView *view;
    QComboBox *modelBox;
    QPushButton *spawnAngle;
    QCheckBox *grid;
    QColor bg = QColor(0x3d6b8f);
    ThumbCam cam;
    QTimer timer;

    QString source() const {
        const QString rel = modelBox->currentData().toString();
        return QFileInfo(rel).isRelative() ? pj.dir() + QLatin1Char('/') + rel : rel;
    }

    /// The model's spawn icon in the spawnlist, if it has one.
    const SpawnItem *spawnItem() const {
        const QString rel = modelBox->currentData().toString();
        std::function<const SpawnItem *(const SpawnCategory &)> find = [&](const SpawnCategory &c) -> const SpawnItem * {
            for (const SpawnItem &it : c.items)
                if (!it.header && it.source == rel && it.cam.dist > 0) return &it;
            for (const SpawnCategory &k : c.children)
                if (const SpawnItem *f = find(k)) return f;
            return nullptr;
        };
        for (const SpawnCategory &c : spawn.lists)
            if (const SpawnItem *f = find(c)) return f;
        return nullptr;
    }

    void refit() {
        if (modelBox->count() == 0) return;
        cam = fitCam(source(), cam, 1.0);
        spawnAngle->setEnabled(spawnItem() != nullptr);
        draw();
    }

    QImage snap(int size) {
        if (modelBox->count() == 0 || !render) return QImage();
        if (setGrid) setGrid(grid->isChecked());
        const QImage model = render(source(), cam, size, size);
        if (setGrid) setGrid(false);
        QImage out(size, size, QImage::Format_RGB32);
        out.fill(bg);
        QPainter p(&out);
        p.drawImage(0, 0, model);
        return out;
    }

    void draw() { view->setPixmap(QPixmap::fromImage(snap(384))); }
};

// ------------------------------------------------------------------ the window

class WorkshopDialog : public QDialog {
public:
    std::function<void(const QString &)> status;

    WorkshopDialog(QWidget *parent, const Project &pj, const SteamFind &sf, const SteamStatus &st, ThumbRenderer render,
                   std::function<void(bool)> setGrid, std::function<bool()> compile)
        : QDialog(parent), pj(pj), render(std::move(render)), setGrid(std::move(setGrid)), compile(std::move(compile)) {
        setWindowTitle(QStringLiteral("Manage Workshop — %1").arg(pj.name));
        tools.find(sf);
        steamId = st.steamId;
        steamRunning = st.running || st.unsure;
        wp.load(pj);
        auto *root = new QHBoxLayout(this);
        root->setContentsMargins(0, 0, 0, 0);
        root->setSpacing(0);

        // --- left: your addons
        auto *left = new QWidget;
        left->setMinimumWidth(320);
        left->setMaximumWidth(400);
        left->setAutoFillBackground(true);
        QPalette lp = left->palette();
        lp.setColor(QPalette::Window, lp.color(QPalette::Window).darker(108));
        left->setPalette(lp);
        auto *lv = new QVBoxLayout(left);
        auto *head = new QHBoxLayout;
        head->addWidget(new QLabel(QStringLiteral("<b>Your addons</b>")));
        head->addStretch(1);
        signedIn = new QLabel(st.persona.isEmpty() ? QStringLiteral("Steam: signed in") : QStringLiteral("Signed in as %1").arg(st.persona.toHtmlEscaped()));
        signedIn->setEnabled(false);
        head->addWidget(signedIn);
        lv->addLayout(head);
        filter = new QLineEdit;
        filter->setPlaceholderText(QStringLiteral("Filter…"));
        filter->setClearButtonEnabled(true);
        lv->addWidget(filter);
        list = new QListWidget;
        list->setIconSize(QSize(64, 64));
        list->setSpacing(2);
        list->setUniformItemSizes(true);
        lv->addWidget(list, 1);
        auto *foot = new QHBoxLayout;
        countLabel = new QLabel;
        countLabel->setWordWrap(true);
        QObject::connect(countLabel, &QLabel::linkActivated, this, [this] { showListDetails(); });
        auto *refreshBtn = new QPushButton(QStringLiteral("Refresh"));
        foot->addWidget(countLabel, 1);
        foot->addWidget(refreshBtn);
        lv->addLayout(foot);
        auto *newBtn = new QPushButton(QStringLiteral("+ New Submission"));
        lv->addWidget(newBtn);
        root->addWidget(left);

        // --- right: the form
        auto *right = new QWidget;
        auto *rv = new QVBoxLayout(right);
        rv->setContentsMargins(18, 14, 18, 12);
        banner = new QLabel;
        banner->setWordWrap(true);
        banner->setTextFormat(Qt::RichText);
        banner->setOpenExternalLinks(true);
        banner->setStyleSheet(QStringLiteral("QLabel{background:palette(highlight);color:palette(highlighted-text);border-radius:6px;padding:10px 12px}"));
        heading = new QLabel;
        QFont hf = uiScaled(heading->font(), 1.4);
        hf.setBold(true);
        heading->setFont(hf);
        rv->addWidget(heading);
        rv->addWidget(banner);
        auto *body = new QHBoxLayout;
        body->setSpacing(18);
        // thumbnail column
        auto *tc = new QVBoxLayout;
        tc->addWidget(new QLabel(QStringLiteral("<b>Thumbnail</b>")));
        thumb = new QLabel;
        thumb->setFixedSize(236, 236);
        thumb->setAlignment(Qt::AlignCenter);
        thumb->setFrameShape(QFrame::StyledPanel);
        tc->addWidget(thumb);
        replaceThumb = new QCheckBox(QStringLiteral("Replace the thumbnail too"));
        tc->addWidget(replaceThumb);
        auto *tb = new QHBoxLayout;
        chooseImg = new QPushButton(QStringLiteral("Choose Image…"));
        snapImg = new QPushButton(QStringLiteral("Snap from Model…"));
        tb->addWidget(chooseImg);
        tb->addWidget(snapImg);
        tc->addLayout(tb);
        tc->addStretch(1);
        body->addLayout(tc);
        // fields
        auto *fc = new QVBoxLayout;
        fc->setSpacing(10);
        titleLabel = new QLabel(QStringLiteral("<b>Title</b>"));
        title = new QLineEdit(wp.title);
        descLabel = new QLabel(QStringLiteral("<b>Description</b>"));
        desc = new QPlainTextEdit(wp.description);
        desc->setPlaceholderText(QStringLiteral("What's in it, and where to find it in the spawn menu"));
        desc->setFixedHeight(80);
        notesLabel = new QLabel(QStringLiteral("<b>Change notes</b> <span style='color:gray'>(shown on the Workshop page; plain ASCII)</span>"));
        notes = new QPlainTextEdit;
        notes->setPlaceholderText(QStringLiteral("What changed: e.g. added two-story houses, fixed collision on the garage"));
        notes->setFixedHeight(100);
        fc->addWidget(titleLabel);
        fc->addWidget(title);
        fc->addWidget(descLabel);
        fc->addWidget(desc);
        fc->addWidget(notesLabel);
        fc->addWidget(notes);
        fc->addWidget(new QLabel(QStringLiteral("<b>Addon folder</b>")));
        auto *folder = new QLabel(QStringLiteral("<tt>%1</tt>").arg(tildePath(pj.addonDir()).toHtmlEscaped()));
        folder->setTextInteractionFlags(Qt::TextSelectableByMouse);
        fc->addWidget(folder);
        typeTagRow = new QWidget;
        auto *ttr = new QHBoxLayout(typeTagRow);
        ttr->setContentsMargins(0, 0, 0, 0);
        auto *typeCol = new QVBoxLayout;
        typeCol->addWidget(new QLabel(QStringLiteral("<b>Type</b>")));
        typeBox = new QComboBox;
        for (const char *t : kWorkshopTypes) typeBox->addItem(QString::fromLatin1(t) == QStringLiteral("ServerContent") ? QStringLiteral("Server content") : QString::fromLatin1(t).left(1).toUpper() + QString::fromLatin1(t).mid(1), QString::fromLatin1(t));
        typeBox->setCurrentIndex(std::max(0, typeBox->findData(wp.type)));
        typeCol->addWidget(typeBox);
        typeCol->addStretch(1);
        ttr->addLayout(typeCol);
        auto *tagCol = new QVBoxLayout;
        tagCol->addWidget(new QLabel(QStringLiteral("<b>Tags</b> <span style='color:gray'>(up to 2)</span>")));
        auto *tagGrid = new QGridLayout;
        int i = 0;
        for (const char *t : kWorkshopTags) {
            auto *b = new QPushButton(QString::fromLatin1(t).left(1).toUpper() + QString::fromLatin1(t).mid(1));
            b->setCheckable(true);
            b->setChecked(wp.tags.contains(QString::fromLatin1(t)));
            b->setProperty("tag", QString::fromLatin1(t));
            QObject::connect(b, &QPushButton::toggled, this, [this, b](bool on) {
                if (on && checkedTags().size() > 2) b->setChecked(false); // two at most
            });
            tagButtons << b;
            tagGrid->addWidget(b, i / 5, i % 5);
            ++i;
        }
        tagCol->addLayout(tagGrid);
        ttr->addLayout(tagCol, 1);
        fc->addWidget(typeTagRow);
        keepNote = hintLabel(QStringLiteral("Title, description, type and tags stay as they are on the Workshop; change those on its page."));
        fc->addWidget(keepNote);
        checks = new QLabel;
        checks->setTextFormat(Qt::RichText);
        checks->setWordWrap(true);
        checks->setFrameShape(QFrame::StyledPanel);
        checks->setMargin(8);
        fc->addWidget(checks);
        fc->addStretch(1);
        body->addLayout(fc, 1);
        rv->addLayout(body, 1);
        auto *bb = new QHBoxLayout;
        bb->addStretch(1);
        auto *closeBtn = new QPushButton(QStringLiteral("Close"));
        publishBtn = new QPushButton;
        publishBtn->setDefault(true);
        bb->addWidget(closeBtn);
        bb->addWidget(publishBtn);
        rv->addLayout(bb);
        root->addWidget(right, 1);

        QObject::connect(closeBtn, &QPushButton::clicked, this, &QDialog::close);
        QObject::connect(newBtn, &QPushButton::clicked, this, [this] { select(QString()); });
        QObject::connect(refreshBtn, &QPushButton::clicked, this, [this] { fetchList(); });
        QObject::connect(filter, &QLineEdit::textChanged, this, [this] { fillList(); });
        QObject::connect(list, &QListWidget::itemClicked, this, [this](QListWidgetItem *it) { select(it->data(Qt::UserRole).toString()); });
        QObject::connect(chooseImg, &QPushButton::clicked, this, [this] { chooseThumb(); });
        QObject::connect(snapImg, &QPushButton::clicked, this, [this] { snapThumb(); });
        QObject::connect(replaceThumb, &QCheckBox::toggled, this, [this] { syncMode(); });
        QObject::connect(publishBtn, &QPushButton::clicked, this, [this] { publish(); });

        items = itemsFromJson(QJsonDocument::fromJson(QSettings().value("workshop/items").toByteArray()).array());
        fillList();
        select(wp.id);
        runChecks();
        loadThumb();
        resize(1180, 740);
        if (!qEnvironmentVariableIsSet("BONK_SCREENSHOT")) QTimer::singleShot(0, this, [this] { fetchList(); });
    }

    ~WorkshopDialog() override {
        // closing while gmpublish or curl still runs: stop them first, so their
        // "finished" doesn't reach a half-gone window
        for (QProcess *p : findChildren<QProcess *>()) {
            p->disconnect();
            p->kill();
            p->waitForFinished(2000);
        }
    }

    /// Developer aid: press Publish (with change notes / a title filled in).
    void devPublish(const QString &text) {
        if (current.isEmpty()) title->setText(text.isEmpty() ? title->text() : text);
        else notes->setPlainText(text);
        publish();
    }
    void devSelect(const QString &id) { select(id); }
    void devRefresh() {
        fetchList();
        QElapsedTimer t;
        t.start();
        while (proc && t.elapsed() < 10000) QApplication::processEvents(QEventLoop::AllEvents, 50);
    }
    void devSnap(const QString &shot) {
        SnapThumbDialog d(this, pj, render, setGrid);
        d.show();
        for (int i = 0; i < 5; ++i) QApplication::processEvents();
        d.grab().save(shot);
    }

    /// Developer aid: a list as if gmpublish and Steam answered.
    void fakeList(const QString &listOut, const QByteArray &details) {
        items = parseGmpublishList(listOut, &duplicates);
        applyWorkshopDetails(items, details);
        fillList();
        select(wp.id);
    }

private:
    Project pj;
    ThumbRenderer render;
    std::function<void(bool)> setGrid;
    std::function<bool()> compile;
    WorkshopTools tools;
    WorkshopProject wp;
    QList<WorkshopItem> items;
    QString current; // the item being updated ("" = new submission)
    int duplicates = 0;
    QLabel *signedIn, *countLabel, *banner, *heading, *thumb, *titleLabel, *descLabel, *notesLabel, *keepNote, *checks;
    QLineEdit *filter, *title;
    QListWidget *list;
    QCheckBox *replaceThumb;
    QPushButton *chooseImg, *snapImg, *publishBtn;
    QPlainTextEdit *desc, *notes;
    QWidget *typeTagRow;
    QComboBox *typeBox;
    QList<QPushButton *> tagButtons;
    QProcess *proc = nullptr;
    QByteArray procOut;
    QString lastListOutput, steamId, listCommand;
    bool steamRunning = true;
    bool listedFromProfile = false;

    /// What gmpublish said when it was asked for the list (to show and send along).
    void showListDetails() {
        QDialog d(this);
        d.setWindowTitle(QStringLiteral("gmpublish list"));
        auto *v = new QVBoxLayout(&d);
        v->addWidget(hintLabel(QStringLiteral("What <b>%1 list</b> printed%2. If Steam isn't reachable from it, publishing won't work either: send this "
                                              "to whoever helps you with Bonk.")
                                   .arg(QFileInfo(tools.gmpublish).fileName().toHtmlEscaped(),
                                        tools.gmpublishNative ? QString() : QStringLiteral(" (through Proton: %1)").arg(tildePath(tools.proton).toHtmlEscaped()))));
        auto *log = new QPlainTextEdit(lastListOutput.isEmpty() ? QStringLiteral("(nothing)") : lastListOutput);
        log->setReadOnly(true);
        log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        v->addWidget(log, 1);
        auto *bb = new QDialogButtonBox(QDialogButtonBox::Close);
        auto *copy = bb->addButton(QStringLiteral("Copy"), QDialogButtonBox::ActionRole);
        QObject::connect(copy, &QPushButton::clicked, &d, [log] { QApplication::clipboard()->setText(log->toPlainText()); });
        QObject::connect(bb, &QDialogButtonBox::rejected, &d, &QDialog::reject);
        v->addWidget(bb);
        d.resize(720, 420);
        d.exec();
    }

    QStringList checkedTags() const {
        QStringList t;
        for (QPushButton *b : tagButtons)
            if (b->isChecked()) t << b->property("tag").toString();
        return t;
    }

    const WorkshopItem *item(const QString &id) const {
        for (const WorkshopItem &it : items)
            if (it.id == id) return &it;
        return nullptr;
    }

    QIcon iconFor(const WorkshopItem &it) const {
        const QImage img(workshopCacheDir() + QLatin1Char('/') + it.id + QStringLiteral(".jpg"));
        if (img.isNull()) return QIcon();
        return QIcon(QPixmap::fromImage(img.scaled(64, 64, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    }

    void fillList() {
        list->clear();
        const QString f = filter->text().trimmed();
        for (const WorkshopItem &it : items) {
            if (!f.isEmpty() && !it.title.contains(f, Qt::CaseInsensitive) && !it.id.contains(f)) continue;
            const QString when = it.updated ? QLocale().toString(QDateTime::fromSecsSinceEpoch(it.updated).date(), QLocale::ShortFormat) : QStringLiteral("—");
            auto *li = new QListWidgetItem(iconFor(it), QStringLiteral("%1\nID %2 · Updated %3").arg(it.title.isEmpty() ? QStringLiteral("(untitled)") : it.title, it.id, when));
            li->setData(Qt::UserRole, it.id);
            li->setSizeHint(QSize(0, 72));
            list->addItem(li);
            if (it.id == current) li->setSelected(true);
        }
        countLabel->setToolTip(QString());
        countLabel->setText(items.isEmpty() ? QStringLiteral("No addons listed yet")
                                            : QStringLiteral("%1 addon%2%3").arg(items.size()).arg(items.size() == 1 ? QString() : QStringLiteral("s"))
                                                  .arg(duplicates ? QStringLiteral(" · %1 duplicate%2 hidden").arg(duplicates).arg(duplicates == 1 ? QString() : QStringLiteral("s")) : QString())
                                                  + (listedFromProfile ? QStringLiteral(" · <a href='details'>from your profile</a>") : QString()));
    }

    /// Update `id`, or a new submission ("").
    void select(const QString &id) {
        current = item(id) || (!id.isEmpty() && id == wp.id) ? id : QString();
        for (int i = 0; i < list->count(); ++i) list->item(i)->setSelected(list->item(i)->data(Qt::UserRole).toString() == current);
        syncMode();
    }

    void syncMode() {
        const bool update = !current.isEmpty();
        const WorkshopItem *it = item(current);
        const QString name = it && !it->title.isEmpty() ? it->title : wp.title;
        heading->setText(update ? QStringLiteral("Update") : QStringLiteral("New submission"));
        banner->setVisible(update);
        if (update)
            banner->setText(QStringLiteral("<b>You are updating %1</b><br>Workshop ID %2 · this project's addon replaces what's on the Workshop now · "
                                           "<a href='steam://url/CommunityFilePage/%2' style='color:inherit'>Open in Steam</a>")
                                .arg(name.toHtmlEscaped(), current));
        for (QWidget *w : std::initializer_list<QWidget *>{titleLabel, title, descLabel, desc, typeTagRow}) w->setVisible(!update);
        notesLabel->setVisible(update);
        notes->setVisible(update);
        keepNote->setVisible(update);
        replaceThumb->setVisible(update);
        const bool thumbOn = !update || replaceThumb->isChecked();
        chooseImg->setEnabled(thumbOn);
        snapImg->setEnabled(thumbOn);
        publishBtn->setText(update ? QStringLiteral("Publish Update") : QStringLiteral("Publish to Workshop"));
    }

    void loadThumb() {
        const QImage img(WorkshopProject::thumbPath(pj));
        if (img.isNull()) {
            thumb->setText(QStringLiteral("No thumbnail yet\n\n512 × 512 JPG"));
            return;
        }
        thumb->setPixmap(QPixmap::fromImage(img.scaled(232, 232, Qt::KeepAspectRatio, Qt::SmoothTransformation)));
    }

    void chooseThumb() {
        const QString f = QFileDialog::getOpenFileName(this, QStringLiteral("Workshop thumbnail"), pj.dir(), QStringLiteral("Images (*.png *.jpg *.jpeg)"));
        if (f.isEmpty()) return;
        QString err;
        const QImage img(f);
        if (img.isNull() || !saveWorkshopJpg(img, WorkshopProject::thumbPath(pj), &err))
            QMessageBox::warning(this, windowTitle(), QStringLiteral("Couldn't use that image. %1").arg(err));
        loadThumb();
    }

    void snapThumb() {
        if (projectModels(pj).isEmpty()) {
            QMessageBox::information(this, windowTitle(), QStringLiteral("This project has no models to take a picture of yet."));
            return;
        }
        SnapThumbDialog d(this, pj, render, setGrid);
        if (d.exec() != QDialog::Accepted) return;
        QString err;
        if (!saveWorkshopJpg(d.result, WorkshopProject::thumbPath(pj), &err)) QMessageBox::warning(this, windowTitle(), err);
        loadThumb();
    }

    void runChecks() {
        AddonCheck c;
        c.run(pj.addonDir());
        auto row = [](bool ok, const QString &t) {
            return QStringLiteral("<div>%1 %2</div>").arg(ok ? QStringLiteral("<span style='color:#3fae6a'>✔</span>") : QStringLiteral("<span style='color:#d8a531'>⚠</span>"), t);
        };
        QString h = QStringLiteral("<b>Before publishing</b>");
        h += row(c.models > 0, c.models ? QStringLiteral("%1 compiled model%2 in models/").arg(c.models).arg(c.models == 1 ? QString() : QStringLiteral("s"))
                                        : QStringLiteral("No compiled models yet: compile first (File → Compile Model)"));
        if (c.newestModel.isValid()) {
            QDateTime newestSrc;
            for (const auto &m : projectModels(pj)) {
                const QDateTime t = QFileInfo(pj.dir() + QLatin1Char('/') + m.second).lastModified();
                if (!newestSrc.isValid() || t > newestSrc) newestSrc = t;
            }
            const bool fresh = !newestSrc.isValid() || c.newestModel >= newestSrc;
            h += row(fresh, fresh ? QStringLiteral("Compiled since the models last changed") : QStringLiteral("A model changed after the last compile: compile again first?"));
        }
        h += row(true, c.addonJson ? QStringLiteral("addon.json ready") : QStringLiteral("addon.json will be written"));
        Spawnlists sl;
        const bool wantSpawn = sl.load(pj) && !sl.empty();
        if (wantSpawn) h += row(c.spawnlist, c.spawnlist ? QStringLiteral("Spawnlist and spawn icons included") : QStringLiteral("The spawnlist isn't in the addon yet: it's written on the next compile"));
        if (!c.skipped.isEmpty())
            h += row(false, QStringLiteral("%1 file%2 the Workshop won't take will be left out: %3").arg(c.skipped.size()).arg(c.skipped.size() == 1 ? QString() : QStringLiteral("s"))
                                .arg(c.skipped.mid(0, 3).join(QStringLiteral(", ")).toHtmlEscaped() + (c.skipped.size() > 3 ? QStringLiteral("…") : QString())));
        QString why;
        if (!tools.ready(&why)) h += row(false, why.toHtmlEscaped());
        else h += row(true, QStringLiteral("gmad and gmpublish %1").arg(tools.gmadNative && tools.gmpublishNative ? QStringLiteral("(Linux)") : QStringLiteral("(through Proton)")));
        checks->setText(h);
        publishBtn->setEnabled(c.models > 0 && tools.ready(nullptr));
    }

    // ---- the list: gmpublish, then Steam's public details

    void fetchList() {
        if (proc) return;
        QString why;
        if (!tools.ready(&why)) { // no gmpublish: the profile page still lists them
            lastListOutput = why;
            listFromProfile();
            return;
        }
        countLabel->setText(QStringLiteral("Asking Steam for your addons…"));
        procOut.clear();
        proc = tools.start(this, true, {QStringLiteral("list")}, pj.dir());
        listCommand = QStringLiteral("$ %1 %2\n").arg(proc->program(), proc->arguments().join(QLatin1Char(' ')));
        QObject::connect(proc, &QProcess::readyRead, this, [this] { procOut += proc->readAll(); });
        QObject::connect(proc, &QProcess::finished, this, [this](int code, QProcess::ExitStatus st) {
            proc->deleteLater();
            proc = nullptr;
            const QString text = terminalText(procOut);
            lastListOutput = listCommand + text + (st == QProcess::NormalExit ? QStringLiteral("\n(exit code %1)").arg(code) : QStringLiteral("\n(it crashed)"));
            QList<WorkshopItem> got = parseGmpublishList(text, &duplicates);
            if (got.isEmpty()) {
                // gmpublish couldn't list them: your public Workshop page can
                countLabel->setText(QStringLiteral("gmpublish didn't list them; asking your Steam profile…"));
                QApplication::processEvents();
                QString why;
                got = fetchPublicWorkshopList(steamId, &why);
                duplicates = 0;
                listedFromProfile = !got.isEmpty();
                if (got.isEmpty()) {
                    countLabel->setText(QStringLiteral("<a href='details'>Couldn't list your addons: details</a>"));
                    countLabel->setToolTip(QStringLiteral("gmpublish listed nothing, and your profile page: %1").arg(why));
                    return;
                }
            } else {
                listedFromProfile = false;
            }
            // keep what we knew about each
            for (WorkshopItem &n : got)
                if (const WorkshopItem *o = item(n.id)) {
                    n.preview = o->preview;
                    n.updated = o->updated;
                    if (n.title.isEmpty()) n.title = o->title;
                }
            items = got;
            saveCache();
            fillList();
            select(current.isEmpty() ? wp.id : current);
            fetchDetails();
        });
        QObject::connect(proc, &QProcess::errorOccurred, this, [this](QProcess::ProcessError e) {
            if (e != QProcess::FailedToStart || !proc) return;
            lastListOutput = listCommand + QStringLiteral("Couldn't start it: ") + proc->errorString();
            proc->deleteLater();
            proc = nullptr;
            listFromProfile();
        });
        proc->start();
    }

    /// The list from your public profile page alone.
    void listFromProfile() {
        countLabel->setText(QStringLiteral("Asking your Steam profile for your addons…"));
        QApplication::processEvents();
        QString why;
        QList<WorkshopItem> got = fetchPublicWorkshopList(steamId, &why);
        duplicates = 0;
        listedFromProfile = !got.isEmpty();
        if (got.isEmpty()) {
            countLabel->setText(QStringLiteral("<a href='details'>Couldn't list your addons: details</a>"));
            countLabel->setToolTip(why);
            return;
        }
        for (WorkshopItem &n : got)
            if (const WorkshopItem *o = item(n.id)) {
                n.preview = o->preview;
                n.updated = o->updated;
            }
        items = got;
        saveCache();
        fillList();
        select(current.isEmpty() ? wp.id : current);
        fetchDetails();
    }

    void saveCache() { QSettings().setValue("workshop/items", QJsonDocument(itemsToJson(items)).toJson(QJsonDocument::Compact)); }

    /// Thumbnails, titles and dates from Steam's public Workshop lookup (with curl).
    void fetchDetails() {
        if (items.isEmpty() || QStandardPaths::findExecutable(QStringLiteral("curl")).isEmpty()) return;
        QStringList args = {QStringLiteral("-s"), QStringLiteral("-m"), QStringLiteral("20"), QStringLiteral("-d"), QStringLiteral("itemcount=%1").arg(items.size())};
        for (int i = 0; i < items.size(); ++i) args << QStringLiteral("-d") << QStringLiteral("publishedfileids[%1]=%2").arg(i).arg(items[i].id);
        args << QStringLiteral("https://api.steampowered.com/ISteamRemoteStorage/GetPublishedFileDetails/v1/");
        auto *p = new QProcess(this);
        QObject::connect(p, &QProcess::finished, this, [this, p] {
            applyWorkshopDetails(items, p->readAllStandardOutput());
            p->deleteLater();
            saveCache();
            fillList();
            syncMode();
            fetchThumbs();
        });
        p->start(QStringLiteral("curl"), args);
    }

    void fetchThumbs() {
        QDir().mkpath(workshopCacheDir());
        for (const WorkshopItem &it : items) {
            if (it.preview.isEmpty() || !it.preview.startsWith(QStringLiteral("https://"))) continue;
            const QString file = workshopCacheDir() + QLatin1Char('/') + it.id + QStringLiteral(".jpg");
            const QFileInfo fi(file);
            if (fi.exists() && fi.lastModified().toSecsSinceEpoch() >= it.updated) continue;
            auto *p = new QProcess(this);
            QObject::connect(p, &QProcess::finished, this, [this, p] {
                p->deleteLater();
                fillList();
            });
            p->start(QStringLiteral("curl"), {QStringLiteral("-s"), QStringLiteral("-m"), QStringLiteral("30"), QStringLiteral("-o"), file, it.preview});
        }
    }

    // ---- publishing

    void writeAddonJson() {
        const QString path = pj.addonDir() + QStringLiteral("/addon.json");
        QJsonObject o;
        QFile f(path);
        if (f.open(QIODevice::ReadOnly)) o = QJsonDocument::fromJson(f.readAll()).object();
        f.close();
        if (current.isEmpty()) {
            o["title"] = title->text().trimmed().isEmpty() ? pj.name : title->text().trimmed();
            o["type"] = typeBox->currentData().toString();
            QJsonArray t;
            for (const QString &s : checkedTags()) t.append(s);
            o["tags"] = t;
            if (!desc->toPlainText().trimmed().isEmpty()) o["description"] = desc->toPlainText().trimmed();
        } else {
            if (!o.contains("title")) o["title"] = wp.title;
            if (!o.contains("type")) o["type"] = wp.type;
            if (!o.contains("tags")) o["tags"] = QJsonArray();
        }
        if (!o.contains("ignore")) o["ignore"] = QJsonArray{"*.psd", "*.bonkmat", "*.txt"};
        if (f.open(QIODevice::WriteOnly | QIODevice::Truncate)) f.write(QJsonDocument(o).toJson());
    }

    void publish();
};

/// The publishing window: steps ticking off, gmad and gmpublish's output, the result.
class PublishDialog : public QDialog {
public:
    QString id; // the Workshop ID once known

    explicit PublishDialog(QWidget *parent, const QString &name) : QDialog(parent) {
        setWindowTitle(QStringLiteral("Publishing — %1").arg(name));
        auto *v = new QVBoxLayout(this);
        headline = new QLabel(QStringLiteral("<b>Publishing…</b>"));
        QFont hf = uiScaled(headline->font(), 1.3);
        headline->setFont(hf);
        v->addWidget(headline);
        stepsBox = new QVBoxLayout;
        v->addLayout(stepsBox);
        bar = new QProgressBar;
        bar->setRange(0, 0);
        bar->setTextVisible(false);
        bar->setFixedHeight(8);
        v->addWidget(bar);
        log = new QPlainTextEdit;
        log->setReadOnly(true);
        log->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));
        log->setMinimumHeight(160);
        v->addWidget(log, 1);
        note = hintLabel(QString());
        v->addWidget(note);
        auto *bb = new QHBoxLayout;
        copyBtn = new QPushButton(QStringLiteral("Copy Workshop Link"));
        bb->addWidget(copyBtn);
        bb->addStretch(1);
        closeBtn = new QPushButton(QStringLiteral("Close"));
        steamBtn = new QPushButton(QStringLiteral("Open in Steam"));
        bb->addWidget(closeBtn);
        bb->addWidget(steamBtn);
        v->addLayout(bb);
        copyBtn->setEnabled(false);
        steamBtn->setEnabled(false);
        QObject::connect(closeBtn, &QPushButton::clicked, this, &QDialog::accept);
        QObject::connect(copyBtn, &QPushButton::clicked, this, [this] {
            QApplication::clipboard()->setText(QStringLiteral("https://steamcommunity.com/sharedfiles/filedetails/?id=") + id);
        });
        QObject::connect(steamBtn, &QPushButton::clicked, this, [this] { QDesktopServices::openUrl(QUrl(QStringLiteral("steam://url/CommunityFilePage/") + id)); });
        resize(640, 460);
    }

    int addStep(const QString &text) {
        auto *row = new QHBoxLayout;
        auto *icon = new QLabel(QStringLiteral("○"));
        icon->setFixedWidth(18);
        auto *t = new QLabel(text);
        auto *n = new QLabel;
        n->setEnabled(false);
        row->addWidget(icon);
        row->addWidget(t, 1);
        row->addWidget(n);
        stepsBox->addLayout(row);
        steps << Step{icon, n};
        return int(steps.size()) - 1;
    }
    void stepState(int i, int state, const QString &n = QString()) { // 0 waiting, 1 running, 2 done, 3 failed
        if (i < 0 || i >= steps.size()) return;
        static const char *marks[4] = {"○", "◐", "✔", "✖"};
        static const char *cols[4] = {"gray", "palette(highlight)", "#3fae6a", "#e0436b"};
        steps[i].icon->setText(QStringLiteral("<span style='color:%1'>%2</span>").arg(QString::fromLatin1(cols[state]), QString::fromUtf8(marks[state])));
        if (!n.isNull()) steps[i].note->setText(n);
    }
    void line(const QString &l) { log->appendPlainText(l); }
    void finish(bool ok, const QString &what, const QString &hint) {
        bar->setRange(0, 1);
        bar->setValue(1);
        headline->setText(ok ? QStringLiteral("<b>%1</b>").arg(what.toHtmlEscaped()) : QStringLiteral("<b>✖ %1</b>").arg(what.toHtmlEscaped()));
        note->setText(hint);
        copyBtn->setEnabled(ok && !id.isEmpty());
        steamBtn->setEnabled(ok && !id.isEmpty());
    }

private:
    struct Step { QLabel *icon, *note; };
    QList<Step> steps;
    QLabel *headline, *note;
    QVBoxLayout *stepsBox;
    QProgressBar *bar;
    QPlainTextEdit *log;
    QPushButton *copyBtn, *closeBtn, *steamBtn;
};

void WorkshopDialog::publish() {
    runChecks();
    if (!publishBtn->isEnabled()) return;
    const bool update = !current.isEmpty();
    const bool withThumb = !update || replaceThumb->isChecked();
    const QString thumbFile = WorkshopProject::thumbPath(pj);
    if (withThumb && !QFileInfo::exists(thumbFile)) {
        QMessageBox::information(this, windowTitle(), QStringLiteral("Choose a thumbnail first, or snap one from a model."));
        return;
    }
    if (update && notes->toPlainText().trimmed().isEmpty() && !qEnvironmentVariableIsSet("BONK_ASSUME_YES") &&
        QMessageBox::question(this, windowTitle(), QStringLiteral("Publish the update without change notes?")) != QMessageBox::Yes)
        return;
    // the form, kept with the project
    if (!update) {
        wp.title = title->text().trimmed();
        wp.description = desc->toPlainText().trimmed();
        wp.type = typeBox->currentData().toString();
        wp.tags = checkedTags();
    }
    wp.save(pj);
    writeAddonJson();

    auto *pd = new PublishDialog(this, update ? (item(current) ? item(current)->title : wp.title) : wp.title);
    pd->setAttribute(Qt::WA_DeleteOnClose);
    const int sCheck = pd->addStep(QStringLiteral("Checked the addon folder"));
    const int sPack = pd->addStep(QStringLiteral("Pack it with gmad"));
    const int sUp = pd->addStep(update ? QStringLiteral("Upload the update with gmpublish") : QStringLiteral("Upload it with gmpublish"));
    const int sIcon = update && withThumb ? pd->addStep(QStringLiteral("Upload the new thumbnail")) : -1;
    AddonCheck c;
    c.run(pj.addonDir());
    pd->stepState(sCheck, 2, QStringLiteral("%1 model%2%3").arg(c.models).arg(c.models == 1 ? QString() : QStringLiteral("s"))
                                    .arg(c.spawnlist ? QStringLiteral(", spawnlist") : QString()));
    pd->show();

    const QString gma = pj.buildDir() + QLatin1Char('/') + spawnSlug(pj.name) + QStringLiteral(".gma");
    QDir().mkpath(pj.buildDir());
    QFile::remove(gma);
    // the steps run one after another
    struct Run {
        QStringList args;
        bool publishTool;
        int step;
    };
    auto runs = std::make_shared<QList<Run>>();
    runs->append({{QStringLiteral("create"), QStringLiteral("-folder"), tools.path(false, pj.addonDir()), QStringLiteral("-out"), tools.path(false, gma),
                   QStringLiteral("-warninvalid")}, false, sPack});
    if (update) {
        QStringList a = {QStringLiteral("update"), QStringLiteral("-addon"), tools.path(true, gma), QStringLiteral("-id"), current};
        const QString n = asciiNotes(notes->toPlainText());
        if (!n.isEmpty()) a << QStringLiteral("-changes") << n;
        runs->append({a, true, sUp});
        if (withThumb) runs->append({{QStringLiteral("update"), QStringLiteral("-icon"), tools.path(true, thumbFile), QStringLiteral("-id"), current}, true, sIcon});
    } else {
        runs->append({{QStringLiteral("create"), QStringLiteral("-addon"), tools.path(true, gma), QStringLiteral("-icon"), tools.path(true, thumbFile)}, true, sUp});
    }
    auto next = std::make_shared<std::function<void(int)>>();
    QPointer<PublishDialog> dlg(pd);
    QPointer<WorkshopDialog> self(this);
    *next = [=](int i) {
        if (!dlg || !self) return;
        if (i >= runs->size()) {
            QString id = update ? current : pd->id;
            if (!id.isEmpty()) { // the item this project publishes to from now on
                wp.id = id;
                wp.save(pj);
            }
            pd->id = id;
            pd->finish(true, update ? QStringLiteral("Updated! Bonk!") : QStringLiteral("Published! Bonk!"),
                       update ? QStringLiteral("Steam may take a few minutes to show the update.")
                              : QStringLiteral("New items start out hidden on the Workshop: make it public from its page when you're ready."));
            if (status) status(update ? QStringLiteral("Updated %1 on the Workshop").arg(id) : QStringLiteral("Published to the Workshop as %1").arg(id));
            if (QSettings().value("workshop/openAfterPublish", true).toBool()) openWorkshopPage(id, steamRunning);
            fetchList();
            if (!update) select(id);
            return;
        }
        const Run r = runs->at(i);
        pd->stepState(r.step, 1);
        QStringList shown;
        for (const QString &x : r.args) shown << (x.contains(QLatin1Char(' ')) ? QLatin1Char('"') + x + QLatin1Char('"') : x);
        pd->line(QStringLiteral("$ %1 %2").arg(QFileInfo(r.publishTool ? tools.gmpublish : tools.gmad).fileName(), shown.join(QLatin1Char(' '))));
        QProcess *p = tools.start(self, r.publishTool, r.args, pj.dir());
        auto out = std::make_shared<QString>();
        QObject::connect(p, &QProcess::readyRead, self, [=] {
            const QString t = terminalText(p->readAll());
            *out += t + QLatin1Char('\n');
            for (const QString &l : t.split(QLatin1Char('\n'), Qt::SkipEmptyParts))
                if (!protonChatter(l.trimmed()) && dlg) dlg->line(l.trimmed());
        });
        QObject::connect(p, &QProcess::finished, self, [=](int code, QProcess::ExitStatus st) {
            p->deleteLater();
            if (!dlg) return;
            bool ok = st == QProcess::NormalExit && code == 0;
            if (!r.publishTool) {
                ok = ok && QFileInfo::exists(gma);
                if (ok) dlg->stepState(r.step, 2, QStringLiteral("%1 · %2 MB").arg(QFileInfo(gma).fileName()).arg(QFileInfo(gma).size() / 1048576.0, 0, 'f', 1));
            } else {
                if (out->contains(QStringLiteral("rror"), Qt::CaseSensitive) && !out->contains(QStringLiteral("Success"), Qt::CaseInsensitive)) ok = false;
                if (!update) {
                    static const QRegularExpression idRe(QStringLiteral("(\\d{8,})"));
                    QString found;
                    for (auto it = idRe.globalMatch(*out); it.hasNext();) found = it.next().captured(1);
                    if (!found.isEmpty()) dlg->id = found;
                    if (ok && found.isEmpty()) ok = false;
                }
                if (ok) dlg->stepState(r.step, 2, update ? QString() : QStringLiteral("ID %1").arg(dlg->id));
            }
            if (!ok) {
                dlg->stepState(r.step, 3);
                QString hint = QStringLiteral("The log above has the details.");
                if (out->contains(QStringLiteral("Steam"), Qt::CaseInsensitive))
                    hint = QStringLiteral("Make sure Steam is running and signed in, then try again.");
                if (out->contains(QStringLiteral("jpg"), Qt::CaseInsensitive) || out->contains(QStringLiteral("icon"), Qt::CaseInsensitive))
                    hint += QStringLiteral(" The thumbnail must be a 512 × 512 baseline JPG: snap or choose it again in Bonk.");
                dlg->finish(false, r.publishTool ? QStringLiteral("gmpublish didn't finish the upload") : QStringLiteral("gmad couldn't pack the addon"), hint);
                return;
            }
            (*next)(i + 1);
        });
        QObject::connect(p, &QProcess::errorOccurred, self, [=](QProcess::ProcessError e) {
            if (e != QProcess::FailedToStart || !dlg) return;
            dlg->stepState(r.step, 3);
            dlg->finish(false, QStringLiteral("Couldn't start %1").arg(p->program()), QString());
        });
        p->start();
    };
    (*next)(0);
}
