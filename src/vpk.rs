// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Steventechno and the Bonk! contributors
//! Valve Pak (.vpk) directories, version 1 and 2: where every file of a game's
//! stock content lives. A "pak01_dir.vpk" holds the file tree (and small
//! files, or the first bytes of a file, "preloaded" right in the tree); the
//! data is in "pak01_000.vpk", "pak01_001.vpk"… or after the tree in the
//! directory file itself. Only reading is supported.

use std::path::{Path, PathBuf};

const SIGNATURE: u32 = 0x55aa_1234;
/// An archive index meaning "in the directory file, after the tree".
const IN_DIR: u16 = 0x7fff;

/// One file in a VPK. Its bytes are `preload` followed by `len` bytes at
/// `offset` in `file` (either part may be empty).
#[derive(Clone, Debug, PartialEq)]
pub struct Entry {
    /// Lowercase, "/" separated, with extension: "materials/brick/brickwall001a.vmt"
    pub path: String,
    /// (offset in the directory file, length)
    pub preload: (u64, u64),
    pub file: PathBuf,
    pub offset: u64,
    pub len: u64,
}

/// The "_dir.vpk" that goes with a VPK file ("pak01_003.vpk" → "pak01_dir.vpk");
/// a single-file VPK (no numbered archives) is its own directory.
pub fn dir_file(p: &Path) -> PathBuf {
    let name = p.file_name().map(|n| n.to_string_lossy().into_owned()).unwrap_or_default();
    let lower = name.to_ascii_lowercase();
    if let Some(stem) = lower.strip_suffix(".vpk") {
        if stem.len() > 4 && stem.as_bytes()[stem.len() - 4] == b'_' && stem[stem.len() - 3..].bytes().all(|c| c.is_ascii_digit()) {
            let dir = p.with_file_name(format!("{}_dir.vpk", &name[..stem.len() - 4]));
            if dir.exists() {
                return dir;
            }
        }
    }
    p.to_path_buf()
}

fn archive_file(dir: &Path, index: u16) -> PathBuf {
    if index == IN_DIR {
        return dir.to_path_buf();
    }
    let name = dir.file_name().map(|n| n.to_string_lossy().into_owned()).unwrap_or_default();
    let base = if name.to_ascii_lowercase().ends_with("_dir.vpk") { &name[..name.len() - 8] } else { name.trim_end_matches(".vpk") };
    dir.with_file_name(format!("{base}_{index:03}.vpk"))
}

struct Cur<'a> {
    d: &'a [u8],
    i: usize,
}

impl Cur<'_> {
    fn u16(&mut self) -> Result<u16, String> {
        let b = self.d.get(self.i..self.i + 2).ok_or("the VPK's file list is cut short")?;
        self.i += 2;
        Ok(u16::from_le_bytes([b[0], b[1]]))
    }
    fn u32(&mut self) -> Result<u32, String> {
        let b = self.d.get(self.i..self.i + 4).ok_or("the VPK's file list is cut short")?;
        self.i += 4;
        Ok(u32::from_le_bytes([b[0], b[1], b[2], b[3]]))
    }
    fn cstr(&mut self) -> Result<String, String> {
        let rest = self.d.get(self.i..).ok_or("the VPK's file list is cut short")?;
        let end = rest.iter().position(|&c| c == 0).ok_or("the VPK's file list is cut short")?;
        self.i += end + 1;
        Ok(String::from_utf8_lossy(&rest[..end]).into_owned())
    }
}

