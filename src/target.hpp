/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <string>
#include <ir.hpp>

struct InstructionPrecolorer;
struct CodeGenerator;
struct ElfFile;

/*
 * This is the base register definition. Each target architecture should extend it to contain information specific
 * to that architecture's registers.
 */
struct BaseRegisterDef
{
  enum Usage
  {
    GENERAL,
    SPECIAL
  };
  
  BaseRegisterDef(Usage usage, const std::string& name);
  virtual ~BaseRegisterDef() { }

  Usage       usage;
  std::string name;
};

/*
 * A TargetMachine describes an architecture that we can generate code for. It describes the physical details of
 * the machine, as well as models for precoloring the interference graph etc.
 */
struct TargetMachine
{
  TargetMachine(const std::string& name, ParseResult& parse,
                                         unsigned int numRegisters,
                                         unsigned int numGeneralRegisters,
                                         unsigned int generalRegisterSize,
                                         unsigned int numIntParamColors,
                                         unsigned int functionReturnColor);
  virtual ~TargetMachine();

  virtual InstructionPrecolorer* CreateInstructionPrecolorer() = 0;
  virtual CodeGenerator* CreateCodeGenerator(ElfFile& file) = 0;

  std::string       name;
  unsigned int      numRegisters;
  BaseRegisterDef** registerSet;
  unsigned int      numGeneralRegisters;
  unsigned int      generalRegisterSize;

  unsigned int      numIntParamColors;
  unsigned int*     intParamColors;
  unsigned int      functionReturnColor;

  TypeRef*          intrinsicTypes[NUM_INTRINSIC_OP_TYPES];
};
