// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Steventechno and the Bonk! contributors
//! Stock game content: the Source games installed through Steam, and one
//! file index over everything a game (or a single .vpk, or a loose folder)
//! holds. A game is read the way the engine mounts it, from the SearchPaths
//! in its gameinfo.txt: its .vpk archives and loose folders, first one wins.
//!
//! Used by File → Import Source Textures… (materials) and the Spawnlist
//! Editor's From Games tab (models, copied out with their textures so their
//! spawn icons can be drawn).

use crate::mdl;
use crate::vmf;
use crate::vpk;
use std::collections::HashMap;
use std::path::{Path, PathBuf};
use std::rc::Rc;

// ------------------------------------------------------------------ the index

/// Where a file's bytes are.
#[derive(Clone, Debug, PartialEq)]
pub enum Loc {
    /// A plain file on disk
    Loose(PathBuf),
    /// Pieces of archives: (file, offset, length), read one after the other
    Packed(Vec<(Rc<PathBuf>, u64, u64)>),
}

impl Loc {
    pub fn read(&self, max: Option<u64>) -> Option<Vec<u8>> {
        use std::io::{Read, Seek, SeekFrom};
        match self {
            Loc::Loose(p) => {
                let mut f = std::fs::File::open(p).ok()?;
                let mut v = Vec::new();
                match max {
                    Some(m) => f.take(m).read_to_end(&mut v).ok()?,
                    None => f.read_to_end(&mut v).ok()?,
                };
                Some(v)
            }
            Loc::Packed(parts) => {
                let mut v = Vec::new();
                for (file, off, len) in parts {
                    let want = match max {
                        Some(m) => (*len).min(m.saturating_sub(v.len() as u64)),
                        None => *len,
                    };
                    if want == 0 {
                        continue;
                    }
                    let mut f = std::fs::File::open(file.as_path()).ok()?;
                    f.seek(SeekFrom::Start(*off)).ok()?;
                    let start = v.len();
                    f.take(want).read_to_end(&mut v).ok()?;
                    if (v.len() - start) as u64 != want {
                        return None; // a missing or short archive
                    }
                }
                Some(v)
            }
        }
    }
}

/// Every file of a game, a VPK or a folder: lowercase relative path → where it is.
#[derive(Default, Debug)]
pub struct Index {
    pub files: HashMap<String, Loc>,
    /// What was read, in mount order (for showing)
    pub mounts: Vec<PathBuf>,
}

impl Index {
    pub fn read(&self, rel: &str) -> Option<Vec<u8>> {
        self.files.get(&norm(rel))?.read(None)
    }
    pub fn has(&self, rel: &str) -> bool {
        self.files.contains_key(&norm(rel))
    }
    /// Files under `prefix` ("materials/"), sorted, without the prefix.
    pub fn list(&self, prefix: &str) -> Vec<(&str, &Loc)> {
        let prefix = norm(prefix);
        let mut v: Vec<(&str, &Loc)> = self.files.iter().filter_map(|(k, l)| k.strip_prefix(prefix.as_str()).map(|r| (r, l))).collect();
        v.sort_by(|a, b| a.0.cmp(b.0));
        v
    }

    /// Add a VPK (files already in the index stay: earlier mounts win).
    fn add_vpk(&mut self, p: &Path) -> Result<(), String> {
        let (dir, entries) = vpk::open(p)?;
        let dir_rc = Rc::new(dir.clone());
        let mut archives: HashMap<PathBuf, Rc<PathBuf>> = HashMap::new();
        for e in entries {
            if self.files.contains_key(&e.path) {
                continue;
            }
            let mut parts = Vec::with_capacity(2);
            if e.preload.1 > 0 {
                parts.push((dir_rc.clone(), e.preload.0, e.preload.1));
            }
            if e.len > 0 {
                let f = archives.entry(e.file.clone()).or_insert_with(|| Rc::new(e.file.clone())).clone();
                parts.push((f, e.offset, e.len));
            }
            self.files.insert(e.path, Loc::Packed(parts));
        }
        self.mounts.push(dir);
        Ok(())
    }

