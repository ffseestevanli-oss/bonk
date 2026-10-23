// SPDX-License-Identifier: GPL-3.0-or-later
//! Compiled Source models (.mdl + .vvd + .vtx), read for *showing* them:
//! spawn icons for models in an addon folder (the Spawnlist Editor's From
//! Addons tab and the standalone Spawnlist Creator). Only what's drawn is
//! read — LOD 0 of each body part's first bodygroup option, with its
//! textures. Turning a model back into brushes is not part of Bonk.

use crate::editor::Editor;
use crate::geom;
use crate::math::{v3, V3};
use crate::tex::TexMap;
use std::path::{Path, PathBuf};

// ------------------------------------------------------------------ bytes

struct R<'a>(&'a [u8]);

impl<'a> R<'a> {
    fn bytes(&self, o: usize, n: usize) -> Result<&'a [u8], String> {
        self.0.get(o..o.checked_add(n).ok_or("bad offset")?).ok_or_else(|| format!("cut short at {o}"))
    }
    fn u16(&self, o: usize) -> Result<u16, String> {
        let b = self.bytes(o, 2)?;
        Ok(u16::from_le_bytes([b[0], b[1]]))
    }
    fn i16(&self, o: usize) -> Result<i16, String> {
        Ok(self.u16(o)? as i16)
    }
    fn u32(&self, o: usize) -> Result<u32, String> {
        let b = self.bytes(o, 4)?;
        Ok(u32::from_le_bytes([b[0], b[1], b[2], b[3]]))
    }
    fn i32(&self, o: usize) -> Result<i32, String> {
        Ok(self.u32(o)? as i32)
    }
    fn f32(&self, o: usize) -> Result<f32, String> {
        Ok(f32::from_bits(self.u32(o)?))
    }
    fn v3(&self, o: usize) -> Result<V3, String> {
        Ok(v3(self.f32(o)?, self.f32(o + 4)?, self.f32(o + 8)?))
    }
    fn cstr(&self, o: usize) -> Result<String, String> {
        let rest = self.0.get(o..).ok_or("bad string offset")?;
        let end = rest.iter().position(|&c| c == 0).unwrap_or(rest.len());
        Ok(String::from_utf8_lossy(&rest[..end]).into_owned())
    }
    fn at(base: usize, rel: i32) -> Result<usize, String> {
        let o = base as i64 + rel as i64;
        if o < 0 {
            return Err("negative offset".into());
        }
        Ok(o as usize)
    }
}

/// studiomdl turns every model 90° about Z; this turns it back, so a compiled
/// model sits the way Bonk's own models do (and the spawn icon camera agrees).
fn unrotate(p: V3) -> V3 {
    v3(p.y, -p.x, p.z)
}

// ------------------------------------------------------------------ the model

#[derive(Clone, Copy, Debug)]
pub struct Vert {
    pub pos: V3,
    pub normal: V3,
    pub uv: (f32, f32),
}

#[derive(Clone, Debug, Default)]
pub struct Mesh {
    /// Index into Model::textures
    pub texture: usize,
    pub tris: Vec<[Vert; 3]>,
}

#[derive(Clone, Debug, Default)]
pub struct Model {
    /// Texture names as in the MDL ("brick_wall", or with folders)
    pub textures: Vec<String>,
    /// $cdmaterials folders, in order
    pub cdmaterials: Vec<String>,
    pub meshes: Vec<Mesh>,
}

const VVD_VERT: usize = 48;

