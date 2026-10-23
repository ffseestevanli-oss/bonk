// SPDX-License-Identifier: GPL-3.0-or-later
//! Walk mode (the ` key): walk around the model in first person, as the
//! player would, against its collision model. Sizes and speeds are Garry's
//! Mod's: a 32 × 32 × 72 player (36 crouched), eyes at 64 (28 crouched),
//! walking at 200 units/s (400 with Shift, 60 crouched), jump power 200,
//! gravity 600, and steps up to 18 units high climbed without jumping.

use crate::math::{v3, V3};

pub const HALF_WIDTH: f32 = 16.0;
pub const STAND_HEIGHT: f32 = 72.0;
pub const CROUCH_HEIGHT: f32 = 36.0;
pub const STAND_EYE: f32 = 64.0;
pub const CROUCH_EYE: f32 = 28.0;
pub const WALK_SPEED: f32 = 200.0;
pub const RUN_SPEED: f32 = 400.0;
pub const CROUCH_SPEED: f32 = 60.0;
pub const JUMP_SPEED: f32 = 200.0;
pub const GRAVITY: f32 = 600.0;
pub const STEP: f32 = 18.0;

/// A convex collision piece: its planes (normal, distance) and bounds.
#[derive(Clone, Debug)]
pub struct Solid {
    pub planes: Vec<(V3, f32)>,
    pub lo: V3,
    pub hi: V3,
}

/// What the player is holding down this step.
#[derive(Clone, Copy, Debug, Default)]
pub struct Input {
    pub forward: f32, // -1..1
    pub side: f32,    // -1..1 (right +)
    pub jump: bool,
    pub crouch: bool,
    pub run: bool,
}

#[derive(Clone, Debug)]
pub struct Walker {
    /// Feet: the middle of the bottom of the player's box
    pub feet: V3,
    pub vel_z: f32,
    pub on_ground: bool,
    pub crouched: bool,
    start: V3,
    floor_z: f32,
}

impl Walker {
    pub fn height(&self) -> f32 {
        if self.crouched { CROUCH_HEIGHT } else { STAND_HEIGHT }
    }
    pub fn eye(&self) -> V3 {
        self.feet + v3(0.0, 0.0, if self.crouched { CROUCH_EYE } else { STAND_EYE })
    }

    /// A player standing where `eye` is (moved up out of anything solid), or
    /// None if there's nothing to walk on.
    pub fn new(eye: V3, solids: &[Solid]) -> Option<Self> {
        if solids.is_empty() {
            return None;
        }
        let floor_z = solids.iter().map(|s| s.lo.z).fold(f32::MAX, f32::min);
        let mut w = Walker { feet: eye - v3(0.0, 0.0, STAND_EYE), vel_z: 0.0, on_ground: false, crouched: false, start: V3::default(), floor_z };
        let mut tries = 0;
        while w.blocked(w.feet, STAND_HEIGHT, solids) && tries < 512 {
            w.feet.z += 8.0;
            tries += 1;
        }
        w.start = w.feet;
        Some(w)
    }

    /// Whether the player's box (feet at `feet`, `h` tall) overlaps a solid.
    pub fn blocked(&self, feet: V3, h: f32, solids: &[Solid]) -> bool {
        let half = v3(HALF_WIDTH, HALF_WIDTH, h * 0.5);
        let c = feet + v3(0.0, 0.0, h * 0.5);
        const EPS: f32 = 0.01;
        solids.iter().any(|s| {
            // box against box first (the brush's bounds, grown by the player)
            if c.x <= s.lo.x - half.x + EPS || c.x >= s.hi.x + half.x - EPS || c.y <= s.lo.y - half.y + EPS || c.y >= s.hi.y + half.y - EPS
                || c.z <= s.lo.z - half.z + EPS || c.z >= s.hi.z + half.z - EPS
            {
                return false;
            }
            // then every face plane pushed out by the box's reach along its normal
            s.planes.iter().all(|&(n, d)| n.dot(c) < d + n.x.abs() * half.x + n.y.abs() * half.y + n.z.abs() * half.z - EPS)
        })
    }

