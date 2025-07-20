# FAR plán

## 🎯 Cíl
Emulovat IBM‑XT‑klon s procesorem Intel 8088 (cca 8 MHz) a pamětí 64–640 KB. Hlavním cílem je nabootovat BIOS `ami_8088_bios_31jan89.bin` (verze 31. ledna 1989) a umožnit spouštění jednoduchých real‑mode programů.

## 📚 Kontext
Inspirací je emulátor PCem, který podporuje právě tento BIOS pro AMI XT klony. Projekt může využít open‑source komponenty nebo vlastní interpret CPU.

## 🛠 Architektura
- **Core emulátoru** – nekonečná smyčka CPU s emulací paměti (RAM/ROM) a I/O portů. Využívá rozhraní Windows Hypervisor Platform (známé také jako WHPX), takže je kompatibilní s nástroji jako QEMU.
- **Načtení BIOSu** – ROM je načtena z `ami_8088_bios_31jan89.bin` a zmapována do adresního prostoru `0xF0000–0xFFFFF`.
- **Reset & Boot** – po resetu se nastaví `CS:IP = F000:FFF0` a spustí se BIOS.
- **I/O a debug** – logování instrukcí a portů s možností přepnout podrobný výstup.
- **Testovací aplikace** – jednoduchý loader, který vyvolá `INT 10h` a zobrazí „HELLO“.
- **Konfigurace** – volba velikosti RAM, cesta k ROM a možnost simulovat cold start.

### Přehled spouštění BIOSu
1. Po resetu CPU provede skok z adresy `0xFFFF0` do začátku ROM (`F000:xxxx`).
2. BIOS inicializuje segmentové registry a zásobník.
3. Proběhne POST – test paměti a zařízení, postupné kódy se zapisují na port `0x80`.
4. Po inicializaci se nastaví tabulka přerušení a služeb `INT 10h`, `INT 13h` atd.
5. Nakonec BIOS zavolá `INT 19h`, čímž předá řízení zavaděči operačního systému.

### Poznámka k PCem
Emulátor PCem mapuje ROM na adresu `0xF0000` a resetovací vektor obsahuje
instrukci `jmp far` do této oblasti. Stejný princip používá i tento projekt.

## 🛣️ Roadmap
1. **M1** – Čtení ROM a její mapování do paměti (`ivt.fw`).
2. **M2** – Volitelně načíst originální BIOS `ami_8088_bios_31jan89.bin`.
3. **M3** – Reset CPU a spuštění BIOSu.
4. **M4** – Emulace prázdné smyčky BIOSu (NULL-period restart).
5. **M5** – Demo přes `INT 10h` zobrazující „HELLO“.
6. **M6** – Logování a debug rozhraní.

## 🔄 Stav implementace
- [x] Inicializace WHPX a vytvoření VM
- [x] Mapování 1 MiB paměti
- [x] Načtení BIOSu na adresu `0xF0000`
- [x] Volitelný BIOS `ami_8088_bios_31jan89.bin` s fallbackem na `ivt.fw`
- [x] Nastavení reset vektoru na `F000:FFF0`
- [x] Emulace I/O portů (tisk, klávesnice, disk, POST)
- [x] Podpora `INT 10h` a jednoduchý textový výstup
- [x] Načtení diskového obrazu a `INT 13h` (jednosektorový stub)
- [x] Ukázkový program „HELLO“
- [x] Skript pro disassemblování BIOSu
- [x] Plná textová CGA paměť 80×25 s vykreslováním do SDL okna

## 🧪 Integrace a testy
- Unit testy pro čtení/zápis paměti a správné mapování BIOSu.
- Funkční test: po spuštění musí být v logu vidět start BIOSu a zobrazený text „HELLO“.
- CI pipeline s buildem, testy a přehledem pokrytí.
