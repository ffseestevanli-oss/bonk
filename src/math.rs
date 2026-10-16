// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Steventechno and the Bonk! contributors
//! Minimal vector / matrix math (Source-style Z-up world).

use std::ops::{Add, Mul, Neg, Sub};

#[derive(Clone, Copy, Debug, Default, PartialEq)]
pub struct V3 {
    pub x: f32,
    pub y: f32,
    pub z: f32,
}

pub const fn v3(x: f32, y: f32, z: f32) -> V3 {
    V3 { x, y, z }
}

impl V3 {
    pub fn get(&self, i: usize) -> f32 {
        match i {
            0 => self.x,
            1 => self.y,
            _ => self.z,
        }
    }
    pub fn set(&mut self, i: usize, v: f32) {
        match i {
            0 => self.x = v,
            1 => self.y = v,
            _ => self.z = v,
        }
    }
    pub fn dot(self, o: V3) -> f32 {
        self.x * o.x + self.y * o.y + self.z * o.z
    }
    pub fn cross(self, o: V3) -> V3 {
        v3(
            self.y * o.z - self.z * o.y,
            self.z * o.x - self.x * o.z,
            self.x * o.y - self.y * o.x,
        )
    }
    pub fn len(self) -> f32 {
        self.dot(self).sqrt()
    }
    pub fn norm(self) -> V3 {
        let l = self.len();
        if l > 1e-6 {
            self * (1.0 / l)
        } else {
            self
        }
    }
    pub fn min(self, o: V3) -> V3 {
        v3(self.x.min(o.x), self.y.min(o.y), self.z.min(o.z))
    }
    pub fn max(self, o: V3) -> V3 {
        v3(self.x.max(o.x), self.y.max(o.y), self.z.max(o.z))
    }
}

impl Add for V3 {
    type Output = V3;
    fn add(self, o: V3) -> V3 {
        v3(self.x + o.x, self.y + o.y, self.z + o.z)
    }
}
impl Sub for V3 {
    type Output = V3;
    fn sub(self, o: V3) -> V3 {
        v3(self.x - o.x, self.y - o.y, self.z - o.z)
    }
}
impl Mul<f32> for V3 {
    type Output = V3;
    fn mul(self, s: f32) -> V3 {
        v3(self.x * s, self.y * s, self.z * s)
    }
}
impl Neg for V3 {
    type Output = V3;
    fn neg(self) -> V3 {
        v3(-self.x, -self.y, -self.z)
    }
}

/// Column-major 4x4 matrix (OpenGL convention).
#[derive(Clone, Copy, Debug)]
pub struct M4(pub [f32; 16]);

impl M4 {
    pub fn mul(&self, o: &M4) -> M4 {
        let mut r = [0.0f32; 16];
        for c in 0..4 {
            for row in 0..4 {
                let mut s = 0.0;
                for k in 0..4 {
                    s += self.0[k * 4 + row] * o.0[c * 4 + k];
                }
                r[c * 4 + row] = s;
            }
        }
        M4(r)
    }

    pub fn ortho(l: f32, r: f32, b: f32, t: f32, n: f32, f: f32) -> M4 {
        let mut m = [0.0f32; 16];
        m[0] = 2.0 / (r - l);
        m[5] = 2.0 / (t - b);
        m[10] = -2.0 / (f - n);
        m[12] = -(r + l) / (r - l);
        m[13] = -(t + b) / (t - b);
        m[14] = -(f + n) / (f - n);
        m[15] = 1.0;
        M4(m)
    }

    pub fn perspective(fovy_rad: f32, aspect: f32, n: f32, f: f32) -> M4 {
        let t = 1.0 / (fovy_rad * 0.5).tan();
        let mut m = [0.0f32; 16];
        m[0] = t / aspect;
        m[5] = t;
        m[10] = (f + n) / (n - f);
        m[11] = -1.0;
        m[14] = 2.0 * f * n / (n - f);
        M4(m)
    }

    pub fn look_at(eye: V3, center: V3, up: V3) -> M4 {
        let f = (center - eye).norm();
        let s = f.cross(up).norm();
        let u = s.cross(f);
        M4([
            s.x, u.x, -f.x, 0.0, //
            s.y, u.y, -f.y, 0.0, //
            s.z, u.z, -f.z, 0.0, //
            -s.dot(eye), -u.dot(eye), f.dot(eye), 1.0,
        ])
    }

    /// Transform a point and return normalized device coordinates (None if behind the camera).
    #[allow(dead_code)]
    pub fn project(&self, p: V3) -> Option<V3> {
        let m = &self.0;
        let x = m[0] * p.x + m[4] * p.y + m[8] * p.z + m[12];
        let y = m[1] * p.x + m[5] * p.y + m[9] * p.z + m[13];
        let z = m[2] * p.x + m[6] * p.y + m[10] * p.z + m[14];
        let w = m[3] * p.x + m[7] * p.y + m[11] * p.z + m[15];
        if w <= 1e-6 {
            return None;
        }
        Some(v3(x / w, y / w, z / w))
    }
}

/// Snap a value to the nearest multiple of `grid`.
pub fn snap(v: f32, grid: f32) -> f32 {
    (v / grid).round() * grid
}

/// Ray / axis-aligned box intersection. Returns the entry distance.
#[allow(dead_code)]
pub fn ray_aabb(origin: V3, dir: V3, mins: V3, maxs: V3) -> Option<f32> {
    let mut tmin = f32::NEG_INFINITY;
    let mut tmax = f32::INFINITY;
    for i in 0..3 {
        let o = origin.get(i);
        let d = dir.get(i);
        let (lo, hi) = (mins.get(i), maxs.get(i));
        if d.abs() < 1e-8 {
            if o < lo || o > hi {
                return None;
            }
        } else {
            let mut t1 = (lo - o) / d;
            let mut t2 = (hi - o) / d;
            if t1 > t2 {
                std::mem::swap(&mut t1, &mut t2);
            }
            tmin = tmin.max(t1);
            tmax = tmax.min(t2);
            if tmin > tmax {
                return None;
            }
        }
    }
    if tmax < 0.0 {
        return None;
    }
    Some(tmin.max(0.0))
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn ray_hits_box() {
        let t = ray_aabb(v3(-100.0, 32.0, 32.0), v3(1.0, 0.0, 0.0), v3(0.0, 0.0, 0.0), v3(64.0, 64.0, 64.0));
        assert_eq!(t, Some(100.0));
        assert!(ray_aabb(v3(-100.0, 100.0, 32.0), v3(1.0, 0.0, 0.0), V3::default(), v3(64.0, 64.0, 64.0)).is_none());
    }

    #[test]
    fn snapping() {
        assert_eq!(snap(13.0, 16.0), 16.0);
        assert_eq!(snap(-9.0, 16.0), -16.0);
        assert_eq!(snap(7.0, 16.0), 0.0);
    }

    #[test]
    fn look_at_projects_center() {
        let eye = v3(0.0, 0.0, 0.0);
        let p = M4::perspective(1.2, 1.0, 1.0, 1000.0).mul(&M4::look_at(eye, v3(1.0, 0.0, 0.0), v3(0.0, 0.0, 1.0)));
        let ndc = p.project(v3(100.0, 0.0, 0.0)).unwrap();
        assert!(ndc.x.abs() < 1e-4 && ndc.y.abs() < 1e-4);
        // +Y is to the left when looking down +X with Z up
        assert!(p.project(v3(100.0, 10.0, 0.0)).unwrap().x < 0.0);
        assert!(p.project(v3(100.0, 0.0, 10.0)).unwrap().y > 0.0);
    }
}
