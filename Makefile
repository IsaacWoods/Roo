# Copyright (C) 2017, Isaac Woods.
# See LICENCE.md

# The Tier is where we are at in development. Developers should leave this at "DEV", CI will set it at "CI" and
# final releases are built as "PROD". Make doesn't make this easy to enforce, but anything else won't set the
# correct flags!
TIER ?= "DEV"

CXX ?= clang++
IGNORED_WARNINGS = -Wno-unused-result -Wno-trigraphs -Wno-vla -Wno-nested-anon-types -Wno-missing-braces -Wno-vla-extension
CFLAGS = -Wall -Wextra -pedantic -O0 -std=c++1z -g -Isrc $(IGNORED_WARNINGS)
LFLAGS = -Wall -Wextra -pedantic -O0 -std=c++1z -g -Isrc

ifeq ($(TIER), "DEV")
	CFLAGS += -Werror -O0
	LFLAGS += -Werror -O0 -rdynamic
else ifeq ($(TIER), "CI")
	CFLAGS += -Werror -O0
	LFLAGS += -Werror -O0 -rdynamic
else ifeq ($(TIER), "PROD")
	CFLAGS += -O3
	LFLAGS += -O3
endif

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
	$(BUILD_DIR)/codegen.o \
	$(BUILD_DIR)/elf/elf.o \
	$(BUILD_DIR)/passes/dotEmitter.o \
	$(BUILD_DIR)/passes/variableResolver.o \
	$(BUILD_DIR)/passes/typeChecker.o \
	$(BUILD_DIR)/passes/conditionFolder.o \
	$(BUILD_DIR)/passes/scopeResolver.o \
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
	test -d $(BUILD_DIR) || (mkdir -p $(BUILD_DIR)/passes && mkdir -p $(BUILD_DIR)/x64 && mkdir -p $(BUILD_DIR)/elf)
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
