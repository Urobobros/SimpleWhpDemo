# FAR Planning

## Functionality
This project aims to emulate an 8088-class environment by leveraging Windows Hypervisor Platform (WHPX). At a minimum it should:

- Initialize WHPX and create a virtual machine.
- Map 1 MB of real-mode memory for the guest.
- Load a BIOS ROM at `0xF0000` and start execution in real mode.
- Provide hypervisor exits for `HLT` instructions so the host can regain control.
- Capture and emulate I/O ports for devices such as CGA video, keyboard and disk.
- Log every I/O port access, including writes to the POST/IO delay port `0x0080`.
- Offer basic BIOS services, including `INT 0x10` for text output and `INT 0x13` for disk access.
- Display simple text using the CGA subsystem.

## Architecture
Core components and their interactions are planned as follows:

- **Hypervisor Initialization** – set up WHPX, create the VM, and configure a virtual processor.
- **Memory Mapping** – allocate and map guest physical memory, then load the BIOS image at `0xF0000`.
- **CPU Setup** – initialize real-mode registers (`CS`, `IP`, `CR0`) before running the virtual CPU.
- **Exit Handling** – process hypervisor exits, primarily `HLT`, to coordinate host and guest operations.
- **I/O Dispatch** – trap guest I/O, forwarding reads and writes to emulated devices.
- **BIOS Subsystem** – implement basic BIOS interrupts so real-mode programs can call into firmware services.
- **Video Subsystem** – emulate an 80×25 CGA text mode for character output.

## Roadmap
Planned development phases are:

1. **Minimal Viable Product** – initialize WHPX, map memory, load the BIOS, set registers, and handle `HLT` exits.
2. **Basic I/O** – capture port accesses and emulate CGA text output and simple keyboard input.
3. **Interrupt Support** – implement BIOS interrupts (`INT 0x10` and `INT 0x13`) and establish an interrupt descriptor table.
4. **Disk I/O** – add disk image loading and implement BIOS disk services for reading sectors.
5. **Interactive Demo** – run a real-mode program that prints “Hello from CGA” and document how to execute it in the repository README.

## Implementation Status
Progress on the roadmap items is tracked below:

- [x] Initialize WHPX and create VM
- [x] Map 1 MB real-mode memory
- [x] Load BIOS ROM at `0xF0000`
- [x] Setup real-mode registers (CS, IP, CR0)
- [x] Implement HLT exit handling
- [x] Capture/emulate I/O ports (CGA, keyboard, disk)
  - Disk port `0x00FF` now reads and writes from the loaded disk image
  - POST port `0x0080` is logged for debugging
- [x] BIOS INT 0x10 text output support
- [x] BIOS INT 0x13 disk access
- [x] Simple screen output demo ("Hello from CGA")
- [x] Add disk image loading support
- [ ] Add interrupt descriptor table and IRQ routing
- [x] Document how to run the demo in README.md
