// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Steventechno and the Bonk! contributors
//! VTF 7.2 writer for Source: DXT1 (opaque), DXT5 (alpha) or uncompressed
//! BGR(A)888(8) for normal maps, with a full mip chain and the 16px
//! thumbnail the engine expects. No external tools needed.

use std::io::Write;

const FMT_BGR888: u32 = 3;
const FMT_BGRA8888: u32 = 12;
const FMT_DXT1: u32 = 13;
const FMT_DXT5: u32 = 15;

const FLAG_NORMAL: u32 = 0x80;
const FLAG_ONEBITALPHA: u32 = 0x1000;
const FLAG_EIGHTBITALPHA: u32 = 0x2000;

#[derive(Clone, Copy, Default)]
pub struct VtfOptions {
    /// A normal map ($bumpmap): stored uncompressed, flagged as normal
    pub normal: bool,
    /// Alpha is used as a cut-out ($alphatest) rather than blended
    pub alpha_test: bool,
}

/// An RGBA8 image, rows top to bottom.
#[derive(Clone)]
pub struct Image {
    pub w: usize,
    pub h: usize,
    pub px: Vec<u8>,
}

impl Image {
    fn at(&self, x: usize, y: usize) -> [u8; 4] {
        let i = (y.min(self.h - 1) * self.w + x.min(self.w - 1)) * 4;
        [self.px[i], self.px[i + 1], self.px[i + 2], self.px[i + 3]]
    }

    /// Half size (box filter); odd sizes round down, never below 1.
    fn half(&self) -> Image {
        let (w, h) = ((self.w / 2).max(1), (self.h / 2).max(1));
        let mut px = Vec::with_capacity(w * h * 4);
        for y in 0..h {
            for x in 0..w {
                let mut s = [0u32; 4];
                for (dx, dy) in [(0, 0), (1, 0), (0, 1), (1, 1)] {
                    let p = self.at(x * 2 + dx, y * 2 + dy);
                    for c in 0..4 {
                        s[c] += p[c] as u32;
                    }
                }
                for c in s {
                    px.push(((c + 2) / 4) as u8);
                }
            }
        }
        Image { w, h, px }
    }

    /// Resample to exactly w × h (nearest-ish box), for the thumbnail.
    fn resized(&self, w: usize, h: usize) -> Image {
        let mut px = Vec::with_capacity(w * h * 4);
        for y in 0..h {
            for x in 0..w {
                let (x0, x1) = (x * self.w / w, ((x + 1) * self.w / w).max(x * self.w / w + 1));
                let (y0, y1) = (y * self.h / h, ((y + 1) * self.h / h).max(y * self.h / h + 1));
                let mut s = [0u64; 4];
                let mut n = 0u64;
                for yy in y0..y1 {
                    for xx in x0..x1 {
                        let p = self.at(xx, yy);
                        for c in 0..4 {
                            s[c] += p[c] as u64;
                        }
                        n += 1;
                    }
                }
                for c in s {
                    px.push((c / n.max(1)) as u8);
                }
            }
        }
        Image { w, h, px }
    }

    fn has_alpha(&self) -> bool {
        self.px.chunks(4).any(|p| p[3] < 250)
    }
}

fn to565(c: [f32; 3]) -> u16 {
    let r = (c[0].clamp(0.0, 255.0) * 31.0 / 255.0).round() as u16;
    let g = (c[1].clamp(0.0, 255.0) * 63.0 / 255.0).round() as u16;
    let b = (c[2].clamp(0.0, 255.0) * 31.0 / 255.0).round() as u16;
    (r << 11) | (g << 5) | b
}

fn from565(c: u16) -> [f32; 3] {
    let r = ((c >> 11) & 31) as f32 * 255.0 / 31.0;
    let g = ((c >> 5) & 63) as f32 * 255.0 / 63.0;
    let b = (c & 31) as f32 * 255.0 / 31.0;
    [r, g, b]
}

