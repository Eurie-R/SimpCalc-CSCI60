/*
 * Finds every input file in the current folder (name contains "input" and
 * ends in ".txt"), and for each one, e.g. sample_input_1.txt:
 *  1.  Scan pass:  writes every token to sample_output_scan_1.txt
 *  2. Parse pass: writes the parser's messages to sample_output_parse_1.txt
 * Two passes are needed because the scan output must list every token of
 * the file even when the parser stops at the first syntax error.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <sys/stat.h>
#include "scanner.h"
#include "parser.h"

#define MAX_FILES 1024
#define MAX_NAME  256

static char input_names[MAX_FILES][MAX_NAME];

// Returns 1 if `name` looks like an input file: contains "input",
//   ends in ".txt", and does not contain "output".
static int is_input_name(const char *name) {
    size_t len = strlen(name);
    return strstr(name, "input") != NULL
        && strstr(name, "output") == NULL
        && len >= 4
        && strcmp(name + len - 4, ".txt") == 0;
}

// Returns 1 if `name` is a regular file (not a folder).
static int is_regular_file(const char *name) {
    struct stat st;
    return stat(name, &st) == 0 && S_ISREG(st.st_mode);
}

// qsort comparison for the file-name table.
static int compare_names(const void *a, const void *b) {
    return strcmp((const char *)a, (const char *)b);
}

// Collects the input file names of the current folder into input_names,
//   sorted alphabetically. Returns how many were found, or -1 on error.
static int find_input_files(void) {
    DIR *dir = opendir(".");
    struct dirent *entry;
    int count = 0;

    if (dir == NULL) {
        perror("Cannot open current folder");
        return -1;
    }
    while ((entry = readdir(dir)) != NULL) {
        const char *name = entry->d_name;
        if (!is_input_name(name) || !is_regular_file(name))
            continue;
        if (strlen(name) >= MAX_NAME) {
            fprintf(stderr, "Skipping %s: file name too long\n", name);
            continue;
        }
        if (count == MAX_FILES) {
            fprintf(stderr, "Too many input files; only the first %d are processed\n",
                    MAX_FILES);
            break;
        }
        strcpy(input_names[count++], name);
    }
    closedir(dir);
    qsort(input_names, count, MAX_NAME, compare_names);
    return count;
}

// Builds the output file name by replacing the first "input" in
//   `in_name` with `replacement` (e.g. "output_scan").
static void make_output_name(const char *in_name, const char *replacement,
                             char *out_name, size_t size) {
    const char *pos = strstr(in_name, "input");
    int prefix_len = (int)(pos - in_name);
    snprintf(out_name, size, "%.*s%s%s",
             prefix_len, in_name, replacement, pos + strlen("input"));
}

// Writes one token to the scan output. Normal tokens are one line
//   (name padded to 31 columns, then the lexeme); lexical errors are two
//   lines, in the exact format of the reference outputs.
static void write_token(FILE *out, const Token *t) {
    switch (t->type == T_ERROR ? t->error : ERR_NONE) {
    case ERR_ILLEGAL:
        fprintf(out, "Lexical Error: Illegal character/character sequence   on line %d\n",
                t->line);
        fprintf(out, "Error  on line %d\n", t->line);
        break;
    case ERR_NUMBER:
        fprintf(out, "Lexical Error: Invalid number format   on line %d\n", t->line);
        fprintf(out, "Error   on line %d\n", t->line);
        break;
    case ERR_UNTERMINATED:
        fprintf(out, "Lexical Error: Unterminated  on line %d\n", t->line);
        fprintf(out, "Error   on line %d\n", t->line);
        break;
    case ERR_BANG:
        fprintf(out, "Lexical Error reading character ! on line %d\n", t->line);
        fprintf(out, "Error  on line %d\n", t->line);
        break;
    case ERR_NONE:
        fprintf(out, "%-31s%s\n", token_name(t->type), t->lexeme);
        break;
    }
}

// Scan pass: writes every token of `in_name` to `out_name`.
//   Returns 1 on success, 0 if a file could not be opened.
static int scan_file(const char *in_name, const char *out_name) {
    FILE *in = fopen(in_name, "r");
    FILE *out;
    Token t;

    if (in == NULL) {
        perror(in_name);
        return 0;
    }
    out = fopen(out_name, "w");
    if (out == NULL) {
        perror(out_name);
        fclose(in);
        return 0;
    }
    scanner_init(in);
    do {
        t = gettoken();
        write_token(out, &t);

        if (t.type == T_ERROR) { //stop if there's an error
            fclose(out);
            fclose(in);
            return 0;
        }
    } while (t.type != T_EOF);

    fclose(out);
    fclose(in);
    return 1;
}

// Parse pass: parses `in_name` and writes the result to `out_name`.
//   Returns 1 on success, 0 if a file could not be opened.
static int parse_to_file(const char *in_name, const char *out_name) {
    FILE *in = fopen(in_name, "r");
    FILE *out;

    if (in == NULL) {
        perror(in_name);
        return 0;
    }
    out = fopen(out_name, "w");
    if (out == NULL) {
        perror(out_name);
        fclose(in);
        return 0;
    }
    parse_file(in, out, in_name);

    fclose(out);
    fclose(in);
    return 1;
}

int main(void) {
    char scan_name[MAX_NAME + 16];
    char parse_name[MAX_NAME + 16];
    int count = find_input_files();
    int i;

    if (count < 0)
        return 1;
    if (count == 0) {
        printf("No input files found (expected names like sample_input_1.txt).\n");
        return 0;
    }
    for (i = 0; i < count; i++) {
        const char *name = input_names[i];
        make_output_name(name, "output_scan", scan_name, sizeof scan_name);
        make_output_name(name, "output_parse", parse_name, sizeof parse_name);

        if (scan_file(name, scan_name) && parse_to_file(name, parse_name))
            printf("Processed %s -> %s, %s\n", name, scan_name, parse_name);
    }
    return 0;
}
