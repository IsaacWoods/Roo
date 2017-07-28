/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <cstdint>
#include <codegen.hpp>
#include <x64/precolorer.hpp>
#include <x64/codeGenerator.hpp>

enum Reg
{
  RAX,
  RBX,
  RCX,
  RDX,
  RSP,
  RBP,
  RSI,
  RDI,
  R8,
  R9,
  R10,
  R11,
  R12,
  R13,
  R14,
  R15,
  NUM_REGISTERS
};

struct RegisterPimpl
{
  uint8_t opcodeOffset;
};

struct TargetMachine_x64 : TargetMachine
{
  TargetMachine_x64();

  InstructionPrecolorer* CreateInstructionPrecolorer();
  CodeGenerator* CreateCodeGenerator(ElfFile& file);
};