/// One 4×4 color block, DXT1 four-color mode.
fn color_block(px: &[[u8; 4]; 16], out: &mut Vec<u8>) {
    // endpoints: the extremes along the block's principal direction
    let mut mean = [0f32; 3];
    for p in px {
        for c in 0..3 {
            mean[c] += p[c] as f32 / 16.0;
        }
    }
    let mut cov = [[0f32; 3]; 3];
    for p in px {
        let d = [p[0] as f32 - mean[0], p[1] as f32 - mean[1], p[2] as f32 - mean[2]];
        for i in 0..3 {
            for j in 0..3 {
                cov[i][j] += d[i] * d[j];
            }
        }
    }
    let mut axis = [1f32, 1.0, 1.0];
    for _ in 0..8 {
        let n = [
            cov[0][0] * axis[0] + cov[0][1] * axis[1] + cov[0][2] * axis[2],
            cov[1][0] * axis[0] + cov[1][1] * axis[1] + cov[1][2] * axis[2],
            cov[2][0] * axis[0] + cov[2][1] * axis[1] + cov[2][2] * axis[2],
        ];
        let l = (n[0] * n[0] + n[1] * n[1] + n[2] * n[2]).sqrt();
        if l < 1e-6 {
            break;
        }
        axis = [n[0] / l, n[1] / l, n[2] / l];
    }
    let proj = |p: &[u8; 4]| (p[0] as f32 - mean[0]) * axis[0] + (p[1] as f32 - mean[1]) * axis[1] + (p[2] as f32 - mean[2]) * axis[2];
    let (mut lo, mut hi) = (f32::MAX, f32::MIN);
    for p in px {
        let t = proj(p);
        lo = lo.min(t);
        hi = hi.max(t);
    }
    let at = |t: f32| [mean[0] + axis[0] * t, mean[1] + axis[1] * t, mean[2] + axis[2] * t];
    let (mut c0, mut c1) = (to565(at(hi)), to565(at(lo)));
    if c0 < c1 {
        std::mem::swap(&mut c0, &mut c1);
    }
    let mut idx = 0u32;
    if c0 != c1 {
        let (a, b) = (from565(c0), from565(c1));
        let pal = [a, b, [(2.0 * a[0] + b[0]) / 3.0, (2.0 * a[1] + b[1]) / 3.0, (2.0 * a[2] + b[2]) / 3.0], [(a[0] + 2.0 * b[0]) / 3.0, (a[1] + 2.0 * b[1]) / 3.0, (a[2] + 2.0 * b[2]) / 3.0]];
        for (i, p) in px.iter().enumerate() {
            let best = (0..4)
                .min_by(|&x, &y| {
                    let d = |k: usize| (0..3).map(|c| (pal[k][c] - p[c] as f32).powi(2)).sum::<f32>();
                    d(x).partial_cmp(&d(y)).unwrap()
                })
                .unwrap() as u32;
            idx |= best << (i * 2);
        }
    }
    out.extend_from_slice(&c0.to_le_bytes());
    out.extend_from_slice(&c1.to_le_bytes());
    out.extend_from_slice(&idx.to_le_bytes());
}

/// One 4×4 alpha block, DXT5 eight-value mode.
fn alpha_block(px: &[[u8; 4]; 16], out: &mut Vec<u8>) {
    let a0 = px.iter().map(|p| p[3]).max().unwrap();
    let a1 = px.iter().map(|p| p[3]).min().unwrap();
    out.push(a0);
    out.push(a1);
    let mut bits: u64 = 0;
    if a0 != a1 {
        let pal: Vec<f32> = (0..8)
            .map(|k| match k {
                0 => a0 as f32,
                1 => a1 as f32,
                k => ((8 - k) as f32 * a0 as f32 + (k - 1) as f32 * a1 as f32) / 7.0,
            })
            .collect();
        for (i, p) in px.iter().enumerate() {
            let best = (0..8).min_by(|&x, &y| (pal[x] - p[3] as f32).abs().partial_cmp(&(pal[y] - p[3] as f32).abs()).unwrap()).unwrap() as u64;
            bits |= best << (i * 3);
        }
    }
    out.extend_from_slice(&bits.to_le_bytes()[..6]);
}

fn compress(img: &Image, alpha: bool) -> Vec<u8> {
    let mut out = Vec::new();
    for by in 0..img.h.div_ceil(4).max(1) {
        for bx in 0..img.w.div_ceil(4).max(1) {
            let mut px = [[0u8; 4]; 16];
            for (i, p) in px.iter_mut().enumerate() {
                *p = img.at(bx * 4 + i % 4, by * 4 + i / 4);
            }
            if alpha {
                alpha_block(&px, &mut out);
            }
            color_block(&px, &mut out);
        }
    }
    out
}

fn uncompressed(img: &Image, alpha: bool) -> Vec<u8> {
    let mut out = Vec::with_capacity(img.w * img.h * 4);
    for p in img.px.chunks(4) {
        out.extend_from_slice(&[p[2], p[1], p[0]]);
        if alpha {
            out.push(p[3]);
        }
    }
    out
}