    /// Add a loose folder (the one holding materials/, models/…).
    fn add_dir(&mut self, root: &Path) {
        fn walk(dir: &Path, rel: &str, out: &mut Index, depth: usize) {
            let Ok(rd) = std::fs::read_dir(dir) else { return };
            for e in rd.flatten() {
                let name = e.file_name().to_string_lossy().to_ascii_lowercase();
                let r = if rel.is_empty() { name } else { format!("{rel}/{name}") };
                let Ok(ft) = e.file_type() else { continue };
                if ft.is_dir() || (ft.is_symlink() && e.path().is_dir()) {
                    // only the content folders a game reads models and materials from
                    if depth == 0 && !matches!(r.as_str(), "materials" | "models") {
                        continue;
                    }
                    if depth < 24 {
                        walk(&e.path(), &r, out, depth + 1);
                    }
                } else if depth > 0 && !out.files.contains_key(&r) {
                    out.files.insert(r, Loc::Loose(e.path()));
                }
            }
        }
        walk(root, "", self, 0);
        self.mounts.push(root.to_path_buf());
    }
}

fn norm(rel: &str) -> String {
    let mut s = rel.trim().replace('\\', "/").to_ascii_lowercase();
    while s.contains("//") {
        s = s.replace("//", "/");
    }
    s.trim_start_matches('/').to_string()
}

/// A folder with a gameinfo.txt in it: a Source game (or mod).
pub fn is_game_dir(p: &Path) -> bool {
    p.is_dir() && mdl::find_ci(p, "gameinfo.txt").is_some_and(|g| g.is_file())
}

/// Open a .vpk (any part of one), a game folder (with gameinfo.txt), or a
/// loose content folder (an addon or a mod: the folder holding materials/).
pub fn open(p: &Path) -> Result<Index, String> {
    let mut ix = Index::default();
    if p.is_file() {
        if !p.to_string_lossy().to_ascii_lowercase().ends_with(".vpk") {
            return Err("That isn't a .vpk file.".into());
        }
        ix.add_vpk(p)?;
    } else if is_game_dir(p) {
        let mounts = search_paths(p);
        let mut errors = Vec::new();
        for m in &mounts {
            if m.is_dir() {
                ix.add_dir(m);
            } else if let Err(e) = ix.add_vpk(m) {
                errors.push(e);
            }
        }
        if ix.files.is_empty() {
            return Err(errors.into_iter().next().unwrap_or_else(|| "That game has no content Bonk can read.".into()));
        }
    } else if p.is_dir() {
        ix.add_dir(p);
        if ix.files.is_empty() {
            return Err("No materials or models in that folder.".into());
        }
    } else {
        return Err(format!("{} isn't there.", p.display()));
    }
    Ok(ix)
}

// ------------------------------------------------------------------ gameinfo.txt

/// What a game mounts, in order: VPK directories and loose folders that exist.
/// Sound archives are left out (nothing to import there), and so are
/// wildcard folders (custom/*, addons/*) and platform-only paths.
pub fn search_paths(game: &Path) -> Vec<PathBuf> {
    let Some(gi) = mdl::find_ci(game, "gameinfo.txt") else { return Vec::new() };
    let text = std::fs::read(&gi).map(|b| String::from_utf8_lossy(&b).into_owned()).unwrap_or_default();
    let root = vmf::parse(&text);
    let Some(info) = root.children.iter().find(|c| c.name.eq_ignore_ascii_case("gameinfo")).or(root.children.first()) else { return Vec::new() };
    let Some(sp) = info.kid("filesystem").and_then(|f| f.kid("searchpaths")) else { return Vec::new() };
    let base = game.parent().unwrap_or(game).to_path_buf();
    let mut out: Vec<PathBuf> = Vec::new();
    for (key, val) in &sp.kv {
        if !key.to_ascii_lowercase().split('+').any(|k| k == "game") {
            continue;
        }
        let mut v = val.trim().replace('\\', "/");
        let mut from = base.clone();
        for (token, dir) in [("|gameinfo_path|", game), ("|all_source_engine_paths|", base.as_path())] {
            if v.len() >= token.len() && v[..token.len()].eq_ignore_ascii_case(token) {
                v = v[token.len()..].to_string();
                from = dir.to_path_buf();
            }
        }
        let v = v.trim_start_matches('/');
        if v.ends_with('*') {
            continue;
        }
        let lower = v.to_ascii_lowercase();
        let found = if lower.ends_with(".vpk") {
            if lower.contains("sound") {
                continue;
            }
            // "hl2/hl2_textures.vpk" means hl2/hl2_textures_dir.vpk
            let dir_name = format!("{}_dir.vpk", &v[..v.len() - 4]);
            mdl::find_ci(&from, &dir_name).filter(|p| p.is_file()).or_else(|| mdl::find_ci(&from, v).filter(|p| p.is_file()))
        } else if v.is_empty() || v == "." {
            Some(from.clone())
        } else {
            mdl::find_ci(&from, v.trim_end_matches('/')).filter(|p| p.is_dir())
        };
        if let Some(p) = found {
            let p = p.canonicalize().unwrap_or(p);
            if !out.contains(&p) {
                out.push(p);
            }
        }
    }
    out
}

