#[allow(dead_code)]
#[derive(Copy, Clone)]
struct Serial {
    thr: u8,
    ier: u8,
    lcr: u8,
    mcr: u8,
    dll: u8,
    dlm: u8,
}

impl Serial {
    const fn new() -> Self {
        Serial {
            thr: 0,
            ier: 0,
            lcr: 0,
            mcr: 0,
            dll: 0,
            dlm: 0,
        }
    }
}

static mut COM1: Serial = Serial::new();

pub fn serial_write(port: u16, val: u8) {
    unsafe {
        match port {
            0x03F8 => {
                if COM1.lcr & 0x80 != 0 {
                    COM1.dll = val;
                } else {
                    COM1.thr = val;
                }
            }
            0x03F9 => {
                if COM1.lcr & 0x80 != 0 {
                    COM1.dlm = val;
                } else {
                    COM1.ier = val;
                }
            }
            0x03FB => COM1.lcr = val,
            0x03FC => COM1.mcr = val,
            _ => {}
        }
    }
}

pub fn serial_read(port: u16) -> u8 {
    unsafe {
        match port {
            0x03F8 => {
                if COM1.lcr & 0x80 != 0 { COM1.dll } else { COM1.thr }
            }
            0x03F9 => {
                if COM1.lcr & 0x80 != 0 { COM1.dlm } else { COM1.ier }
            }
            0x03FA => 0x01, // IIR: no interrupts pending
            0x03FB => COM1.lcr,
            0x03FC => COM1.mcr,
            0x03FD => 0x60, // LSR: THR empty
            0x03FE => 0x00, // MSR
            _ => 0
        }
    }
}
