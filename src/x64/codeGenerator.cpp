/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <x64/codeGenerator.hpp>
#include <x64/emitter.hpp>

#define E(...) \
  Emit(errorState, thing, target, __VA_ARGS__);

ElfThing* CodeGenerator_x64::GenerateBootstrap(ElfThing* thing, ParseResult& parse)
{
  ElfSymbol* entrySymbol = nullptr;
  ErrorState* errorState = new ErrorState();

  /*
   * We actually iterate the entire list (even after we're found an entry point) to check that there aren't
   * multiple.
   */
  for (CodeThing* thing : parse.codeThings)
  {
    if (thing->type != CodeThing::Type::FUNCTION)
    {
      continue;
    }

    if (thing->attribs.isEntry)
    {
      if (entrySymbol)
      {
        RaiseError(errorState, ERROR_MULTIPLE_ENTRY_POINTS, entrySymbol->name->str, thing->mangledName.c_str());
      }
      entrySymbol = thing->symbol;
    }
  }

  if (!entrySymbol)
  {
    RaiseError(errorState, ERROR_NO_ENTRY_FUNCTION);
  }

  // Clearly mark the outermost stack frame
  E(I::XOR_REG_REG, RBP, RBP);

  // Call the entry point
  E(I::CALL32, 0x0);
  new ElfRelocation(file, thing, thing->length - sizeof(uint32_t), ElfRelocation::Type::R_X86_64_PC32, entrySymbol, -0x4);

  // Call the SYS_EXIT system call
  // The return value of Main() should be in RAX
  E(I::MOV_REG_REG, RBX, RAX);
  E(I::MOV_REG_IMM32, RAX, 1u);
  E(I::INT_IMM8, 0x80);

  delete errorState;
  return thing;
}

#undef E
#define E(...) \
  Emit(code->errorState, elfThing, target, __VA_ARGS__);

ElfThing* CodeGenerator_x64::Generate(CodeThing* code, ElfThing* rodataThing)
{
  // Don't generate empty functions
  if (!(code->airHead))
  {
    return nullptr;
  }

  ElfThing* elfThing = new ElfThing(GetSection(file, ".text"), code->symbol);

  // Enter a new stack frame
  E(I::PUSH_REG, RBP);
  E(I::MOV_REG_REG, RBP, RSP);

  // Allocate requested space for local variables
  if (code->neededStackSpace > 0u)
  {
    E(I::SUB_REG_IMM32, RSP, code->neededStackSpace);
  }

  // Emit the instructions for the body of the thing
  // TODO: eww
  this->code = code;
  this->elfThing = elfThing;
  this->rodataThing = rodataThing;

  for (AirInstruction* instruction = code->airHead;
       instruction;
       instruction = instruction->next)
  {
    Dispatch(instruction);
  }

  /*
   * If we should auto-return, leave the stack frame and return.
   * Otherwise, it will be done by return statements in the function's code
   */
  if (code->shouldAutoReturn)
  {
    // Clean up local variables
    if (code->neededStackSpace > 0u)
    {
      E(I::ADD_REG_IMM32, RSP, code->neededStackSpace);
    }

    E(I::LEAVE);
    E(I::RET);
  }

  return elfThing;
}

void CodeGenerator_x64::Visit(LabelInstruction* instruction, void*)
{
  /*
   * This doesn't correspond to a real instruction, so we don't emit anything.
   *
   * However, we do need to know where this label lies in the stream as it's emitted, so we can refer to it
   * while doing relocations later.
   */
  instruction->offset = elfThing->length;
}

