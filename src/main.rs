#![allow(static_mut_refs)]
use std::f32::consts::PI;
use std::fs::File;
use std::io::Read;
use std::thread::sleep;
use std::time::{Duration, Instant};
use std::{
    ffi::c_void,
    ptr::null_mut,
    slice,
    sync::atomic::{AtomicPtr, Ordering},
};

use font8x8::legacy::BASIC_LEGACY;
use openal_sys as al;
use sdl2::render::WindowCanvas;
use sdl2::{EventPump, Sdl, pixels::Color, rect::Rect};

use aligned::*;
#[macro_use]
mod portlog;
use windows::{
    Win32::{
        Foundation::*,
        Storage::FileSystem::*,
        System::{Hypervisor::*, Memory::*},
    },
    core::{Error, HRESULT, PCWSTR, Result},
};

#[link(name = "Kernel32")]
unsafe extern "system" {
    fn Beep(freq: u32, dur: u32) -> BOOL;
}

const DEFAULT_BIOS: &str = "ami_8088_bios_31jan89.bin\0";
const FALLBACK_BIOS: &str = "ivt.fw\0";
/// Total address space mapped for the guest (1 MiB).
const GUEST_MEM_SIZE: usize = 0x100000;
/// Conventional RAM size reported through port 0x62.
const GUEST_RAM_KB: usize = 640;

/// Duration for the startup beep and speaker output in milliseconds.
const BEEP_DURATION_MS: u32 = 300;

fn openal_beep(freq: u32, dur_ms: u32) {
    unsafe {
        let device = al::alcOpenDevice(std::ptr::null());
        if device.is_null() {
            return;
        }
        let context = al::alcCreateContext(device, std::ptr::null());
        if context.is_null() {
            al::alcCloseDevice(device);
            return;
        }
        al::alcMakeContextCurrent(context);

        let mut buffer = 0;
        al::alGenBuffers(1, &mut buffer);
        let sample_rate = 44_100u32;
        let samples_len = (dur_ms as usize * sample_rate as usize) / 1000;
        let mut samples: Vec<i16> = Vec::with_capacity(samples_len);
        for n in 0..samples_len {
            let t = n as f32 / sample_rate as f32;
            let val = if (t * freq as f32).fract() < 0.5 {
                0.8
            } else {
                -0.8
            };
            samples.push((val * i16::MAX as f32) as i16);
        }
        al::alBufferData(
            buffer,
            al::AL_FORMAT_MONO16,
            samples.as_ptr().cast(),
            (samples.len() * std::mem::size_of::<i16>()) as i32,
            sample_rate as i32,
        );

        let mut source = 0;
        al::alGenSources(1, &mut source);
        al::alSourcei(source, al::AL_BUFFER, buffer as i32);
        al::alSourcePlay(source);
        sleep(Duration::from_millis(dur_ms as u64));
        al::alDeleteSources(1, &source);
        al::alDeleteBuffers(1, &buffer);
        al::alcMakeContextCurrent(std::ptr::null_mut());
        al::alcDestroyContext(context);
        al::alcCloseDevice(device);
    }
}

static GLOBAL_EMULATOR_HANDLE: AtomicPtr<c_void> = AtomicPtr::new(null_mut());
static GLOBAL_EMULATOR_CALLBACKS: WHV_EMULATOR_CALLBACKS = WHV_EMULATOR_CALLBACKS {
    Size: size_of::<WHV_EMULATOR_CALLBACKS>() as u32,
    Reserved: 0,
    WHvEmulatorIoPortCallback: Some(emu_io_port_callback),
    WHvEmulatorMemoryCallback: Some(emu_memory_callback),
    WHvEmulatorGetVirtualProcessorRegisters: Some(emu_get_vcpu_reg_callback),
    WHvEmulatorSetVirtualProcessorRegisters: Some(emu_set_vcpu_reg_callback),
    WHvEmulatorTranslateGvaPage: Some(emu_translate_gva_callback),
};

static mut SDL_CANVAS: Option<WindowCanvas> = None;
static mut SDL_PUMP: Option<EventPump> = None;
static mut SDL_CONTEXT: Option<Sdl> = None;

const IO_PORT_DMA_ADDR0: u16 = 0x0000;
const IO_PORT_DMA_COUNT0: u16 = 0x0001;
const IO_PORT_KBD_DATA: u16 = 0x0060;
const IO_PORT_KBD_STATUS: u16 = 0x0064;
const IO_PORT_DISK_DATA: u16 = 0x00FF;
const IO_PORT_POST: u16 = 0x0080;
const IO_PORT_PIC_MASTER_CMD: u16 = 0x0020;
const IO_PORT_PIC_MASTER_DATA: u16 = 0x0021;
const IO_PORT_PIC_SLAVE_CMD: u16 = 0x00A0;
const IO_PORT_PIC_SLAVE_DATA: u16 = 0x00A1;
const IO_PORT_SYS_CTRL: u16 = 0x0061;
const IO_PORT_SYS_PORTC: u16 = 0x0062;
const IO_PORT_MDA_MODE: u16 = 0x03B8;
const IO_PORT_CGA_MODE: u16 = 0x03D8;
const IO_PORT_DMA_PAGE3: u16 = 0x0083;
const IO_PORT_DMA_MASK: u16 = 0x000A;
const IO_PORT_DMA_MODE: u16 = 0x000B;
const IO_PORT_VIDEO_MISC_B8: u16 = 0x00B8;
const IO_PORT_SPECIAL_213: u16 = 0x0213;
const IO_PORT_PIT_CMD: u16 = 0x0008;
const IO_PORT_DMA_TEMP: u16 = 0x000D;
const IO_PORT_DMA_CLEAR: u16 = 0x000C;
const IO_PORT_PIT_COUNTER0: u16 = 0x0040;
const IO_PORT_PIT_COUNTER1: u16 = 0x0041;
const IO_PORT_PIT_COUNTER2: u16 = 0x0042;
const IO_PORT_PIT_CONTROL: u16 = 0x0043;
const IO_PORT_TIMER_MISC: u16 = 0x0063;
const IO_PORT_DMA_PAGE1: u16 = 0x0081;
const IO_PORT_PORT_0210: u16 = 0x0210;
const IO_PORT_PORT_0278: u16 = 0x0278;
const IO_PORT_PORT_02FA: u16 = 0x02FA;
const IO_PORT_PORT_0378: u16 = 0x0378;
const IO_PORT_PORT_03BC: u16 = 0x03BC;
const IO_PORT_PORT_03FA: u16 = 0x03FA;
const IO_PORT_PORT_0201: u16 = 0x0201;
const IO_PORT_CRTC_INDEX_MDA: u16 = 0x03B4;
const IO_PORT_CRTC_DATA_MDA: u16 = 0x03B5;
const IO_PORT_ATTR_MDA: u16 = 0x03B9;
const IO_PORT_CRTC_INDEX_CGA: u16 = 0x03D4;
const IO_PORT_CRTC_DATA_CGA: u16 = 0x03D5;
const IO_PORT_ATTR_CGA: u16 = 0x03D9;
const IO_PORT_CGA_STATUS: u16 = 0x03DA;
const IO_PORT_FDC_DOR: u16 = 0x03F2;
const IO_PORT_FDC_STATUS: u16 = 0x03F4;
const IO_PORT_FDC_DATA: u16 = 0x03F5;

