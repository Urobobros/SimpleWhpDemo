# Plán pro přidání podpory AMI BIOSu

Tento dokument popisuje kroky potřebné k integraci obrazu AMI BIOSu do projektu **SimpleWhpDemo** a k vytvoření podrobného výpisu logu, který ukáže jednotlivé fáze startu.

## 1. Příprava obrazu BIOSu
1. Získejte binární soubor AMI BIOSu (např. `ami_bios.bin`).
2. Ujistěte se, že soubor má správnou velikost a je určen pro reálný režim x86.
3. Uložte soubor do adresáře projektu.

## 2. Rozšíření kódu pro načtení BIOSu
1. V souboru `src/main.rs` přidejte funkci `load_bios`, která načte BIOS z binárního souboru a zapíše jej na adresu `0xF0000` ve virtuální paměti.
2. Zavolejte `load_bios` před načtením programu v hlavní funkci `main`.

### Příklad volání
```rust
vm.load_bios("ami_bios.bin\0", 0xF0000)?;
```

## 3. Logování průběhu
1. Přidejte strukturované logování pomocí `println!` nebo knihovny `log`.
2. Logujte tyto události:
   - Inicializace WHPX
   - Vytvoření virtuálního stroje
   - Načítání AMI BIOSu
   - Načítání programu
   - Začátek běhu
   - Jednotlivé výstupy BIOSu (pokud je lze zachytit)
   - Ukončení běhu

Ukázka logu:
```
[INFO] Inicializace WHPX
[INFO] Vytvořen virtuální stroj
[INFO] Načítám ami_bios.bin na adresu 0xF0000
[INFO] Načítám hello.com na adresu 0x10100
[INFO] ======== Spuštění ========
[BIOS] ...
[INFO] ========= Konec ==========
```

## 4. Ošetření chyb
1. Každý krok by měl vracet `Result` a při chybě vypište popis problému.
2. Zajistěte, aby log obsahoval jasnou informaci, která fáze selhala.

## 5. Další úpravy
- Je možné implementovat jednoduchý výstup z BIOSu do logu pomocí emulace portů nebo přesměrování textového výstupu.
- V budoucnu lze rozšířit emulátor o další periférie, které BIOS vyžaduje (např. časovač, klávesnici).


## 6. Inspirace a cíle
- Při integraci AMI BIOSu se budeme inspirovat projektem **PCem**, zejména jeho způsobem spouštění BIOSu s podporou VGA.
- K běhu s WHPX využijeme **QEMU**, aby bylo možné zobrazit okno s emulovaným počítačem.
- Hlavním cílem je mít viditelný náběh BIOSu a umožnit stisknutím klávesy `Delete` vstoupit do nastavení AMI BIOSu a provádět změny.
