# Kontrolní seznam

- [ ] Načíst a alokovat paměť RAM/ROM dle konfigurace
- [ ] ROM ami_8088_bios_31jan89.bin dostupná a zapsaná do ROM oblasti
- [ ] CPU reset → nastavit CS:IP, SP, DS, ES
- [ ] Core emulace: FETCH, DECODE, EXECUTE cyklus
- [ ] Emulace instrukcí: MOV, INT, JMP, CALL, RET, atd.
- [ ] Emulace IO portů pro BIOS operace (např. video)
- [ ] Debug log CPU cyklů + IO
- [ ] Demo: Program volající INT 0x10, vytiskne text
- [ ] Log konzole či grafika „HELLO“ zobrazení
- [ ] Edge‑cases: neexistující instrukce, adresa mimo RAM
- [ ] CI konfigurace + regresní testy
- [ ] Dokumentace pro uživatele: jak spustit demo od A do Z
