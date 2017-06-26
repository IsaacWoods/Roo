/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <cstdio>
#include <ast.hpp>

/*
 * NOTE(Isaac): This allows the codegen module to store platform-dependent
 * information about each register.
 */
struct register_pimpl;

struct register_def
{
  enum reg_usage
  {
    GENERAL,
    SPECIAL
  }               usage;
  const char*     name;
  register_pimpl* pimpl;
};

struct CodegenTarget
{
  CodegenTarget();
  ~CodegenTarget();

  const char*   name;
  unsigned int  numRegisters;
  register_def* registerSet;
  unsigned int  generalRegisterSize;

  unsigned int  numIntParamColors;
  unsigned int* intParamColors;

  unsigned int  functionReturnColor;
};

struct code_generator
{
  FILE* output;
};

void Generate(const char* outputPath, CodegenTarget& target, ParseResult& result);

void CreateCodeGenerator(code_generator& generator, const char* outputPath);
void FreeCodeGenerator(code_generator& generator);
void GenCodeSection(code_generator& generator, ParseResult& parse);
void GenDataSection(code_generator& generator, ParseResult& parse);