fn read_vvd(d: &[u8]) -> Result<Vec<Vert>, String> {
    let r = R(d);
    if r.bytes(0, 4)? != b"IDSV" {
        return Err("not a .vvd (IDSV)".into());
    }
    let num_lod0 = r.i32(16)?.max(0) as usize;
    let num_fixups = r.i32(48)?.max(0) as usize;
    let fixup_start = r.i32(52)?.max(0) as usize;
    let vert_start = r.i32(56)?.max(0) as usize;
    let vert = |i: usize| -> Result<Vert, String> {
        let o = vert_start + i * VVD_VERT;
        Ok(Vert { pos: unrotate(r.v3(o + 16)?), normal: unrotate(r.v3(o + 28)?), uv: (r.f32(o + 40)?, r.f32(o + 44)?) })
    };
    let mut out = Vec::with_capacity(num_lod0);
    if num_fixups == 0 {
        for i in 0..num_lod0 {
            out.push(vert(i)?);
        }
    } else {
        for f in 0..num_fixups {
            let o = fixup_start + f * 12;
            let (lod, src, n) = (r.i32(o)?, r.i32(o + 4)?.max(0) as usize, r.i32(o + 8)?.max(0) as usize);
            if lod >= 0 {
                for i in src..src + n {
                    out.push(vert(i)?);
                }
            }
        }
    }
    Ok(out)
}

/// [bodypart][model][mesh] → LOD 0 triangles (mesh-relative vertex ids)
type VtxTris = Vec<Vec<Vec<Vec<[u16; 3]>>>>;

fn read_vtx(d: &[u8]) -> Result<VtxTris, String> {
    let r = R(d);
    if r.i32(0)? != 7 {
        return Err(format!("unexpected .vtx version {}", r.i32(0)?));
    }
    let num_bp = r.i32(28)?.max(0) as usize;
    let bp_off = r.i32(32)?;
    let mut out = Vec::new();
    for b in 0..num_bp {
        let bp = R::at(0, bp_off)? + b * 8;
        let (num_models, model_off) = (r.i32(bp)?.max(0) as usize, r.i32(bp + 4)?);
        let mut models = Vec::new();
        for m in 0..num_models {
            let mo = R::at(bp, model_off)? + m * 8;
            let (num_lods, lod_off) = (r.i32(mo)?, r.i32(mo + 4)?);
            let mut meshes = Vec::new();
            if num_lods > 0 {
                let lo = R::at(mo, lod_off)?;
                let (num_meshes, mesh_off) = (r.i32(lo)?.max(0) as usize, r.i32(lo + 4)?);
                for k in 0..num_meshes {
                    let me = R::at(lo, mesh_off)? + k * 9;
                    let (num_sg, sg_off) = (r.i32(me)?.max(0) as usize, r.i32(me + 4)?);
                    let mut tris = Vec::new();
                    for s in 0..num_sg {
                        let sg = R::at(me, sg_off)? + s * 25;
                        let (nv, vo, ni, io) = (r.i32(sg)?.max(0) as usize, r.i32(sg + 4)?, r.i32(sg + 8)?.max(0) as usize, r.i32(sg + 12)?);
                        let vbase = R::at(sg, vo)?;
                        let orig: Vec<u16> = (0..nv).map(|i| r.u16(vbase + i * 9 + 4)).collect::<Result<_, _>>()?;
                        let ibase = R::at(sg, io)?;
                        let idx: Vec<u16> = (0..ni).map(|i| r.u16(ibase + i * 2)).collect::<Result<_, _>>()?;
                        for t in idx.chunks_exact(3) {
                            let g = |i: u16| orig.get(i as usize).copied().ok_or("index out of range");
                            tris.push([g(t[0])?, g(t[1])?, g(t[2])?]);
                        }
                    }
                    meshes.push(tris);
                }
            }
            models.push(meshes);
        }
        out.push(models);
    }
    Ok(out)
}