    /// Move by `d`, stopping where something is in the way (one axis at a time,
    /// in small steps). Returns which axes were stopped.
    fn slide(&mut self, d: V3, solids: &[Solid]) -> [bool; 3] {
        let h = self.height();
        let mut stopped = [false; 3];
        for axis in 0..3 {
            let amount = d.get(axis);
            if amount == 0.0 {
                continue;
            }
            let steps = (amount.abs() / 4.0).ceil().max(1.0) as i32;
            let inc = amount / steps as f32;
            for _ in 0..steps {
                let mut next = self.feet;
                next.set(axis, next.get(axis) + inc);
                if self.blocked(next, h, solids) {
                    // close the gap a little at a time
                    let mut lo = 0.0f32;
                    let mut hi = 1.0f32;
                    for _ in 0..8 {
                        let mid = (lo + hi) * 0.5;
                        let mut t = self.feet;
                        t.set(axis, t.get(axis) + inc * mid);
                        if self.blocked(t, h, solids) { hi = mid } else { lo = mid }
                    }
                    let f = self.feet.get(axis) + inc * lo;
                    self.feet.set(axis, f);
                    stopped[axis] = true;
                    break;
                }
                self.feet = next;
            }
        }
        stopped
    }

    /// One step of `dt` seconds, facing `yaw` degrees.
    pub fn step(&mut self, dt: f32, yaw: f32, input: Input, solids: &[Solid]) {
        let dt = dt.clamp(0.0, 0.05);
        // crouching: down at once, up only when there's room
        if input.crouch && !self.crouched {
            self.crouched = true;
            if !self.on_ground {
                self.feet.z += STAND_HEIGHT - CROUCH_HEIGHT; // tuck the legs up in the air
                if self.blocked(self.feet, CROUCH_HEIGHT, solids) {
                    self.feet.z -= STAND_HEIGHT - CROUCH_HEIGHT;
                }
            }
        } else if !input.crouch && self.crouched && !self.blocked(self.feet, STAND_HEIGHT, solids) {
            self.crouched = false;
        }
        let speed = if self.crouched { CROUCH_SPEED } else if input.run { RUN_SPEED } else { WALK_SPEED };
        let (sy, cy) = yaw.to_radians().sin_cos();
        let fwd = v3(cy, sy, 0.0);
        let right = v3(sy, -cy, 0.0);
        let mut wish = fwd * input.forward + right * input.side;
        if wish.len() > 1.0 {
            wish = wish.norm();
        }
        if input.jump && self.on_ground {
            self.vel_z = JUMP_SPEED;
            self.on_ground = false;
        }
        self.vel_z -= GRAVITY * dt;
        // across: walk, and climb a step that's in the way
        let across = wish * (speed * dt);
        if across.len() > 0.0 {
            let before = self.feet;
            let hit = self.slide(across, solids);
            if (hit[0] || hit[1]) && self.on_ground {
                // try it again from a step higher, then settle back down
                let mut up = before;
                up.z += STEP;
                if !self.blocked(up, self.height(), solids) {
                    let kept = self.feet;
                    self.feet = up;
                    let hit2 = self.slide(across, solids);
                    let gained = (self.feet - up).len() > (kept - before).len() + 0.01;
                    if gained || !(hit2[0] || hit2[1]) {
                        self.slide(v3(0.0, 0.0, -STEP), solids);
                    } else {
                        self.feet = kept;
                    }
                }
            }
        }
        // up and down
        let hit = self.slide(v3(0.0, 0.0, self.vel_z * dt), solids);
        if hit[2] {
            if self.vel_z < 0.0 {
                self.on_ground = true;
            }
            self.vel_z = 0.0;
        } else {
            // walking off an edge: still on the ground if it's just a step down
            if self.on_ground && self.vel_z <= 0.0 {
                let before = self.feet;
                let h = self.slide(v3(0.0, 0.0, -STEP), solids);
                if h[2] {
                    self.vel_z = 0.0;
                } else {
                    self.feet = before;
                    self.on_ground = false;
                }
            } else {
                self.on_ground = false;
            }
        }
        // fell off the model: back to the start
        if self.feet.z < self.floor_z - 2048.0 {
            self.feet = self.start;
            self.vel_z = 0.0;
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    fn bx(lo: V3, hi: V3) -> Solid {
        let planes = vec![
            (v3(1.0, 0.0, 0.0), hi.x),
            (v3(-1.0, 0.0, 0.0), -lo.x),
            (v3(0.0, 1.0, 0.0), hi.y),
            (v3(0.0, -1.0, 0.0), -lo.y),
            (v3(0.0, 0.0, 1.0), hi.z),
            (v3(0.0, 0.0, -1.0), -lo.z),
        ];
        Solid { planes, lo, hi }
    }

    fn run(w: &mut Walker, secs: f32, yaw: f32, input: Input, solids: &[Solid]) {
        let mut t = 0.0;
        while t < secs {
            w.step(1.0 / 60.0, yaw, input, solids);
            t += 1.0 / 60.0;
        }
    }

    #[test]
    fn falls_onto_the_floor_and_walks_into_a_wall() {
        let solids = vec![bx(v3(-512.0, -512.0, -16.0), v3(512.0, 512.0, 0.0)), bx(v3(200.0, -512.0, 0.0), v3(216.0, 512.0, 128.0))];
        let mut w = Walker::new(v3(0.0, 0.0, 200.0), &solids).unwrap();
        run(&mut w, 2.0, 0.0, Input::default(), &solids);
        assert!(w.on_ground && w.feet.z.abs() < 0.1, "landed: {:?}", w.feet);
        assert!((w.eye().z - 64.0).abs() < 0.1, "HL2 eye height");
        run(&mut w, 2.0, 0.0, Input { forward: 1.0, ..Default::default() }, &solids);
        assert!(w.feet.x < 200.0 - 15.9 && w.feet.x > 200.0 - 16.5, "stopped at the wall: {:?}", w.feet);
    }

    #[test]
    fn climbs_steps_but_not_walls_and_jumps() {
        let solids = vec![
            bx(v3(-512.0, -512.0, -16.0), v3(512.0, 512.0, 0.0)),
            bx(v3(100.0, -64.0, 0.0), v3(140.0, 64.0, 16.0)), // a 16-high step
            bx(v3(140.0, -64.0, 0.0), v3(180.0, 64.0, 32.0)), // and another
            bx(v3(300.0, -512.0, 0.0), v3(316.0, 512.0, 30.0)), // a 30-high ledge: too high to step
        ];
        let mut w = Walker::new(v3(0.0, 0.0, 64.0), &solids).unwrap();
        run(&mut w, 0.5, 0.0, Input::default(), &solids);
        run(&mut w, 0.9, 0.0, Input { forward: 1.0, ..Default::default() }, &solids);
        assert!(w.feet.z > 31.0, "up the steps: {:?}", w.feet);
        run(&mut w, 2.0, 0.0, Input { forward: 1.0, ..Default::default() }, &solids);
        assert!(w.feet.x < 300.0 - 15.0, "the ledge stops a walk: {:?}", w.feet);
        // a jump clears it (200 up at gravity 600 rises 33)
        run(&mut w, 1.5, 0.0, Input { forward: 1.0, jump: true, ..Default::default() }, &solids);
        assert!(w.feet.x > 300.0 - 15.0, "jumped onto it: {:?}", w.feet);
    }

    #[test]
    fn crouching_gets_under_a_low_ceiling_and_stays_down_there() {
        let solids = vec![bx(v3(-512.0, -512.0, -16.0), v3(512.0, 512.0, 0.0)), bx(v3(100.0, -512.0, 50.0), v3(400.0, 512.0, 60.0))];
        let mut w = Walker::new(v3(0.0, 0.0, 64.0), &solids).unwrap();
        run(&mut w, 0.5, 0.0, Input::default(), &solids);
        run(&mut w, 1.0, 0.0, Input { forward: 1.0, ..Default::default() }, &solids);
        assert!(w.feet.x < 100.0, "standing doesn't fit: {:?}", w.feet);
        run(&mut w, 2.0, 0.0, Input { forward: 1.0, crouch: true, ..Default::default() }, &solids);
        assert!(w.feet.x > 120.0 && w.crouched, "crouched under it: {:?}", w.feet);
        run(&mut w, 0.2, 0.0, Input::default(), &solids);
        assert!(w.crouched, "no room to stand up yet");
    }
}
