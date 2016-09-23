/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <cstdio>
#include <ir.hpp>

enum reg
{
  RAX,
  RBX,
  RCX,
  RDX,
  
  RSI,
  RDI,
  RBP,
  RSP,

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

const char* GetRegisterName(reg r);

/*
 * The state of a register at a particular point in the program.
 */
struct register_state
{
  enum register_usage
  {
    FREE,
    IN_USE,
    UNUSABLE
  } usage;

  /*
   * The variable occupying this register if it's in use.
   * Null otherwise
   */
  variable_def* variable;
};

struct register_state_set
{
  const char*     tag;
  register_state  registers[NUM_REGISTERS];

  register_state& operator[](reg r)
  {
    return registers[r];
  }
};

struct code_generator
{
  FILE* output;
  unsigned int tabCount;
};

void CreateCodeGenerator(code_generator& generator, const char* outputPath);
void FreeCodeGenerator(code_generator& generator);
void GenCodeSection(code_generator& generator, parse_result& parse);
void GenDataSection(code_generator& generator, parse_result& parse);
