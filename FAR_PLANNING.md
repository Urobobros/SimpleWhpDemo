# FAR plán

## 🎯 Cíl
Emulovat IBM‑XT‑klon s procesorem Intel 8088 (cca 8 MHz) a pamětí 64–640 KB. Hlavním cílem je nabootovat BIOS `ami_8088_bios_31jan89.bin` (verze 31. ledna 1989) a umožnit spouštění jednoduchých real‑mode programů.

## 📚 Kontext
Inspirací je emulátor PCem, který podporuje právě tento BIOS pro AMI XT klony. Projekt může využít open‑source komponenty nebo vlastní interpret CPU.

## 🛠 Architektura
- **Core emulátoru** – nekonečná smyčka CPU s emulací paměti (RAM/ROM) a I/O portů.
- **Načtení BIOSu** – ROM je načtena z `ami_8088_bios_31jan89.bin` a zmapována do adresního prostoru `0xF0000–0xFFFFF`.
- **Reset & Boot** – po resetu se nastaví `CS:IP = F000:FFF0` a spustí se BIOS.
- **I/O a debug** – logování instrukcí a portů s možností přepnout podrobný výstup.
- **Testovací aplikace** – jednoduchý loader, který vyvolá `INT 10h` a zobrazí „HELLO“.
- **Konfigurace** – volba velikosti RAM, cesta k ROM a možnost simulovat cold start.

## ✅ Milníky
1. **M1** – Čtení ROM a její mapování do paměti.
2. **M2** – Reset CPU a spuštění BIOSu.
3. **M3** – Emulace prázdné smyčky BIOSu (NULL-period restart).
4. **M4** – Demo přes `INT 10h` zobrazující „HELLO“.
5. **M5** – Logování a debug rozhraní.

## 🔄 Stav implementace
- [x] Inicializace WHPX a vytvoření VM
- [x] Mapování 1 MiB paměti
- [x] Načtení BIOSu na adresu `0xF0000`
- [x] Nastavení reset vektoru na `F000:FFF0`
- [x] Emulace I/O portů (tisk, klávesnice, disk, POST)
- [x] Podpora `INT 10h` a jednoduchý textový výstup
- [x] Načtení diskového obrazu a `INT 13h` (jednosektorový stub)
- [x] Ukázkový program „HELLO“
- [ ] Plná textová CGA paměť 80×25 (plánováno)

## 🧪 Integrace a testy
- Unit testy pro čtení/zápis paměti a správné mapování BIOSu.
- Funkční test: po spuštění musí být v logu vidět start BIOSu a zobrazený text „HELLO“.
- CI pipeline s buildem, testy a přehledem pokrytí.
