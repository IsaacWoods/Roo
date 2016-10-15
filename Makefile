# Copyright (C) 2016, Isaac Woods. All rights reserved.
# See LICENCE.md

CXX ?= g++
CFLAGS = -Wall -Wextra -pedantic -O0 -std=c++14 -g -Isrc
LFLAGS = -Wall -Wextra -pedantic -O0 -std=c++14 -g -Isrc

OBJS = \
  src/roo.o \
	src/common.o \
  src/parsing.o \
	src/ast.o \
	src/air.o \
	src/codegen_x64elf.o \

.PHONY: graphAST
.PHONY: clean

roo: $(OBJS)
	$(CXX) -o $@ $^ $(LFLAGS)

%.o: %.cpp
	$(CXX) -o $@ -c $< $(CFLAGS)

graphAST:
	dot -Tpng ast.dot -o ast.png

clean:
	find . -name '*.o' | xargs rm roo
