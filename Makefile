# Copyright (C) 2016, Isaac Woods. All rights reserved.
# See LICENCE.md

CXX ?= g++
CFLAGS = -Wall -Wextra -pedantic -O2 -std=c++11 -g -Isrc
LFLAGS = -Wall -Wextra -pedantic -O2 -std=c++11 -g -Isrc

OBJS = \
  src/roo.o \
  src/parsing.o \
	src/ir.o \
	src/codegen.o \

.PHONY: clean

roo: $(OBJS)
	$(CXX) -o $@ $^ $(LFLAGS)

%.o: %.cpp
	$(CXX) -o $@ -c $< $(CFLAGS)

clean:
	find . -name '*.o' | xargs rm roo