/// The name a game's gameinfo.txt gives it ("game" key), if any.
fn gameinfo_name(game: &Path) -> Option<String> {
    let gi = mdl::find_ci(game, "gameinfo.txt")?;
    let text = std::fs::read(&gi).ok().map(|b| String::from_utf8_lossy(&b).into_owned())?;
    let root = vmf::parse(&text);
    let info = root.children.iter().find(|c| c.name.eq_ignore_ascii_case("gameinfo")).or(root.children.first())?;
    info.get("game").or(info.get("title")).map(|s| s.trim().to_string()).filter(|s| !s.is_empty())
}

// ------------------------------------------------------------------ Steam

/// The library folders listed in a libraryfolders.vdf (new and old layouts).
pub fn libraries_from_vdf(text: &str) -> Vec<String> {
    let root = vmf::parse(text);
    let mut out = Vec::new();
    for top in &root.children {
        // old: "1" "/path"; new: "0" { "path" "/path" … }
        for (k, v) in &top.kv {
            if k.parse::<u32>().is_ok() && (v.contains('/') || v.contains('\\')) {
                out.push(v.replace("\\\\", "/").replace('\\', "/"));
            }
        }
        for c in &top.children {
            if let Some(p) = c.get("path") {
                out.push(p.replace("\\\\", "/").replace('\\', "/"));
            }
        }
    }
    out
}

/// A Source game found in a Steam library.
#[derive(Clone, Debug, PartialEq)]
pub struct Game {
    pub name: String,
    /// The folder with gameinfo.txt in it (".../Half-Life 2/hl2")
    pub dir: PathBuf,
}

/// Names for the folders of Valve's Source games (gameinfo.txt names are
/// sometimes stylized, like HL2's "HALF-LIFE'").
fn known_name(folder: &str) -> Option<&'static str> {
    Some(match folder {
        "garrysmod" => "Garry's Mod",
        "hl2" => "Half-Life 2",
        "episodic" => "Half-Life 2: Episode One",
        "ep2" => "Half-Life 2: Episode Two",
        "lostcoast" => "Half-Life 2: Lost Coast",
        "hl2mp" => "Half-Life 2: Deathmatch",
        "hl1" => "Half-Life: Source",
        "hl1mp" => "Half-Life Deathmatch: Source",
        "cstrike" => "Counter-Strike: Source",
        "dod" => "Day of Defeat: Source",
        "tf" => "Team Fortress 2",
        "portal" => "Portal",
        "portal2" => "Portal 2",
        "left4dead" => "Left 4 Dead",
        "left4dead2" => "Left 4 Dead 2",
        "csgo" => "Counter-Strike: Global Offensive",
        "bms" => "Black Mesa",
        "synergy" => "Synergy",
        "zps" => "Zombie Panic! Source",
        "nmrih" => "No More Room in Hell",
        "insurgency" => "Insurgency",
        "dinodday" => "Dino D-Day",
        "ageofchivalry" => "Age of Chivalry",
        "pvkii" => "Pirates, Vikings and Knights II",
        "fof" => "Fistful of Frags",
        "empires" => "Empires",
        "infra" => "INFRA",
        "estrangedact1" => "Estranged: Act I",
        "mapbase" => "Mapbase",
        _ => return None,
    })
}

