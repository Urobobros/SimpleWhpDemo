use std::process::Command;
use cc;

fn main() {
    Command::new("nasm")
        .args(&["-f", "bin", "tests/ivt.asm", "-o"])
        .arg("ivt.fw")
        .arg("-l")
        .arg("ivt.lst")
        .status()
        .unwrap();
    cc::Build::new()
        .files([
            "SimpleWhpDemo/io.c",
            "SimpleWhpDemo/dma.c",
            "SimpleWhpDemo/fdc.c",
            "SimpleWhpDemo/pic.c",
            "SimpleWhpDemo/sound_speaker.c",
            "SimpleWhpDemo/pit.c",
            "SimpleWhpDemo/serial.c",
            "SimpleWhpDemo/keyboard.c",
            "SimpleWhpDemo/nmi.c",
            "SimpleWhpDemo/timer.c",
            "SimpleWhpDemo/stubs.c",
        ])
        .include("SimpleWhpDemo")
        // Use stub Windows headers when building on non-Windows hosts
        .include("tests")
        .compile("swemu");
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
    println!("cargo::rerun-if-changed=SimpleWhpDemo/io.c");
    println!("cargo::rerun-if-changed=SimpleWhpDemo/dma.c");
    println!("cargo::rerun-if-changed=SimpleWhpDemo/fdc.c");
    println!("cargo::rerun-if-changed=SimpleWhpDemo/pic.c");
    println!("cargo::rerun-if-changed=SimpleWhpDemo/pit.c");
    println!("cargo::rerun-if-changed=SimpleWhpDemo/serial.c");
    println!("cargo::rerun-if-changed=SimpleWhpDemo/keyboard.c");
    println!("cargo::rerun-if-changed=SimpleWhpDemo/nmi.c");
    println!("cargo::rerun-if-changed=SimpleWhpDemo/timer.c");
    println!("cargo::rerun-if-changed=SimpleWhpDemo/stubs.c");
    println!("cargo::rerun-if-changed=SimpleWhpDemo/stubs.h");
}
