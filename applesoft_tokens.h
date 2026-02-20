/*
 * Applesoft BASIC token expansion for Apple II LIST output.
 * When the Apple II sends a program listing to the printer, it sends tokenized
 * bytes (0x80-0xFF) rather than expanded keywords. This table maps token bytes
 * to their keyword strings so LIST output renders correctly.
 *
 * Reference: Applesoft II token list (e.g. BASIC.keywords.tokens.txt)
 */
#ifndef APPLESOFT_TOKENS_H
#define APPLESOFT_TOKENS_H

#ifdef __cplusplus
extern "C" {
#endif

/* Returns keyword string for token byte, or NULL if not a token.
 * Caller must output each character of the string. */
const char *applesoft_token_to_str(unsigned char token);

#ifdef __cplusplus
}
#endif

#endif /* APPLESOFT_TOKENS_H */