/// The Source games (and source mods) installed in these Steam libraries,
/// Garry's Mod first, then by name.
pub fn find_games(libraries: &[PathBuf]) -> Vec<Game> {
    let mut out: Vec<Game> = Vec::new();
    let mut seen: Vec<PathBuf> = Vec::new();
    let mut add = |out: &mut Vec<Game>, dir: PathBuf, fallback: &str| {
        let canon = dir.canonicalize().unwrap_or(dir.clone());
        if seen.contains(&canon) || search_paths(&dir).is_empty() {
            return;
        }
        seen.push(canon);
        let folder = dir.file_name().map(|n| n.to_string_lossy().to_ascii_lowercase()).unwrap_or_default();
        let name = known_name(&folder).map(str::to_string).or_else(|| gameinfo_name(&dir)).unwrap_or_else(|| fallback.to_string());
        out.push(Game { name, dir });
    };
    let sorted = |d: &Path| -> Vec<PathBuf> {
        let mut v: Vec<PathBuf> = std::fs::read_dir(d).map(|r| r.flatten().map(|e| e.path()).filter(|p| p.is_dir()).collect()).unwrap_or_default();
        v.sort();
        v
    };
    for lib in libraries {
        let steamapps = mdl::find_ci(lib, "steamapps").unwrap_or_else(|| lib.join("steamapps"));
        for install in sorted(&steamapps.join("common")) {
            let install_name = install.file_name().map(|n| n.to_string_lossy().into_owned()).unwrap_or_default();
            for sub in sorted(&install) {
                if is_game_dir(&sub) {
                    add(&mut out, sub, &install_name);
                }
            }
        }
        for m in sorted(&steamapps.join("sourcemods")) {
            if is_game_dir(&m) {
                let n = m.file_name().map(|n| n.to_string_lossy().into_owned()).unwrap_or_default();
                add(&mut out, m, &n);
            }
        }
    }
    // the same name twice (an "hl2" folder inside another game): say whose it is
    let names: Vec<String> = out.iter().map(|g| g.name.clone()).collect();
    for g in out.iter_mut() {
        if names.iter().filter(|n| **n == g.name).count() > 1 {
            if let Some(install) = g.dir.parent().and_then(|p| p.file_name()) {
                g.name = format!("{} ({})", g.name, install.to_string_lossy());
            }
        }
    }
    out.sort_by(|a, b| {
        let gm = |g: &Game| g.dir.file_name().is_none_or(|n| !n.to_string_lossy().eq_ignore_ascii_case("garrysmod"));
        (gm(a), a.name.to_lowercase()).cmp(&(gm(b), b.name.to_lowercase()))
    });
    out
}

/// Steam library folders: the Steam installs given plus every library their
/// libraryfolders.vdf lists.
pub fn libraries(steam_roots: &[PathBuf]) -> Vec<PathBuf> {
    let mut out: Vec<PathBuf> = Vec::new();
    let push = |p: PathBuf, out: &mut Vec<PathBuf>| {
        let p = p.canonicalize().unwrap_or(p);
        if p.join("steamapps").is_dir() && !out.contains(&p) {
            out.push(p);
        }
    };
    for r in steam_roots {
        push(r.clone(), &mut out);
        for vdf in [r.join("steamapps/libraryfolders.vdf"), r.join("config/libraryfolders.vdf")] {
            if let Ok(t) = std::fs::read(&vdf) {
                for l in libraries_from_vdf(&String::from_utf8_lossy(&t)) {
                    push(PathBuf::from(l), &mut out);
                }
            }
        }
    }
    out
}

/// Where Steam usually lives on this system.
pub fn default_steam_roots() -> Vec<PathBuf> {
    let mut v = Vec::new();
    #[cfg(windows)]
    {
        for env in ["ProgramFiles(x86)", "ProgramFiles"] {
            if let Some(pf) = std::env::var_os(env) {
                v.push(PathBuf::from(pf).join("Steam"));
            }
        }
    }
    #[cfg(not(windows))]
    if let Some(h) = std::env::var_os("HOME").map(PathBuf::from) {
        for r in [
            ".local/share/Steam",
            ".steam/steam",
            ".steam/root",
            ".var/app/com.valvesoftware.Steam/.local/share/Steam",
            ".var/app/com.valvesoftware.Steam/data/Steam",
            "snap/steam/common/.local/share/Steam",
        ] {
            v.push(h.join(r));
        }
    }
    v
}

// ------------------------------------------------------------------ copying models out

