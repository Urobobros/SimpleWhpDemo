# Development Checklist

- [x] Initialize WHPX and create VM
- [x] Map 1 MB real‑mode memory
- [x] Load BIOS ROM at 0xF0000
- [x] Setup real‑mode registers (CS, IP, CR0)
- [x] Implement HLT exit handling
- [x] Capture/emulate I/O ports (CGA, keyboard, disk)
- [x] Log POST port 0x0080 accesses
- [x] BIOS INT 0x10 support (e.g. text output)
- [x] BIOS INT 0x13 disk access
- [x] Simple screen output demo ("Hello from CGA")
- [x] Add disk image loading support
- [ ] Add interrupt descriptor table and IRQ routing
- [x] Document how to run the demo in README.md
- [ ] Implement full 80×25 CGA text memory (PCem-style)