fn port_name(port: u16) -> &'static str {
    match port {
        IO_PORT_DMA_ADDR0 => "DMA_ADDR0",
        IO_PORT_DMA_COUNT0 => "DMA_CNT0",
        IO_PORT_KBD_DATA => "KBD_DATA",
        IO_PORT_KBD_STATUS => "KBD_STATUS",
        IO_PORT_DISK_DATA => "DISK_DATA",
        IO_PORT_POST => "POST",
        IO_PORT_PIC_MASTER_CMD => "PIC_MASTER_CMD",
        IO_PORT_PIC_MASTER_DATA => "PIC_MASTER_DATA",
        IO_PORT_PIC_SLAVE_CMD => "PIC_SLAVE_CMD",
        IO_PORT_PIC_SLAVE_DATA => "PIC_SLAVE_DATA",
        IO_PORT_SYS_CTRL => "SYS_CTRL",
        IO_PORT_SYS_PORTC => "SYS_PORTC",
        IO_PORT_MDA_MODE => "MDA_MODE",
        IO_PORT_CGA_MODE => "CGA_MODE",
        IO_PORT_DMA_PAGE3 => "DMA_PAGE3",
        IO_PORT_DMA_MASK => "DMA_MASK",
        IO_PORT_DMA_MODE => "DMA_MODE",
        IO_PORT_DMA_CLEAR => "DMA_CLEAR",
        IO_PORT_VIDEO_MISC_B8 => "VIDEO_MISC_B8",
        IO_PORT_SPECIAL_213 => "PORT_213",
        IO_PORT_DMA_TEMP => "DMA_TEMP",
        IO_PORT_PIT_COUNTER0 => "PIT_COUNTER0",
        IO_PORT_PIT_COUNTER1 => "PIT_COUNTER1",
        IO_PORT_PIT_COUNTER2 => "PIT_COUNTER2",
        IO_PORT_PIT_CONTROL => "PIT_CONTROL",
        IO_PORT_PIT_CMD => "PIT_CMD",
        IO_PORT_TIMER_MISC => "TIMER_MISC",
        IO_PORT_DMA_PAGE1 => "DMA_PAGE1",
        IO_PORT_PORT_0210 => "PORT_0210",
        IO_PORT_PORT_0278 => "PORT_0278",
        IO_PORT_PORT_02FA => "PORT_02FA",
        IO_PORT_PORT_0378 => "PORT_0378",
        IO_PORT_PORT_03BC => "PORT_03BC",
        IO_PORT_PORT_03FA => "PORT_03FA",
        IO_PORT_PORT_0201 => "PORT_0201",
        IO_PORT_CRTC_INDEX_MDA => "MDA_INDEX",
        IO_PORT_CRTC_DATA_MDA => "MDA_DATA",
        IO_PORT_ATTR_MDA => "MDA_ATTR",
        IO_PORT_CRTC_INDEX_CGA => "CGA_INDEX",
        IO_PORT_CRTC_DATA_CGA => "CGA_DATA",
        IO_PORT_ATTR_CGA => "CGA_ATTR",
        IO_PORT_CGA_STATUS => "CGA_STATUS",
        IO_PORT_FDC_DOR => "FDC_DOR",
        IO_PORT_FDC_STATUS => "FDC_STATUS",
        IO_PORT_FDC_DATA => "FDC_DATA",
        _ => "UNKNOWN",
    }
}

const DISK_IMAGE_SIZE: usize = 512;
static mut DISK_IMAGE: [u8; DISK_IMAGE_SIZE] = [0; DISK_IMAGE_SIZE];
static mut DISK_OFFSET: usize = 0;
static mut LAST_UNKNOWN_PORT: u16 = 0;
static mut UNKNOWN_PORT_COUNT: u32 = 0;
static mut PIC_MASTER_IMR: u8 = 0;
static mut PIC_SLAVE_IMR: u8 = 0;
static mut SYS_CTRL: u8 = 0;
const PIT_FREQUENCY: u64 = 1_193_182;

#[derive(Copy, Clone)]
struct PitChannel {
    count: u16,
    reload: u16,
    mode: u8,
    access: u8,
    bcd: bool,
    latched: bool,
    latch: u16,
    rw_low: bool,
}

impl PitChannel {
    const fn new() -> Self {
        Self {
            count: 0,
            reload: 0,
            mode: 0,
            access: 0,
            bcd: false,
            latched: false,
            latch: 0,
            rw_low: true,
        }
    }
}

static mut PIT_CONTROL: u8 = 0;
static mut PIT_CHANNELS: [PitChannel; 3] =
    [PitChannel::new(), PitChannel::new(), PitChannel::new()];
static mut PIT_LAST_UPDATE: Option<Instant> = None;
static mut PIT_FRACTIONAL_TICKS: f64 = 0.0;
static mut CGA_MODE: u8 = 0;
static mut MDA_MODE: u8 = 0;
static mut DMA_TEMP: u8 = 0;
static mut DMA_MODE: u8 = 0;
static mut DMA_MASK: u8 = 0;
static mut DMA_CLEAR: u8 = 0;
static mut DMA_PAGE1: u8 = 0;
static mut PORT_0210_VAL: u8 = 0;
static mut PORT_0278_VAL: u8 = 0;
static mut PORT_02FA_VAL: u8 = 0;
static mut PORT_0378_VAL: u8 = 0;
static mut PORT_03BC_VAL: u8 = 0;
static mut PORT_03FA_VAL: u8 = 0;
static mut PORT_0201_VAL: u8 = 0;
static mut SPEAKER_ON: bool = false;
static mut CRTC_MDA_INDEX: u8 = 0;
static mut CRTC_MDA_DATA: u8 = 0;
static mut CRTC_MDA_REGS: [u8; 32] = [0; 32];
static mut ATTR_MDA: u8 = 0;
static mut CRTC_CGA_INDEX: u8 = 0;
static mut CRTC_CGA_DATA: u8 = 0;
static mut CRTC_CGA_REGS: [u8; 32] = [0; 32];
static mut ATTR_CGA: u8 = 0;
static mut CGA_STATUS: u8 = 0;
static mut CGA_LAST_TOGGLE: Option<Instant> = None;
const CGA_TOGGLE_PERIOD: Duration = Duration::from_millis(16);
static mut FDC_DOR: u8 = 0;
static mut FDC_STATUS: u8 = 0;
static mut FDC_DATA: u8 = 0;
static mut DMA_CHAN: [u8; 8] = [0; 8];
/// Memory size reported by the BIOS (in KB).
const MEM_SIZE_KB: usize = GUEST_RAM_KB;
/// Value returned by reading port 0x62.
const MEM_NIBBLE: u8 = ((MEM_SIZE_KB - 64) / 32) as u8;

const CGA_COLS: usize = 80;
const CGA_ROWS: usize = 25;
const CGA_COLORS: [(u8, u8, u8); 16] = [
    (0, 0, 0),
    (0, 0, 170),
    (0, 170, 0),
    (0, 170, 170),
    (170, 0, 0),
    (170, 0, 170),
    (170, 85, 0),
    (170, 170, 170),
    (85, 85, 85),
    (85, 85, 255),
    (85, 255, 85),
    (85, 255, 255),
    (255, 85, 85),
    (255, 85, 255),
    (255, 255, 85),
    (255, 255, 255),
];
static mut CGA_BUFFER: [u16; CGA_COLS * CGA_ROWS] = [0x0720; CGA_COLS * CGA_ROWS];
static mut CGA_CURSOR: usize = 0;
static mut CGA_SHADOW: [u16; CGA_COLS * CGA_ROWS] = [0x0720; CGA_COLS * CGA_ROWS];

