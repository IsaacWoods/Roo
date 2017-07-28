/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <string>
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
struct InstructionPrecolorer;
struct CodeGenerator;
struct ElfFile;
struct TargetMachine
{
  TargetMachine(const std::string& name, unsigned int numRegisters,
                                         unsigned int numGeneralRegisters,
                                         unsigned int generalRegisterSize,
                                         unsigned int numIntParamColors,
                                         unsigned int functionReturnColor);
  virtual ~TargetMachine();

  virtual InstructionPrecolorer* CreateInstructionPrecolorer() = 0;
  virtual CodeGenerator* CreateCodeGenerator(ElfFile& file) = 0;

  std::string   name;
  unsigned int  numRegisters;
  RegisterDef*  registerSet;
  unsigned int  numGeneralRegisters;
  unsigned int  generalRegisterSize;

  unsigned int  numIntParamColors;
  unsigned int* intParamColors;

  unsigned int  functionReturnColor;
};

struct InstructionPrecolorer : AirPass<void>
{
  InstructionPrecolorer() : AirPass() { }
  virtual ~InstructionPrecolorer() { }

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
  CodeGenerator(TargetMachine* target)
    :AirPass()
    ,target(target)
  {
  }
  virtual ~CodeGenerator() { }

  TargetMachine* target;

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

void Generate(const std::string& outputPath, TargetMachine* target, ParseResult& result);
