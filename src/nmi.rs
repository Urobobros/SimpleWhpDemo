pub static mut NMI_MASK: u8 = 0;

pub fn nmi_write(_port: u16, val: u8) {
    unsafe { NMI_MASK = val & 0x80; }
}