fn update_pit() {
    unsafe {
        let now = Instant::now();
        if let Some(last) = PIT_LAST_UPDATE {
            let elapsed = now.duration_since(last);
            let mut ticks_f = elapsed.as_secs_f64() * PIT_FREQUENCY as f64 + PIT_FRACTIONAL_TICKS;
            let ticks = ticks_f.floor() as u64;
            PIT_FRACTIONAL_TICKS = ticks_f - ticks as f64;
            if ticks > 0 {
                for ch in &mut PIT_CHANNELS {
                    let reload = if ch.reload == 0 {
                        0x10000u32
                    } else {
                        ch.reload as u32
                    };
                    let mut count = if ch.count == 0 {
                        0x10000u32
                    } else {
                        ch.count as u32
                    };
                    let mut remaining = ticks as i64;
                    while remaining > 0 {
                        if remaining as u32 >= count {
                            remaining -= count as i64;
                            count = reload;
                        } else {
                            count -= remaining as u32;
                            remaining = 0;
                        }
                    }
                    ch.count = if count == 0x10000 { 0 } else { count as u16 };
                }
            }
            PIT_LAST_UPDATE = Some(now);
        } else {
            PIT_LAST_UPDATE = Some(now);
        }
    }
}

fn pit_read(idx: usize) -> u8 {
    unsafe {
        let ch = &mut PIT_CHANNELS[idx];
        let mut val: u32 = if ch.latched {
            ch.latch as u32
        } else {
            ch.count as u32
        };
        if val == 0 {
            val = 0x10000;
        }
        let byte = if ch.access == 2 {
            (val >> 8) as u8
        } else if ch.access == 3 {
            let out = if ch.rw_low {
                (val & 0xFF) as u8
            } else {
                (val >> 8) as u8
            };
            ch.rw_low = !ch.rw_low;
            if !ch.rw_low {
                ch.latched = false;
            }
            out
        } else {
            ch.latched = false;
            (val & 0xFF) as u8
        };
        if ch.access != 3 {
            ch.latched = false;
        }
        byte
    }
}

fn pit_write(idx: usize, val: u8) {
    unsafe {
        let ch = &mut PIT_CHANNELS[idx];
        match ch.access {
            2 => {
                ch.reload = (ch.reload & 0x00FF) | ((val as u16) << 8);
                ch.count = ch.reload;
            }
            3 => {
                if ch.rw_low {
                    ch.reload = (ch.reload & 0xFF00) | val as u16;
                    ch.rw_low = false;
                } else {
                    ch.reload = (ch.reload & 0x00FF) | ((val as u16) << 8);
                    ch.count = ch.reload;
                    ch.rw_low = true;
                }
            }
            _ => {
                ch.reload = (ch.reload & 0xFF00) | val as u16;
                ch.count = ch.reload;
            }
        }
    }
}

fn cga_put_char(ch: u8) {
    unsafe {
        if ch == b'\r' {
            CGA_CURSOR -= CGA_CURSOR % CGA_COLS;
            return;
        }
        if ch == b'\n' {
            CGA_CURSOR += CGA_COLS;
        } else {
            if CGA_CURSOR >= CGA_COLS * CGA_ROWS {
                CGA_BUFFER.copy_within(CGA_COLS.., 0);
                for i in 0..CGA_COLS {
                    CGA_BUFFER[CGA_COLS * (CGA_ROWS - 1) + i] = 0x0720;
                }
                CGA_CURSOR -= CGA_COLS;
            }
            CGA_BUFFER[CGA_CURSOR] = 0x0700 | (ch as u16);
            CGA_CURSOR += 1;
        }
        if CGA_CURSOR >= CGA_COLS * CGA_ROWS {
            CGA_CURSOR = CGA_COLS * CGA_ROWS - 1;
        }
        render_cga_window();
    }
}

fn print_cga_buffer(mem: *const u8) {
    unsafe {
        println!("\n----- CGA Text Buffer -----");
        for r in 0..CGA_ROWS {
            for c in 0..CGA_COLS {
                let mut cell = CGA_BUFFER[r * CGA_COLS + c];
                if !mem.is_null() {
                    cell = *(mem.add(0xB8000 + 2 * (r * CGA_COLS + c)) as *const u16);
                }
                let mut ch = (cell & 0xFF) as u8;
                if ch == 0 {
                    ch = b' ';
                }
                print!("{}", ch as char);
            }
            println!("");
        }
    }
}

fn clear_cga_buffer(mem: *mut u8) {
    unsafe {
        CGA_CURSOR = 0;
        for i in 0..CGA_COLS * CGA_ROWS {
            CGA_BUFFER[i] = 0x0720;
            CGA_SHADOW[i] = 0x0720;
            if !mem.is_null() {
                *(mem.add(0xB8000 + 2 * i) as *mut u16) = 0x0720;
            }
        }
        render_cga_window();
    }
}

fn sync_cga_from_memory(mem: *const u8) {
    unsafe {
        let mut dirty = false;
        for i in 0..CGA_COLS * CGA_ROWS {
            let val = *(mem.add(0xB8000 + 2 * i) as *const u16);
            if CGA_SHADOW[i] != val {
                CGA_SHADOW[i] = val;
                CGA_BUFFER[i] = val;
                dirty = true;
            }
        }
        if dirty {
            render_cga_window();
        }
    }
}

fn render_cga_window() {
    unsafe {
        if let Some(canvas) = SDL_CANVAS.as_mut() {
            if let Some(pump) = SDL_PUMP.as_mut() {
                for _ in pump.poll_iter() {}
            }
            canvas.set_draw_color(Color::RGB(0, 0, 0));
            canvas.clear();
            for r in 0..CGA_ROWS {
                for c in 0..CGA_COLS {
                    let cell = CGA_BUFFER[r * CGA_COLS + c];
                    let ch = (cell & 0xFF) as usize;
                    let attr = ((cell >> 8) & 0xFF) as usize;
                    let fg = attr & 0x0F;
                    let bg = (attr >> 4) & 0x07;
                    let blink = attr & 0x80 != 0;
                    let bgc = CGA_COLORS[bg];
                    canvas.set_draw_color(Color::RGB(bgc.0, bgc.1, bgc.2));
                    let _ = canvas.fill_rect(Rect::new((c * 8) as i32, (r * 8) as i32, 8, 8));
                    if !blink {
                        let fgc = CGA_COLORS[fg];
                        canvas.set_draw_color(Color::RGB(fgc.0, fgc.1, fgc.2));
                        let glyph = BASIC_LEGACY[ch];
                        for (y, row_bits) in glyph.iter().enumerate() {
                            for x in 0..8 {
                                if (row_bits >> x) & 1 != 0 {
                                    let px = (c * 8 + (7 - x)) as i32;
                                    let py = (r * 8 + y) as i32;
                                    let _ = canvas.fill_rect(Rect::new(px, py, 1, 1));
                                }
                            }
                        }
                    }
                }
            }
            canvas.present();
        }
    }
}

