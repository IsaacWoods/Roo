/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <x64/precolorer.hpp>
#include <x64/x64.hpp>

void InstructionPrecolorer_x64::Visit(LabelInstruction* /*instruction*/,     void*) { }
void InstructionPrecolorer_x64::Visit(ReturnInstruction* /*instruction*/,    void*) { }
void InstructionPrecolorer_x64::Visit(JumpInstruction* /*instruction*/,      void*) { }
void InstructionPrecolorer_x64::Visit(MovInstruction* /*instruction*/,       void*) { }
void InstructionPrecolorer_x64::Visit(UnaryOpInstruction* /*instruction*/,   void*) { }
void InstructionPrecolorer_x64::Visit(BinaryOpInstruction* /*instruction*/,  void*) { }
void InstructionPrecolorer_x64::Visit(CallInstruction* /*instruction*/,      void*) { }

void InstructionPrecolorer_x64::Visit(CmpInstruction* instruction, void*)
{
  /*
   * We should be able to assume that not both of the slots are constants, otherwise the comparison should have
   * been eliminated by the optimizer.
   */
  Assert(!(instruction->a->IsConstant() && instruction->b->IsConstant()), "Constant comparison not eliminated");

  /*
   * x86 only allows us to compare a immediate against RAX, so the slot that isn't the immediate must be colored
   * as being RAX before register allocation.
   */
  if (instruction->a->IsConstant())
  {
    instruction->b->color = RAX;
  }
  else if (instruction->b->IsConstant())
  {
    instruction->a->color = RAX;
  }
}
