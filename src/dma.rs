pub static mut DMA_PAGE: [u8; 16] = [0; 16];
pub static mut DMA_ADDR: [u16; 4] = [0; 4];
pub static mut DMA_COUNT: [u16; 4] = [0; 4];
pub static mut DMA_MODE: u8 = 0;
pub static mut DMA_MASK: u8 = 0;
static mut FLIP_FLOP: bool = false;

pub fn dma_write(port: u16, val: u8) {
    unsafe {
        if port <= 0x0007 {
            let chan = ((port >> 1) & 3) as usize;
            if port & 1 == 0 {
                if FLIP_FLOP {
                    DMA_ADDR[chan] = (DMA_ADDR[chan] & 0xFF00) | val as u16;
                } else {
                    DMA_ADDR[chan] = (DMA_ADDR[chan] & 0x00FF) | ((val as u16) << 8);
                }
            } else {
                if FLIP_FLOP {
                    DMA_COUNT[chan] = (DMA_COUNT[chan] & 0xFF00) | val as u16;
                } else {
                    DMA_COUNT[chan] = (DMA_COUNT[chan] & 0x00FF) | ((val as u16) << 8);
                }
            }
            DMA_CHAN[(port - 0x0000) as usize] = val;
            FLIP_FLOP = !FLIP_FLOP;
        } else {
            match port {
                0x000A => DMA_MASK = val,
                0x000B => DMA_MODE = val,
                0x000C => FLIP_FLOP = false,
                0x000F => DMA_MASK = val,
                _ => {},
            }
        }
    }
}

pub fn dma_page_write(port: u16, val: u8) {
    unsafe { DMA_PAGE[(port & 0xF) as usize] = val; }
}
pub static mut DMA_CHAN: [u8; 8] = [0; 8];

pub fn dma_read(port: u16) -> u8 {
    unsafe { DMA_CHAN[(port - 0x0000) as usize] }
}