const INITIAL_VCPU_COUNT: usize = 40;
const INITIAL_VCPU_REGISTER_NAMES: [WHV_REGISTER_NAME; INITIAL_VCPU_COUNT] = [
    WHvX64RegisterRax,
    WHvX64RegisterRcx,
    WHvX64RegisterRdx,
    WHvX64RegisterRbx,
    WHvX64RegisterRsp,
    WHvX64RegisterRbp,
    WHvX64RegisterRsi,
    WHvX64RegisterRdi,
    WHvX64RegisterR8,
    WHvX64RegisterR9,
    WHvX64RegisterR10,
    WHvX64RegisterR11,
    WHvX64RegisterR12,
    WHvX64RegisterR13,
    WHvX64RegisterR14,
    WHvX64RegisterR15,
    WHvX64RegisterRip,
    WHvX64RegisterRflags,
    WHvX64RegisterEs,
    WHvX64RegisterCs,
    WHvX64RegisterSs,
    WHvX64RegisterDs,
    WHvX64RegisterFs,
    WHvX64RegisterGs,
    WHvX64RegisterLdtr,
    WHvX64RegisterTr,
    WHvX64RegisterIdtr,
    WHvX64RegisterGdtr,
    WHvX64RegisterCr0,
    WHvX64RegisterCr2,
    WHvX64RegisterCr3,
    WHvX64RegisterCr4,
    WHvX64RegisterDr0,
    WHvX64RegisterDr1,
    WHvX64RegisterDr2,
    WHvX64RegisterDr3,
    WHvX64RegisterDr6,
    WHvX64RegisterDr7,
    WHvX64RegisterXCr0,
    WHvX64RegisterFpControlStatus,
];
// Note: WHV_REGISTER_VALUE should be aligned on 16-byte boundary. However, the definition of it isn't aligned to 16-byte boundary.
const INITIAL_VCPU_REGISTER_VALUES: Aligned<A16, [WHV_REGISTER_VALUE; INITIAL_VCPU_COUNT]> =
    Aligned([
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0xFFF0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0xFFF0 },
        WHV_REGISTER_VALUE { Reg64: 0x2 },
        WHV_REGISTER_VALUE {
            Segment: WHV_X64_SEGMENT_REGISTER {
                Base: 0,
                Limit: 0xFFFF,
                Selector: 0,
                Anonymous: WHV_X64_SEGMENT_REGISTER_0 { Attributes: 0x93 },
            },
        },
        WHV_REGISTER_VALUE {
            Segment: WHV_X64_SEGMENT_REGISTER {
                Base: 0xF0000,
                Limit: 0xFFFF,
                Selector: 0xF000,
                Anonymous: WHV_X64_SEGMENT_REGISTER_0 { Attributes: 0x9B },
            },
        },
        WHV_REGISTER_VALUE {
            Segment: WHV_X64_SEGMENT_REGISTER {
                Base: 0,
                Limit: 0xFFFF,
                Selector: 0,
                Anonymous: WHV_X64_SEGMENT_REGISTER_0 { Attributes: 0x93 },
            },
        },
        WHV_REGISTER_VALUE {
            Segment: WHV_X64_SEGMENT_REGISTER {
                Base: 0,
                Limit: 0xFFFF,
                Selector: 0,
                Anonymous: WHV_X64_SEGMENT_REGISTER_0 { Attributes: 0x93 },
            },
        },
        WHV_REGISTER_VALUE {
            Segment: WHV_X64_SEGMENT_REGISTER {
                Base: 0,
                Limit: 0xFFFF,
                Selector: 0,
                Anonymous: WHV_X64_SEGMENT_REGISTER_0 { Attributes: 0x93 },
            },
        },
        WHV_REGISTER_VALUE {
            Segment: WHV_X64_SEGMENT_REGISTER {
                Base: 0,
                Limit: 0xFFFF,
                Selector: 0,
                Anonymous: WHV_X64_SEGMENT_REGISTER_0 { Attributes: 0x93 },
            },
        },
        WHV_REGISTER_VALUE {
            Segment: WHV_X64_SEGMENT_REGISTER {
                Base: 0,
                Limit: 0xFFFF,
                Selector: 0,
                Anonymous: WHV_X64_SEGMENT_REGISTER_0 { Attributes: 0x82 },
            },
        },
        WHV_REGISTER_VALUE {
            Segment: WHV_X64_SEGMENT_REGISTER {
                Base: 0,
                Limit: 0xFFFF,
                Selector: 0,
                Anonymous: WHV_X64_SEGMENT_REGISTER_0 { Attributes: 0x83 },
            },
        },
        WHV_REGISTER_VALUE {
            Table: WHV_X64_TABLE_REGISTER {
                Base: 0,
                Pad: [0; 3],
                Limit: 0xFFFF,
            },
        },
        WHV_REGISTER_VALUE {
            Table: WHV_X64_TABLE_REGISTER {
                Base: 0,
                Pad: [0; 3],
                Limit: 0xFFFF,
            },
        },
        WHV_REGISTER_VALUE { Reg64: 0x10 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0 },
        WHV_REGISTER_VALUE { Reg64: 0xFFFF0FF0 },
        WHV_REGISTER_VALUE { Reg64: 0x400 },
        WHV_REGISTER_VALUE { Reg64: 1 },
        WHV_REGISTER_VALUE {
            FpControlStatus: WHV_X64_FP_CONTROL_STATUS_REGISTER {
                Anonymous: WHV_X64_FP_CONTROL_STATUS_REGISTER_0 {
                    FpControl: 0x40,
                    FpStatus: 0,
                    FpTag: 0x55,
                    Reserved: 0,
                    LastFpOp: 0,
                    Anonymous: WHV_X64_FP_CONTROL_STATUS_REGISTER_0_0 { LastFpRip: 0 },
                },
            },
        },
    ]);

#[repr(C)]
struct SimpleVirtualMachine {
    handle: WHV_PARTITION_HANDLE,
    vmem: *mut c_void,
    size: usize,
}

impl SimpleVirtualMachine {
    fn new(memory_size: usize) -> Result<Self> {
        match unsafe { WHvCreatePartition() } {
            Ok(h) => {
                let vcpu_count_prop = WHV_PARTITION_PROPERTY { ProcessorCount: 1 };
                if let Err(e) = unsafe {
                    WHvSetPartitionProperty(
                        h,
                        WHvPartitionPropertyCodeProcessorCount,
                        (&raw const vcpu_count_prop).cast(),
                        size_of::<WHV_PARTITION_PROPERTY>() as u32,
                    )
                } {
                    panic!("Failed to setup vCPU Count! Reason: {e}");
                }
                if let Err(e) = unsafe { WHvSetupPartition(h) } {
                    panic!("Failed to setup partition! Reason: {e}");
                }
                if let Err(e) = unsafe { WHvCreateVirtualProcessor(h, 0, 0) } {
                    panic!("Failed to create vCPU! Reason: {e}");
                } else {
                    if let Err(e) = unsafe {
                        WHvSetVirtualProcessorRegisters(
                            h,
                            0,
                            INITIAL_VCPU_REGISTER_NAMES.as_ptr(),
                            INITIAL_VCPU_COUNT as u32,
                            INITIAL_VCPU_REGISTER_VALUES.as_ptr(),
                        )
                    } {
                        panic!("Failed to initialize vCPU registers! Reason: {e}");
                    }
                }
                let p = unsafe { VirtualAlloc(None, memory_size, MEM_COMMIT, PAGE_READWRITE) };
                if p.is_null() {
                    panic!(
                        "Failed to allocate virtual memory! Reason: {}",
                        unsafe { GetLastError() }.to_hresult().message()
                    );
                }
                if let Err(e) = unsafe {
                    WHvMapGpaRange(
                        h,
                        p,
                        0,
                        memory_size as u64,
                        WHvMapGpaRangeFlagRead
                            | WHvMapGpaRangeFlagWrite
                            | WHvMapGpaRangeFlagExecute,
                    )
                } {
                    panic!("Failed to map guest memory! Reason: {e}");
                }
                if let Err(e) = unsafe {
                    WHvMapGpaRange(
                        h,
                        p,
                        memory_size as u64,
                        memory_size as u64,
                        WHvMapGpaRangeFlagRead
                            | WHvMapGpaRangeFlagWrite
                            | WHvMapGpaRangeFlagExecute,
                    )
                } {
                    panic!("Failed to map high memory mirror! Reason: {e}");
                }
                Ok(Self {
                    handle: h,
                    vmem: p,
                    size: memory_size,
                })
            }
            Err(e) => Err(e),
        }
    }

