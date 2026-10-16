# ControlDiagPSX

Diagnostic tool for the original PlayStation (PSX), developed as a homebrew
application with [PSn00bSDK](https://github.com/Lameguy64/PSn00bSDK). It turns
the console into a handy tester for DualShock controllers: buttons, analog
sticks and vibration motors.

---

## English

### What it does

ControlDiagPSX is a 2D diagnostic utility that lets you verify, on real
hardware or in an emulator, that a PlayStation controller works correctly. The
interface is rendered with the GPU in a dark "test bench" style and shows, in
real time:

* **Controller detection** — reads both controller ports (P1/P2) and shows the
  detected type (`DIG`, `ANA`, `STK`, `NEG`, `GUN`, ...). DualShock controllers
  are automatically switched to analog mode using the standard init sequence.
* **Buttons** — every button (d-pad, `L1/L2/R1/R2`, `SELECT`, `START`, `△`, `□`,
  `○`, `×`, `L3`, `R3`) lights up green while pressed. Face buttons keep their
  PlayStation colours on the border.
* **Analog sticks** — two circular wells show a cursor that follows the stick,
  with the raw `X`/`Y` values printed underneath. Pressing `L3`/`R3` lights up
  the ring around the stick.
* **Vibration** — holding `L2` runs the big (left) motor through a sweep and
  holding `R2` turns the small (right) motor on. Two bars show the level sent
  to each motor.
* **Frame counter** — an `FRT` counter in the header keeps ticking so you can
  tell at a glance that the tool is alive (and that the screen isn't frozen).
* **Boot splash** — a logo screen with a technical grid, radar rings, the
  "TEST DIAG / FSmaker" wordmark and a loading bar.

### Building

Requirements:

* [PSn00bSDK](https://github.com/Lameguy64/PSn00bSDK) 0.24 (bundles the
  `mipsel-none-elf` GCC toolchain and `mkpsxiso`).

Build and produce the executable and the CD images:

```sh
./build.sh
```

`build.sh` expects the SDK to be installed under
`$HOME/opt/PSn00bSDK-0.24-Linux`; override it with the `PSN00BSDK_ROOT`
environment variable if yours lives elsewhere.

Output files (in `build/`):

| File          | Description                                        |
|---------------|----------------------------------------------------|
| `dstest.iso`  | Bootable CD image                                  |
| `dstest.bin`  | CD image data track (2352-byte sectors)            |
| `dstest.cue`  | Cue sheet for the data track                       |

### Running

* **Emulator** — load `dstest.cue` in an emulator with a PS1 BIOS (or HLE).
* **ODE (PicoStation, xStation, PSIO...)** — burn or copy `dstest.cue` +
  `dstest.bin`.
* **PicoStation** — rename `dstest.cue` → `UNIROM.cue` and `dstest.bin` →
  `UNIROM.bin` on the SD card (FAT32).

The disc image is bootable without a license file, so it works out of the box
on ODEs and with FreePSXBoot/UniROM. On a console with a modchip that enforces
the license check (PAL/JP), uncomment the `<license>` entry in `iso.xml` and
provide a matching `license.dat`.

### License

Released under the **MIT License** — see [LICENSE](LICENSE). You are free to
use, modify and redistribute this project, including forks and derivative
works; just keep the copyright notice. Forks and pull requests are welcome.

Note: `spi.c`/`spi.h` come from the PSn00bSDK example and are licensed under
the Mozilla Public License 2.0 (see the file headers); MIT applies to the rest
of the project.

### Credits

The SPI controller driver (`spi.c`/`spi.h`) is taken from the PSn00bSDK
example (© 2021 spicyjpeg, MPL licensed). UI and pad logic are custom.

---

## Italiano

### Cosa fa

ControlDiagPSX è un programma diagnostico homebrew per la PlayStation (PSX),
scritto con [PSn00bSDK](https://github.com/Lameguy64/PSn00bSDK). Trasforma la
console in un comodo tester per i controller DualShock: pulsanti, stick
analogici e motori di vibrazione.

L'interfaccia è disegnata via GPU in stile "banco di prova" scuro e mostra in
tempo reale:

* **Rilevamento controller** — legge entrambe le porte (P1/P2) e mostra il tipo
  rilevato (`DIG`, `ANA`, `STK`, `NEG`, `GUN`, ...). I DualShock vengono
  commutati automaticamente in modalità analogica tramite la sequenza di init
  standard.
* **Pulsanti** — ogni tasto (d-pad, `L1/L2/R1/R2`, `SELECT`, `START`, `△`, `□`,
  `○`, `×`, `L3`, `R3`) si illumina di verde quando viene premuto. I tasti
  frontali mantengono i colori PlayStation sul bordo.
* **Stick analogici** — due pozzetti circolari mostrano un cursore che segue lo
  stick, con i valori grezzi `X`/`Y` sotto. Premendo `L3`/`R3` si accende
  l'anello attorno allo stick.
* **Vibrazione** — tenendo premuto `L2` si fa oscillare il motore grande
  (sinistro), con `R2` si accende il motore piccolo (destro). Due barre
  mostrano il livello inviato a ciascun motore.
* **Contatore frame** — un contatore `FRT` nell'header continua a scorrere, così
  capisci a colpo d'occhio che il tool è vivo (e che lo schermo non è bloccato).
* **Splash di avvio** — una schermata logo con griglia tecnica, anelli radar,
  la scritta "TEST DIAG / FSmaker" e una barra di caricamento.

### Compilazione

Requisiti:

* [PSn00bSDK](https://github.com/Lameguy64/PSn00bSDK) 0.24 (include la
  toolchain GCC `mipsel-none-elf` e `mkpsxiso`).

Compila e genera l'eseguibile e le immagini CD:

```sh
./build.sh
```

`build.sh` si aspetta l'SDK in `$HOME/opt/PSn00bSDK-0.24-Linux`; se il tuo è in
un altro percorso, sovrascrivilo con la variabile `PSN00BSDK_ROOT`.

File di output (in `build/`):

| File          | Descrizione                                     |
|---------------|-------------------------------------------------|
| `dstest.iso`  | Immagine CD avviabile                           |
| `dstest.bin`  | Traccia dati dell'immagine CD (settori 2352)    |
| `dstest.cue`  | Cue sheet della traccia dati                    |

### Esecuzione

* **Emulatore** — carica `dstest.cue` in un emulatore con BIOS PS1 (o HLE).
* **ODE (PicoStation, xStation, PSIO...)** — masterizza o copia `dstest.cue` +
  `dstest.bin`.
* **PicoStation** — rinomina `dstest.cue` → `UNIROM.cue` e `dstest.bin` →
  `UNIROM.bin` sulla scheda SD (FAT32).

L'immagine CD è avviabile senza file di licenza, quindi funziona subito sugli
ODE e con FreePSXBoot/UniROM. Su una console con modchip che impone il check di
licenza (PAL/JP), decommenta la voce `<license>` in `iso.xml` e fornisci un
`license.dat` corrispondente.

### Licenza

Rilasciato con **licenza MIT** — vedi [LICENSE](LICENSE). Sei libero di usarlo,
modificarlo e ridistribuirlo, inclusi fork e lavori derivati; basta mantenere
l'avviso di copyright. Fork e pull request sono benvenuti.

Nota: `spi.c`/`spi.h` provengono dall'esempio di PSn00bSDK e sono sotto
Mozilla Public License 2.0 (vedi gli header dei file); la MIT vale per il resto
del progetto.

### Crediti

Il driver SPI dei controller (`spi.c`/`spi.h`) proviene dall'esempio di
PSn00bSDK (© 2021 spicyjpeg, licenza MPL). UI e logica dei pad sono originali.
