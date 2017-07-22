/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <ir.hpp>
#include <error.hpp>
#include <x64/x64.hpp>

struct ElfThing;

/*
 * +r - add an register opcode offset to the primary opcode
 * [...] - denotes a prefix byte
 * (...) - denotes bytes that follow the opcode, in order
 */
enum class I
{
  CMP_REG_REG,          // (ModR/M)
  CMP_RAX_IMM32,        // (4-byte immediate)
  PUSH_REG,             // +r
  POP_REG,              // +r
  ADD_REG_REG,          // [opcodeSize] (ModR/M)
  SUB_REG_REG,          // [opcodeSize] (ModR/M)
  MUL_REG_REG,          // [opcodeSize] (ModR/M)
  DIV_REG_REG,          // [opcodeSize] (ModR/M)
  XOR_REG_REG,          // [opcodeSize] (ModR/M)
  ADD_REG_IMM32,        // [opcodeSize] (ModR/M [extension]) (4-byte immediate)
  SUB_REG_IMM32,        // [opcodeSize] (ModR/M [extension]) (4-byte immediate)
  MUL_REG_IMM32,        // [opcodeSize] (ModR/M [extension]) (4-byte immediate)
  DIV_REG_IMM32,        // [opcodeSize] (ModR/M [extension]) (4-byte immediate)
  MOV_REG_REG,          // [opcodeSize] (ModR/M)
  MOV_REG_IMM32,        // +r (4-byte immediate)
  MOV_REG_IMM64,        // [immSize] +r (8-byte immedite)
  MOV_REG_BASE_DISP,    // [opcodeSize] (ModR/M) (1-byte/4-byte displacement)
  INC_REG,              // (ModR/M [extension])
  DEC_REG,              // (ModR/M [extension])
  NOT_REG,              // (ModR/M [extension])
  NEG_REG,              // (ModR/M [extension])
  CALL32,               // (4-byte offset to RIP)
  INT_IMM8,             // (1-byte immediate)
  LEAVE,
  RET,
  JMP,                  // (4-byte offset to RIP)
  JE,                   // (4-byte offset to RIP)
  JNE,                  // (4-byte offset to RIP)
  JO,                   // (4-byte offset to RIP)
  JNO,                  // (4-byte offset to RIP)
  JS,                   // (4-byte offset to RIP)
  JNS,                  // (4-byte offset to RIP)
  JG,                   // (4-byte offset to RIP)
  JGE,                  // (4-byte offset to RIP)
  JL,                   // (4-byte offset to RIP)
  JLE,                  // (4-byte offset to RIP)
  JPE,                  // (4-byte offset to RIP)
  JPO,                  // (4-byte offset to RIP)
};

void Emit(ErrorState& errorState, ElfThing* thing, TargetMachine& target, I instruction, ...);