/// Read a VPK directory from its bytes; `dir` is where it was read from (to
/// name the archives next to it).
pub fn parse(d: &[u8], dir: &Path) -> Result<Vec<Entry>, String> {
    let mut c = Cur { d, i: 0 };
    if c.u32().map_err(|_| "That isn't a VPK file.")? != SIGNATURE {
        return Err("That isn't a VPK file (or it's one of the numbered parts without its _dir.vpk).".into());
    }
    let version = c.u32()?;
    let tree = c.u32()? as usize;
    let header = match version {
        1 => 12,
        2 => 28,
        v => return Err(format!("VPK version {v} isn't supported (Source 2 games use a different format).")),
    };
    let tree_end = header + tree;
    if d.len() < tree_end {
        return Err("That VPK is cut short (an unfinished download?).".into());
    }
    c.i = header;
    let mut out = Vec::new();
    loop {
        let ext = c.cstr()?;
        if ext.is_empty() {
            break;
        }
        loop {
            let folder = c.cstr()?;
            if folder.is_empty() {
                break;
            }
            loop {
                let name = c.cstr()?;
                if name.is_empty() {
                    break;
                }
                let _crc = c.u32()?;
                let preload = c.u16()? as u64;
                let archive = c.u16()?;
                let offset = c.u32()? as u64;
                let len = c.u32()? as u64;
                if c.u16()? != 0xffff {
                    return Err("The VPK's file list is damaged.".into());
                }
                let pre_at = c.i as u64;
                c.i += preload as usize;
                if c.i > tree_end {
                    return Err("The VPK's file list is damaged.".into());
                }
                let mut path = String::new();
                let folder = folder.trim().trim_matches('/');
                if !folder.is_empty() {
                    path += folder;
                    path.push('/');
                }
                path += &name;
                if ext.trim() != "" {
                    path.push('.');
                    path += &ext;
                }
                let path = path.replace('\\', "/").to_ascii_lowercase();
                let (file, offset) = if archive == IN_DIR { (dir.to_path_buf(), tree_end as u64 + offset) } else { (archive_file(dir, archive), offset) };
                out.push(Entry { path, preload: (pre_at, preload), file, offset, len });
            }
        }
    }
    Ok(out)
}

/// Read a VPK's directory from disk (any of its parts may be given).
pub fn open(p: &Path) -> Result<(PathBuf, Vec<Entry>), String> {
    let dir = dir_file(p);
    let bytes = std::fs::read(&dir).map_err(|e| format!("Couldn't open {}: {e}", dir.display()))?;
    let entries = parse(&bytes, &dir)?;
    Ok((dir, entries))
}

#[cfg(test)]
pub mod tests {
    use super::*;

    /// A VPK directory (version 1 or 2) holding `files`: (path, preloaded
    /// bytes, archive index, data). Data for IN_DIR goes after the tree;
    /// numbered archives are returned to be written next to it.
    pub fn build(version: u32, files: &[(&str, &[u8], u16, &[u8])]) -> (Vec<u8>, Vec<(u16, Vec<u8>)>) {
        let mut tree = Vec::new();
        let mut after = Vec::new();
        let mut archives: Vec<(u16, Vec<u8>)> = Vec::new();
        // one ext / folder / file block per file keeps the builder simple
        for (path, pre, arch, data) in files {
            let (folder, file) = path.rsplit_once('/').unwrap_or((" ", path));
            let (stem, ext) = file.rsplit_once('.').unwrap_or((file, " "));
            for s in [ext, folder, stem] {
                tree.extend_from_slice(s.as_bytes());
                tree.push(0);
            }
            let offset = if *arch == IN_DIR {
                let o = after.len();
                after.extend_from_slice(data);
                o
            } else {
                let a = match archives.iter_mut().find(|a| a.0 == *arch) {
                    Some(a) => a,
                    None => {
                        archives.push((*arch, Vec::new()));
                        archives.last_mut().unwrap()
                    }
                };
                let o = a.1.len();
                a.1.extend_from_slice(data);
                o
            };
            tree.extend_from_slice(&0u32.to_le_bytes());
            tree.extend_from_slice(&(pre.len() as u16).to_le_bytes());
            tree.extend_from_slice(&arch.to_le_bytes());
            tree.extend_from_slice(&(offset as u32).to_le_bytes());
            tree.extend_from_slice(&(data.len() as u32).to_le_bytes());
            tree.extend_from_slice(&0xffffu16.to_le_bytes());
            tree.extend_from_slice(pre);
            tree.extend_from_slice(&[0, 0]); // end of this folder's files, end of this extension's folders
        }
        tree.push(0);
        let mut out = Vec::new();
        out.extend_from_slice(&SIGNATURE.to_le_bytes());
        out.extend_from_slice(&version.to_le_bytes());
        out.extend_from_slice(&(tree.len() as u32).to_le_bytes());
        if version == 2 {
            out.extend_from_slice(&(after.len() as u32).to_le_bytes());
            out.extend_from_slice(&[0; 12]);
        }
        out.extend_from_slice(&tree);
        out.extend_from_slice(&after);
        (out, archives)
    }

