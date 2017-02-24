# Copyright (C) 2016, Isaac Woods. All rights reserved.
# See LICENCE.md

CXX ?= g++
CFLAGS = -Wall -Wextra -pedantic -O0 -std=c++14 -g -Isrc -Wno-unused-result
LFLAGS = -Wall -Wextra -pedantic -O0 -std=c++14 -g -Isrc

OBJS = \
  src/main.o \
	src/common.o \
	src/scheduler.o \
	src/error.o \
	src/ir.o \
  src/parsing.o \
	src/ast.o \
	src/air.o \
	src/codegen_x64elf.o \
	src/elf.o \

AST_PASSES = \
	src/pass_resolveVars.hpp \
	src/pass_typeChecker.hpp \

STD_OBJECTS = \
	std/io.o \

.PHONY: clean install lines
.DEFAULT: roo

roo: $(OBJS) $(STD_OBJECTS) $(AST_PASSES)
	$(CXX) -o $@ $(OBJS) $(LFLAGS)

%.o: %.cpp
	$(CXX) -o $@ -c $< $(CFLAGS)

%.o: %.s
	nasm -felf64 -o $@ $<

clean:
	find . -name '*.o' -or -name '*.dot' -or -name '*.png' | xargs rm roo

install:
	mkdir -p ~/.vim/syntax
	cp roo.vim ~/.vim/syntax/roo.vim

lines:
	cloc --read-lang-def=cloc.cfg .