/// Encode a VTF file.
#[allow(dead_code)]
pub fn encode(img: &Image, opt: VtfOptions) -> Vec<u8> {
    encode_frames(std::slice::from_ref(img), opt)
}

/// Encode a VTF with one or more frames (all the same size; for AnimatedTexture).
pub fn encode_frames(frames: &[Image], opt: VtfOptions) -> Vec<u8> {
    let img = &frames[0];
    let alpha = frames.iter().any(|f| f.has_alpha());
    let (format, mut flags) = if opt.normal {
        (if alpha { FMT_BGRA8888 } else { FMT_BGR888 }, FLAG_NORMAL)
    } else if alpha {
        (FMT_DXT5, 0)
    } else {
        (FMT_DXT1, 0)
    };
    if alpha {
        flags |= if opt.alpha_test { FLAG_ONEBITALPHA } else { FLAG_EIGHTBITALPHA };
    }
    // mip chain per frame, largest first
    let chains: Vec<Vec<Image>> = frames
        .iter()
        .map(|f| {
            let mut mips = vec![f.clone()];
            while mips.last().unwrap().w > 1 || mips.last().unwrap().h > 1 {
                let n = mips.last().unwrap().half();
                mips.push(n);
            }
            mips
        })
        .collect();
    let mips = &chains[0];
    // reflectivity: average linear color
    let mut refl = [0f64; 3];
    for p in img.px.chunks(4) {
        for c in 0..3 {
            refl[c] += (p[c] as f64 / 255.0).powf(2.2);
        }
    }
    let n = (img.w * img.h).max(1) as f64;
    // thumbnail: at most 16 on the long side, same aspect
    let (tw, th) = if img.w >= img.h { (16.min(img.w), ((16 * img.h) / img.w).clamp(1, 16)) } else { (((16 * img.w) / img.h).clamp(1, 16), 16.min(img.h)) };
    let thumb = img.resized(tw, th);

    let mut o: Vec<u8> = Vec::new();
    o.extend_from_slice(b"VTF\0");
    o.extend_from_slice(&7u32.to_le_bytes());
    o.extend_from_slice(&2u32.to_le_bytes());
    o.extend_from_slice(&80u32.to_le_bytes());
    o.extend_from_slice(&(img.w as u16).to_le_bytes());
    o.extend_from_slice(&(img.h as u16).to_le_bytes());
    o.extend_from_slice(&flags.to_le_bytes());
    o.extend_from_slice(&(frames.len() as u16).to_le_bytes()); // frames
    o.extend_from_slice(&0u16.to_le_bytes()); // first frame
    o.extend_from_slice(&[0u8; 4]);
    for r in refl {
        o.extend_from_slice(&((r / n) as f32).to_le_bytes());
    }
    o.extend_from_slice(&[0u8; 4]);
    o.extend_from_slice(&1.0f32.to_le_bytes()); // bumpmap scale
    o.extend_from_slice(&format.to_le_bytes());
    o.push(mips.len() as u8);
    o.extend_from_slice(&FMT_DXT1.to_le_bytes()); // thumbnail format
    o.push(tw as u8);
    o.push(th as u8);
    o.extend_from_slice(&1u16.to_le_bytes()); // depth
    o.resize(80, 0);
    o.extend_from_slice(&compress(&thumb, false));
    // smallest mip first; within a mip level, every frame in turn
    for level in (0..mips.len()).rev() {
        for chain in &chains {
            let m = &chain[level];
            let data = match format {
                FMT_DXT1 => compress(m, false),
                FMT_DXT5 => compress(m, true),
                _ => uncompressed(m, alpha),
            };
            o.extend_from_slice(&data);
        }
    }
    o
}

pub fn write(path: &str, img: &Image, opt: VtfOptions) -> Result<(), String> {
    write_frames(path, std::slice::from_ref(img), opt)
}

pub fn write_frames(path: &str, frames: &[Image], opt: VtfOptions) -> Result<(), String> {
    if frames.is_empty() || frames.len() > 1024 {
        return Err("bad frame count".into());
    }
    for img in frames {
        if img.w == 0 || img.h == 0 || img.w > 65535 || img.h > 65535 || img.px.len() < img.w * img.h * 4 || img.w != frames[0].w || img.h != frames[0].h {
            return Err("bad image size".into());
        }
    }
    if let Some(dir) = std::path::Path::new(path).parent() {
        std::fs::create_dir_all(dir).map_err(|e| format!("couldn't create {}: {}", dir.display(), e))?;
    }
    let data = encode_frames(frames, opt);
    let tmp = format!("{}.tmp", path);
    let mut f = std::fs::File::create(&tmp).map_err(|e| format!("couldn't write {}: {}", tmp, e))?;
    f.write_all(&data).map_err(|e| format!("couldn't write {}: {}", tmp, e))?;
    drop(f);
    std::fs::rename(&tmp, path).map_err(|e| format!("couldn't save {}: {}", path, e))
}