void CodeGenerator_x64::Visit(ReturnInstruction* instruction, void*)
{
  if (instruction->returnValue)
  {
    switch (instruction->returnValue->GetType())
    {
      case SlotType::INT_CONSTANT:
      {
        E(I::MOV_REG_IMM32, RAX, dynamic_cast<ConstantSlot<int>*>(instruction->returnValue)->value);
      } break;

      case SlotType::UNSIGNED_INT_CONSTANT:
      {
        E(I::MOV_REG_IMM32, RAX, dynamic_cast<ConstantSlot<unsigned int>*>(instruction->returnValue)->value);
      } break;

      case SlotType::FLOAT_CONSTANT:
      {
        // TODO: work out how floats work
      } break;

      case SlotType::BOOL_CONSTANT:
      {
        E(I::MOV_REG_IMM32, RAX, (dynamic_cast<ConstantSlot<bool>*>(instruction->returnValue) ? 1u : 0u));
      } break;

      case SlotType::STRING_CONSTANT:
      {
        E(I::MOV_REG_IMM64, RAX, 0x00);
        new ElfRelocation(file, elfThing, elfThing->length-sizeof(uint64_t), ElfRelocation::Type::R_X86_64_64,
                          rodataThing->symbol, dynamic_cast<ConstantSlot<StringConstant*>*>(instruction->returnValue)->value->offset);
      } break;

      case SlotType::VARIABLE:
      case SlotType::PARAMETER:
      case SlotType::TEMPORARY:
      case SlotType::RETURN_RESULT:
      {
        Assert(instruction->returnValue->IsColored(), "Vars etc. need to be in registers atm");
        E(I::MOV_REG_REG, RAX, instruction->returnValue->color);
      } break;

      case SlotType::MEMBER:
      {
        MemberSlot* returnValue = dynamic_cast<MemberSlot*>(instruction->returnValue);
        Assert(returnValue->parent->IsColored(), "Parent must be in a register");
        E(I::MOV_REG_BASE_DISP, RAX, returnValue->parent->color, returnValue->member->offset);
      } break;
    }
  }

  // Clean up local variables
  if (code->neededStackSpace > 0u)
  {
    E(I::ADD_REG_IMM32, RSP, code->neededStackSpace);
  }

  E(I::LEAVE);
  E(I::RET);
}

void CodeGenerator_x64::Visit(JumpInstruction* instruction, void*)
{
  switch (instruction->condition)
  {
    /*
     * TODO: The instructions we actually need to emit here depend on whether the operands of the comparison
     * were unsigned or signed. We should take this into account
     */
    case JumpInstruction::Condition::UNCONDITIONAL:       E(I::JMP, 0x00);  break;
    case JumpInstruction::Condition::IF_EQUAL:            E(I::JE,  0x00);  break;
    case JumpInstruction::Condition::IF_NOT_EQUAL:        E(I::JNE, 0x00);  break;
    case JumpInstruction::Condition::IF_OVERFLOW:         E(I::JO,  0x00);  break;
    case JumpInstruction::Condition::IF_NOT_OVERFLOW:     E(I::JNO, 0x00);  break;
    case JumpInstruction::Condition::IF_SIGN:             E(I::JS,  0x00);  break;
    case JumpInstruction::Condition::IF_NOT_SIGN:         E(I::JNS, 0x00);  break;
    case JumpInstruction::Condition::IF_GREATER:          E(I::JG,  0x00);  break;
    case JumpInstruction::Condition::IF_GREATER_OR_EQUAL: E(I::JGE, 0x00);  break;
    case JumpInstruction::Condition::IF_LESSER:           E(I::JL,  0x00);  break;
    case JumpInstruction::Condition::IF_LESSER_OR_EQUAL:  E(I::JLE, 0x00);  break;
    case JumpInstruction::Condition::IF_PARITY_EVEN:      E(I::JPE, 0x00);  break;
    case JumpInstruction::Condition::IF_PARITY_ODD:       E(I::JPO, 0x00);  break;
  }

  new ElfRelocation(file, elfThing, elfThing->length-sizeof(uint32_t), ElfRelocation::Type::R_X86_64_PC32, code->symbol, -0x4, instruction->label);
}