/// Read a model: .mdl (structure, textures), .vvd (vertices), .vtx (triangles).
pub fn read_model(mdl: &[u8], vvd: &[u8], vtx: &[u8]) -> Result<Model, String> {
    let r = R(mdl);
    if r.bytes(0, 4)? != b"IDST" {
        return Err("not a .mdl (IDST)".into());
    }
    let version = r.i32(4)?;
    if !(44..=49).contains(&version) {
        return Err(format!("unsupported .mdl version {version}"));
    }
    let mut m = Model::default();
    let (num_tex, tex_idx) = (r.i32(204)?.max(0) as usize, r.i32(208)?.max(0) as usize);
    for i in 0..num_tex {
        let t = tex_idx + i * 64;
        m.textures.push(r.cstr(R::at(t, r.i32(t)?)?)?.replace('\\', "/"));
    }
    let (num_cd, cd_idx) = (r.i32(212)?.max(0) as usize, r.i32(216)?.max(0) as usize);
    for i in 0..num_cd {
        m.cdmaterials.push(r.cstr(r.i32(cd_idx + i * 4)?.max(0) as usize)?.replace('\\', "/"));
    }
    let (num_skinref, skin_idx) = (r.i32(220)?.max(0) as usize, r.i32(228)?.max(0) as usize);
    let skin = |i: usize| -> usize { r.i16(skin_idx + i * 2).map(|s| s.max(0) as usize).unwrap_or(i) };
    let verts = read_vvd(vvd)?;
    let tris = read_vtx(vtx)?;
    let (num_bp, bp_idx) = (r.i32(232)?.max(0) as usize, r.i32(236)?.max(0) as usize);
    for b in 0..num_bp {
        let bp = bp_idx + b * 16;
        let (num_models, model_idx) = (r.i32(bp + 4)?.max(0) as usize, r.i32(bp + 12)?);
        for mi in 0..num_models.min(1) {
            let mo = R::at(bp, model_idx)? + mi * 148;
            let (num_meshes, mesh_idx) = (r.i32(mo + 72)?.max(0) as usize, r.i32(mo + 76)?);
            let first = r.i32(mo + 84)?.max(0) as usize / VVD_VERT;
            for k in 0..num_meshes {
                let me = R::at(mo, mesh_idx)? + k * 116;
                let skinref = r.i32(me)?.max(0) as usize;
                let voff = r.i32(me + 12)?.max(0) as usize;
                let texture = if skinref < num_skinref { skin(skinref) } else { skinref };
                let mut mesh = Mesh { texture, tris: Vec::new() };
                if let Some(t) = tris.get(b).and_then(|x| x.get(mi)).and_then(|x| x.get(k)) {
                    for tri in t {
                        let g = |i: u16| verts.get(first + voff + i as usize).copied().ok_or("vertex out of range");
                        mesh.tris.push([g(tri[0])?, g(tri[1])?, g(tri[2])?]);
                    }
                }
                m.meshes.push(mesh);
            }
        }
    }
    Ok(m)
}

// ------------------------------------------------------------------ files in an addon folder

/// A file in a folder tree, matching names case-insensitively (models made on
/// Windows often disagree with their files about capitals).
pub fn find_ci(root: &Path, rel: &str) -> Option<PathBuf> {
    let mut cur = root.to_path_buf();
    for part in rel.split('/').filter(|p| !p.is_empty()) {
        let exact = cur.join(part);
        if exact.exists() {
            cur = exact;
            continue;
        }
        let lower = part.to_lowercase();
        let hit = std::fs::read_dir(&cur).ok()?.flatten().find(|e| e.file_name().to_string_lossy().to_lowercase() == lower)?;
        cur = hit.path();
    }
    Some(cur)
}

/// The addon folder a model is in (the folder holding its models/ folder).
pub fn addon_root(mdl: &Path) -> Option<PathBuf> {
    let mut p = mdl.parent();
    while let Some(d) = p {
        if d.file_name().is_some_and(|n| n.to_string_lossy().eq_ignore_ascii_case("models")) {
            return d.parent().map(Path::to_path_buf);
        }
        p = d.parent();
    }
    None
}

