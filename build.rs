use std::process::Command;

fn main() {
    Command::new("nasm")
        .args(&["tests/ivt.asm", "-o"])
        .arg(&format!("ivt.fw"))
        .arg("-l")
        .arg(&format!("ivt.lst"))
        .status()
        .unwrap();
    Command::new("nasm")
        .args(&["tests/hello_dos.asm", "-o"])
        .arg(&format!("hello.com"))
        .arg("-l")
        .arg(&format!("hello.lst"))
        .status()
        .unwrap();
    Command::new("nasm")
        .args(&["tests/keyboard.asm", "-o"])
        .arg(&format!("keyboard.com"))
        .arg("-l")
        .arg(&format!("keyboard.lst"))
        .status()
        .unwrap();
    println!("cargo::rerun-if-changed=tests/ivt.asm");
    println!("cargo::rerun-if-changed=tests/hello_dos.asm");
    println!("cargo::rerun-if-changed=tests/keyboard.asm");
}
