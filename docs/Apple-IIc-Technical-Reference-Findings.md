# Apple IIc Technical Reference & App Notes: Print/Serial Output Guidance

Findings from the Apple II Serial Port Firmware Interface Commands, IIc Technical Notes, and related docs.

## 1. Serial Port Data Format (Firmware Interface Commands)

**Data format (nD):**
- `Control-I 0D` = 8 data bits, 1 stop bit (default)
- `Control-I 1D` = **7 data bits**, 1 stop bit
- n=2,3,4–7 for other combinations

**Parity (nP):**
- `Control-I 0P` = None (default)
- `Control-I 1P` = Odd
- `Control-I 2P` = None
- `Control-I 3P` = Even
- `Control-I 5P` = **MARK (1)** — parity bit always 1

**7 data bits + MARK parity:** When configured with `Control-I 1D` and `Control-I 5P`, the interface sends 7 data bits plus a parity bit that is always 1. This matches the ImageWriter Technical Reference rule: *8th bit always 1 for data*.

## 2. IIc Serial Port Behavior

- **Default (printer port):** 9600 baud, 8 data bits, no parity, 2 stop bits, ~80 chars/line
- **Command character:** Control-I ($09) in printer mode; Control-A ($01) in communications mode
- **Automatic line feed:** `Control-I LE` adds LF after each CR; `Control-I LD` disables it
- **Control-I K / Control-I L:** Temporarily disable/re-enable automatic LF (for 3rd-party printers)

## 3. IIc ROM Versions (TN.AIIC.007)

| ROM ($FBBF) | Serial firmware behavior |
|-------------|--------------------------|
| 255 (Original) | Does NOT mask incoming LF; no XON/XOFF |
| 0 (3.5 ROM) | Masks incoming LF by default; supports XON/XOFF |
| 3, 4 (Memory-expandable) | Same as 3.5 ROM |
| 5 (IIc Plus) | mini-DIN 8 serial connectors |

## 4. Z Command — Suppress Control Character Parsing

> "Sending Control-I Z CR to the serial-port firmware prevents the firmware from recognizing any further control characters... All tabbing and line formatting are disabled after a Control-I Z command."
>
> *Use when transmitting data (e.g., graphics) that contains bit patterns that could be mistaken for control characters.*

**Implication:** With `Control-I Z` active, the firmware passes bytes through without interpreting Control-I as a command. Data is effectively raw. To re-enable parsing, the serial firmware must be re-initialized.

## 5. Sending the Command Character Through the Port

On the IIc (unlike the Super Serial Card), the command character cannot be sent by doubling it. To send Control-I:

1. `Control-I Control-A` — change command char to Control-A  
2. `Control-I` — the byte to send  
3. `Control-A Control-I` — restore command char to Control-I  

## 6. COUT / Output Path

- COUT at $FDED is the standard character output entry point.
- Output goes through CSW ($36) to the active driver (screen, serial, etc.).
- `PR#1` routes COUT to the printer serial port (slot 1).
- All Applesoft PRINT/OUTDO output ultimately goes through COUT.

## 7. 80-Column Control Characters (when 80-col firmware active)

When 80-column firmware is active (e.g., PR#3), control chars output via COUT include:
- Ctrl-N = Normal display
- Ctrl-O = Inverse display  
- Ctrl-Q = 40-column mode  
- Ctrl-R = 80-column mode  
- Ctrl-M = Return (CR)

**Note:** Screen memory stores inverse video via the high bit. An 80-column screen dump (TN.MISC.001) sends raw screen bytes (including high bit) to COUT, so inverse characters are preserved in the output stream.

## 8. Recommendations for ImageWriter Emulator

1. **High-bit stripping:** Keep the rule: when high bit is 1, strip to 7-bit ASCII; when high bit is 0, pass through as control. This matches both Applesoft OUTDO behavior and 7+mark parity.
2. **7+mark parity:** If the IIc is set to 7 data bits + mark parity (`1D` + `5P`), the received byte is 8 bits with parity in the high bit; stripping it yields correct 7-bit data.
3. **Line endings:** Continue mapping 0x8D, 0xFD, 0xA9 to CR; IIc serial may append LF automatically depending on LE/LD.
4. **Control-I ($09):** In normal operation it can appear in the stream as a command character; if the user has sent `Control-I Z`, it may appear as raw data. Handle $09 appropriately (e.g., don’t treat it as a special command in our preprocessing).
5. **IIc ROM version:** Different ROMs affect LF masking and XON/XOFF; document which IIc ROM version was used when capturing session dumps.