/// The material files a model needs, as "materials/…" paths in the index:
/// each texture's VMT (through the model's $cdmaterials folders), the VMT a
/// patch material includes, and the VTFs they name.
pub fn model_material_files(ix: &Index, model: &mdl::Model) -> Vec<String> {
    let mut out = Vec::new();
    let mut queue: Vec<String> = Vec::new();
    for t in &model.textures {
        let t = norm(t);
        let mut cands: Vec<String> = model.cdmaterials.iter().map(|cd| norm(&format!("{}/{}", cd.trim_matches('/'), t))).collect();
        cands.push(t.clone());
        if let Some(c) = cands.into_iter().find(|c| ix.has(&format!("materials/{c}.vmt"))) {
            queue.push(c);
        }
    }
    let mut done = std::collections::HashSet::new();
    while let Some(m) = queue.pop() {
        let vmt = format!("materials/{m}.vmt");
        if !done.insert(vmt.clone()) {
            continue;
        }
        let Some(bytes) = ix.read(&vmt) else { continue };
        out.push(vmt);
        let Some(v) = vmf::parse_vmt(&String::from_utf8_lossy(&bytes)) else { continue };
        if let Some(inc) = v.get("include") {
            queue.push(vmf::clean_path(inc));
        }
        for (k, val) in &v.params {
            if matches!(k.as_str(), "$basetexture" | "$bumpmap" | "$normalmap" | "$detail" | "$phongexponenttexture" | "$lightwarptexture" | "$selfillummask") {
                let vtf = format!("materials/{}.vtf", vmf::clean_path(val));
                if ix.has(&vtf) && done.insert(vtf.clone()) {
                    out.push(vtf);
                }
            }
        }
    }
    out
}

