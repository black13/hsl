# Copyright (c) 2010, 2011 Juergen Nickelsen <ni@jnickelsen.de>
# See the file COPYRIGHT for details.

#2345678901234567890123456789012345678901234567890123456789012345678901234567890
#        1         2         3         4         5         6         7         8
HEADERS = objects.h hashmap.h cbasics.h xmemory.h printer.h reader.h signals.h \
	strbuf.h functions.h eval.h names.h builtins.h io.h session.h gc.h \
	tunables.h numbers.h
SOURCES = main.c hashmap.c xmemory.c objects.c printer.c reader.c signals.c \
	strbuf.c vectors.c xdump.c eval.c builtins.c io.c session.c gc.c \
	ob_common.c numbers.c
OBJECTS = $(subst .c,.o,$(SOURCES))
HOBJECTS =  objects.o xmemory.o xdump.o strbuf.o
CFLAGS  = -g -O # -O4 -DNDEBUG
LDLIBS  = -lm
CC      = gcc -Wall -Werror -std=c99 -m64
TARGET  = hsl

all: $(TARGET)

$(TARGET): $(OBJECTS)
	$(CC) $(CFLAGS) -o $(TARGET) $(OBJECTS) $(LDFLAGS) $(LDLIBS)

$(OBJECTS): $(HEADERS) Makefile

hashmaptest: $(HOBJECTS) hashmap.c
	$(CC) $(CFLAGS) -DHASHMAP_MAIN -o hashmaptest hashmap.c $(HOBJECTS)

test: $(TARGET) test/tests.lisp
	./$(TARGET) test/tests.lisp

clean:
	rm -f core core.* *~ *.o $(TARGET) cscope.out
