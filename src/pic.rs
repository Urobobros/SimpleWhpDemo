pub static mut MASTER_IMR: u8 = 0;
pub static mut SLAVE_IMR: u8 = 0;

pub fn pic_write(port: u16, val: u8) {
    unsafe {
        match port {
            0x20 | 0x21 => MASTER_IMR = val,
            0xA0 | 0xA1 => SLAVE_IMR = val,
            _ => {},
        }
    }
}
