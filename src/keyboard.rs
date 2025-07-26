use once_cell::sync::Lazy;
use std::collections::VecDeque;
use std::io::Read;
use std::sync::Mutex;

pub struct Keyboard {
    queue: VecDeque<u8>,
    pb: u8,
}

impl Keyboard {
    pub fn new() -> Self {
        Keyboard {
            queue: VecDeque::with_capacity(16),
            pb: 0,
        }
    }

    fn push_scancode(&mut self, code: u8) {
        if self.queue.len() < 16 {
            self.queue.push_back(code);
        }
    }

    fn read_data_inner(&mut self) -> u8 {
        if let Some(v) = self.queue.pop_front() {
            v
        } else {
            let mut buf = [0u8; 1];
            if std::io::stdin().read_exact(&mut buf).is_ok() {
                buf[0]
            } else {
                0
            }
        }
    }

    fn read_status_inner(&self) -> u8 {
        if self.queue.is_empty() { 0 } else { 1 }
    }

    fn write_inner(&mut self, port: u16, val: u8) {
        if port == 0x61 {
            if (self.pb & 0x40) == 0 && (val & 0x40) != 0 {
                self.queue.clear();
                self.push_scancode(0xaa);
            }
            self.pb = val;
        }
    }
}

static KEYBOARD: Lazy<Mutex<Keyboard>> = Lazy::new(|| Mutex::new(Keyboard::new()));

pub fn keyboard_xt_write(port: u16, val: u8) {
    let mut kb = KEYBOARD.lock().unwrap();
    kb.write_inner(port, val);
}

pub fn keyboard_xt_read_data() -> u8 {
    let mut kb = KEYBOARD.lock().unwrap();
    kb.read_data_inner()
}

pub fn keyboard_xt_read_status() -> u8 {
    let kb = KEYBOARD.lock().unwrap();
    kb.read_status_inner()
}
