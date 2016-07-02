# Copyright (C) 2016, Isaac Woods. All rights reserved.
# See LICENCE.md

CC ?= gcc
CFLAGS = -Wall -Wextra -pedantic -O2 -std=c11 -g
LFLAGS = -Wall -Wextra -pedantic -O2 -std=c11 -g

OBJS = \
  roo.o \

.PHONY: clean

roo: $(OBJS)
	$(CC) -o $@ $^

%.o: %.c
	$(CC) -o $@ -c $< $(CFLAGS)

clean:
	find . -name '*.o' | xargs rm
