/*
 * Applesoft BASIC token expansion - implementation.
 * Token values $80-$EA map to keywords; $FD $FE $FF = "("
 */
#include "applesoft_tokens.h"
#include <stddef.h>

static const char *const tokens[128] = {
	/* 0x80 */ "END",    "FOR",    "NEXT",   "DATA",   "INPUT",  "DEL",    "DIM",    "READ",
	/* 0x88 */ "GR",     "TEXT",   "PR#",    "IN#",    "CALL",   "PLOT",   "HLIN",   "VLIN",
	/* 0x90 */ "HGR2",   "HGR",    "HCOLOR=","HPLOT",  "DRAW",   "XDRAW",  "HTAB",   "HOME",
	/* 0x98 */ "ROT=",   "SCALE=", "SHLOAD", "TRACE",  "NOTRACE","NORMAL", "INVERSE","FLASH",
	/* 0xA0 */ "COLOR=", "POP",    "VTAB",   "HIMEM:", "LOMEM:", "ONERR",  "RESUME", "RECALL",
	/* 0xA8 */ "STORE",  "SPEED=", "LET",    "GOTO",   "RUN",    "IF",     "RESTORE","&",
	/* 0xB0 */ "GOSUB",  "RETURN", "REM",    "STOP",   "ON",     "WAIT",   "LOAD",   "SAVE",
	/* 0xB8 */ "DEF FN", "POKE",   "PRINT",  "CONT",   "LIST",   "CLEAR",  "GET",    "NEW",
	/* 0xC0 */ "TAB(",   "TO",     "FN",     "SPC(",   "THEN",   "AT",     "NOT",    "STEP",
	/* 0xC8 */ "+",      "-",      "*",      "/",      "^",      "AND",    "OR",     ">",
	/* 0xD0 */ "=",      "<",      "SGN",    "INT",    "ABS",    "USR",    "FRE",    "SCRN(",
	/* 0xD8 */ "PDL",    "POS",    "SQR",    "RND",    "LOG",    "EXP",    "COS",    "SIN",
	/* 0xE0 */ "TAN",    "ATN",    "PEEK",   "LEN",    "STR$",  "VAL",    "ASC",    "CHR$",
	/* 0xE8 */ "LEFT$",  "RIGHT$", "MID$",   NULL,     NULL,     NULL,     NULL,     NULL,
	/* 0xF0 */ NULL,     NULL,     NULL,     NULL,     NULL,     NULL,     NULL,     NULL,
	/* 0xF8 */ NULL,     NULL,     NULL,     NULL,     NULL,     "(",      "(",      "(",  /* $FD $FE $FF */
};

const char *applesoft_token_to_str(unsigned char token)
{
	if (token < 0x80) return NULL;
	/* 0xB0-0xB9: GOSUB,RETURN,REM,STOP,ON,WAIT,LOAD,SAVE,DEF FN,POKE - these
	 * strip to digits 0-9; in LIST output, line numbers use inverse video so
	 * 0xB1 can mean "1" not RETURN. Skip expansion for these to avoid breaking
	 * line numbers. */
	if (token >= 0xB0 && token <= 0xB9) return NULL;
	if (token >= 0x80 && token <= 0xFF) {
		const char *s = tokens[token - 0x80];
		return s;
	}
	return NULL;
}