    fn load_program(&self, file_name: &str, offset: usize) -> Result<usize> {
        let path = file_name.encode_utf16();
        let v: Vec<u16> = path.collect();
        match unsafe {
            CreateFileW(
                PCWSTR(v.as_ptr()),
                GENERIC_READ.0,
                FILE_SHARE_READ,
                None,
                OPEN_EXISTING,
                FILE_ATTRIBUTE_NORMAL,
                None,
            )
        } {
            Ok(h) => {
                let mut file_size = 0;
                let r = match unsafe { GetFileSizeEx(h, &raw mut file_size) } {
                    Ok(_) => {
                        if offset + file_size as usize > self.size {
                            panic!("Overflow happens while loading {file_name}!");
                        }
                        let mut size = 0;
                        let addr: *mut u8 = unsafe { self.vmem.byte_add(offset).cast() };
                        let buffer: &mut [u8] =
                            unsafe { slice::from_raw_parts_mut(addr, file_size as usize) };
                        unsafe { ReadFile(h, Some(buffer), Some(&raw mut size), None) }
                            .map(|_| size as usize)
                    }
                    Err(e) => Err(e),
                };
                let _ = unsafe { CloseHandle(h) };
                r
            }
            Err(e) => Err(e),
        }
    }

    fn run(&self) {
        let mut exit_ctxt: WHV_RUN_VP_EXIT_CONTEXT = WHV_RUN_VP_EXIT_CONTEXT::default();
        let mut cont_exec = true;
        while cont_exec {
            if let Err(e) = unsafe {
                WHvRunVirtualProcessor(
                    self.handle,
                    0,
                    (&raw mut exit_ctxt).cast(),
                    size_of::<WHV_RUN_VP_EXIT_CONTEXT>() as u32,
                )
            } {
                println!("Failed to run vCPU! Reason: {e}");
                cont_exec = false;
            } else {
                #[allow(non_upper_case_globals)]
                match exit_ctxt.ExitReason {
                    WHvRunVpExitReasonX64IoPortAccess => {
                        match self.try_emulate_io(&raw const exit_ctxt.VpContext, unsafe {
                            &raw const exit_ctxt.Anonymous.IoPortAccess
                        }) {
                            Ok(st) => {
                                let s = unsafe { st.AsUINT32 };
                                if s != 1 {
                                    println!(
                                        "Failed to emulate I/O instruction! Status=0x{:08X}",
                                        s
                                    )
                                }
                            }
                            Err(e) => {
                                println!("Failed to emulate I/O instruction! Reason: {e}");
                                cont_exec = false;
                            }
                        }
                    }
                    WHvRunVpExitReasonX64Halt => {
                        // Treat HLT as a NOP so BIOS busy
                        // loops keep running even with
                        // interrupts disabled.
                        let rip_name = WHvX64RegisterRip;
                        let mut rip_val = WHV_REGISTER_VALUE {
                            Reg64: exit_ctxt.VpContext.Rip,
                        };
                        // HLT is a 1-byte instruction
                        unsafe {
                            rip_val.Reg64 += 1;
                            WHvSetVirtualProcessorRegisters(self.handle, 0, &rip_name, 1, &rip_val);
                        }
                        cont_exec = true;
                    }
                    _ => {
                        println!("Unknown Exit Reason: 0x{:X}!", exit_ctxt.ExitReason.0);
                        cont_exec = false;
                    }
                }
                sync_cga_from_memory(self.vmem as *const u8);
            }
        }
    }

    fn try_emulate_io(
        &self,
        vcpu_ctxt: *const WHV_VP_EXIT_CONTEXT,
        io_ctxt: *const WHV_X64_IO_PORT_ACCESS_CONTEXT,
    ) -> std::result::Result<WHV_EMULATOR_STATUS, Error> {
        unsafe {
            WHvEmulatorTryIoEmulation(
                GLOBAL_EMULATOR_HANDLE.load(Ordering::Relaxed),
                (self as *const Self).cast(),
                vcpu_ctxt,
                io_ctxt,
            )
        }
    }

    fn patch_reset_vector(&self) {
        unsafe {
            let mem = self.vmem as *mut u8;
            let jump: [u8; 5] = [0xEA, 0x00, 0x00, 0x00, 0xF0];
            std::ptr::copy_nonoverlapping(jump.as_ptr(), mem.add(0xFFFF0), 5);
        }
    }

    fn mirror_region(&self, offset: usize, size: usize, total: usize) {
        if size == 0 || size >= total {
            return;
        }
        unsafe {
            let mem = self.vmem as *mut u8;
            let src = mem.add(offset);
            let mut pos = size;
            while pos < total {
                std::ptr::copy_nonoverlapping(src, mem.add(offset + pos), size);
                pos += size;
            }
        }
    }
}

fn load_disk_image(path: &str) -> bool {
    if let Ok(mut f) = File::open(path) {
        let mut buf = [0u8; DISK_IMAGE_SIZE];
        if f.read_exact(&mut buf).is_ok() {
            unsafe {
                DISK_IMAGE = buf;
            }
            return true;
        }
    }
    false
}

impl Drop for SimpleVirtualMachine {
    fn drop(&mut self) {
        if let Err(e) = unsafe { WHvDeletePartition(self.handle) } {
            panic!("Failed to delete virtual machine! Reason: {e}");
        }
        if let Err(e) = unsafe { VirtualFree(self.vmem, 0, MEM_RELEASE) } {
            panic!("Failed to release virtual memory! Reason: {e}");
        }
        println!("Successfully deleted virtual machine!");
    }
}

