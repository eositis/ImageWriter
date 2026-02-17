# Config File

ImageWriter saves and loads settings from `~/.imagewriterrc` (or `$HOME/.imagewriterrc`).

## When config is used

- **Loaded** at startup before parsing command-line options. Config values act as defaults.
- **Saved** after each successful run (file mode, serial mode, or interactive).
- **Overridden** by any command-line options you pass.

## Launching without options

If you run `./imagewriter` with no arguments:

1. Config is loaded.
2. If `last_mode=1` and `last_file` is set, file mode runs with that file.
3. If `last_mode=2` and `serial_port` is set, serial mode runs.
4. Otherwise, usage is printed.

So after a normal run, you can often just run `./imagewriter` to repeat the last job.

## Format

Simple key=value, one per line. Lines starting with `#` are comments.

```
dpi=144
paper=0
banner=0
multipage=0
output=bmp
serial_port=/dev/cu.usbserial-xxx
serial_baud=9600
debug=0
printer_name=
last_file=Printer.txt
last_mode=1
```

## Keys

| Key | Description |
|-----|-------------|
| dpi | Resolution (default 144) |
| paper | 0=US Letter, 1=A4 |
| banner | Banner length (0=normal) |
| multipage | 0 or 1 |
| output | bmp, text, ps, colorps, printer |
| serial_port | Serial device path |
| serial_baud | 300, 1200, 2400, 9600, 19200 |
| debug | 0 or 1 (serial debug dump) |
| printer_name | Printer name for -o printer |
| last_file | Last input file (for "run again") |
| last_mode | 1=file, 2=serial |
