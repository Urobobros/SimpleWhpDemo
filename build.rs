use std::process::Command;

fn main() {
    Command::new("nasm")
        .args(&["-f", "bin", "tests/ivt.asm", "-o"])
        .arg("ivt.fw")
        .arg("-l")
        .arg("ivt.lst")
        .status()
        .unwrap();
    Command::new("nasm")
        .args(&["-f", "bin", "tests/hello_dos.asm", "-o"])
        .arg("hello.com")
        .arg("-l")
        .arg("hello.lst")
        .status()
        .unwrap();
    Command::new("nasm")
        .args(&["-f", "bin", "tests/keyboard.asm", "-o"])
        .arg("keyboard.com")
        .arg("-l")
        .arg("keyboard.lst")
        .status()
        .unwrap();
    Command::new("nasm")
        .args(&["-f", "bin", "tests/beep.asm", "-o"])
        .arg("beep.com")
        .arg("-l")
        .arg("beep.lst")
        .status()
        .unwrap();
    println!("cargo::rerun-if-changed=tests/ivt.asm");
    println!("cargo::rerun-if-changed=tests/hello_dos.asm");
    println!("cargo::rerun-if-changed=tests/keyboard.asm");
    println!("cargo::rerun-if-changed=tests/beep.asm");
}
