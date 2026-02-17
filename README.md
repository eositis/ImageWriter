# ImageWriter
Greg Kennedy, 2026

## Overview
This is a tool to handle Apple ImageWriter printer control language.  Given one or more input files, it will render them to .bmp images, pure .txt files, or PostScript documents.

In fact, almost none of the code was written by me - the ImageWriter interpreter and graphics plotting was taken wholesale from the [GSport IIGS emulator](https://github.com/david-schmidt/gsport/blob/main/src/imagewriter.cpp), in turn borrowed from KEGS and DOSBox.  My contribution was simply a wrapper program to make it work from the CLI, and Makefile that "works on my machine" :)

The idea for this came from [a post by Colin Leroy-Mira](https://www.colino.net/wordpress/en/print-from-an-apple-ii-to-any-modern-printer/) but since I lack physical hardware, it's an all-software solution.

Because the original code requires SDL 1.2 for plotting graphics, so does this version.  Sorry!

## Config file

Settings are saved to `~/.imagewriterrc` after each run. When launched without command-line options, the app reads from this file and:
- Runs file mode with the last used input file, or
- Runs serial mode if a serial port was last used

Command-line options override config file values.

### Interactive mode
`./imagewriter -i`

Starts an interactive session with menus for:
- **Input source:** File(s) or Serial port
- **Serial port:** Select from list or enter path manually
- **Output format:** bmp, text, ps, colorps, printer
- **Printer:** Select from available printers (when output is printer)
- **DPI, paper size, banner, multipage:** Yes/no or numbered choices
- **Run mode:** Foreground (with status commentary) or Background daemon

In foreground mode, the app reports its activity: waiting for input, receiving input, page 1, page 2, printing, etc.

### File mode (printer dump files)
`./imagewriter [-d dpi] [-p pageSize] [-b bannerSize] [-o outputType] [-m] input.txt [input2.txt ...]`

### Serial port mode (live from USB-serial)
Connect an Apple II (or other computer) to a USB-serial adapter. Configure the adapter as a null-modem or direct connection to the computer's printer port.

`./imagewriter [-d dpi] [-p pageSize] [-b bannerSize] [-o outputType] [-B baud] [-D] -s <port>`

* `-s <port>` - Serial port path (e.g. `/dev/cu.usbserial-A50285BI` on macOS, `/dev/ttyUSB0` on Linux)
* `-B <baud>` - Baud rate (default 9600, ImageWriter II standard). Also: 300, 1200, 2400, 19200
* `-D` - Debug: dump raw serial data to `imagewriter_session_YYYYMMDD_HHMMSS.bin` (for replay with file mode)

### List serial ports
`./imagewriter -l`

**Serial wiring:** The Apple II uses TTL or RS-232 depending on the interface card. A USB-serial adapter (FTDI, CH340, Prolific, etc.) with appropriate level shifting may be needed; some setups use a null-modem adapter. See [docs/Serial-Port-Setup.md](docs/Serial-Port-Setup.md) for full details.

* DPI is arbitrary, but note that the print head in the ImageWriter could only achieve something like 144 DPI, so multiples of this are probably good choices.
* Paper sizes (use `-p` with 0 or 1):
  - 0: US Letter (8.5 × 11 in)
  - 1: A4 (210 × 297 mm)
* bannerSize asks how many pages you want to use for your banner.  I think.  Default 0 (no banner)
* outputType asks for one of these
```
	"text" - write text components to a text file
	"png" - would write to PNG if I could get libpng working
	"colorps" - color postscript file?
	"ps" - grayscale postscript file
	"printer" - send to system printer queue (macOS/Linux: lp/lpr; Windows: print dialog)
	anything else - BMP (default) - this is all I tested sorry
```
* multiPageOutput - for PS / ColorPS, output each page in a one multi-page doc, instead of as separate per-page .ps.  (Default 0)

You may specify multiple input.txt files.  To create input files, you can use something like [AppleWin](https://github.com/AppleWin/AppleWin) with the "printer dump filename" to log printer commands to a file, then feed them here.

## Output
The output writer code seems to put out files of the format `page####.bmp` with an increasing number, or for multi-page ps files, `doc####.ps`.

There is also a font path specified but I don't know what that does nor do I care.  The Print Shop can make perfectly good cards without it, and that's all I wrote this for.
