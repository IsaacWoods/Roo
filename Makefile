# Copyright (C) 2017, Isaac Woods.
# See LICENCE.md

CXX ?= clang++
IGNORED_WARNINGS = -Wno-unused-result -Wno-trigraphs -Wno-vla -Wno-nested-anon-types -Wno-missing-braces -Wno-vla-extension
CFLAGS = -Wall -Wextra -pedantic -O0 -std=c++1z -g -Isrc $(IGNORED_WARNINGS)
LFLAGS = -Wall -Wextra -pedantic -O0 -std=c++1z -g -Isrc

BUILD_DIR=./build

OBJS = \
  $(BUILD_DIR)/main.o \
	$(BUILD_DIR)/common.o \
	$(BUILD_DIR)/error.o \
	$(BUILD_DIR)/ast.o \
	$(BUILD_DIR)/ir.o \
  $(BUILD_DIR)/parsing.o \
	$(BUILD_DIR)/module.o \
	$(BUILD_DIR)/air.o \
	$(BUILD_DIR)/elf.o \
	$(BUILD_DIR)/passes/dotEmitter.o \
	$(BUILD_DIR)/passes/variableResolver.o \
	$(BUILD_DIR)/passes/typeChecker.o \
	$(BUILD_DIR)/passes/conditionFolder.o \
	$(BUILD_DIR)/x64/x64.o \
	$(BUILD_DIR)/x64/precolorer.o \
	$(BUILD_DIR)/x64/emitter.o \
	$(BUILD_DIR)/x64/codeGenerator.o \

STD_OBJECTS = \
	Prelude-dir/stuff.o \

.PHONY: clean install lines prelude
.DEFAULT: roo

roo: $(OBJS) $(STD_OBJECTS)
	$(CXX) -o $@ $(OBJS) $(LFLAGS)

$(BUILD_DIR)/%.o: src/%.cpp
	test -d $(BUILD_DIR)/passes || (mkdir -p $(BUILD_DIR)/passes && mkdir -p $(BUILD_DIR)/x64)
	$(CXX) -o $@ -c $< $(CFLAGS)

%.o: %.s
	nasm -felf64 -o $@ $<

clean:
	rm -rf $(BUILD_DIR)
	rm -rf *.dot
	rm -f roo
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
