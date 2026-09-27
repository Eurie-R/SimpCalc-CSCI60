#ifndef PARSER_H
#define PARSER_H

#include <stdio.h>

// Parses one SimpCalc file, reading tokens with gettoken().
//   Writes the recognized statements (or the first syntax error) to `out`.
//   `display_name` is the file name used in the final
//   "<name> is a valid SimpCalc program" line.
//   Returns 1 if the program is valid, 0 otherwise.
int parse_file(FILE *in, FILE *out, const char *display_name);

#endif //PARSER_H 