void CodeGenerator_x64::Visit(MovInstruction* instruction, void*)
{
  switch (instruction->dest->GetType())
  {
    case SlotType::VARIABLE:
    case SlotType::PARAMETER:
    case SlotType::TEMPORARY:
    case SlotType::RETURN_RESULT:
    {
      Assert(instruction->dest->IsColored(), "Destination slot must be colored if it should be in a register");

      switch (instruction->src->GetType())
      {
        case SlotType::INT_CONSTANT:
        {
          E(I::MOV_REG_IMM32, instruction->dest->color, dynamic_cast<ConstantSlot<int>*>(instruction->src)->value);
        } break;

        case SlotType::UNSIGNED_INT_CONSTANT:
        {
          E(I::MOV_REG_IMM32, instruction->dest->color, dynamic_cast<ConstantSlot<unsigned int>*>(instruction->src)->value);
        } break;

        case SlotType::FLOAT_CONSTANT:
        {
          // TODO
        } break;

        case SlotType::BOOL_CONSTANT:
        {
          E(I::MOV_REG_IMM32, instruction->dest->color, dynamic_cast<ConstantSlot<bool>*>(instruction->src)->value ? 1u : 0u);
        } break;
        
        case SlotType::STRING_CONSTANT:
        {
          E(I::MOV_REG_IMM64, instruction->dest->color, 0x00);
          new ElfRelocation(file, elfThing, elfThing->length - sizeof(uint64_t), ElfRelocation::Type::R_X86_64_64,
                            rodataThing->symbol, dynamic_cast<ConstantSlot<StringConstant*>*>(instruction->src)->value->offset);
        } break;

        case SlotType::VARIABLE:
        case SlotType::PARAMETER:
        case SlotType::TEMPORARY:
        case SlotType::RETURN_RESULT:
        {
          Assert(instruction->src->IsColored(), "Source slot must be colored as it should also be in a register");
          E(I::MOV_REG_REG, instruction->dest->color, instruction->src->color);
        } break;

        case SlotType::MEMBER:
        {
          E(I::MOV_REG_BASE_DISP, instruction->dest->color, RBP, dynamic_cast<MemberSlot*>(instruction->src)->GetBasePointerOffset());
        } break;
      }
    } break;

    case SlotType::MEMBER:
    {
      MemberSlot* memberSlot = dynamic_cast<MemberSlot*>(instruction->dest);
      switch (instruction->src->GetType())
      {
        case SlotType::INT_CONSTANT:
        {
          E(I::MOV_BASE_DISP_IMM32, RBP, memberSlot->GetBasePointerOffset(), dynamic_cast<ConstantSlot<int>*>(instruction->src)->value);
        } break;

        case SlotType::UNSIGNED_INT_CONSTANT:
        {
          E(I::MOV_BASE_DISP_IMM32, RBP, memberSlot->GetBasePointerOffset(), dynamic_cast<ConstantSlot<unsigned int>*>(instruction->src)->value);
        } break;

        case SlotType::FLOAT_CONSTANT:
        {
          // TODO
        } break;

        case SlotType::BOOL_CONSTANT:
        {
          E(I::MOV_BASE_DISP_IMM32, RBP, memberSlot->GetBasePointerOffset(), dynamic_cast<ConstantSlot<bool>*>(instruction->src)->value ? 1u : 0u);
        } break;

        case SlotType::STRING_CONSTANT:
        {
          E(I::MOV_BASE_DISP_IMM64, RBP, memberSlot->GetBasePointerOffset(), 0x00);
          new ElfRelocation(file, elfThing, elfThing->length - sizeof(uint64_t), ElfRelocation::Type::R_X86_64_64,
                            rodataThing->symbol, dynamic_cast<ConstantSlot<StringConstant*>*>(instruction->src)->value->offset);
        } break;

        case SlotType::VARIABLE:
        case SlotType::PARAMETER:
        case SlotType::TEMPORARY:
        case SlotType::RETURN_RESULT:
        {
          Assert(instruction->src->IsColored(), "Source slot must be colored if it should be in a register");
          E(I::MOV_BASE_DISP_REG, RBP, memberSlot->GetBasePointerOffset(), instruction->src->color);
        } break;

        case SlotType::MEMBER:
        {
          // TODO: I don't think we can do this on x64!?!?!?
          // This should be a TargetConstraint
        } break;
      }
    } break;
    
    default:
    {
      RaiseError(ICE_GENERIC, "Can't move into slot that isn't a VARIABLE, MEMBER, PARAMETER, TEMPORARY or RETURN_RESULT!");
    } break;
  }
}

