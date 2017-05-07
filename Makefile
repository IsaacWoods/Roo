# Copyright (C) 2017, Isaac Woods.
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
	src/auto_doAstPasses.o \
	src/module.o \

STD_OBJECTS = \
	Prelude/stuff.o \

.PHONY: clean install lines
.DEFAULT: roo

roo: $(OBJS) $(STD_OBJECTS)
	$(CXX) -o $@ $(OBJS) $(LFLAGS)

%.o: %.cpp
	$(CXX) -o $@ -c $< $(CFLAGS)

%.o: %.s
	nasm -felf64 -o $@ $<

src/auto_doAstPasses.o:
#	python genPassFile.py
	$(CXX) -o $@ -c src/auto_doAstPasses.cpp $(CFLAGS)

clean:
	find . -name '*.o' -or -name '*.dot' -or -name '*.png' | xargs rm roo

install:
	mkdir -p ~/.vim/syntax
	cp roo.vim ~/.vim/syntax/roo.vim

lines:
	cloc --read-lang-def=cloc.cfg .
