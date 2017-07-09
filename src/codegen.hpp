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
struct RegisterPimpl;

struct RegisterDef
{
  enum Usage
  {
    GENERAL,
    SPECIAL
  }               usage;
  const char*     name;
  RegisterPimpl*  pimpl;
};

struct CodegenTarget
{
  CodegenTarget();
  ~CodegenTarget();

  const char*         name;
  const unsigned int  numRegisters;
  RegisterDef*        registerSet;
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
/*
struct CodeGenerator : AirPass<void>
{
};*/

void Generate(const char* outputPath, CodegenTarget& target, ParseResult& result);