void CodeGenerator_x64::Visit(CmpInstruction* instruction, void*)
{
  if (instruction->a->IsColored() && instruction->b->IsColored())
  {
    E(I::CMP_REG_REG, instruction->a->color, instruction->b->color);
  }
  else
  {
    Slot* reg;
    Slot* immediate;

    if (instruction->a->IsConstant())
    {
      immediate = instruction->a;
      reg       = instruction->b;
    }
    else
    {
      Assert(instruction->b->IsConstant(), "Either both sides must be colored, or one must be a constant");
      immediate = instruction->b;
      reg       = instruction->a;
    }

    Assert(reg->color == RAX, "Can only compare immediate against RAX on x86");
    switch (immediate->GetType())
    {
      case SlotType::UNSIGNED_INT_CONSTANT:
      {
        E(I::CMP_RAX_IMM32, dynamic_cast<ConstantSlot<unsigned int>*>(immediate)->value);
      } break;

      case SlotType::INT_CONSTANT:
      {
        E(I::CMP_RAX_IMM32, dynamic_cast<ConstantSlot<int>*>(immediate)->value);
      } break;

      case SlotType::FLOAT_CONSTANT:
      {
        // TODO
      } break;

      default:
      {
        RaiseError(code->errorState, ICE_UNHANDLED_SLOT_TYPE, "SlotType", "CodeGenerator_x64::CmpInstruction");
      } break;
    }
  }
}

void CodeGenerator_x64::Visit(UnaryOpInstruction* instruction, void*)
{
  Assert(instruction->result->IsColored(), "Result must be in a register");
  if (instruction->operand->IsConstant())
  {
    switch (instruction->operand->GetType())
    {
      case SlotType::UNSIGNED_INT_CONSTANT:
      {
        E(I::MOV_REG_IMM32, dynamic_cast<ConstantSlot<unsigned int>*>(instruction->operand)->value);
      } break;

      case SlotType::INT_CONSTANT:
      {
        E(I::MOV_REG_IMM32, dynamic_cast<ConstantSlot<int>*>(instruction->operand)->value);
      };

      case SlotType::FLOAT_CONSTANT:
      {
        // TODO
      } break;

      default:
      {
        RaiseError(code->errorState, ICE_UNHANDLED_SLOT_TYPE, "SlotType", "CodeGenerator_x64::UnaryOpInstruction");
      }
    }
  }
  else
  {
    E(I::MOV_REG_REG, instruction->result->color, instruction->operand->color);
  }

  switch (instruction->op)
  {
    case UnaryOpInstruction::Operation::INCREMENT:    E(I::INC_REG, instruction->result->color);  break;
    case UnaryOpInstruction::Operation::DECREMENT:    E(I::DEC_REG, instruction->result->color);  break;
    case UnaryOpInstruction::Operation::NEGATE:       E(I::NEG_REG, instruction->result->color);  break;
    case UnaryOpInstruction::Operation::LOGICAL_NOT:  E(I::NOT_REG, instruction->result->color);  break;
  }
}

