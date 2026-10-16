// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Steventechno and the Bonk! contributors
//! VTF reader: decodes the top mip of the first frame (and face, and slice)
//! of a Source texture into RGBA8. Covers versions 7.0 – 7.5 and the formats
//! Source materials actually use: DXT1/3/5, the 8-bit-per-channel orders,
//! the 16-bit packed ones, luminance/alpha and UV88 normal maps.

use crate::vtf::Image;

#[derive(Clone, Copy, Debug, PartialEq)]
pub struct Info {
    pub w: usize,
    pub h: usize,
    pub format: u32,
    pub frames: usize,
}

fn u16le(d: &[u8], o: usize) -> u32 {
    d[o] as u32 | (d[o + 1] as u32) << 8
}
fn u32le(d: &[u8], o: usize) -> u32 {
    u16le(d, o) | u16le(d, o + 2) << 16
}

/// Bytes for one w × h image in `format` (None for formats we can't size).
fn image_size(format: u32, w: usize, h: usize) -> Option<usize> {
    let blocks = w.div_ceil(4).max(1) * h.div_ceil(4).max(1);
    Some(match format {
        13 | 20 => blocks * 8,
        14 | 15 => blocks * 16,
        0 | 1 | 11 | 12 | 16 | 23 | 26 => w * h * 4,
        2 | 3 | 9 | 10 => w * h * 3,
        4 | 6 | 17 | 18 | 19 | 21 | 22 => w * h * 2,
        5 | 7 | 8 => w * h,
        24 | 25 => w * h * 8,
        _ => return None,
    })
}

struct Header {
    info: Info,
    faces: usize,
    depth: usize,
    mips: usize,
    high_offset: usize,
}

fn header(d: &[u8]) -> Result<Header, String> {
    if d.len() < 64 || &d[0..4] != b"VTF\0" {
        return Err("not a VTF file".into());
    }
    let (major, minor) = (u32le(d, 4), u32le(d, 8));
    if major != 7 || minor > 5 {
        return Err(format!("unsupported VTF version {major}.{minor}"));
    }
    let header_size = u32le(d, 12) as usize;
    let w = u16le(d, 16) as usize;
    let h = u16le(d, 18) as usize;
    let flags = u32le(d, 20);
    let frames = (u16le(d, 24) as usize).max(1);
    let first_frame = u16le(d, 26);
    let format = u32le(d, 52);
    let mips = (d[56] as usize).max(1);
    let low_format = u32le(d, 57);
    let (lw, lh) = (d[61] as usize, d[62] as usize);
    let depth = if minor >= 2 && d.len() >= 65 { (u16le(d, 63) as usize).max(1) } else { 1 };
    let faces = if flags & 0x4000 != 0 {
        if minor < 5 && first_frame != 0xffff { 7 } else { 6 }
    } else {
        1
    };
    if w == 0 || h == 0 {
        return Err("empty image".into());
    }
    let high_offset = if minor >= 3 {
        let n = u32le(d, 68) as usize;
        let mut off = None;
        for i in 0..n.min(32) {
            let o = 80 + i * 8;
            if o + 8 > d.len() {
                break;
            }
            if d[o] == 0x30 && d[o + 1] == 0 && d[o + 2] == 0 {
                off = Some(u32le(d, o + 4) as usize);
            }
        }
        off.ok_or("no image data in this VTF")?
    } else {
        let low = if low_format == u32::MAX || lw == 0 || lh == 0 { 0 } else { image_size(low_format, lw, lh).unwrap_or(0) };
        header_size + low
    };
    Ok(Header { info: Info { w, h, format, frames }, faces, depth, mips, high_offset })
}

/// Size and format, without decoding.
pub fn info(d: &[u8]) -> Result<Info, String> {
    header(d).map(|h| h.info)
}

