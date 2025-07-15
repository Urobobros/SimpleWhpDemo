# Fázový plán implementace AMI BIOSu

Tento dokument rozděluje kroky popsané v [ami_bios_plan.md](ami_bios_plan.md) do několika fází. U každého úkolu je možné zaškrtnout jeho splnění.

## Fáze 1: Základní integrace BIOSu
- [ ] Získat binární soubor `ami_bios.bin` a uložit jej do projektu
- [x] Implementovat funkci `load_bios` v `src/main.rs`
- [x] Načíst BIOS na adresu `0xF0000` před spuštěním programu
- [x] Přidat základní logy o spuštění WHPX a vytvoření VM

## Fáze 2: Detailní logování a ošetření chyb
- [x] Logovat jednotlivé fáze startu, včetně načítání BIOSu a programu
- [x] Zachytávat případné textové výstupy BIOSu do logu
- [x] Ošetřit chyby a přidat jejich popis do logu

## Fáze 3: Inspirace z PCem a QEMU
- [ ] Prostudovat projekt **PCem** pro postup spouštění BIOSu a VGA
- [ ] Spouštět projekt pod **QEMU** s WHPX a viditelným oknem
- [ ] Ověřit možnost vstoupit do nastavení AMI BIOSu klávesou `Delete`

## Fáze 4: Rozšíření emulátoru
- [ ] Přidat emulaci nezbytných periférií (např. časovač, klávesnici)
- [ ] Umožnit ukládání nastavení BIOSu
- [ ] Zdokumentovat celý postup implementace
