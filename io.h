/* Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
 * See the file COPYRIGHT for details.
 */

#include <stdarg.h>
#include "objects.h"
#include "session.h"

#define IO_READ    0
#define IO_WRITE   1

extern obp_t the_Stdin;
extern obp_t the_Stdout;
extern obp_t the_Stderr;

void init_io(void);

obp_t make_stream_port(char *fname, char *fmode);
obp_t make_string_port(char *name);
obp_t port_print(obp_t port, char *s);
obp_t port_printf(obp_t port, char *format, ...);
obp_t port_vprintf(obp_t port, char *format, va_list arglist);
obp_t close_port(obp_t port);
obp_t port_putc(obp_t port, char c);
obp_t port_write(obp_t port, char *s, uint len);
obp_t get_port_string(obp_t port);
obp_t port_read(obp_t port, uint len);
obp_t port_getc(obp_t port);
obp_t port_ungetc(obp_t port, int c);
char *port_type_name(port_type_t type);
obp_t port_tty(obp_t port);
obp_t port_flush(obp_t port);
obp_t load_file(char *fname, session_context_t *sc, int level);
