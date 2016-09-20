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
  R15
};

const char* GetRegisterName(reg r);

struct code_generator
{
  FILE* output;
  unsigned int tabCount;
};

void CreateCodeGenerator(code_generator& generator, const char* outputPath);
void FreeCodeGenerator(code_generator& generator);
void GenCodeSection(code_generator& generator, parse_result& parse);
void GenDataSection(code_generator& generator, parse_result& parse);
