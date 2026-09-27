/*
 * Usage: call scanner_init() once per input file, then call gettoken()
 * repeatedly. After the EndofFile token, further calls keep returning
 * EndofFile. The scanner prints nothing; lexical errors come back as
 * T_ERROR tokens and the caller decides how to report them.
 */
#ifndef SCANNER_H
#define SCANNER_H

#include <stdio.h>
#include "token.h"

/* Start scanning a new file: resets the line counter and all internal state. */
void scanner_init(FILE *in);

/* Returns the next token from the input. */
Token gettoken(void);

#endif /* SCANNER_H */
