/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <string>
#include <cstdint>
#include <ir.hpp>
#include <codegen.hpp>

enum Reg_x64
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

struct RegisterDef_x64 : BaseRegisterDef
{
  RegisterDef_x64(BaseRegisterDef::Usage usage, const std::string& name, uint8_t opcodeOffset);

  uint8_t opcodeOffset;
};

struct TargetMachine_x64 : TargetMachine
{
  TargetMachine_x64(ParseResult& parse);

  InstructionPrecolorer* CreateInstructionPrecolorer();
  CodeGenerator* CreateCodeGenerator(ElfFile& file);
};
