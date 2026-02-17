# Serial Port Setup for ImageWriter Emulator

This document describes how to use the ImageWriter emulator with a USB-serial adapter to receive print jobs directly from an Apple II or other computer, emulating an ImageWriter printer in real time.

## Overview

The ImageWriter emulator can operate in two modes:

1. **File mode** – Read printer dump files (e.g. from AppleWin or other emulators) and render to BMP, text, or PostScript
2. **Serial mode** – Listen on a USB-serial port for incoming data from a physical computer, piping each byte into the ImageWriter engine as it arrives

Serial mode allows you to connect an Apple II (or compatible) to a modern computer and print to virtual ImageWriter output without saving intermediate dump files.

## Requirements

- USB-serial adapter (FTDI, CH340, Prolific, or similar)
- Appropriate cabling and/or level shifting for your Apple II interface
- macOS or Linux (serial support; Windows stub exists but full implementation is pending)

## Quick Start

### 1. List available serial ports

```bash
./imagewriter -l
```

Example output:

```
Available serial ports (ImageWriter-compatible):
  Use -s /dev/<name> to connect

  /dev/cu.usbserial-A50285BI
  /dev/cu.usbmodem14201
```

### 2. Connect and listen

```bash
./imagewriter -s /dev/cu.usbserial-A50285BI
```

Output defaults to BMP files (`page1.bmp`, `page2.bmp`, …) in the current directory. Press **Ctrl+C** to stop; the current page will be ejected and saved.

### 3. Specify output format

```bash
./imagewriter -s /dev/cu.usbserial-A50285BI -o bmp    # BMP (default)
./imagewriter -s /dev/cu.usbserial-A50285BI -o ps     # PostScript
./imagewriter -s /dev/cu.usbserial-A50285BI -o text   # Plain text
```

## Serial Port Configuration

### ImageWriter II defaults

The emulator configures the serial port to match the ImageWriter II standard:

| Setting   | Default | Notes                                      |
|----------|---------|--------------------------------------------|
| Baud rate| 9600    | ImageWriter II default; LQ uses 19200      |
| Data bits| 8       |                                            |
| Parity   | None    |                                            |
| Stop bits| 1       |                                            |
| Format   | 8N1     | Raw binary pass-through                    |

### Baud rate option

ImageWriter II DIP switches support 300, 1200, 2400, 9600, and 19200 baud. Use `-B` to match your computer's configuration:

```bash
./imagewriter -s /dev/cu.usbserial-xxx -B 9600   # Default
./imagewriter -s /dev/cu.usbserial-xxx -B 19200  # ImageWriter LQ
./imagewriter -s /dev/cu.usbserial-xxx -B 2400   # Slower connections
```

## Port discovery (macOS)

Common device name patterns:

- `/dev/cu.usbserial-*` – FTDI and similar USB-serial adapters
- `/dev/cu.usbmodem*` – CDC ACM (e.g. Arduino, some adapters)
- `/dev/cu.SLAB*` – Prolific PL2303
- `/dev/cu.PL2303*` – Prolific (alternate)
- `/dev/cu.ch34*` – CH340/CH341

Use `cu.*` for call-out (initiating) connections; it avoids waiting on modem control lines.

## Port discovery (Linux)

- `/dev/ttyUSB*` – USB-serial (FTDI, Prolific, CH340, etc.)
- `/dev/ttyACM*` – CDC ACM devices
- `/dev/ttyS*` – Built-in serial ports

You may need to add your user to the `dialout` group:

```bash
sudo usermod -a -G dialout $USER
```

Then log out and back in for the change to take effect.

## Hardware wiring

The Apple II uses different serial interfaces depending on the card:

- **Super Serial Card** – RS-232 levels; may need null-modem crossover
- **Apple II Serial Interface Card** – Similar RS-232
- **Other interfaces** – TTL or custom levels

Typical setup:

1. **Apple II** (printer port) → **Null-modem adapter** (if needed) → **USB-serial adapter** → **Computer running ImageWriter emulator**

2. Ensure TX on the Apple side connects to RX on the USB adapter, and vice versa (null-modem handles this).

3. Match baud rate on both sides. The ImageWriter DIP switches (SW-2 #1 and #2) set the printer baud; configure the Apple II software to match.

### Debug mode: capture raw serial data

Use `-D` to save a raw dump of all bytes received from the serial port to a timestamped session file:

```bash
./imagewriter -s /dev/cu.usbserial-xxx -D
```

This creates `imagewriter_session_YYYYMMDD_HHMMSS.bin` in the current directory. The file can be replayed later with file mode: `./imagewriter -o bmp imagewriter_session_20260216_143022.bin`.

## Command reference

```
Serial mode:  ./imagewriter [-d dpi] [-p paper] [-b banner] [-o output] [-B baud] [-D] -s <port>
List ports:   ./imagewriter -l
```

| Option | Description |
|--------|-------------|
| `-s <port>` | Serial port path (required for serial mode) |
| `-l` | List available serial ports |
| `-B <baud>` | Baud rate: 300, 1200, 2400, 9600, 19200 (default: 9600) |
| `-o <type>` | Output: `bmp`, `text`, `ps`, `colorps`, `printer` (default: bmp) |
| `-D` | Debug: dump raw serial data to `imagewriter_session_YYYYMMDD_HHMMSS.bin` |
| `-d <dpi>` | Resolution (default: 144) |
| `-p <n>` | Paper size: 0=US Letter, 1=A4 |
| `-b <n>` | Banner length (0=normal page) |

## Troubleshooting

**No ports listed with `-l`**

- Verify the USB-serial adapter is connected and recognized by the OS
- On Linux, check `dmesg` for the device; ensure `dialout` group membership
- Try both `cu.*` and `tty.*` device names on macOS if applicable

**Permission denied when opening port**

- On Linux: `sudo usermod -a -G dialout $USER`, then re-login
- Ensure no other program has the port open (terminal, minicom, etc.)

**Garbled or missing output**

- Confirm baud rates match on both sides
- Check cable wiring (TX/RX, null-modem if needed)
- Verify the Apple II printer driver is configured for the correct interface and speed

**Connection drops**

- Some USB adapters may disconnect under load; try a different port or adapter
- Ensure adequate power; avoid long or low-quality USB cables