fn c565(c: u32) -> [u8; 3] {
    let r = (c >> 11) & 31;
    let g = (c >> 5) & 63;
    let b = c & 31;
    [((r << 3) | (r >> 2)) as u8, ((g << 2) | (g >> 4)) as u8, ((b << 3) | (b >> 2)) as u8]
}

/// One DXT color block (8 bytes) into 16 RGBA texels.
fn color_block(b: &[u8], dxt1: bool, out: &mut [[u8; 4]; 16]) {
    let (c0, c1) = (u16le(b, 0), u16le(b, 2));
    let (p0, p1) = (c565(c0), c565(c1));
    let mut pal = [[p0[0], p0[1], p0[2], 255], [p1[0], p1[1], p1[2], 255], [0; 4], [0; 4]];
    if c0 > c1 || !dxt1 {
        for k in 0..3 {
            pal[2][k] = ((2 * p0[k] as u32 + p1[k] as u32) / 3) as u8;
            pal[3][k] = ((p0[k] as u32 + 2 * p1[k] as u32) / 3) as u8;
        }
        pal[2][3] = 255;
        pal[3][3] = 255;
    } else {
        for k in 0..3 {
            pal[2][k] = ((p0[k] as u32 + p1[k] as u32) / 2) as u8;
        }
        pal[2][3] = 255;
        pal[3] = [0, 0, 0, 0];
    }
    let bits = u32le(b, 4);
    for (i, o) in out.iter_mut().enumerate() {
        *o = pal[((bits >> (2 * i)) & 3) as usize];
    }
}

fn decode_dxt(d: &[u8], w: usize, h: usize, format: u32) -> Image {
    let mut px = vec![0u8; w * h * 4];
    let bsize = if format == 13 || format == 20 { 8 } else { 16 };
    let (bw, bh) = (w.div_ceil(4).max(1), h.div_ceil(4).max(1));
    let mut texels = [[0u8; 4]; 16];
    for by in 0..bh {
        for bx in 0..bw {
            let o = (by * bw + bx) * bsize;
            let blk = &d[o..o + bsize];
            match format {
                13 | 20 => color_block(blk, true, &mut texels),
                14 => {
                    color_block(&blk[8..], false, &mut texels);
                    for (i, t) in texels.iter_mut().enumerate() {
                        let a = (blk[i / 2] >> (4 * (i % 2))) & 15;
                        t[3] = a * 17;
                    }
                }
                _ => {
                    color_block(&blk[8..], false, &mut texels);
                    let (a0, a1) = (blk[0] as u32, blk[1] as u32);
                    let mut pal = [a0, a1, 0, 0, 0, 0, 0, 0];
                    if a0 > a1 {
                        for k in 1..7 {
                            pal[k + 1] = ((7 - k as u32) * a0 + k as u32 * a1) / 7;
                        }
                    } else {
                        for k in 1..5 {
                            pal[k + 1] = ((5 - k as u32) * a0 + k as u32 * a1) / 5;
                        }
                        pal[6] = 0;
                        pal[7] = 255;
                    }
                    let mut bits: u64 = 0;
                    for k in 0..6 {
                        bits |= (blk[2 + k] as u64) << (8 * k);
                    }
                    for (i, t) in texels.iter_mut().enumerate() {
                        t[3] = pal[((bits >> (3 * i)) & 7) as usize] as u8;
                    }
                }
            }
            for (i, t) in texels.iter().enumerate() {
                let (x, y) = (bx * 4 + i % 4, by * 4 + i / 4);
                if x < w && y < h {
                    let p = (y * w + x) * 4;
                    px[p..p + 4].copy_from_slice(t);
                }
            }
        }
    }
    Image { w, h, px }
}