/// Copy models ("models/…/x.mdl") out of the index into `dest`, each with
/// its .vvd / .vtx / .phy and the materials it uses, so it can be drawn like
/// a model in an addon folder. Returns how many models were copied.
pub fn extract_models(ix: &Index, models: &[String], dest: &Path) -> Result<usize, String> {
    let mut n = 0;
    let mut errors = Vec::new();
    for m in models {
        let m = norm(m);
        let Some(stem) = m.strip_suffix(".mdl") else { continue };
        let Some(mdl_bytes) = ix.read(&m) else {
            errors.push(format!("{m}: not in there"));
            continue;
        };
        let mut files = vec![(m.clone(), mdl_bytes.clone())];
        for ext in ["vvd", "dx90.vtx", "dx80.vtx", "sw.vtx", "vtx", "phy"] {
            let f = format!("{stem}.{ext}");
            if let Some(b) = ix.read(&f) {
                files.push((f, b));
            }
        }
        let vvd = files.iter().find(|f| f.0.ends_with(".vvd")).map(|f| f.1.clone());
        let vtx = files.iter().find(|f| f.0.ends_with(".vtx")).map(|f| f.1.clone());
        if let (Some(vvd), Some(vtx)) = (vvd, vtx) {
            if let Ok(model) = mdl::read_model(&mdl_bytes, &vvd, &vtx) {
                for f in model_material_files(ix, &model) {
                    if !dest.join(&f).exists() {
                        if let Some(b) = ix.read(&f) {
                            files.push((f, b));
                        }
                    }
                }
            }
        }
        let mut ok = true;
        for (rel, bytes) in files {
            let p = dest.join(&rel);
            if let Err(e) = p.parent().map(std::fs::create_dir_all).unwrap_or(Ok(())).and_then(|_| std::fs::write(&p, bytes)) {
                errors.push(format!("{rel}: {e}"));
                ok = false;
                break;
            }
        }
        n += ok as usize;
    }
    if n == 0 && !errors.is_empty() {
        return Err(errors.join("\n"));
    }
    Ok(n)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::vpk::tests::build;

    fn tmp(name: &str) -> PathBuf {
        let d = std::env::temp_dir().join(format!("bonk_content_{name}_{}", std::process::id()));
        let _ = std::fs::remove_dir_all(&d);
        std::fs::create_dir_all(&d).unwrap();
        d
    }

    #[test]
    fn libraryfolders_old_and_new() {
        let new = r#""libraryfolders"
        {
            "0" { "path" "/home/me/.local/share/Steam" "label" "" "apps" { "4000" "123" } }
            "1" { "path" "/run/media/me/Games/SteamLibrary" }
        }"#;
        assert_eq!(libraries_from_vdf(new), ["/home/me/.local/share/Steam", "/run/media/me/Games/SteamLibrary"]);
        let old = "\"LibraryFolders\"\n{\n\t\"TimeNextStatsReport\"\t\"1\"\n\t\"1\"\t\"D:\\\\SteamLibrary\"\n}";
        assert_eq!(libraries_from_vdf(old), ["D:/SteamLibrary"]);
    }

    /// A fake Steam library with Garry's Mod (a VPK of its own, HL2's
    /// textures in sourceengine/, a loose override) and Half-Life 2.
    fn fake_library(root: &Path) {
        let common = root.join("steamapps/common");
        let gm = common.join("GarrysMod/garrysmod");
        std::fs::create_dir_all(&gm).unwrap();
        std::fs::create_dir_all(common.join("GarrysMod/sourceengine")).unwrap();
        std::fs::write(
            gm.join("gameinfo.txt"),
            r#""GameInfo"
            {
                game "Garry's Mod"
                FileSystem
                {
                    SteamAppId 4000
                    SearchPaths
                    {
                        game+mod            garrysmod/addons/*
                        game+mod+mod_write  |gameinfo_path|.
                        game                |gameinfo_path|garrysmod.vpk
                        game                sourceengine/hl2_textures.vpk
                        game                sourceengine/hl2_sound_misc.vpk
                        platform            platform/platform_misc.vpk
                    }
                }
            }"#,
        )
        .unwrap();
        let (own, _) = build(2, &[("materials/gm/wall.vmt", b"\"VertexLitGeneric\" { \"$basetexture\" \"gm/wall\" }", 0x7fff, b""), ("materials/gm/wall.vtf", b"", 0x7fff, b"GMVTF")]);
        std::fs::write(gm.join("garrysmod_dir.vpk"), own).unwrap();
        let (hl2, arch) = build(
            1,
            &[
                ("materials/brick/brickwall001a.vmt", b"\"LightmappedGeneric\" { \"$basetexture\" \"brick/brickwall001a\" }", 0x7fff, b""),
                ("materials/brick/brickwall001a.vtf", b"", 0, b"HL2VTF"),
                ("materials/gm/wall.vtf", b"", 0, b"SHADOWED"),
            ],
        );
        std::fs::write(common.join("GarrysMod/sourceengine/hl2_textures_dir.vpk"), hl2).unwrap();
        for (i, a) in arch {
            std::fs::write(common.join(format!("GarrysMod/sourceengine/hl2_textures_{i:03}.vpk")), a).unwrap();
        }
        std::fs::create_dir_all(gm.join("materials/brick")).unwrap();
        std::fs::write(gm.join("materials/brick/brickwall001a.vmt"), "\"VertexLitGeneric\" {}").unwrap(); // a loose override
        // Half-Life 2, with a stylized gameinfo name
        let hl2 = common.join("Half-Life 2/hl2");
        std::fs::create_dir_all(&hl2).unwrap();
        std::fs::write(hl2.join("gameinfo.txt"), "GameInfo { game \"HALF-LIFE'\" FileSystem { SearchPaths { game |gameinfo_path|. } } }").unwrap();
        std::fs::create_dir_all(hl2.join("materials")).unwrap();
        std::fs::write(hl2.join("materials/a.vmt"), "x").unwrap();
        // not a Source game: no gameinfo.txt
        std::fs::create_dir_all(common.join("Proton 9.0/files")).unwrap();
    }

    #[test]
    fn finds_source_games_in_a_library() {
        let root = tmp("lib");
        fake_library(&root);
        let games = find_games(&[root.clone()]);
        let names: Vec<&str> = games.iter().map(|g| g.name.as_str()).collect();
        assert_eq!(names, ["Garry's Mod", "Half-Life 2"]);
        assert!(games[0].dir.ends_with("GarrysMod/garrysmod"));
        let _ = std::fs::remove_dir_all(&root);
    }

    #[test]
    fn mounts_a_game_like_the_engine() {
        let root = tmp("mount");
        fake_library(&root);
        let gm = root.join("steamapps/common/GarrysMod/garrysmod");
        let sp = search_paths(&gm);
        let names: Vec<String> = sp.iter().map(|p| p.file_name().unwrap().to_string_lossy().into_owned()).collect();
        // addons/* (wildcard), the sound VPK and platform paths are left out
        assert_eq!(names, ["garrysmod", "garrysmod_dir.vpk", "hl2_textures_dir.vpk"]);
        let ix = open(&gm).unwrap();
        // the loose folder comes first, so its VMT wins over the VPK's
        assert_eq!(ix.read("materials/brick/brickwall001a.vmt").unwrap(), b"\"VertexLitGeneric\" {}");
        assert_eq!(ix.read("Materials\\Brick\\BrickWall001a.vtf").unwrap(), b"HL2VTF");
        // garrysmod.vpk is mounted before hl2_textures.vpk
        assert_eq!(ix.read("materials/gm/wall.vtf").unwrap(), b"GMVTF");
        let mats: Vec<&str> = ix.list("materials/").into_iter().map(|x| x.0).filter(|k| k.ends_with(".vmt")).collect();
        assert_eq!(mats, ["brick/brickwall001a.vmt", "gm/wall.vmt"]);
        // reading only the start of a file
        assert_eq!(ix.files["materials/gm/wall.vtf"].read(Some(2)).unwrap(), b"GM");
        let _ = std::fs::remove_dir_all(&root);
    }

    #[test]
    fn opens_a_single_vpk_or_a_folder() {
        let root = tmp("single");
        fake_library(&root);
        let se = root.join("steamapps/common/GarrysMod/sourceengine");
        let ix = open(&se.join("hl2_textures_000.vpk")).unwrap();
        assert_eq!(ix.files.len(), 3);
        assert!(open(&se).is_err()); // no materials/ or models/ in it
        let ix = open(&root.join("steamapps/common/Half-Life 2")).unwrap_or_default();
        assert!(ix.files.is_empty()); // the install folder holds hl2/, which has the content
        assert!(open(&root.join("steamapps/common/GarrysMod/garrysmod/gameinfo.txt")).is_err());
        let _ = std::fs::remove_dir_all(&root);
    }

    #[test]
    fn libraries_from_steam_roots() {
        let root = tmp("roots");
        let other = tmp("roots_other");
        std::fs::create_dir_all(root.join("steamapps")).unwrap();
        std::fs::create_dir_all(other.join("steamapps")).unwrap();
        std::fs::write(
            root.join("steamapps/libraryfolders.vdf"),
            format!("\"libraryfolders\" {{ \"0\" {{ \"path\" \"{}\" }} \"1\" {{ \"path\" \"{}\" }} \"2\" {{ \"path\" \"/gone\" }} }}", root.display(), other.display()),
        )
        .unwrap();
        let libs = libraries(&[root.clone(), root.join("missing")]);
        assert_eq!(libs, [root.canonicalize().unwrap(), other.canonicalize().unwrap()]);
        let _ = std::fs::remove_dir_all(&root);
        let _ = std::fs::remove_dir_all(&other);
    }

    #[test]
    fn copies_a_models_materials_out() {
        let root = tmp("extract");
        let content = root.join("game");
        std::fs::create_dir_all(content.join("materials/models/props")).unwrap();
        std::fs::write(content.join("materials/models/props/crate.vmt"), "patch { include \"materials/models/props/crate_base.vmt\" insert { $bumpmap models/props/crate_n } }").unwrap();
        std::fs::write(content.join("materials/models/props/crate_base.vmt"), "VertexLitGeneric { $basetexture \"models\\props\\crate\" }").unwrap();
        std::fs::write(content.join("materials/models/props/crate.vtf"), "V").unwrap();
        std::fs::write(content.join("materials/models/props/crate_n.vtf"), "N").unwrap();
        std::fs::create_dir_all(content.join("models/props")).unwrap();
        for ext in ["mdl", "vvd", "dx90.vtx", "phy"] {
            std::fs::write(content.join(format!("models/props/Crate.{ext}")), ext).unwrap();
        }
        let ix = open(&content).unwrap();
        let model = mdl::Model { textures: vec!["Crate".into(), "missing".into()], cdmaterials: vec!["models\\props\\".into()], ..Default::default() };
        let mut files = model_material_files(&ix, &model);
        files.sort();
        assert_eq!(
            files,
            ["materials/models/props/crate.vmt", "materials/models/props/crate.vtf", "materials/models/props/crate_base.vmt", "materials/models/props/crate_n.vtf"]
        );
        // a model that isn't there
        assert!(extract_models(&ix, &["models/nope.mdl".into()], &root.join("out")).is_err());
        // the model's own files come along (this fake .mdl can't be read, so no materials)
        assert_eq!(extract_models(&ix, &["models/props/crate.mdl".into(), "models/nope.mdl".into()], &root.join("out")).unwrap(), 1);
        assert_eq!(std::fs::read(root.join("out/models/props/crate.dx90.vtx")).unwrap(), b"dx90.vtx");
        assert!(root.join("out/models/props/crate.phy").exists());
        let _ = std::fs::remove_dir_all(&root);
    }
}
