# AMI XT BIOS Boot and I/O Initialization

This document explains how PCem loads the `amixt` BIOS and sets up I/O handlers when emulating the AMI XT clone.

## BIOS loading

BIOS loading occurs in `loadbios()` (`src/memory/mem_bios.c`). For the AMI XT clone the code reads the file `amixt/ami_8088_bios_31jan89.bin` into memory:

```c
switch (romset) {
        case ROM_AMIXT:
                f = romfopen("amixt/ami_8088_bios_31jan89.bin", "rb");
                if (!f)
                        break;
                romfread(rom + 0xE000, 8192, 1, f);
                fclose(f);
                return 1;
}
```

The `romfopen` helper searches configured ROM paths and opens the file, while `romfread` reads 8 KB from the image into the `rom` array starting at offset `0xE000`. After loading, the BIOS is mapped into memory via `mem_add_bios()`.

## I/O handler setup

When the AMI XT model starts, `xt_init()` (`src/models/model.c`) is called. This function performs common hardware initialization and registers I/O handlers for many devices:

```c
void common_init() {
        dma_init();
        fdc_add();
        pic_init();
        pit_init();
        serial1_init(0x3f8, 4, 1);
        serial2_init(0x2f8, 3, 1);
}

void xt_init() {
        common_init();
        mem_add_bios();
        pit_set_out_func(&pit, 1, pit_refresh_timer_xt);
        keyboard_xt_init();
        nmi_init();
}
```

Below is an overview of the key I/O registrations performed by these functions:

| Device | Ports | Code location |
|-------|-------|---------------|
| DMA controller | `0x0000`–`0x000F`, `0x0080`–`0x0087` | `dma_init()` |
| Floppy controller | `0x03F0`–`0x03F7` | `fdc_add()` |
| Programmable Interrupt Controller | `0x0020`–`0x0021` | `pic_init()` |
| PIC (slave) | `0x00A0`–`0x00A1` | `pic2_init()` |
| PIT timer | `0x0040`–`0x0043` | `pit_init()` |
| Keyboard controller | `0x0060`–`0x0063` | `keyboard_xt_init()` |
| Serial port COM1 | `0x03F8`–`0x03FF` | `serial1_init()` |
| Serial port COM2 | `0x02F8`–`0x02FF` | `serial2_init()` |
| NMI mask | `0x00A0` | `nmi_init()` |

Each of these functions uses `io_sethandler()` to bind read/write callbacks to specific port ranges. For example, `dma_init()` registers the DMA callbacks:

```c
void dma_init() {
        io_sethandler(0x0000, 0x0010, dma_read, NULL, NULL, dma_write, NULL, NULL, NULL);
        io_sethandler(0x0080, 0x0008, dma_page_read, NULL, NULL, dma_page_write, NULL, NULL, NULL);
        dma_ps2.is_ps2 = 0;
}
```

The `io_sethandler` function (defined in `src/io.c`) stores handler pointers in arrays indexed by port number so that reads and writes are dispatched correctly during emulation.

## Video initialization

After these handlers are in place, `model_init()` attaches the configured video card. For the AMI XT clone a CGA adapter is used. The CGA BIOS fonts are loaded inside `video_init()` and the device registers its own port handlers (such as `0x03D4`–`0x03D5`) much like the table above.
Unit test `tests/test_cga_init` verifies that these CGA I/O ports are registered and demonstrates a simple VRAM write by printing the first byte on the terminal.

## Registered I/O handlers

The following snippet from `io_init()` shows how the handler tables are cleared before registration:

```c
void io_init() {
        int c;
        pclog("io_init\n");
        for (c = 0; c < 0x10000; c++) {
                port_inb[c][0] = NULL;
                port_inb_name[c][0] = NULL;
                ...
                port_outl[c][1] = NULL;
                port_outl_name[c][1] = NULL;
                port_priv[c][0] = NULL;
                port_priv[c][1] = NULL;
        }
}
```

When devices call `io_sethandler`, these tables are filled with pointers to the appropriate read/write functions along with optional debug names.

---
This overview describes how the AMI XT BIOS is loaded and which I/O ports are registered when the emulator initializes this machine.
