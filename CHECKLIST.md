# Development Checklist

- [ ] Initialize WHPX and create VM
- [ ] Map 1 MB real‑mode memory
- [ ] Load BIOS ROM at 0xF0000
- [ ] Setup real‑mode registers (CS, IP, CR0)
- [ ] Implement HLT exit handling
- [ ] Capture/emulate I/O ports (CGA, keyboard, disk)
- [ ] BIOS INT 0x10 support (e.g. text output)
- [ ] BIOS INT 0x13 disk access
- [ ] Simple screen output demo ("Hello from CGA")
- [ ] Add disk image loading support
- [ ] Add interrupt descriptor table and IRQ routing
- [ ] Document how to run the demo in README.md
