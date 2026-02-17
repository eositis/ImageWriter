# Printer Queue Output

This document describes how printer queue output works in the ImageWriter emulator and the implementation approach used.

## Current Behavior

### Output modes (`imagewriter.cpp`)

The `outputPage()` function handles output based on the `output` string (from `-o`):

| Output type | Windows | macOS/Linux | File location |
|-------------|---------|-------------|---------------|
| `bmp` | ✓ | ✓ | `./page1.bmp`, `./page2.bmp`, … |
| `text` | ✓ | ✓ | `./printer.txt` |
| `ps` | ✓ | ✓ | `./page1.ps` or `./doc1.ps` (multipage) |
| `colorps` | ✓ | ✓ | `./page1.ps` or `./doc1.ps` (multipage) |
| `png` | ✓ (if C_LIBPNG) | ✓ | `./page1.png` |
| **`printer`** | ✓ Win32 GDI | **No-op** | — |

### Where files are written

- `findNextName()` (lines 1616–1641) builds paths as `document_path` + `"./page"` + number + extension.
- `document_path` is always `""`, so output goes to the current directory.
- Path format: `./page1.bmp`, `./page2.bmp`, etc. (or `./doc1.ps` for multipage PS).

### Windows printer path (existing)

On Windows, `output == "printer"` uses Win32 GDI:

1. **Constructor** (lines 217–254): `PrintDlg()` to obtain `printerDC` (user selects printer).
2. **outputPage()** (lines 1672–1813): `StartDoc()`, `StartPage()`, `StretchBlt()` to send the page bitmap to the printer DC, `EndPage()`, `EndDoc()`.
3. **finishMultipage()** (lines 2215–2220): `EndDoc(printerDC)` when closing.

### macOS/Linux printer path (gap)

For `output == "printer"` on non-Windows, the `#else` branch (around line 1812) only contains:

```cpp
#else
    //LOG_MSG("PRINTER: Direct printing not supported under this OS");
#endif
```

So the page is never written anywhere when `-o printer` is used on macOS or Linux.

---

## Modifications Required

### 1. macOS and Linux: implement printer output

Implement the `#else` branch in `outputPage()` when `output == "printer"`.

#### Option A: CUPS via shell (`lp` / `lpr`) – recommended

Use the existing page surface and CUPS print commands:

1. Write each page to a temporary file (BMP or PostScript).
2. Run `lp filename` or `lpr filename` to send it to the default printer.
3. Remove the temp file after printing.

**Pros:** No new dependencies; uses existing SDL_Surface → BMP/PS code.  
**Cons:** Temporary files; depends on `lp`/`lpr` being in `PATH` (standard on macOS and most Linux).

**Suggested flow:**
- Write to something like `/tmp/imagewriter_XXXXXX.bmp` (or `.ps`).
- `system("lp /tmp/imagewriter_XXXXXX.bmp")` or `popen()` for better error handling.
- `unlink()` the temp file.

#### Option B: CUPS API (libcups)

Use `libcups` for direct job submission:

1. Convert `SDL_Surface` to a format CUPS accepts (raster, PDF, or PostScript).
2. Call `cupsCreateJob()`, `cupsStartDocument()`, write data, `cupsFinishDocument()`, `cupsFinishJob()`.

**Pros:** No temp files; can target specific printers and options.  
**Cons:** New dependency; more code; need to handle surface → raster/PDF/PS conversion.

#### Option C: PostScript + `lp` (simple variant of A)

Reuse the existing PostScript path:

1. Produce the same PS stream as `-o ps` into a temp `.ps` file.
2. Run `lp /tmp/imagewriter_XXXXXX.ps`.

PostScript is widely supported by CUPS, so this avoids BMP conversion and fits well with the current PS logic.

---

### 2. Printer selection (optional)

**Current (Windows):** `PrintDlg()` lets the user choose a printer.

**macOS/Linux:** By default, `lp` uses the system default printer. To support selection:

- Add an option like `-o printer:PrinterName` or `-P PrinterName`.
- Pass it through to `lp -d PrinterName` or `lp -o printer=PrinterName`.

---

### 3. Multipage handling

For `-o printer` with `multipageOutput`:

- **Windows:** Uses `outputHandle` to keep the same print job; multiple pages go to a single job.
- **Unix:** Can either:
  - Submit one job per page (simpler), or
  - Use a single multipage PostScript file and `lp` that file once (reuse existing multipage PS logic).

---

### 4. Output directory option (separate concern)

To control where files are written (not only the printer):

- Add a flag, e.g. `-O <dir>` or `--output-dir <dir>`.
- Set `document_path` to that directory (with trailing `/`) before `findNextName()` is used.
- Ensure `findNextName()` uses `document_path` correctly for `bmp`, `ps`, `text`, etc.

This is independent of printer output but would help when using `-o bmp` etc. with a specific output folder.

---

## Implementation (macOS/Linux)

When `-o printer` is used on non-Windows systems, each page is:

1. Written to a unique temp file (`/tmp/imagewriter_XXXXXX`) as BMP via `SDL_SaveBMP`
2. Sent to the printer queue with `lp` (or `lpr` if `lp` is not available)
3. Deleted after successful print; kept on failure with a message

The command used is: `lp "path" 2>/dev/null || lpr "path" 2>/dev/null` — this tries `lp` first (CUPS on macOS and most Linux), then falls back to `lpr` if needed.

---

## Summary of code changes

| Location | Change |
|----------|--------|
| `imagewriter.cpp` ~line 1812 | Implemented `#else` branch for `output == "printer"` on non-Windows: write temp BMP, call `lp`/`lpr`, unlink. |
| `imagewriter.cpp` constructor | On non-Windows, no need to show a dialog; printer selection can be deferred to `lp -d` if supported. |
| `imagewriter.cpp` `finishMultipage()` | On non-Windows with `output == "printer"`, either no-op or ensure all pages have been sent (depending on per-page vs multipage behavior). |
| `main.c` | Document `-o printer`; optionally add `-P <printer>` and pass it through to the Imagewriter layer. |
| `imagewriter.h` / constructor | If printer selection is added, extend the API to pass a printer name (e.g. `char* printerName`). |
| Makefile | No changes for Option A; add `-lcups` if using libcups. |

---

## Recommended implementation (Option A)

1. **`outputPage()` – `#else` branch:**
   - Generate a unique temp path, e.g. `mkstemp()` or `tmpnam()` → `/tmp/imagewriter_XXXXXX.ps`.
   - Reuse the existing PostScript generation logic (similar to `-o ps`) to write that file.
   - Call `lp` (or `lpr`) on the temp file; check return value.
   - `unlink()` the temp file.
   - Optionally support `lp -d PrinterName` if a printer name is available.

2. **Error handling:**
   - If `lp` fails, keep the temp file and print a message with its path so the user can print manually.
   - Or fall back to writing to the current directory (e.g. `./pageN.ps`) and inform the user.

3. **Multipage:**
   - For `multipageOutput`, build a single multipage PostScript file (as with `-o ps` multipage), then `lp` that file once and delete it.
