pub static mut DMA_PAGE: [u8; 16] = [0; 16];
pub static mut DMA_ADDR: [u16; 4] = [0; 4];
pub static mut DMA_COUNT: [u16; 4] = [0; 4];
pub static mut DMA_MODE: u8 = 0;
pub static mut DMA_MASK: u8 = 0;
pub static mut DMA_TEMP: u8 = 0;
pub static mut DMA_CLEAR: u8 = 0;
pub static mut DMA_COMMAND: u8 = 0;
pub static mut DMA_STATUS: u8 = 0;
static mut FLIP_FLOP: bool = false;

pub fn dma_write(port: u16, val: u8) {
    unsafe {
        if port <= 0x0007 {
            let chan = ((port >> 1) & 3) as usize;
            if port & 1 == 0 {
                if FLIP_FLOP {
                    DMA_ADDR[chan] = (DMA_ADDR[chan] & 0x00FF) | ((val as u16) << 8);
                } else {
                    DMA_ADDR[chan] = (DMA_ADDR[chan] & 0xFF00) | val as u16;
                }
            } else {
                if FLIP_FLOP {
                    DMA_COUNT[chan] = (DMA_COUNT[chan] & 0x00FF) | ((val as u16) << 8);
                } else {
                    DMA_COUNT[chan] = (DMA_COUNT[chan] & 0xFF00) | val as u16;
                }
            }
            FLIP_FLOP = !FLIP_FLOP;
        } else {
            match port {
                0x0008 => DMA_COMMAND = val,
                0x000A => DMA_MASK = val,
                0x000B => DMA_MODE = val,
                0x000C => {
                    FLIP_FLOP = false;
                    DMA_CLEAR = val;
                }
                0x000D => DMA_TEMP = val,
                0x000F => DMA_MASK = val,
                _ => {}
            }
        }
    }
}

pub fn dma_page_write(port: u16, val: u8) {
    unsafe {
        DMA_PAGE[(port & 0xF) as usize] = val;
    }
}

pub fn dma_read(port: u16) -> u8 {
    unsafe {
        if port <= 0x0007 {
            let chan = ((port >> 1) & 3) as usize;
            let val = if port & 1 == 0 {
                DMA_ADDR[chan]
            } else {
                DMA_COUNT[chan]
            };
            let byte = if FLIP_FLOP {
                (val >> 8) as u8
            } else {
                (val & 0xFF) as u8
            };
            FLIP_FLOP = !FLIP_FLOP;
            byte
        } else if port == 0x0008 {
            let v = DMA_STATUS;
            DMA_STATUS = 0;
            v
        } else if port >= 0x0080 && port <= 0x008F {
            DMA_PAGE[(port & 0xF) as usize]
        } else {
            0
        }
    }
}
