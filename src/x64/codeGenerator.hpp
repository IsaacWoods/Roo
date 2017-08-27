/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <string>
#include <ir.hpp>
#include <codegen.hpp>
#include <elf/elf.hpp>
#include <x64/x64.hpp>

struct CodeGenerator_x64 : CodeGenerator
{
  CodeGenerator_x64(TargetMachine* target, ElfFile& file)
    :CodeGenerator(target)
    ,file(file)
  {
  }
  ~CodeGenerator_x64() { }

  ElfThing* Generate(CodeThing* code, ElfThing* rodataThing);
  ElfThing* GenerateBootstrap(ElfThing* thing, ParseResult& parse);

  /*
   * These are for the CodeThing currently being generated
   * TODO: Ideally they should be stored in a state rather than here.
   */
  ElfFile&        file;
  ElfThing*       elfThing;
  CodeThing*      code;
  ElfThing*       rodataThing;

  void Visit(LabelInstruction* instruction,     void*);
  void Visit(ReturnInstruction* instruction,    void*);
  void Visit(JumpInstruction* instruction,      void*);
  void Visit(MovInstruction* instruction,       void*);
  void Visit(CmpInstruction* instruction,       void*);
  void Visit(UnaryOpInstruction* instruction,   void*);
  void Visit(BinaryOpInstruction* instruction,  void*);
  void Visit(CallInstruction* instruction,      void*);
private:
  void MoveSlotToRegister(Reg_x64 reg, Slot* slot);
};
