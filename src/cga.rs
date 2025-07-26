pub static mut MODE: u8 = 0;
pub static mut COLOR: u8 = 0;

pub fn cga_out(port: u16, val: u8) {
    unsafe {
        match port {
            0x3D8 => MODE = val,
            0x3D9 => COLOR = val,
            _ => {}
        }
    }
}
