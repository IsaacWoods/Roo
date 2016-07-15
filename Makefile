# Copyright (C) 2016, Isaac Woods. All rights reserved.
# See LICENCE.md

CC = gcc
CFLAGS = -Wall -Wextra -pedantic -O2 -std=c11 -g -Isrc
LFLAGS = -Wall -Wextra -pedantic -O2 -std=c11 -g -Isrc

OBJS = \
  src/roo.o \
  src/parsing.o \

.PHONY: clean

roo: $(OBJS)
	$(CC) -o $@ $^ $(LFLAGS)

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

clean:
	find . -name '*.o' | xargs rm roo