#[cfg(test)]
mod tests {
    #[test]
    fn frames_are_stored_for_animated_texture() {
        let mk = |c: u8| Image { w: 8, h: 8, px: (0..64).flat_map(|_| [c, 0, 0, 255]).collect() };
        let data = encode_frames(&[mk(255), mk(0), mk(128)], VtfOptions::default());
        assert_eq!(u16::from_le_bytes([data[24], data[25]]), 3, "frame count in the header");
        // frame 0 decodes as the first image
        let img = crate::vtfread::decode(&data).unwrap();
        assert_eq!((img.w, img.h), (8, 8));
        assert!(img.px[0] > 200, "first frame is the red one: {}", img.px[0]);
        let one = encode_frames(&[mk(1)], VtfOptions::default()).len();
        let two = encode_frames(&[mk(1), mk(2)], VtfOptions::default()).len();
        assert_eq!(data.len() - two, two - one, "each frame adds its own data");
        assert!(two > one);
    }

    use super::*;

    fn checker(w: usize, h: usize, alpha: bool) -> Image {
        let mut px = Vec::new();
        for y in 0..h {
            for x in 0..w {
                let on = (x / 4 + y / 4) % 2 == 0;
                px.extend_from_slice(&[if on { 220 } else { 30 }, 90, if on { 40 } else { 200 }, if alpha && x < w / 2 { 0 } else { 255 }]);
            }
        }
        Image { w, h, px }
    }

    #[test]
    fn dxt1_file_layout_and_size() {
        let v = encode(&checker(64, 32, false), VtfOptions::default());
        assert_eq!(&v[0..4], b"VTF\0");
        assert_eq!(u32::from_le_bytes(v[52..56].try_into().unwrap()), FMT_DXT1);
        assert_eq!(v[56], 7); // 64x32 → 1x1 is 7 levels
        // header + 16x8 thumb (4x2 blocks) + mips 64x32,32x16,16x8,8x4,4x2,2x1,1x1 in DXT1 blocks
        let blocks = |w: usize, h: usize| w.div_ceil(4).max(1) * h.div_ceil(4).max(1);
        let mips: usize = [(64, 32), (32, 16), (16, 8), (8, 4), (4, 2), (2, 1), (1, 1)].iter().map(|&(w, h)| blocks(w, h) * 8).sum();
        assert_eq!(v.len(), 80 + blocks(16, 8) * 8 + mips);
    }

    #[test]
    fn alpha_uses_dxt5_and_normal_maps_stay_uncompressed() {
        let v = encode(&checker(16, 16, true), VtfOptions::default());
        assert_eq!(u32::from_le_bytes(v[52..56].try_into().unwrap()), FMT_DXT5);
        assert!(u32::from_le_bytes(v[20..24].try_into().unwrap()) & FLAG_EIGHTBITALPHA != 0);
        let n = encode(&checker(16, 16, false), VtfOptions { normal: true, alpha_test: false });
        assert_eq!(u32::from_le_bytes(n[52..56].try_into().unwrap()), FMT_BGR888);
        assert!(u32::from_le_bytes(n[20..24].try_into().unwrap()) & FLAG_NORMAL != 0);
    }

    #[test]
    fn dxt1_block_round_trips_two_colours() {
        let mut px = [[0u8, 0, 0, 255]; 16];
        for (i, p) in px.iter_mut().enumerate() {
            if i % 2 == 0 {
                *p = [255, 255, 255, 255];
            }
        }
        let mut out = Vec::new();
        color_block(&px, &mut out);
        let c0 = u16::from_le_bytes([out[0], out[1]]);
        let c1 = u16::from_le_bytes([out[2], out[3]]);
        assert_eq!((c0, c1), (0xFFFF, 0));
        let idx = u32::from_le_bytes(out[4..8].try_into().unwrap());
        for i in 0..16 {
            assert_eq!((idx >> (i * 2)) & 3, if i % 2 == 0 { 0 } else { 1 });
        }
    }
}
