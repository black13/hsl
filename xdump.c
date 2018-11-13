/* Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
 * See the file COPYRIGHT for details.
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>
#include "functions.h"

#ifdef MIN
#undef MIN
#endif	/* MIN */
#define MIN(a, b) ((a) < (b) ? (a) : (b))
#define NPRINTCHAR '.'

void hexdump(FILE *out,
             unsigned const char *buffer,
             size_t len, size_t addr, size_t offset,
             int width, int do_addr, int do_plain, int do_newline)
{
    int c = 0 ;                 /* Position of character in buffer. */
    int lc ;                    /* Position of character in line. */
    int lcmax ;                 /* Number of characters in line. */

    while (len > 0) {
        if (do_addr) {
            fprintf(out, "%08zx: ", addr - offset) ;
        }
        for (lc = 0; lc < offset; lc++) { /* emtpy spaces until offset */
            if (lc == width / 2 && do_plain) { putc(' ', out) ; }
            fprintf(out, "   ") ;
        }
        for ( ; lc < MIN(width, len); lc++) { /* hex numbers */
            if (lc == width / 2 && do_plain) { putc(' ', out) ; }
            fprintf(out, "%02x ", buffer[c + lc - offset]) ;
        }
        lcmax = lc - offset ;
        for ( ; lc < width; lc++) { /* empty spaces up to width */
            if (lc == width / 2 && do_plain) { putc(' ', out) ; }
            fprintf(out, "   ") ;
        }
        if (do_plain) {
            for (lc = 0; lc < offset; lc++) {
                putc(' ', out) ;
            }
            fprintf(out, " \"") ;
            for ( ; lc < MIN(width, len); lc++) {
                putc(isprint(buffer[c + lc])
                     ? buffer[c + lc - offset]
                     : NPRINTCHAR, out) ;
            }
            fprintf(out, "\"") ;
        }
        if (do_newline) { fprintf(out, "\n") ; }
        len -= lcmax ;
        addr += lcmax ;
        c += lcmax ;
        offset = 0 ;		/* Offset is valid only in the first line. */
    }
}

void xdump(FILE *out, void *buffer, size_t len)
{
	hexdump(out, (unsigned const char *) buffer, len, 0, 0, 16, 1, 1, 1);
}

#ifdef MAIN

#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <sysexits.h>
#include <stdlib.h>

#define DEFAULTWIDTH 16

char *progname ;
char *usage_format =
    "%s: [-w width] [-anp] [file1 ...]\n\
  -w : output width in bytes (default %d)\n\
  -a : no address output\n\
  -n : no newlines\n\
  -p : no plain text output\n" ;

void usage(FILE *out)
{
    fprintf(out, usage_format, progname, DEFAULTWIDTH) ;
    exit(out == stdout ? EX_OK : EX_USAGE) ;
}

int width = DEFAULTWIDTH ;
int print_address = 1, print_plain = 1, print_newline = 1 ;

void do_file(int fd)
{
    unsigned long addr = 0 ;
    unsigned long len ;
    unsigned char buffer[BUFSIZ] ;
    unsigned int offset = 0 ;
    
    
    while ((len = read(fd, buffer, BUFSIZ)) > 0) {
        hexdump(stdout, buffer, len, addr + offset, offset,
                width, print_address, print_plain, print_newline) ;
        addr += len ;
        offset = 0 ;
    }
    if (len < 0) {
	perror("read") ;
    }
}


int main(int argc, char *argv[])
{
    int in, errors = 0 ;
    int c ;

    progname = strrchr(argv[0], '/') ? strrchr(argv[0], '/') + 1 : argv[0] ;    
    while ((c = getopt(argc, argv, "w:anp")) != EOF) {
        switch (c) {
          case 'w': width = atoi(optarg) ; break ;
          case 'a': print_address = 0 ;    break ;
          case 'n': print_newline = 0 ;    break ;
          case 'p': print_plain = 0 ;      break ;
          case '?':
          case 'h': usage(stdout) ;        break ;
          default:  usage(stderr) ;        break ;
        }
    }
    if (width == 0) { usage(stderr) ; }
    argc -= optind - 1 ;
    argv += optind - 1 ;
    
    if (argc == 1) {
        do_file(STDIN_FILENO) ;
    } else {
	while (*++argv) {
            if (!strcmp(*argv, "-")) {
                do_file(STDIN_FILENO) ;
            } else {
                if ((in = open(*argv, O_RDONLY)) == -1) {
                    perror(*argv) ;
                    errors++ ;
                    continue ;
                }
                do_file(in) ;
                close(in) ;
            }
	}
    }
    return errors ;
}

#endif /* MAIN */
