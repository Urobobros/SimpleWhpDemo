pub static mut MODE: u8 = 0;
pub static mut COLOR: u8 = 0;
pub static mut CRTC_INDEX: u8 = 0;
pub static mut CRTC_REGS: [u8; 32] = [0; 32];
pub static mut STATUS: u8 = 0;
pub static mut LAST_DISP_TOGGLE: Option<std::time::Instant> = None;
pub static mut VSYNC_END: Option<std::time::Instant> = None;
const DISPLAY_TOGGLE_PERIOD: std::time::Duration =
    std::time::Duration::from_millis(16);

pub fn cga_init() {
    unsafe {
        MODE = 0;
        COLOR = 0;
        STATUS = 0;
        let now = std::time::Instant::now();
        LAST_DISP_TOGGLE = Some(now);
        VSYNC_END = Some(now);
    }
}

pub fn cga_out(port: u16, val: u8) {
    unsafe {
        match port {
            0x3D4 | 0x3D6 => CRTC_INDEX = val & 0x1F,
            0x3D5 | 0x3D7 => CRTC_REGS[CRTC_INDEX as usize] = val,
            0x3D8 => MODE = val,
            0x3D9 => COLOR = val,
            _ => {}
        }
    }
}

pub fn cga_in(port: u16) -> u8 {
    unsafe {
        match port {
            0x3D4 => CRTC_INDEX,
            0x3D5 => CRTC_REGS[CRTC_INDEX as usize],
            0x3DA => {
                let now = std::time::Instant::now();
                if let Some(last) = LAST_DISP_TOGGLE {
                    if now.duration_since(last) >= DISPLAY_TOGGLE_PERIOD {
                        STATUS ^= 0x01;
                        let next = last + DISPLAY_TOGGLE_PERIOD;
                        LAST_DISP_TOGGLE = Some(next);
                        VSYNC_END = Some(next + std::time::Duration::from_millis(2));
                    }
                } else {
                    LAST_DISP_TOGGLE = Some(now);
                    VSYNC_END = Some(now);
                }
                if let Some(vend) = VSYNC_END {
                    if now < vend {
                        STATUS |= 0x08;
                    } else {
                        STATUS &= !0x08;
                    }
                } else {
                    STATUS &= !0x08;
                }
                STATUS
            }
            _ => 0xFF,
        }
    }
}
