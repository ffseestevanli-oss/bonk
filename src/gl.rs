// SPDX-License-Identifier: GPL-3.0-or-later
// Copyright (C) 2026 Steventechno and the Bonk! contributors
//! Hand-rolled OpenGL 3.3 core function loader (no crates needed).
//! Function pointers come from Qt's current context via `getProcAddress`.

#![allow(non_snake_case, dead_code)]

use std::ffi::{c_char, c_void, CString};

pub type GetProc = extern "C" fn(*const c_char) -> *const c_void;

pub const COLOR_BUFFER_BIT: u32 = 0x4000;
pub const DEPTH_BUFFER_BIT: u32 = 0x0100;
pub const LINES: u32 = 0x0001;
pub const TRIANGLES: u32 = 0x0004;
pub const DEPTH_TEST: u32 = 0x0B71;
pub const BLEND: u32 = 0x0BE2;
pub const CULL_FACE: u32 = 0x0B44;
pub const POLYGON_OFFSET_FILL: u32 = 0x8037;
pub const LEQUAL: u32 = 0x0203;
pub const SRC_ALPHA: u32 = 0x0302;
pub const ONE_MINUS_SRC_ALPHA: u32 = 0x0303;
pub const FLOAT: u32 = 0x1406;
pub const FALSE: u8 = 0;
pub const ARRAY_BUFFER: u32 = 0x8892;
pub const STREAM_DRAW: u32 = 0x88E0;
pub const VERTEX_SHADER: u32 = 0x8B31;
pub const FRAGMENT_SHADER: u32 = 0x8B30;
pub const COMPILE_STATUS: u32 = 0x8B81;
pub const LINK_STATUS: u32 = 0x8B82;
pub const TEXTURE_2D: u32 = 0x0DE1;
pub const TEXTURE0: u32 = 0x84C0;
pub const RGBA: u32 = 0x1908;
pub const RGBA8: i32 = 0x8058;
pub const UNSIGNED_BYTE: u32 = 0x1401;
pub const TEXTURE_MIN_FILTER: u32 = 0x2801;
pub const TEXTURE_MAG_FILTER: u32 = 0x2800;
pub const TEXTURE_WRAP_S: u32 = 0x2802;
pub const TEXTURE_WRAP_T: u32 = 0x2803;
pub const LINEAR: i32 = 0x2601;
pub const LINEAR_MIPMAP_LINEAR: i32 = 0x2703;
pub const REPEAT: i32 = 0x2901;
pub const UNPACK_ALIGNMENT: u32 = 0x0CF5;

macro_rules! gl_fns {
    ($($name:ident : $sym:literal => fn($($a:ty),*) $(-> $r:ty)?;)*) => {
        pub struct Gl {
            $(pub $name: unsafe extern "system" fn($($a),*) $(-> $r)?,)*
        }
        impl Gl {
            /// # Safety
            /// A GL context must be current and `gp` must return valid pointers for it.
            pub unsafe fn load(gp: GetProc) -> Result<Gl, String> {
                Ok(Gl {
                    $($name: {
                        let c = CString::new($sym).unwrap();
                        let p = gp(c.as_ptr());
                        if p.is_null() {
                            return Err(format!("OpenGL function {} not available", $sym));
                        }
                        std::mem::transmute::<*const c_void, unsafe extern "system" fn($($a),*) $(-> $r)?>(p)
                    },)*
                })
            }
        }
    };
}

gl_fns! {
    ClearColor: "glClearColor" => fn(f32, f32, f32, f32);
    Clear: "glClear" => fn(u32);
    Enable: "glEnable" => fn(u32);
    Disable: "glDisable" => fn(u32);
    DepthFunc: "glDepthFunc" => fn(u32);
    DepthMask: "glDepthMask" => fn(u8);
    BlendFunc: "glBlendFunc" => fn(u32, u32);
    PolygonOffset: "glPolygonOffset" => fn(f32, f32);
    CreateShader: "glCreateShader" => fn(u32) -> u32;
    ShaderSource: "glShaderSource" => fn(u32, i32, *const *const c_char, *const i32);
    CompileShader: "glCompileShader" => fn(u32);
    GetShaderiv: "glGetShaderiv" => fn(u32, u32, *mut i32);
    GetShaderInfoLog: "glGetShaderInfoLog" => fn(u32, i32, *mut i32, *mut c_char);
    DeleteShader: "glDeleteShader" => fn(u32);
    CreateProgram: "glCreateProgram" => fn() -> u32;
    AttachShader: "glAttachShader" => fn(u32, u32);
    LinkProgram: "glLinkProgram" => fn(u32);
    GetProgramiv: "glGetProgramiv" => fn(u32, u32, *mut i32);
    GetProgramInfoLog: "glGetProgramInfoLog" => fn(u32, i32, *mut i32, *mut c_char);
    UseProgram: "glUseProgram" => fn(u32);
    GetUniformLocation: "glGetUniformLocation" => fn(u32, *const c_char) -> i32;
    UniformMatrix4fv: "glUniformMatrix4fv" => fn(i32, i32, u8, *const f32);
    Uniform1f: "glUniform1f" => fn(i32, f32);
    Uniform2f: "glUniform2f" => fn(i32, f32, f32);
    Uniform3f: "glUniform3f" => fn(i32, f32, f32, f32);
    Uniform4f: "glUniform4f" => fn(i32, f32, f32, f32, f32);
    Uniform1i: "glUniform1i" => fn(i32, i32);
    GenTextures: "glGenTextures" => fn(i32, *mut u32);
    DeleteTextures: "glDeleteTextures" => fn(i32, *const u32);
    BindTexture: "glBindTexture" => fn(u32, u32);
    ActiveTexture: "glActiveTexture" => fn(u32);
    TexImage2D: "glTexImage2D" => fn(u32, i32, i32, i32, i32, i32, u32, u32, *const c_void);
    TexParameteri: "glTexParameteri" => fn(u32, u32, i32);
    GenerateMipmap: "glGenerateMipmap" => fn(u32);
    PixelStorei: "glPixelStorei" => fn(u32, i32);
    GenVertexArrays: "glGenVertexArrays" => fn(i32, *mut u32);
    BindVertexArray: "glBindVertexArray" => fn(u32);
    GenBuffers: "glGenBuffers" => fn(i32, *mut u32);
    BindBuffer: "glBindBuffer" => fn(u32, u32);
    BufferData: "glBufferData" => fn(u32, isize, *const c_void, u32);
    VertexAttribPointer: "glVertexAttribPointer" => fn(u32, i32, u32, u8, i32, *const c_void);
    EnableVertexAttribArray: "glEnableVertexAttribArray" => fn(u32);
    DrawArrays: "glDrawArrays" => fn(u32, i32, i32);
}
