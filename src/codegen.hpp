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

/*
 * A TargetMachine describes an architecture that we can generate code for. It describes the physical details of
 * the machine, as well as models for precoloring the interference graph etc.
 */
struct TargetMachine
{
  TargetMachine();
  ~TargetMachine();

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

  virtual void Visit(LabelInstruction* instruction,     void*) = 0;
  virtual void Visit(ReturnInstruction* instruction,    void*) = 0;
  virtual void Visit(JumpInstruction* instruction,      void*) = 0;
  virtual void Visit(MovInstruction* instruction,       void*) = 0;
  virtual void Visit(CmpInstruction* instruction,       void*) = 0;
  virtual void Visit(UnaryOpInstruction* instruction,   void*) = 0;
  virtual void Visit(BinaryOpInstruction* instruction,  void*) = 0;
  virtual void Visit(CallInstruction* instruction,      void*) = 0;
};

struct ElfThing;
struct CodeGenerator : AirPass<void>
{
  CodeGenerator(TargetMachine& target)
    :AirPass()
    ,target(target)
  {
  }
  virtual ~CodeGenerator() { }

  TargetMachine& target;

  virtual ElfThing* Generate(CodeThing* code, ElfThing* rodataThing) = 0;
  virtual ElfThing* GenerateBootstrap(ElfThing* thing, ParseResult& parse) = 0;

  virtual void Visit(LabelInstruction* instruction,     void*) = 0;
  virtual void Visit(ReturnInstruction* instruction,    void*) = 0;
  virtual void Visit(JumpInstruction* instruction,      void*) = 0;
  virtual void Visit(MovInstruction* instruction,       void*) = 0;
  virtual void Visit(CmpInstruction* instruction,       void*) = 0;
  virtual void Visit(UnaryOpInstruction* instruction,   void*) = 0;
  virtual void Visit(BinaryOpInstruction* instruction,  void*) = 0;
  virtual void Visit(CallInstruction* instruction,      void*) = 0;
};

void Generate(const std::string& outputPath, TargetMachine& target, ParseResult& result);
