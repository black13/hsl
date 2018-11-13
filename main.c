/* Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
 * See the file COPYRIGHT for details.
 */
/*
 * here be main()
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sysexits.h>
#include "builtins.h"
#include "io.h"
#include "session.h"
#include "reader.h"
#include "signals.h"
#include "printer.h"
#include "numbers.h"

#define PROGRAM_NAME "hsl"

extern int getopt(int argc, char * const argv[], const char *optstring);
extern int optind;

int opt_trace = 0;
int opt_interactive = 1;

void usage(FILE *out, char *message)
{
        if (message) {
                fputs(PROGRAM_NAME ": ", out);
                fputs(message, out);
                putc('\n', out);
        }
        fputs("usage: " PROGRAM_NAME " [-it] [file1 ...]\n", out);
        exit(out == stderr ? EX_USAGE : 0);
}


int main(int argc, char *argv[])
{
        int opt_char;
        int file_arguments = 0;         /* be non-interactive per default if we
                                           had files to load on the command
                                           line */
        
        while ((opt_char = getopt(argc, argv, "hit?")) != EOF) {
                switch (opt_char) {
                    case 'i':
                        opt_interactive = 1;
                        break;
                    case 't':
                        opt_trace = 1;
                        break;
                    case 'h':
                        usage(stdout, 0);
                        break;
                    default:
                        usage(stderr, 0);
                        break;
                }
        }

        setbuf(stdout, 0);
        setbuf(stderr, 0);
        init_objects();
        init_io();
        init_reader();
        init_builtins();
        init_numbers();
        if (opt_trace) {
                traceflag = 1;
        }
        argc -= optind - 1;
        argv += optind - 1;

        obp_t val = the_Nil;
        if (argc > 1) {
                while (*++argv) {
                        session_context_t *sc =
                                new_session(the_Stdin, the_Stderr, 0);
                        val = load_file(*argv, sc, 0);
                        free_session(sc);
                        if (IS_EXIT(val)) {
                                print_expr(val, the_Stderr);
                                terpri(the_Stderr);
                        }
                }
                file_arguments = 1;
        }
        if (!file_arguments || opt_interactive) {
                session_context_t *sc = new_session(the_Stdin, the_Stdout, 1);
                val = repl(sc, 0);
                free_session(sc);
        }
        return val == the_Nil;
}
