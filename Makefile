# Copyright (C) 2016, Isaac Woods. All rights reserved.
# See LICENCE.md

CXX ?= g++
CFLAGS = -Wall -Wextra -pedantic -O0 -std=c++11 -g -Isrc
LFLAGS = -Wall -Wextra -pedantic -O0 -std=c++11 -g -Isrc

OBJS = \
  src/roo.o \
	src/common.o \
	src/token_type.o \
  src/parsing.o \
	src/ast.o \
	src/codegen.o \
	src/regAlloc.o \

.PHONY: clean

roo: $(OBJS)
	$(CXX) -o $@ $^ $(LFLAGS)

%.o: %.cpp
	$(CXX) -o $@ -c $< $(CFLAGS)

clean:
	find . -name '*.o' | xargs rm roo
