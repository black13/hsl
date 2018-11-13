/* Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
 * See the file COPYRIGHT for details.
 */

#include <stdio.h>
#include "objects.h"
#include "session.h"

void init_reader(void);

void new_reader(session_context_t *sc);
obp_t read_expr(session_context_t *sc);