/// A VMT's "$basetexture" (and whether it's see-through: 1 alpha-tested, 2 translucent).
pub fn vmt_base(text: &str) -> (Option<String>, u8) {
    let mut base = None;
    let mut alpha = 0u8;
    for line in text.lines() {
        let l = line.split("//").next().unwrap_or("").trim();
        let mut parts = Vec::new();
        let mut rest = l;
        while !rest.is_empty() {
            rest = rest.trim_start();
            if let Some(r) = rest.strip_prefix('"') {
                let end = r.find('"').unwrap_or(r.len());
                parts.push(r[..end].to_string());
                rest = r.get(end + 1..).unwrap_or("");
            } else {
                let end = rest.find(char::is_whitespace).unwrap_or(rest.len());
                if end == 0 {
                    break;
                }
                parts.push(rest[..end].to_string());
                rest = &rest[end..];
            }
        }
        if parts.len() < 2 {
            continue;
        }
        let k = parts[0].to_lowercase();
        let v = parts[1].trim();
        match k.as_str() {
            "$basetexture" if base.is_none() => {
                let mut b = v.replace('\\', "/");
                while b.contains("//") {
                    b = b.replace("//", "/");
                }
                base = Some(b.trim_end_matches(".vtf").to_string());
            }
            "$translucent" if v != "0" => alpha = 2,
            "$alphatest" if v != "0" && alpha == 0 => alpha = 1,
            _ => {}
        }
    }
    (base, alpha)
}

/// One of the model's materials: (texture store key, width, height) once its
/// texture is loaded, None when it can't be found (drawn in a flat color).
fn load_material(root: &Path, model: &Model, tex: &str) -> Option<(String, u32, u32)> {
    let tex = tex.trim_start_matches('/');
    let mut cands: Vec<String> = model.cdmaterials.iter().map(|cd| format!("{}/{}", cd.trim_matches('/'), tex)).collect();
    cands.push(tex.to_string());
    for c in cands {
        let c = c.trim_start_matches('/').to_string();
        let Some(vmt) = find_ci(root, &format!("materials/{c}.vmt")) else { continue };
        let text = std::fs::read(&vmt).ok().map(|b| String::from_utf8_lossy(&b).into_owned()).unwrap_or_default();
        let (base, alpha) = vmt_base(&text);
        let base = base.unwrap_or_else(|| c.clone());
        let key = format!("addon:{}/{}", root.display(), base.to_lowercase());
        if let Some((w, h)) = crate::texstore::size(&key) {
            return Some((key, w, h));
        }
        let vtf = find_ci(root, &format!("materials/{base}.vtf"))?;
        let img = crate::vtfread::decode(&std::fs::read(vtf).ok()?).ok()?;
        let (w, h) = (img.w as u32, img.h as u32);
        crate::texstore::set(&key, w, h, img.px);
        crate::texstore::set_alpha(&key, alpha, 0.5);
        return Some((key, w, h));
    }
    None
}

/// The texture mapping that reproduces a triangle's UVs exactly (in texels).
fn tri_texmap(p: [V3; 3], uv: [(f32, f32); 3], w: f32, h: f32) -> Option<TexMap> {
    let (e1, e2) = (p[1] - p[0], p[2] - p[0]);
    let (a, b, c) = (e1.dot(e1), e1.dot(e2), e2.dot(e2));
    let det = a * c - b * b;
    if det.abs() < 1e-10 {
        return None;
    }
    // the axis A in the triangle's plane with A·e1 = d1 and A·e2 = d2
    let axis = |d1: f32, d2: f32| {
        let al = (d1 * c - d2 * b) / det;
        let be = (d2 * a - d1 * b) / det;
        e1 * al + e2 * be
    };
    let (u0, u1, u2) = (uv[0].0 * w, uv[1].0 * w, uv[2].0 * w);
    let (v0, v1, v2) = (uv[0].1 * h, uv[1].1 * h, uv[2].1 * h);
    let u = axis(u1 - u0, u2 - u0);
    let v = axis(v1 - v0, v2 - v0);
    Some(TexMap { u, v, shift: [u0 - p[0].dot(u), v0 - p[0].dot(v)], scale: [1.0, 1.0], rot: 0.0 })
}

