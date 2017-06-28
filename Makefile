# Copyright (C) 2017, Isaac Woods.
# See LICENCE.md

CXX ?= g++
#CFLAGS = -Wall -Wextra -Werror -pedantic -O0 -std=c++17 -g -Isrc -Wno-unused-result -Wno-trigraphs
CFLAGS = -Wall -Wextra -pedantic -O0 -std=c++17 -g -Isrc -Wno-unused-result -Wno-trigraphs
LFLAGS = -Wall -Wextra -Werror -pedantic -O0 -std=c++17 -g -Isrc

OBJS = \
  src/main.o \
	src/common.o \
	src/error.o \
	src/ast.o \
	src/ir.o \
  src/parsing.o \
	src/module.o \
	src/dotEmitter.o \
	src/air.o \
	src/codegen_x64elf.o \
	src/elf.o \
	src/variableResolver.o \

STD_OBJECTS = \
	Prelude-dir/stuff.o \

.PHONY: clean install lines prelude
.DEFAULT: roo

roo: $(OBJS) $(STD_OBJECTS)
	$(CXX) -o $@ $(OBJS) $(LFLAGS)

%.o: %.cpp
	$(CXX) -o $@ -c $< $(CFLAGS)

%.o: %.s
	nasm -felf64 -o $@ $<

clean:
	find . -name '*.o' -or -name '*.dot' -or -name '*.png' | xargs rm roo
	rm -f Prelude Prelude.roomod

install:
	mkdir -p ~/.vim/syntax
	cp roo.vim ~/.vim/syntax/roo.vim

lines:
	cloc --read-lang-def=cloc.cfg .

prelude: Prelude-dir/stuff.o
	(cd Prelude-dir ; ../roo)
	cp Prelude-dir/Prelude Prelude
	cp Prelude-dir/Prelude.roomod Prelude.roomod
