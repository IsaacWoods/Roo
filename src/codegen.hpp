/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <cstdio>
#include <ast.hpp>
#include <air.hpp>

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

  const char*         name;
  const unsigned int  numRegisters;
  register_def*       registerSet;
  const unsigned int  numGeneralRegisters;
  const unsigned int  generalRegisterSize;

  const unsigned int  numIntParamColors;
  unsigned int*       intParamColors;

  const unsigned int  functionReturnColor;
};

struct InstructionPrecolorer : AirPass<void>
{
  InstructionPrecolorer() : AirPass() { }

  void Visit(LabelInstruction* instruction,     void*);
  void Visit(ReturnInstruction* instruction,    void*);
  void Visit(JumpInstruction* instruction,      void*);
  void Visit(MovInstruction* instruction,       void*);
  void Visit(CmpInstruction* instruction,       void*);
  void Visit(UnaryOpInstruction* instruction,   void*);
  void Visit(BinaryOpInstruction* instruction,  void*);
  void Visit(CallInstruction* instruction,      void*);
};

void Generate(const char* outputPath, CodegenTarget& target, ParseResult& result);

// XXX TODO: where the fuck are these even implemented or used??
/*
void CreateCodeGenerator(code_generator& generator, const char* outputPath);
void FreeCodeGenerator(code_generator& generator);
void GenCodeSection(code_generator& generator, ParseResult& parse);
void GenDataSection(code_generator& generator, ParseResult& parse);*/
