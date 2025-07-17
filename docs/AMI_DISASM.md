# AMI 8088 BIOS (31 Jan 1989) - Disassembly Notes

This document summarises the initial instructions found in the `ami_8088_bios_31jan89.bin` ROM. Only the first 8 KiB of this BIOS are available in the repository. These lines were produced using the helper script `scripts/disasm_ami.py` which relies on `ndisasm`.

```
$ python3 scripts/disasm_ami.py ami_8088_bios_31jan89.bin 40
```

The ROM begins with an ASCII signature encoded as instructions:

```
0000  41                INC CX
0001  4D                DEC BP
0002  49                DEC CX
0003  2D 20 30          SUB AX,0x3020
0006  31 2F             XOR [BX],BP
0008  33 31             XOR SI,[BX+DI]
000A  2F                DAS
000B  38 39             CMP [BX+DI],BH
```

Interpreted as text it reads `AMI-01/31/89…`.

The first executed routine toggles bits on port `0x61` (timer/speaker):

```
0020  A8 0C             TEST AL,0x0C
0022  74 0A             JZ   0x2E
0024  E4 61             IN   AL,0x61
0026  34 0C             XOR  AL,0x0C
0028  A8 0C             TEST AL,0x0C
002A  74 03             JZ   0x2F
002C  E6 61             OUT  0x61,AL
002E  C3                RET
```
The emulator now tracks this port so these accesses no longer trigger
the unknown‑port handler. When bits 0 and 1 remain set the host beeps continuously until cleared.

After some setup the BIOS calls video services via `INT 10h`, loads data pointers, and continues with POST checks. Throughout POST it writes codes to port `0x80`.

This analysis confirms the reset vector at `0xFFFF0` contains a FAR jump into the ROM, matching the behaviour expected by the emulator.
