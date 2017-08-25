/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <string>
#include <cstdio>
#include <ast.hpp>
#include <air.hpp>
#include <target.hpp>

/*
 * Instruction precoloring allows us to enforce platform-specific restrictions on register allocation. For example,
 * division instructions on x86 and x64 require the operands and results to be in specific registers, which is
 * enforced by the precolorer, before the interference graph is then colored normally.
 */
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