fn decode_plain(d: &[u8], w: usize, h: usize, format: u32) -> Result<Image, String> {
    let n = w * h;
    let mut px = Vec::with_capacity(n * 4);
    let half = |v: u16| -> f32 {
        // IEEE half → f32
        let s = if v & 0x8000 != 0 { -1.0 } else { 1.0 };
        let e = ((v >> 10) & 31) as i32;
        let m = (v & 1023) as f32;
        s * if e == 0 { m * 2f32.powi(-24) } else if e == 31 { f32::INFINITY } else { (1.0 + m / 1024.0) * 2f32.powi(e - 15) }
    };
    for i in 0..n {
        let p = match format {
            0 => [d[i * 4], d[i * 4 + 1], d[i * 4 + 2], d[i * 4 + 3]],
            1 => [d[i * 4 + 3], d[i * 4 + 2], d[i * 4 + 1], d[i * 4]],
            11 => [d[i * 4 + 1], d[i * 4 + 2], d[i * 4 + 3], d[i * 4]],
            12 => [d[i * 4 + 2], d[i * 4 + 1], d[i * 4], d[i * 4 + 3]],
            16 => [d[i * 4 + 2], d[i * 4 + 1], d[i * 4], 255],
            23 | 26 => [d[i * 4], d[i * 4 + 1], d[i * 4 + 2], 255],
            2 | 9 => [d[i * 3], d[i * 3 + 1], d[i * 3 + 2], 255],
            3 | 10 => [d[i * 3 + 2], d[i * 3 + 1], d[i * 3], 255],
            4 => {
                let [r, g, b] = c565(u16le(d, i * 2));
                [r, g, b, 255]
            }
            17 => {
                let [r, g, b] = c565(u16le(d, i * 2));
                [b, g, r, 255]
            }
            18 | 21 => {
                let c = u16le(d, i * 2);
                let ex = |v: u32| ((v << 3) | (v >> 2)) as u8;
                [ex((c >> 10) & 31), ex((c >> 5) & 31), ex(c & 31), if format == 21 && c & 0x8000 == 0 { 0 } else { 255 }]
            }
            19 => {
                let c = u16le(d, i * 2);
                let ex = |v: u32| (v * 17) as u8;
                [ex((c >> 8) & 15), ex((c >> 4) & 15), ex(c & 15), ex((c >> 12) & 15)]
            }
            5 | 7 => [d[i], d[i], d[i], 255],
            8 => [255, 255, 255, d[i]],
            6 => [d[i * 2], d[i * 2], d[i * 2], d[i * 2 + 1]],
            22 => [d[i * 2], d[i * 2 + 1], 255, 255],
            24 => {
                let f = |k: usize| {
                    let v = half(u16le(d, i * 8 + k * 2) as u16).max(0.0);
                    // simple tone map so HDR textures still look like something
                    ((v / (1.0 + v)).powf(1.0 / 2.2) * 255.0).round().clamp(0.0, 255.0) as u8
                };
                [f(0), f(1), f(2), 255]
            }
            25 => [d[i * 8 + 1], d[i * 8 + 3], d[i * 8 + 5], d[i * 8 + 7]],
            _ => return Err(format!("VTF image format {format} isn't supported")),
        };
        px.extend_from_slice(&p);
    }
    Ok(Image { w, h, px })
}

