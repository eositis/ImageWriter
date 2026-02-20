# Applesoft Print Output: Findings from applesoft.m4

Parsed the Applesoft source (applesoft.m4) for clues about printer output. Key findings:

## 1. OUTDO and the 8th Bit (lines 1856–1872)

```asm
OUTDO               ORA #$80                        ; PRINT (A)
                    CMP #$A0                        ; CONTROL CHR?
                    BCC L_OUTDO_1                          ; SKIP IF SO
                    ORA FLASH_BIT                   ; =$40 FOR FLASH, ELSE $00
L_OUTDO_1                  JSR MON_COUT                    ; "AND"S WITH $3F (INVERSE), $7F (FLASH)
```

**Finding:** Every character sent to the output device gets `ORA #$80` before `MON_COUT`, so the high bit is always 1 for normal data. Control characters (< 0xA0 after ORA) skip the FLASH bit but still get ORA #$80. This matches the ImageWriter Technical Reference rule: *8th bit always 1 for data, 0 for control codes*.

## 2. LIST Token Expansion (lines 1155–1177)

```asm
LIST_4              BPL LIST_2                      ; BRANCH IF NOT A TOKEN
                    SEC                             
                    SBC #$7F                        ; CONVERT TOKEN TO INDEX
                    ...
L_LIST_4_4                  JSR GETCHR                      ; PRINT THE KEYWORD
                    ...
                    JSR OUTDO                       ; PRINT LAST CHAR OF KEYWORD
                    LDA #LOCHAR(` ')                        ; PRINT TRAILING SPACE
```

- Tokens in program text have high bit set (0x80–0xFF).
- On LIST, a token byte is looked up in `TOKEN_NAME_TABLE` and the keyword is output character-by-character via OUTDO.
- Standard token values: **PRINT = $BA**, **GOTO = $AB** (lines 205, 282, 397, 412).

## 3. Line Numbers (LINPRT, FOUT)

- Line numbers are converted to decimal digits with `LOCHAR('0')` (0x30) and sent through OUTDO.
- Each digit gets `ORA #$80` → 0xB0–0xB9 on the wire.
- The Applesoft token table comment notes that 0xB0–0xB9 can be inverse-video digits in line numbers (RETURN, REM, STOP, ON, etc. overlap this range).

## 4. String Output (STRPRT, lines 1836–1850)

- String literals (e.g. `PRINT "HELLO"`) are printed one character at a time via OUTDO.
- Each character gets `ORA #$80`.

## 5. Line Endings

- `CRDO` loads `#$0D` and calls OUTDO → 0x8D on the wire.
- Our mapping of 0x8D, 0xFD, 0xA9 → CR is consistent with Apple II line-ending variants.

## 6. Garbled Output Still Unexplained

- **" HELLO"** (bytes `03 a8 62 ac b5`): 0x03 is ETX. The rest would need Apple II character-set / serial encoding to decode fully. Possible alternate charset or control-byte influence.
- **" GOTO 10"** (bytes `7a b4 81 0c 2d d5 aa f0 f2 e3 f0`): Could include a mix of tokens (e.g. 0xAB for GOTO), control bytes, and digits. More analysis needed with an Apple II charset or protocol reference.
- **0xB2 0xB9 → "PRINT "**: Per token table, 0xB2 = REM, 0xB9 = POKE. Our empirical mapping to PRINT works for this IIc LIST output but does not match the standard token table; suggests IIc-specific or path-specific encoding.

## 7. Recommendation

The Applesoft source confirms that all output passes through OUTDO with `ORA #$80`, so stripping the high bit when it is set is correct. For remaining garbled sequences, the next step is to correlate bytes with the Apple II character set (e.g. `iw_charmaps.h` / Signetics 2513) and any IIc serial/80-column specifics.

---

See also: **Apple-IIc-Technical-Reference-Findings.md** — IIc serial firmware (7+mark parity, Control-I, Z command, ROM differences).