    fn read(e: &Entry) -> Vec<u8> {
        let all = std::fs::read(&e.file).unwrap();
        let mut v = std::fs::read(dir_file(&e.file)).unwrap()[e.preload.0 as usize..(e.preload.0 + e.preload.1) as usize].to_vec();
        v.extend_from_slice(&all[e.offset as usize..(e.offset + e.len) as usize]);
        v
    }

    #[test]
    fn reads_both_versions_with_preloads_and_archives() {
        for version in [1, 2] {
            let dir = std::env::temp_dir().join(format!("bonk_vpk_{}_{version}", std::process::id()));
            std::fs::create_dir_all(&dir).unwrap();
            let (d, archives) = build(
                version,
                &[
                    ("materials/Brick/Wall.vmt", b"\"LightmappedGeneric\" {}", IN_DIR, b""),
                    ("materials/brick/wall.vtf", b"", 0, b"VTFDATA"),
                    ("models/a.mdl", b"ID", 1, b"ST"),
                    ("readme", b"", IN_DIR, b"hello"),
                ],
            );
            let dp = dir.join("pak01_dir.vpk");
            std::fs::write(&dp, d).unwrap();
            for (i, a) in archives {
                std::fs::write(dir.join(format!("pak01_{i:03}.vpk")), a).unwrap();
            }
            // picking a numbered part opens the directory
            let (got_dir, e) = open(&dir.join("pak01_001.vpk")).unwrap();
            assert_eq!(got_dir, dp);
            let names: Vec<&str> = e.iter().map(|x| x.path.as_str()).collect();
            assert_eq!(names, ["materials/brick/wall.vmt", "materials/brick/wall.vtf", "models/a.mdl", "readme"]);
            assert_eq!(read(&e[0]), b"\"LightmappedGeneric\" {}");
            assert_eq!(e[1].file, dir.join("pak01_000.vpk"));
            assert_eq!(read(&e[1]), b"VTFDATA");
            assert_eq!(read(&e[2]), b"IDST");
            assert_eq!(e[2].preload.1 + e[2].len, 4);
            assert_eq!(read(&e[3]), b"hello");
            let _ = std::fs::remove_dir_all(&dir);
        }
    }

    #[test]
    fn refuses_what_isnt_a_vpk() {
        assert!(parse(b"GMAD....", Path::new("x.vpk")).is_err());
        let (mut d, _) = build(1, &[("a.txt", b"", IN_DIR, b"x")]);
        d[4] = 3; // version 3
        assert!(parse(&d, Path::new("x.vpk")).unwrap_err().contains("version 3"));
        let (d, _) = build(2, &[("a.txt", b"", IN_DIR, b"x")]);
        assert!(parse(&d[..20], Path::new("x.vpk")).is_err());
    }

    #[test]
    fn single_file_vpks_are_their_own_directory() {
        assert_eq!(dir_file(Path::new("/nowhere/mymod.vpk")), PathBuf::from("/nowhere/mymod.vpk"));
        // no _dir.vpk next to it: left as it is
        assert_eq!(dir_file(Path::new("/nowhere/pak01_002.vpk")), PathBuf::from("/nowhere/pak01_002.vpk"));
        assert_eq!(archive_file(Path::new("/g/hl2_textures_dir.vpk"), 7), PathBuf::from("/g/hl2_textures_007.vpk"));
        assert_eq!(archive_file(Path::new("/g/hl2_textures_dir.vpk"), IN_DIR), PathBuf::from("/g/hl2_textures_dir.vpk"));
    }
}