/// A compiled model as an editor document for drawing: every triangle a
/// one-face brush that carries its texture and UVs.
pub fn load_editor(mdl_path: &Path) -> Result<Editor, String> {
    let read = |ext: &str| std::fs::read(mdl_path.with_extension(ext)).ok();
    let mdl = std::fs::read(mdl_path).map_err(|e| e.to_string())?;
    let vvd = read("vvd").ok_or("no .vvd next to it")?;
    let vtx = read("dx90.vtx").or_else(|| read("vtx")).or_else(|| read("dx80.vtx")).or_else(|| read("sw.vtx")).ok_or("no .vtx next to it")?;
    let model = read_model(&mdl, &vvd, &vtx)?;
    let root = addon_root(mdl_path);
    let mats: Vec<Option<(String, u32, u32)>> =
        model.textures.iter().map(|t| root.as_deref().and_then(|r| load_material(r, &model, t))).collect();
    let mut ed = Editor::new();
    for mesh in &model.meshes {
        let mat = mats.get(mesh.texture).cloned().flatten();
        for t in &mesh.tris {
            let p = [t[0].pos, t[1].pos, t[2].pos];
            let n = (p[1] - p[0]).cross(p[2] - p[0]);
            if n.len() < 1e-6 {
                continue;
            }
            // face the way the model's own vertex normals do
            let vn = t[0].normal + t[1].normal + t[2].normal;
            let (n, idx) = if vn.dot(n) < 0.0 { (n.norm() * -1.0, vec![0, 2, 1]) } else { (n.norm(), vec![0, 1, 2]) };
            let mut face = geom::Face { idx, normal: n, dist: n.dot(p[0]), material: String::new(), tex: TexMap::world_scaled(n, [1.0, 1.0]) };
            if let Some((key, w, h)) = &mat {
                face.material = key.clone();
                if let Some(tm) = tri_texmap(p, [t[0].uv, t[1].uv, t[2].uv], *w as f32, *h as f32) {
                    face.tex = tm;
                }
            } else {
                face.material = "addon:missing".into();
            }
            ed.doc.add_face_brush(p.to_vec(), face);
        }
    }
    if ed.doc.brushes.is_empty() {
        return Err("the model has no triangles to show".into());
    }
    Ok(ed)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn texmap_reproduces_triangle_uvs() {
        let p = [v3(0.0, 0.0, 0.0), v3(10.0, 0.0, 3.0), v3(2.0, 8.0, -1.0)];
        let uv = [(0.1, 0.2), (0.7, 0.25), (0.3, 0.9)];
        let tm = tri_texmap(p, uv, 256.0, 128.0).unwrap();
        for i in 0..3 {
            let (u, v) = tm.uv(p[i]);
            assert!((u - uv[i].0 * 256.0).abs() < 1e-2 && (v - uv[i].1 * 128.0).abs() < 1e-2, "{i}: {u} {v}");
        }
    }

    #[test]
    fn vmt_basetexture_and_alpha() {
        let t = "\"VertexLitGeneric\"\n{\n\t\"$basetexture\" \"models\\\\pack\\\\Brick\"\n\t$translucent 1 // see-through\n}\n";
        assert_eq!(vmt_base(t), (Some("models/pack/Brick".into()), 2));
        assert_eq!(vmt_base("x { $alphatest \"1\" }").1, 0); // one line, no key/value split: ignored
    }

    #[test]
    fn finds_files_whatever_their_capitals() {
        let dir = std::env::temp_dir().join(format!("bonk_mdl_ci_{}", std::process::id()));
        std::fs::create_dir_all(dir.join("Materials/Models")).unwrap();
        std::fs::write(dir.join("Materials/Models/Wood.VMT"), "x").unwrap();
        assert!(find_ci(&dir, "materials/models/wood.vmt").is_some());
        assert!(find_ci(&dir, "materials/models/stone.vmt").is_none());
        assert_eq!(addon_root(&dir.join("models/props/a.mdl")), Some(dir.clone()));
        let _ = std::fs::remove_dir_all(&dir);
    }
}
