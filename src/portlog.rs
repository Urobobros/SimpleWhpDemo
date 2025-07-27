use once_cell::sync::Lazy;
use std::fs::{File, OpenOptions};
use std::io::Write;
use std::sync::Mutex;

#[macro_export]
macro_rules! port_log_tag {
    ($dir:expr, $port:expr, $size:expr, $val:expr, $tag:expr $(,)?) => {
        $crate::portlog::port_log_io_with_tag($dir, $port, $size, $val, $tag);
    };
}

#[macro_export]
macro_rules! port_log {
    ($fmt:expr $(, $args:expr)* $(,)?) => {
        #[cfg(debug_assertions)]
        {
            $crate::portlog::port_log(&format!($fmt $(, $args)*));
        }
    };
}

static PORT_LOG: Lazy<Mutex<Option<File>>> = Lazy::new(|| Mutex::new(None));

pub fn port_log(msg: &str) {
    let mut opt = PORT_LOG.lock().unwrap();
    if opt.is_none() {
        if let Ok(f) = OpenOptions::new()
            .write(true)
            .create(true)
            .truncate(true)
            .open("port.log")
        {
            *opt = Some(f);
        } else {
            return;
        }
    }
    if let Some(file) = opt.as_mut() {
        let _ = file.write_all(msg.as_bytes());
        let _ = file.flush();
    }
}

#[cfg(debug_assertions)]
pub fn port_log_io_with_tag(direction_out: bool, port: u16, size: u8, value: u32, tag: &str) {
    let dir = if direction_out { "OUT" } else { "IN " };
    if size == 1 {
        port_log!("{} port 0x{:04X}, size 1, value 0x{:02X}  # {}\n", dir, port, value & 0xFF, tag);
    } else if size == 2 {
        port_log!("{} port 0x{:04X}, size 2, value 0x{:04X}  # {}\n", dir, port, value & 0xFFFF, tag);
    } else {
        port_log!(
            "{} port 0x{:04X}, size {}, value 0x{:08X}  # {}\n",
            dir,
            port,
            size,
            value,
            tag
        );
    }
}

#[cfg(not(debug_assertions))]
pub fn port_log_io_with_tag(_direction_out: bool, _port: u16, _size: u8, _value: u32, _tag: &str) {}
