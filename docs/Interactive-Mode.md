# Interactive Mode

Run `./imagewriter -i` to start an interactive session. Settings are saved to `~/.imagewriterrc` after each run (including interactive) for use when launching without options. The app will guide you through configuration with menus and prompts instead of command-line arguments.

## Flow

1. **Input source** – File(s) or Serial port
2. **Input configuration** – Depends on choice:
   - **File:** Enter one or more file paths (empty line to finish)
   - **Serial:** Select port from list, or enter path; choose baud rate; optional debug dump
3. **Output format** – bmp, text, ps, colorps, printer
4. **Printer selection** – If output is printer, choose from available printers or use default
5. **DPI** – Resolution (default 144)
6. **Paper size** – US Letter or A4
7. **Banner size** – 0 for normal pages
8. **Multipage** – Yes/no for combining pages in one file/job
9. **Run mode** – Foreground (with status) or Background daemon

## Binary options (Yes/No)

- Multipage output
- Debug serial dump (serial mode only)

These are prompted as `[Y/n]` or `[y/N]` with a default; press Enter to accept.

## Multi-option menus

Numbered lists let you choose by entering a digit. Examples:

- Serial port: pick from detected ports or "Enter path manually"
- Output format: 1–5 for bmp, text, ps, colorps, printer
- Paper size: 1 = US Letter, 2 = A4
- Run mode: 1 = Foreground, 2 = Daemon

## Run modes

### Foreground (current session)

Runs in the terminal with status commentary:

- `[Initializing virtual ImageWriter]`
- `[Listening on /dev/... - Ctrl+C to stop]` (serial)
- `[Waiting for input]` / `[Receiving input]` (serial)
- `[Processing file 1/2: file.txt]` (file mode)
- `[Outputting page 1]`, `[Outputting page 2]`, etc.
- `[Ejecting page]`, `[Complete]`

### Background daemon

Forks to background. No terminal output after startup. Useful for long-running serial listeners. On macOS/Linux, stdout/stderr are redirected to `/dev/null`.

## Printer selection

When output is `printer`, the app runs `lpstat -p` to list printers. You can pick one by number or use the default. Requires CUPS (macOS, most Linux).