/// Decode the largest mip of the first frame.
pub fn decode(d: &[u8]) -> Result<Image, String> {
    let hd = header(d)?;
    let Info { w, h, format, frames } = hd.info;
    let one = |mw: usize, mh: usize, md: usize| image_size(format, mw, mh).map(|s| s * md);
    if one(w, h, 1).is_none() {
        return Err(format!("VTF image format {format} isn't supported"));
    }
    // mips are stored smallest first; skip every smaller one
    let mut off = hd.high_offset;
    for m in (1..hd.mips).rev() {
        let (mw, mh, md) = ((w >> m).max(1), (h >> m).max(1), (hd.depth >> m).max(1));
        off += one(mw, mh, md).unwrap() * frames * hd.faces;
    }
    let size = one(w, h, 1).unwrap();
    if off + size > d.len() {
        return Err("the VTF file is cut short".into());
    }
    let data = &d[off..off + size];
    match format {
        13 | 14 | 15 | 20 => Ok(decode_dxt(data, w, h, format)),
        _ => decode_plain(data, w, h, format),
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::vtf::{encode, VtfOptions};

    fn gradient(w: usize, h: usize, alpha: bool) -> Image {
        let mut px = Vec::new();
        for y in 0..h {
            for x in 0..w {
                px.extend_from_slice(&[(x * 255 / w) as u8, (y * 255 / h) as u8, 128, if alpha { (x * 4) as u8 } else { 255 }]);
            }
        }
        Image { w, h, px }
    }

    fn rmse(a: &Image, b: &Image) -> f64 {
        let s: f64 = a.px.iter().zip(&b.px).map(|(x, y)| (*x as f64 - *y as f64).powi(2)).sum();
        (s / a.px.len() as f64).sqrt()
    }

    #[test]
    fn reads_back_what_bonk_writes() {
        for (alpha, normal) in [(false, false), (true, false), (false, true), (true, true)] {
            let img = gradient(64, 32, alpha);
            let data = encode(&img, VtfOptions { normal, alpha_test: false });
            let i = info(&data).unwrap();
            assert_eq!((i.w, i.h), (64, 32));
            let back = decode(&data).unwrap();
            assert_eq!((back.w, back.h), (64, 32));
            let e = rmse(&img, &back);
            assert!(e < if normal { 0.5 } else { 6.0 }, "alpha {alpha} normal {normal}: rmse {e}");
        }
    }

    #[test]
    fn reads_version_7_4_with_resources() {
        // Re-pack a 7.2 file as 7.4: 80-byte header, then a resource directory
        // pointing at the low- and high-res data.
        let img = gradient(16, 16, false);
        let v2 = encode(&img, VtfOptions::default());
        let low = image_size(13, 16, 16).unwrap(); // thumbnail is 16x16 DXT1
        let body = &v2[80..];
        let mut o = v2[..80].to_vec();
        o[8..12].copy_from_slice(&4u32.to_le_bytes());
        let hs = 80 + 2 * 8 + 8; // 8 bytes padding after the directory
        o[12..16].copy_from_slice(&(hs as u32).to_le_bytes());
        o[68..72].copy_from_slice(&2u32.to_le_bytes());
        o.extend_from_slice(&[0x01, 0, 0, 0]);
        o.extend_from_slice(&(hs as u32).to_le_bytes());
        o.extend_from_slice(&[0x30, 0, 0, 0]);
        o.extend_from_slice(&((hs + low) as u32).to_le_bytes());
        o.resize(hs, 0);
        o.extend_from_slice(body);
        let back = decode(&o).unwrap();
        assert_eq!(back.px, decode(&v2).unwrap().px, "same pixels as the 7.2 file");
        assert!(rmse(&img, &back) < 12.0);
        assert!(decode(b"nope").is_err());
        assert!(decode(&o[..o.len() - 10]).is_err(), "truncated");
    }

    #[test]
    fn plain_formats() {
        // 2x1 BGRA8888 by hand, 7.1 header, no thumbnail
        let mut o = vec![0u8; 64];
        o[0..4].copy_from_slice(b"VTF\0");
        o[4..8].copy_from_slice(&7u32.to_le_bytes());
        o[8..12].copy_from_slice(&1u32.to_le_bytes());
        o[12..16].copy_from_slice(&64u32.to_le_bytes());
        o[16..18].copy_from_slice(&2u16.to_le_bytes());
        o[18..20].copy_from_slice(&1u16.to_le_bytes());
        o[24..26].copy_from_slice(&1u16.to_le_bytes());
        o[52..56].copy_from_slice(&12u32.to_le_bytes());
        o[56] = 1;
        o[57..61].copy_from_slice(&u32::MAX.to_le_bytes());
        o.extend_from_slice(&[10, 20, 30, 40, 50, 60, 70, 80]);
        let img = decode(&o).unwrap();
        assert_eq!(img.px, vec![30, 20, 10, 40, 70, 60, 50, 80]);
    }
}
