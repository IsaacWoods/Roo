/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <x64/codeGenerator.hpp>
#include <x64/emitter.hpp>

#define E(...) \
  Emit(errorState, thing, target, __VA_ARGS__);

static void GenerateBootstrap(ElfFile& elf, CodegenTarget& target, ElfThing* thing, ParseResult& parse)
{
  ElfSymbol* entrySymbol = nullptr;
  ErrorState errorState(ErrorState::Type::GENERAL_STUFF);

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
  new ElfRelocation(elf, thing, thing->length - sizeof(uint32_t), ElfRelocation::Type::R_X86_64_PC32, entrySymbol, -0x4);

  // Call the SYS_EXIT system call
  // The return value of Main() should be in RAX
  E(I::MOV_REG_REG, RBX, RAX);
  E(I::MOV_REG_IMM32, RAX, 1u);
  E(I::INT_IMM8, 0x80);
}

#undef E
#define E(...) \
  Emit(code->errorState, elfThing, target, __VA_ARGS__);

static ElfThing* Generate(ElfFile& file, CodegenTarget& target, CodeThing* code, ElfThing* rodataThing)
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

  // Emit the instructions for the body of the thing
  CodeGenerator_x64 codeGenerator(target, file, elfThing, code, rodataThing);
  
  for (AirInstruction* instruction = code->airHead;
       instruction;
       instruction = instruction->next)
  {
    codeGenerator.Dispatch(instruction);
  }

  /*
   * If we should auto-return, leave the stack frame and return.
   * Otherwise, it will be done by return statements in the function's code
   */
  if (code->shouldAutoReturn)
  {
    E(I::LEAVE);
    E(I::RET);
  }

  return elfThing;
}

void Generate(const std::string& outputPath, CodegenTarget& target, ParseResult& result)
{
  /*
   * If we're compiling a module, we need to produce a relocatable.
   * If not, we want to produce a normal executable.
   */
  ElfFile elf(target, result.isModule);

  // .text
  ElfSection* textSection = new ElfSection(elf, ".text", ElfSection::Type::SHT_PROGBITS, 0x10);
  textSection->flags = SECTION_ATTRIB_A|SECTION_ATTRIB_E;

  // .rodata
  ElfSection* rodataSection = new ElfSection(elf, ".rodata", ElfSection::Type::SHT_PROGBITS, 0x04);
  rodataSection->flags = SECTION_ATTRIB_A;

  // .strtab
  ElfSection* stringTableSection = new ElfSection(elf, ".strtab", ElfSection::Type::SHT_STRTAB, 0x04);

  // .symtab
  ElfSection* symbolTableSection = new ElfSection(elf, ".symtab", ElfSection::Type::SHT_SYMTAB, 0x04);
  symbolTableSection->link = stringTableSection->index;
  symbolTableSection->entrySize = 0x18;

  // Create an ElfThing to put the contents of .rodata into
  ElfSymbol* rodataSymbol = new ElfSymbol(elf, nullptr, ElfSymbol::Binding::SYM_BIND_GLOBAL, ElfSymbol::Type::SYM_TYPE_SECTION, rodataSection->index, 0x00);
  ElfThing* rodataThing = new ElfThing(rodataSection, rodataSymbol);

  if (!(result.isModule))
  {
    ElfSegment* loadSegment = new ElfSegment(elf, ElfSegment::Type::PT_LOAD, SEGMENT_ATTRIB_X|SEGMENT_ATTRIB_R, 0x400000, 0x200000);
    loadSegment->offset = 0x00;
    loadSegment->size.inFile = 0x40;  // NOTE(Isaac): set the tail to the end of the ELF header

    MapSection(elf, loadSegment, textSection);
    MapSection(elf, loadSegment, rodataSection);
  }

  // Link with any files we've been told to
  for (const std::string& file : result.filesToLink)
  {
    // TODO: eww use std::string throughout
    LinkObject(elf, file.c_str());
  }

  // Emit string constants into the .rodata thing
  unsigned int tail = 0u;
  for (StringConstant* constant : result.strings)
  {
    constant->offset = tail;

    for (const char* c = constant->str.c_str();
         *c;
         c++)
    {
      Emit<uint8_t>(rodataThing, *c);
      tail++;
    }

    // Add a null terminator
    Emit<uint8_t>(rodataThing, '\0');
    tail++;
  }

  // --- Generate error states and symbols for things of code ---
  for (CodeThing* thing : result.codeThings)
  {
    thing->errorState = ErrorState(ErrorState::Type::CODE_GENERATION, thing);

    /*
     * If it's a prototype, we want to reference the symbol of an already loaded (hopefully) function.
     */
    if (thing->attribs.isPrototype)
    {
      for (ElfThing* elfThing : textSection->things)
      {
        if (thing->mangledName == elfThing->symbol->name->str)
        {
          thing->symbol = elfThing->symbol;
        }
      }

      if (!(thing->symbol))
      {
        RaiseError(thing->errorState, ERROR_UNIMPLEMENTED_PROTOTYPE, thing->mangledName.c_str());
      }
    }
    else
    {
      thing->symbol = new ElfSymbol(elf, thing->mangledName.c_str(), ElfSymbol::Binding::SYM_BIND_GLOBAL, ElfSymbol::Type::SYM_TYPE_FUNCTION, textSection->index, 0x00);
    }
  }

  // --- Create a thing for the bootstrap, if this isn't a module ---
  if (!(result.isModule))
  {
    ElfSymbol* bootstrapSymbol = new ElfSymbol(elf, "_start", ElfSymbol::Binding::SYM_BIND_GLOBAL, ElfSymbol::Type::SYM_TYPE_FUNCTION, textSection->index, 0x00);
    ElfThing* bootstrapThing = new ElfThing(textSection, bootstrapSymbol);
    GenerateBootstrap(elf, target, bootstrapThing, result);
  }

  // --- Generate `ElfThing`s for each thing of code ---
  for (CodeThing* thing : result.codeThings)
  {
    if (thing->attribs.isPrototype)
    {
      continue;
    }

    Generate(elf, target, thing, rodataThing);
  }

  WriteElf(elf, outputPath.c_str());
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

  E(I::LEAVE);
  E(I::RET);
}

void CodeGenerator_x64::Visit(JumpInstruction* instruction, void*)
{
  switch (instruction->condition)
  {
    /*
     * TODO: The instructions we actually need to emit here depend on whether the operands of the comparison
     * were unsigned or signed. We should take this into accound
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
      E(I::MOV_REG_IMM32, instruction->dest->color, (dynamic_cast<ConstantSlot<bool>*>(instruction->src) ? 1u : 0u));
    } break;

    case SlotType::STRING_CONSTANT:
    {
      E(I::MOV_REG_IMM64, instruction->dest->color, 0x00);
      new ElfRelocation(file, elfThing, elfThing->length - sizeof(uint64_t), ElfRelocation::Type::R_X86_64_64,
                        rodataThing->symbol, dynamic_cast<ConstantSlot<StringConstant*>*>(instruction->src)->value->offset);
    } break;

    case SlotType::VARIABLE:
    case SlotType::PARAMETER:
    case SlotType::MEMBER:
    case SlotType::TEMPORARY:
    case SlotType::RETURN_RESULT:
    {
      Assert(instruction->src->IsColored(), "Should be in a register");
      E(I::MOV_REG_REG, instruction->dest->color, instruction->src->color);
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
