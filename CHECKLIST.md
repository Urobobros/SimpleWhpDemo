# Kontrolní seznam

- [x] Načíst a alokovat paměť RAM/ROM dle konfigurace
- [x] ROM ami_8088_bios_31jan89.bin dostupná a zapsaná do ROM oblasti
- [x] Možnost zadat cestu k BIOSu (výchozí `ami_8088_bios_31jan89.bin`, fallback `ivt.fw`)
- [x] CPU reset → nastavit CS:IP, SP, DS, ES
- [x] Core emulace: FETCH, DECODE, EXECUTE cyklus
- [x] Emulace instrukcí: MOV, INT, JMP, CALL, RET, atd.
- [x] Emulace IO portů pro BIOS operace (např. video)
- [x] Debug log CPU cyklů + IO
- [x] Demo: Program volající INT 0x10, vytiskne text
- [x] Log konzole či grafika „HELLO“ zobrazení
- [ ] Skript pro disassemblování BIOSu
- [ ] Edge‑cases: neexistující instrukce, adresa mimo RAM
- [ ] CI konfigurace + regresní testy
- [x] Dokumentace pro uživatele: jak spustit demo od A do Z