unsafe extern "system" fn emu_io_port_callback(
    _context: *const c_void,
    io_access: *mut WHV_EMULATOR_IO_ACCESS_INFO,
) -> HRESULT {
    unsafe {
        update_pit();
        if (*io_access).Direction == 0 {
            if (*io_access).Port != IO_PORT_SYS_PORTC {
                port_log!(
                    "IN  port 0x{:04X}, size {}\n",
                    (*io_access).Port,
                    (*io_access).AccessSize
                );
            }
            if (*io_access).Port == IO_PORT_KBD_DATA {
                for i in 0..(*io_access).AccessSize {
                    let mut buf = [0u8; 1];
                    if std::io::stdin().read_exact(&mut buf).is_ok() {
                        (*io_access).Data |= (buf[0] as u32) << (i * 8);
                    } else {
                        return E_FAIL;
                    }
                }
                S_OK
            } else if (*io_access).Port == IO_PORT_KBD_STATUS {
                (*io_access).Data = 0;
                S_OK
            } else if (*io_access).Port == IO_PORT_DISK_DATA {
                for i in 0..(*io_access).AccessSize as usize {
                    (*io_access).Data |= (DISK_IMAGE[DISK_OFFSET] as u32) << (i * 8);
                    DISK_OFFSET = (DISK_OFFSET + 1) % DISK_IMAGE_SIZE;
                }
                S_OK
            } else if (*io_access).Port == IO_PORT_POST {
                (*io_access).Data = 0;
                S_OK
            } else if (*io_access).Port == IO_PORT_SYS_CTRL {
                (*io_access).Data = SYS_CTRL as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_SYS_PORTC {
                let base = MEM_NIBBLE;
                let mut val: u8 = if SYS_CTRL & 0x04 != 0 {
                    base & 0xF
                } else {
                    (base >> 4) & 0xF
                };
                if SYS_CTRL & 0x02 != 0 {
                    val |= 0x20;
                }
                (*io_access).Data = val as u32;
                port_log!(
                    "IN  port 0x{:04X}, size {}, value 0x{:02X}\n",
                    (*io_access).Port,
                    (*io_access).AccessSize,
                    val
                );
                S_OK
            } else if (*io_access).Port == IO_PORT_CGA_MODE {
                (*io_access).Data = CGA_MODE as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_MDA_MODE {
                (*io_access).Data = MDA_MODE as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_DMA_MASK {
                (*io_access).Data = DMA_MASK as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_DMA_MODE {
                (*io_access).Data = DMA_MODE as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_DMA_TEMP {
                (*io_access).Data = DMA_TEMP as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_DMA_CLEAR {
                (*io_access).Data = DMA_CLEAR as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_PIT_CONTROL {
                (*io_access).Data = PIT_CONTROL as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_PIT_COUNTER0 {
                let byte = pit_read(0);
                (*io_access).Data = byte as u32;
                port_log!(
                    "IN  port 0x{:04X}, size {}, value 0x{:02X}\n",
                    (*io_access).Port,
                    (*io_access).AccessSize,
                    byte
                );
                S_OK
            } else if (*io_access).Port == IO_PORT_PIT_COUNTER1 {
                let byte = pit_read(1);
                (*io_access).Data = byte as u32;
                port_log!(
                    "IN  port 0x{:04X}, size {}, value 0x{:02X}\n",
                    (*io_access).Port,
                    (*io_access).AccessSize,
                    byte
                );
                S_OK
            } else if (*io_access).Port == IO_PORT_PIT_COUNTER2 {
                let byte = pit_read(2);
                (*io_access).Data = byte as u32;
                port_log!(
                    "IN  port 0x{:04X}, size {}, value 0x{:02X}\n",
                    (*io_access).Port,
                    (*io_access).AccessSize,
                    byte
                );
                S_OK
            } else if (*io_access).Port == IO_PORT_PIC_MASTER_DATA {
                (*io_access).Data = PIC_MASTER_IMR as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_PIC_SLAVE_DATA {
                (*io_access).Data = PIC_SLAVE_IMR as u32;
                S_OK
            } else if (*io_access).Port <= 0x0007 {
                let idx = ((*io_access).Port - IO_PORT_DMA_ADDR0) as usize;
                let byte = DMA_CHAN[idx];
                (*io_access).Data = byte as u32;
                port_log!(
                    "IN  port 0x{:04X}, size {}, value 0x{:02X}\n",
                    (*io_access).Port,
                    (*io_access).AccessSize,
                    byte
                );
                S_OK
            } else if (*io_access).Port == IO_PORT_DMA_PAGE1 {
                (*io_access).Data = DMA_PAGE1 as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_PORT_0210 {
                (*io_access).Data = PORT_0210_VAL as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_PORT_0278 {
                (*io_access).Data = PORT_0278_VAL as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_PORT_02FA {
                (*io_access).Data = PORT_02FA_VAL as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_PORT_0378 {
                (*io_access).Data = PORT_0378_VAL as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_PORT_03BC {
                (*io_access).Data = PORT_03BC_VAL as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_PORT_03FA {
                (*io_access).Data = PORT_03FA_VAL as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_PORT_0201 {
                (*io_access).Data = PORT_0201_VAL as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_CRTC_INDEX_MDA {
                (*io_access).Data = CRTC_MDA_INDEX as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_CRTC_DATA_MDA {
                (*io_access).Data = CRTC_MDA_REGS[CRTC_MDA_INDEX as usize] as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_ATTR_MDA {
                (*io_access).Data = ATTR_MDA as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_CRTC_INDEX_CGA {
                (*io_access).Data = CRTC_CGA_INDEX as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_CRTC_DATA_CGA {
                (*io_access).Data = CRTC_CGA_REGS[CRTC_CGA_INDEX as usize] as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_ATTR_CGA {
                (*io_access).Data = ATTR_CGA as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_CGA_STATUS {
                let now = Instant::now();
                if let Some(last) = CGA_LAST_TOGGLE {
                    if now.duration_since(last) >= CGA_TOGGLE_PERIOD {
                        CGA_STATUS ^= 0x08; // toggle vertical retrace bit
                        CGA_LAST_TOGGLE = Some(now);
                    }
                } else {
                    CGA_LAST_TOGGLE = Some(now);
                }
                (*io_access).Data = CGA_STATUS as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_FDC_DOR {
                (*io_access).Data = FDC_DOR as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_FDC_STATUS {
                (*io_access).Data = FDC_STATUS as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_FDC_DATA {
                (*io_access).Data = FDC_DATA as u32;
                S_OK
            } else if (*io_access).Port == IO_PORT_PIC_MASTER_CMD
                || (*io_access).Port == IO_PORT_PIC_SLAVE_CMD
            {
                (*io_access).Data = 0;
                S_OK
            } else if (*io_access).Port == IO_PORT_DMA_PAGE3
                || (*io_access).Port == IO_PORT_VIDEO_MISC_B8
                || (*io_access).Port == IO_PORT_SPECIAL_213
                || (*io_access).Port == IO_PORT_PIT_CMD
                || (*io_access).Port == IO_PORT_PIT_CONTROL
                || (*io_access).Port == IO_PORT_PIT_COUNTER0
                || (*io_access).Port == IO_PORT_PIT_COUNTER1
                || (*io_access).Port == IO_PORT_TIMER_MISC
            {
                (*io_access).Data = 0;
                S_OK
            } else {
                println!(
                    "Input from port 0x{:04X} ({}) is not implemented!",
                    (*io_access).Port,
                    port_name((*io_access).Port)
                );
                E_NOTIMPL
            }
        } else {
            port_log!(
                "OUT port 0x{:04X}, size {}, value 0x{:X}\n",
                (*io_access).Port,
                (*io_access).AccessSize,
                (*io_access).Data
            );
            if (*io_access).Port == IO_PORT_DISK_DATA {
                for i in 0..(*io_access).AccessSize as usize {
                    DISK_IMAGE[DISK_OFFSET] = ((*io_access).Data >> (i * 8)) as u8;
                    DISK_OFFSET = (DISK_OFFSET + 1) % DISK_IMAGE_SIZE;
                }
                S_OK
            } else if (*io_access).Port == IO_PORT_POST {
                S_OK
            } else if (*io_access).Port == IO_PORT_SYS_CTRL {
                SYS_CTRL = (*io_access).Data as u8;
                let new_state = SYS_CTRL & 0x03 == 0x03;
                if new_state && !SPEAKER_ON {
                    let count = if PIT_CHANNELS[2].reload != 0 {
                        PIT_CHANNELS[2].reload as u32
                    } else {
                        65536
                    };
                    let freq = 1_193_182 / count;
                    let _ = Beep(freq, BEEP_DURATION_MS);
                    openal_beep(freq, BEEP_DURATION_MS);
                }
                SPEAKER_ON = new_state;
                S_OK
            } else if (*io_access).Port == IO_PORT_SYS_PORTC {
                S_OK
            } else if (*io_access).Port == IO_PORT_CGA_MODE {
                CGA_MODE = (*io_access).Data as u8;
                S_OK
            } else if (*io_access).Port == IO_PORT_MDA_MODE {
                MDA_MODE = (*io_access).Data as u8;
                S_OK
            } else if (*io_access).Port == IO_PORT_DMA_MASK {
                DMA_MASK = (*io_access).Data as u8;
                S_OK
            } else if (*io_access).Port == IO_PORT_PIT_CONTROL {
                PIT_CONTROL = (*io_access).Data as u8;
                let cmd = PIT_CONTROL;
                let chan = (cmd >> 6) & 3;
                let access = (cmd >> 4) & 3;
                if access == 0 {
                    if chan < 3 {
                        let ch = &mut PIT_CHANNELS[chan as usize];
                        ch.latch = ch.count;
                        ch.latched = true;
                        ch.access = 3;
                        ch.rw_low = true;
                    }
                } else if chan < 3 {
                    let ch = &mut PIT_CHANNELS[chan as usize];
                    ch.access = access;
                    ch.mode = (cmd >> 1) & 0x7;
                    ch.bcd = cmd & 1 != 0;
                    ch.rw_low = true;
                }
                S_OK
            } else if (*io_access).Port == IO_PORT_PIT_COUNTER0 {
                pit_write(0, (*io_access).Data as u8);
                S_OK
            } else if (*io_access).Port == IO_PORT_PIT_COUNTER1 {
                pit_write(1, (*io_access).Data as u8);
                S_OK
            } else if (*io_access).Port == IO_PORT_PIT_COUNTER2 {
                pit_write(2, (*io_access).Data as u8);
                S_OK
            } else if (*io_access).Port == IO_PORT_DMA_MODE {
                DMA_MODE = (*io_access).Data as u8;
                S_OK
            } else if (*io_access).Port == IO_PORT_DMA_TEMP {
                DMA_TEMP = (*io_access).Data as u8;
                S_OK
            } else if (*io_access).Port == IO_PORT_DMA_CLEAR {
                DMA_CLEAR = (*io_access).Data as u8;
                S_OK
            } else if (*io_access).Port <= 0x0007 {
                let idx = ((*io_access).Port - IO_PORT_DMA_ADDR0) as usize;
                DMA_CHAN[idx] = (*io_access).Data as u8;
                S_OK
            } else if (*io_access).Port == IO_PORT_DMA_PAGE1 {
                DMA_PAGE1 = (*io_access).Data as u8;
                S_OK
            } else if (*io_access).Port == IO_PORT_PORT_0210 {
                PORT_0210_VAL = (*io_access).Data as u8;
                S_OK
            } else if (*io_access).Port == IO_PORT_PORT_0278 {
                PORT_0278_VAL = (*io_access).Data as u8;
                S_OK
            } else if (*io_access).Port == IO_PORT_PORT_02FA {
                PORT_02FA_VAL = (*io_access).Data as u8;
                S_OK
            } else if (*io_access).Port == IO_PORT_PORT_0378 {
                PORT_0378_VAL = (*io_access).Data as u8;
                S_OK
            } else if (*io_access).Port == IO_PORT_PORT_03BC {
                PORT_03BC_VAL = (*io_access).Data as u8;
                S_OK
            } else if (*io_access).Port == IO_PORT_PORT_03FA {
                PORT_03FA_VAL = (*io_access).Data as u8;
                S_OK
            } else if (*io_access).Port == IO_PORT_PORT_0201 {
                PORT_0201_VAL = (*io_access).Data as u8;
                S_OK
            } else if (*io_access).Port == IO_PORT_CRTC_INDEX_MDA {
                CRTC_MDA_INDEX = (*io_access).Data as u8 & 0x1F;
                S_OK
            } else if (*io_access).Port == IO_PORT_CRTC_DATA_MDA {
                CRTC_MDA_DATA = (*io_access).Data as u8;
                CRTC_MDA_REGS[CRTC_MDA_INDEX as usize] = CRTC_MDA_DATA;
                S_OK
            } else if (*io_access).Port == IO_PORT_ATTR_MDA {
                ATTR_MDA = (*io_access).Data as u8;
                S_OK
            } else if (*io_access).Port == IO_PORT_CRTC_INDEX_CGA {
                CRTC_CGA_INDEX = (*io_access).Data as u8 & 0x1F;
                S_OK
            } else if (*io_access).Port == IO_PORT_CRTC_DATA_CGA {
                CRTC_CGA_DATA = (*io_access).Data as u8;
                CRTC_CGA_REGS[CRTC_CGA_INDEX as usize] = CRTC_CGA_DATA;
                S_OK
            } else if (*io_access).Port == IO_PORT_ATTR_CGA {
                ATTR_CGA = (*io_access).Data as u8;
                S_OK
            } else if (*io_access).Port == IO_PORT_CGA_STATUS {
                CGA_STATUS = (*io_access).Data as u8;
                S_OK
            } else if (*io_access).Port == IO_PORT_FDC_DOR {
                FDC_DOR = (*io_access).Data as u8;
                S_OK
            } else if (*io_access).Port == IO_PORT_FDC_STATUS {
                FDC_STATUS = (*io_access).Data as u8;
                S_OK
            } else if (*io_access).Port == IO_PORT_FDC_DATA {
                FDC_DATA = (*io_access).Data as u8;
                S_OK
            } else if (*io_access).Port == IO_PORT_PIC_MASTER_CMD {
                PIC_MASTER_IMR = (*io_access).Data as u8; // treat command as IMR for simplicity
                S_OK
            } else if (*io_access).Port == IO_PORT_PIC_SLAVE_CMD {
                PIC_SLAVE_IMR = (*io_access).Data as u8;
                S_OK
            } else if (*io_access).Port == IO_PORT_PIC_MASTER_DATA {
                PIC_MASTER_IMR = (*io_access).Data as u8;
                S_OK
            } else if (*io_access).Port == IO_PORT_PIC_SLAVE_DATA {
                PIC_SLAVE_IMR = (*io_access).Data as u8;
                S_OK
            } else if (*io_access).Port == IO_PORT_KBD_DATA
                || (*io_access).Port == IO_PORT_KBD_STATUS
            {
                S_OK
            } else if (*io_access).Port == IO_PORT_DMA_PAGE3
                || (*io_access).Port == IO_PORT_VIDEO_MISC_B8
                || (*io_access).Port == IO_PORT_SPECIAL_213
                || (*io_access).Port == IO_PORT_PIT_CMD
                || (*io_access).Port == IO_PORT_PIT_CONTROL
                || (*io_access).Port == IO_PORT_PIT_COUNTER0
                || (*io_access).Port == IO_PORT_PIT_COUNTER1
                || (*io_access).Port == IO_PORT_TIMER_MISC
            {
                // These ports are touched by the BIOS during POST but
                // are not modeled. Simply accept the write.
                S_OK
            } else {
                if (*io_access).Port == LAST_UNKNOWN_PORT {
                    UNKNOWN_PORT_COUNT += 1;
                } else {
                    LAST_UNKNOWN_PORT = (*io_access).Port;
                    UNKNOWN_PORT_COUNT = 1;
                }
                println!(
                    "Unknown I/O Port (0x{:04X}) is accessed!",
                    (*io_access).Port
                );
                if UNKNOWN_PORT_COUNT >= 2 {
                    println!(
                        "Repeated access to unknown port 0x{:04X}, terminating.",
                        (*io_access).Port
                    );
                    std::process::exit(1);
                }
                E_NOTIMPL
            }
        }
    }
}

unsafe extern "system" fn emu_memory_callback(
    context: *const c_void,
    memory_access: *mut WHV_EMULATOR_MEMORY_ACCESS_INFO,
) -> HRESULT {
    unsafe {
        let ctxt: &SimpleVirtualMachine = &(*context.cast());
        let len: usize = (*memory_access).AccessSize as usize;
        let mut gpa = (*memory_access).GpaAddress as usize % ctxt.size;
        for i in 0..len {
            let hva: *mut u8 = ctxt.vmem.byte_add((gpa + i) % ctxt.size).cast();
            if (*memory_access).Direction != 0 {
                unsafe { *hva = (*memory_access).Data[i] };
            } else {
                unsafe { (*memory_access).Data[i] = *hva };
            }
        }
        S_OK
    }
}

unsafe extern "system" fn emu_get_vcpu_reg_callback(
    context: *const c_void,
    reg_names: *const WHV_REGISTER_NAME,
    reg_count: u32,
    reg_values: *mut WHV_REGISTER_VALUE,
) -> HRESULT {
    unsafe {
        let ctxt: &SimpleVirtualMachine = &(*context.cast());
        match WHvGetVirtualProcessorRegisters(ctxt.handle, 0, reg_names, reg_count, reg_values) {
            Ok(_) => S_OK,
            Err(e) => e.code(),
        }
    }
}

unsafe extern "system" fn emu_set_vcpu_reg_callback(
    context: *const c_void,
    reg_names: *const WHV_REGISTER_NAME,
    reg_count: u32,
    reg_values: *const WHV_REGISTER_VALUE,
) -> HRESULT {
    unsafe {
        let ctxt: &SimpleVirtualMachine = &(*context.cast());
        match WHvSetVirtualProcessorRegisters(ctxt.handle, 0, reg_names, reg_count, reg_values) {
            Ok(_) => S_OK,
            Err(e) => e.code(),
        }
    }
}

unsafe extern "system" fn emu_translate_gva_callback(
    context: *const c_void,
    gva_page: u64,
    translate_flags: WHV_TRANSLATE_GVA_FLAGS,
    translation_result: *mut WHV_TRANSLATE_GVA_RESULT_CODE,
    gpa_page: *mut u64,
) -> HRESULT {
    unsafe {
        let ctxt: &SimpleVirtualMachine = &(*context.cast());
        let mut r: WHV_TRANSLATE_GVA_RESULT = WHV_TRANSLATE_GVA_RESULT::default();
        match WHvTranslateGva(
            ctxt.handle,
            0,
            gva_page,
            translate_flags,
            &raw mut r,
            gpa_page,
        ) {
            Ok(_) => {
                *translation_result = r.ResultCode;
                S_OK
            }
            Err(e) => e.code(),
        }
    }
}

fn init_whpx() -> HRESULT {
    let mut hv_present: WHV_CAPABILITY = WHV_CAPABILITY::default();
    let r = unsafe {
        WHvGetCapability(
            WHvCapabilityCodeHypervisorPresent,
            (&raw mut hv_present).cast(),
            size_of::<WHV_CAPABILITY>() as u32,
            None,
        )
    };
    match r {
        Ok(_) => {
            if unsafe { hv_present.HypervisorPresent }.as_bool() {
                let mut eh: *mut c_void = null_mut();
                match unsafe {
                    WHvEmulatorCreateEmulator(&raw const GLOBAL_EMULATOR_CALLBACKS, &raw mut eh)
                } {
                    Ok(_) => {
                        GLOBAL_EMULATOR_HANDLE.store(eh, Ordering::Relaxed);
                        S_OK
                    }
                    Err(e) => e.code(),
                }
            } else {
                S_FALSE
            }
        }
        Err(e) => {
            println!("Failed to query Windows Hypervisor Platform presence! Reason: {e}");
            e.code()
        }
    }
}

fn main() {
    println!("SimpleWhpDemo version {}", env!("CARGO_PKG_VERSION"));
    println!("IVT firmware version 0.1.0");

    // Emit a slightly longer beep so the audio device has time to start up.
    // This helps confirm OpenAL is working before emulation proceeds.
    openal_beep(1000, BEEP_DURATION_MS);
    unsafe {
        CGA_LAST_TOGGLE = Some(Instant::now());
    }
    unsafe {
        if let Ok(sdl) = sdl2::init() {
            if let Ok(video) = sdl.video() {
                if let Ok(window) = video
                    .window(
                        "SimpleWhpDemo",
                        (CGA_COLS * 8) as u32,
                        (CGA_ROWS * 8) as u32,
                    )
                    .position_centered()
                    .build()
                {
                    if let Ok(canvas) = window.into_canvas().accelerated().build() {
                        if let Ok(pump) = sdl.event_pump() {
                            SDL_CONTEXT = Some(sdl);
                            SDL_CANVAS = Some(canvas);
                            SDL_PUMP = Some(pump);
                        }
                    }
                }
            }
        }
    }
    let args: Vec<String> = std::env::args().collect();
    let mut program: Option<&str> = Some("hello.com");
    let mut bios: &str = DEFAULT_BIOS;
    if args.len() >= 2 {
        if args.len() >= 3 {
            program = Some(&args[1]);
            bios = &args[2];
        } else {
            let arg = &args[1];
            if arg.ends_with(".bin") || arg.ends_with(".fw") {
                program = None;
                bios = arg;
            } else {
                program = Some(arg);
            }
        }
    }
    if init_whpx() == S_OK {
        println!("WHPX is present and initalized!");
        if let Ok(vm) = SimpleVirtualMachine::new(GUEST_MEM_SIZE) {
            println!("Successfully created virtual machine!");
            let bios_size = match vm.load_program(bios, 0xF0000) {
                Ok(size) => size,
                Err(_) => {
                    if bios == DEFAULT_BIOS {
                        println!("AMI BIOS not found, falling back to {FALLBACK_BIOS}");
                        let size = vm
                            .load_program(FALLBACK_BIOS, 0xF0000)
                            .expect("Failed to load firmware!");
                        vm.patch_reset_vector();
                        size
                    } else {
                        panic!("Failed to load firmware!");
                    }
                }
            };
            if bios == FALLBACK_BIOS {
                vm.patch_reset_vector();
            }
            if bios_size < 0x10000 {
                vm.mirror_region(0xF0000, bios_size, 0x10000);
            }
            if let Some(p) = program {
                if let Err(e) = vm.load_program(p, 0x10100) {
                    panic!("Failed to load program! Reason: {e}");
                }
            }
            if !load_disk_image("disk.img\0") {
                println!("Warning: disk image not loaded, disk reads will return zeros.");
            }
            println!("============ Program Start ============");
            clear_cga_buffer(vm.vmem as *mut u8);
            vm.run();
            println!("============= Program End =============");
            print_cga_buffer(vm.vmem as *const u8);
        }
        let _ =
            unsafe { WHvEmulatorDestroyEmulator(GLOBAL_EMULATOR_HANDLE.load(Ordering::Relaxed)) };
    }
}
