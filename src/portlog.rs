use once_cell::sync::Lazy;
use std::fs::{File, OpenOptions};
use std::io::Write;
use std::sync::Mutex;

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

#[macro_export]
macro_rules! port_log {
    ($fmt:expr $(, $args:expr)* $(,)?) => {
        $crate::portlog::port_log(&format!($fmt $(, $args)*));
    };
}
