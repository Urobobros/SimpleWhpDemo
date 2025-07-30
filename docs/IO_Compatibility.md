# I/O Compatibility Checklist

This table tracks which I/O modules from PCem have been reimplemented in SimpleWhpDemo. It lists the port ranges, the equivalent initialization functions and the current status of the implementation.

| Modul | Porty (rozsah) | PCem Funkce | SimpleWhpDemo Funkce | Stav | Způsob přenosu | Poznámka |
|------|---------------|-------------|----------------------|------|----------------|---------|
| DMA Controller | 0x0000–0x000F, 0x0080–0x0087 | `dma_init` | `DmaInit` | ✅ | Reimplementace | Chybí chaining test |
| Floppy (FDC) | 0x03F0–0x03F7 | `fdc_add` | `fdc_add` | 🛠️ | Reimplementace | Chybí IRQ logika |
| PIC Master/Slave | 0x0020–0x0021, 0x00A0–0x00A1 | `pic_init`, `pic2_init` | `PicInit` | ✅ | Reimplementace | Ověřeno testy |
| PIT Timer | 0x0040–0x0043 | `pit_init` | `pit_init` | 🛠️ | Částečně hotovo | Chybí režimy 2/3 |
| Keyboard (XT) | 0x0060–0x0063 | `keyboard_xt_init` | `KeyboardXtInit` | ✅ | Reimplementace | Lze rozšířit |
| Serial (COM1/2) | 0x03F8–0x03FF, 0x02F8–0x02FF | `serial_init` variants | `serial1_init`, `serial2_init` | 🛠️ | Reimplementace | Chybí přerušení |
| NMI Mask | 0x00A0 | `nmi_init` | `NmiInit` | ✅ | Reimplementace | Bez problémů |

The goal is a full reimplementation of the PCem I/O layer while respecting SimpleWhpDemo's license. Modules marked as `🛠️` or `⚠️` still need work. Use this list as a roadmap when porting additional functionality.
