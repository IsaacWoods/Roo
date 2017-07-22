/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <string>
#include <ir.hpp>
#include <codegen.hpp>
#include <elf.hpp>

struct CodeGenerator_x64 : CodeGenerator
{
  CodeGenerator_x64(TargetMachine& target, ElfFile& file, ElfThing* elfThing, CodeThing* code, ElfThing* rodataThing)
    :CodeGenerator(target)
    ,file(file)
    ,elfThing(elfThing)
    ,code(code)
    ,rodataThing(rodataThing)
  {
  }
  ~CodeGenerator_x64() { }

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
};

void Generate(const std::string& outputPath, TargetMachine& target, ParseResult& result);