void CodeGenerator_x64::Visit(BinaryOpInstruction* instruction, void*)
{
  Assert(instruction->result->IsColored(), "Result must be in a register");
  if (instruction->left->IsConstant())
  {
    switch (instruction->left->GetType())
    {
      case SlotType::UNSIGNED_INT_CONSTANT:
      {
        E(I::MOV_REG_IMM32, dynamic_cast<ConstantSlot<unsigned int>*>(instruction->left)->value);
      } break;

      case SlotType::INT_CONSTANT:
      {
        E(I::MOV_REG_IMM32, dynamic_cast<ConstantSlot<int>*>(instruction->left)->value);
      };

      case SlotType::FLOAT_CONSTANT:
      {
        // TODO
      } break;

      default:
      {
        RaiseError(code->errorState, ICE_UNHANDLED_SLOT_TYPE, "SlotType", "CodeGenerator_x64::BinaryOpInstruction");
      } break;
    }
  }
  else
  {
    E(I::MOV_REG_REG, instruction->result->color, instruction->left->color);
  }

  if (instruction->right->IsColored())
  {
    switch (instruction->op)
    {
      case BinaryOpInstruction::Operation::ADD:       E(I::ADD_REG_REG, instruction->result->color, instruction->right->color); break;
      case BinaryOpInstruction::Operation::SUBTRACT:  E(I::SUB_REG_REG, instruction->result->color, instruction->right->color); break;
      case BinaryOpInstruction::Operation::MULTIPLY:  E(I::MUL_REG_REG, instruction->result->color, instruction->right->color); break;
      case BinaryOpInstruction::Operation::DIVIDE:    E(I::DIV_REG_REG, instruction->result->color, instruction->right->color); break;
    }
  }
  else
  {
    switch (instruction->right->GetType())
    {
      case SlotType::UNSIGNED_INT_CONSTANT:
      {
        ConstantSlot<unsigned int>* slot = dynamic_cast<ConstantSlot<unsigned int>*>(instruction->right);
        switch (instruction->op)
        {
          case BinaryOpInstruction::Operation::ADD:       E(I::ADD_REG_IMM32, instruction->result->color, slot->value); break;
          case BinaryOpInstruction::Operation::SUBTRACT:  E(I::SUB_REG_IMM32, instruction->result->color, slot->value); break;
          case BinaryOpInstruction::Operation::MULTIPLY:  E(I::MUL_REG_IMM32, instruction->result->color, slot->value); break;
          case BinaryOpInstruction::Operation::DIVIDE:    E(I::DIV_REG_IMM32, instruction->result->color, slot->value); break;
        }
      } break;

      case SlotType::INT_CONSTANT:
      {
        ConstantSlot<int>* slot = dynamic_cast<ConstantSlot<int>*>(instruction->right);
        switch (instruction->op)
        {
          case BinaryOpInstruction::Operation::ADD:       E(I::ADD_REG_IMM32, instruction->result->color, slot->value); break;
          case BinaryOpInstruction::Operation::SUBTRACT:  E(I::SUB_REG_IMM32, instruction->result->color, slot->value); break;
          case BinaryOpInstruction::Operation::MULTIPLY:  E(I::MUL_REG_IMM32, instruction->result->color, slot->value); break;
          case BinaryOpInstruction::Operation::DIVIDE:    E(I::DIV_REG_IMM32, instruction->result->color, slot->value); break;
        }
      } break;

      case SlotType::FLOAT_CONSTANT:
      {
        // TODO
      } break;

      default:
      {
        RaiseError(code->errorState, ICE_UNHANDLED_SLOT_TYPE, "SlotType", "CodeGenerator_x64::BinaryOp");
      } break;
    }
  }
}

void CodeGenerator_x64::Visit(CallInstruction* instruction, void*)
{
  #define SAVE_REG(reg)\
    if (IsColorInUseAtPoint(code, instruction, reg))\
    {\
      E(I::PUSH_REG, reg);\
    }

  #define RESTORE_REG(reg)\
    if (IsColorInUseAtPoint(code, instruction, reg))\
    {\
      E(I::POP_REG, reg);\
    }

  /*
   * These are the registers that must be saved by the caller (if it cares about their contents).
   * NOTE(Isaac): RSP is technically caller-saved, but functions shouldn't leave anything on the stack unless
   * they're specifically meant to, so we don't need to (or occasionally spefically don't want to) restore it.
   */
  SAVE_REG(RAX);
  SAVE_REG(RCX);
  SAVE_REG(RDX);
  SAVE_REG(RSI);
  SAVE_REG(RDI);
  SAVE_REG(R8 );
  SAVE_REG(R9 );
  SAVE_REG(R10);
  SAVE_REG(R11);

  E(I::CALL32, 0x00);
  new ElfRelocation(file, elfThing, elfThing->length-sizeof(uint32_t), ElfRelocation::Type::R_X86_64_PC32, instruction->thing->symbol, -0x4);

  RESTORE_REG(R11);
  RESTORE_REG(R10);
  RESTORE_REG(R9 );
  RESTORE_REG(R8 );
  RESTORE_REG(RDI);
  RESTORE_REG(RSI);
  RESTORE_REG(RDX);
  RESTORE_REG(RCX);
  RESTORE_REG(RAX);

  #undef SAVE_REG
  #undef RESTORE_REG
}
#undef E
