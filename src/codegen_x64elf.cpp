/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cstdarg>
#include <climits>
#include <ctgmath>
#include <common.hpp>
#include <ir.hpp>
#include <elf.hpp>
#include <error.hpp>

enum reg
{
  RAX,
  RBX,
  RCX,
  RDX,
  RSP,
  RBP,
  RSI,
  RDI,
  R8,
  R9,
  R10,
  R11,
  R12,
  R13,
  R14,
  R15,
  NUM_REGISTERS
};

struct register_pimpl
{
  uint8_t opcodeOffset;
};

CodegenTarget::CodegenTarget()
  :name("x64_elf")
  ,numRegisters(16u)
  ,registerSet(new register_def[numRegisters])
  ,numGeneralRegisters(14u)
  ,generalRegisterSize(8u)
  ,numIntParamColors(6u)
  ,intParamColors(new unsigned int[numIntParamColors])
  ,functionReturnColor(RAX)
{
  intParamColors[0u] = RDI;
  intParamColors[1u] = RSI;
  intParamColors[2u] = RDX;
  intParamColors[3u] = RCX;
  intParamColors[4u] = R8;
  intParamColors[5u] = R9;

#define REGISTER(index, name, usage, modRMOffset)\
    registerSet[index] = register_def{usage, name, static_cast<register_pimpl*>(malloc(sizeof(register_pimpl)))};\
    registerSet[index].pimpl->opcodeOffset = modRMOffset;

  REGISTER(RAX, "RAX", register_def::reg_usage::GENERAL, 0u);
  REGISTER(RBX, "RBX", register_def::reg_usage::GENERAL, 3u);
  REGISTER(RCX, "RCX", register_def::reg_usage::GENERAL, 1u);
  REGISTER(RDX, "RDX", register_def::reg_usage::GENERAL, 2u);
  REGISTER(RSP, "RSP", register_def::reg_usage::SPECIAL, 4u);
  REGISTER(RBP, "RBP", register_def::reg_usage::SPECIAL, 5u);
  REGISTER(RSI, "RSI", register_def::reg_usage::GENERAL, 6u);
  REGISTER(RDI, "RDI", register_def::reg_usage::GENERAL, 7u);
  REGISTER(R8 , "R8" , register_def::reg_usage::GENERAL, 8u);
  REGISTER(R9 , "R9" , register_def::reg_usage::GENERAL, 9u);
  REGISTER(R10, "R10", register_def::reg_usage::GENERAL, 10u);
  REGISTER(R11, "R11", register_def::reg_usage::GENERAL, 11u);
  REGISTER(R12, "R12", register_def::reg_usage::GENERAL, 12u);
  REGISTER(R13, "R13", register_def::reg_usage::GENERAL, 13u);
  REGISTER(R14, "R14", register_def::reg_usage::GENERAL, 14u);
  REGISTER(R15, "R15", register_def::reg_usage::GENERAL, 15u);
}

CodegenTarget::~CodegenTarget()
{
  for (unsigned int i = 0u;
       i < numRegisters;
       i++)
  {
    delete registerSet[i].pimpl;
  }

  delete registerSet;
  delete intParamColors;
}

// TODO: precolor division instructions (stuff has to be in weird registers on x86)
void InstructionPrecolorer::Visit(LabelInstruction* /*instruction*/,     void*) { }
void InstructionPrecolorer::Visit(ReturnInstruction* /*instruction*/,    void*) { }
void InstructionPrecolorer::Visit(JumpInstruction* /*instruction*/,      void*) { }
void InstructionPrecolorer::Visit(MovInstruction* /*instruction*/,       void*) { }
void InstructionPrecolorer::Visit(UnaryOpInstruction* /*instruction*/,   void*) { }
void InstructionPrecolorer::Visit(BinaryOpInstruction* /*instruction*/,  void*) { }
void InstructionPrecolorer::Visit(CallInstruction* /*instruction*/,      void*) { }

void InstructionPrecolorer::Visit(CmpInstruction* instruction, void*)
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

/*
 * +r - add an register opcode offset to the primary opcode
 * [...] - denotes a prefix byte
 * (...) - denotes bytes that follow the opcode, in order
 */
enum class i
{
  CMP_REG_REG,          // (ModR/M)
  CMP_RAX_IMM32,        // (4-byte immediate)
  PUSH_REG,             // +r
  POP_REG,              // +r
  ADD_REG_REG,          // [opcodeSize] (ModR/M)
  SUB_REG_REG,          // [opcodeSize] (ModR/M)
  MUL_REG_REG,          // [opcodeSize] (ModR/M)
  DIV_REG_REG,          // [opcodeSize] (ModR/M)
  XOR_REG_REG,          // [opcodeSize] (ModR/M)
  ADD_REG_IMM32,        // [opcodeSize] (ModR/M [extension]) (4-byte immediate)
  SUB_REG_IMM32,        // [opcodeSize] (ModR/M [extension]) (4-byte immediate)
  MUL_REG_IMM32,        // [opcodeSize] (ModR/M [extension]) (4-byte immediate)
  DIV_REG_IMM32,        // [opcodeSize] (ModR/M [extension]) (4-byte immediate)
  MOV_REG_REG,          // [opcodeSize] (ModR/M)
  MOV_REG_IMM32,        // +r (4-byte immediate)
  MOV_REG_IMM64,        // [immSize] +r (8-byte immedite)
  MOV_REG_BASE_DISP,    // [opcodeSize] (ModR/M) (1-byte/4-byte displacement)
  INC_REG,              // (ModR/M [extension])
  DEC_REG,              // (ModR/M [extension])
  NOT_REG,              // (ModR/M [extension])
  NEG_REG,              // (ModR/M [extension])
  CALL32,               // (4-byte offset to RIP)
  INT_IMM8,             // (1-byte immediate)
  LEAVE,
  RET,
  JMP,                  // (4-byte offset to RIP)
  JE,                   // (4-byte offset to RIP)
  JNE,                  // (4-byte offset to RIP)
  JO,                   // (4-byte offset to RIP)
  JNO,                  // (4-byte offset to RIP)
  JS,                   // (4-byte offset to RIP)
  JNS,                  // (4-byte offset to RIP)
  JG,                   // (4-byte offset to RIP)
  JGE,                  // (4-byte offset to RIP)
  JL,                   // (4-byte offset to RIP)
  JLE,                  // (4-byte offset to RIP)
  JPE,                  // (4-byte offset to RIP)
  JPO,                  // (4-byte offset to RIP)
};

/*
 * --- Mod/RM bytes ---
 * A ModR/M byte is used to encode how an opcode's instructions are laid out. It is optionally accompanied
 * by an SIB byte, a one-byte or four-byte displacement and/or a four-byte immediate value.
 *
 * 7       5           2           0
 * +---+---+---+---+---+---+---+---+
 * |  mod  |    reg    |    r/m    |
 * +---+---+---+---+---+---+---+---+
 *
 * `mod` : the addressing mode to use
 *      0b00 - register indirect(r/m=register) or SIB with no displacement(r/m=0b100)
 *      0b01 - one-byte signed displacement follows
 *      0b10 - four-byte signed displacement follows
 *      0b11 - register addressing
 * `reg` : opcode offset of the destination or source register (depending on instruction's direction flag)
 * `r/m` : opcode offset of the other register
 *
 * --- SIB bytes ---
 * An SIB (Scaled Index Byte) byte is used to specify a register of the form `[rax+rbx*4+7]
 *
 * 7       5           2           0
 * +---+---+---+---+---+---+---+---+
 * | scale |   index   |    base   |
 * +---+---+---+---+---+---+---+---+
 *
 * `scale`  : how much to scale the index register's value by
 *      0b00 - x1
 *      0b01 - x2
 *      0b10 - x4
 *      0b11 - x8
 * `index`  : the index register to use
 * `base`   : the base register to use
 */
static void EmitRegisterModRM(ElfThing* thing, CodegenTarget& target, reg a, reg b)
{
  uint8_t modRM = 0b11000000; // NOTE(Isaac): use the register-direct addressing mode
  modRM |= target.registerSet[a].pimpl->opcodeOffset << 3u;
  modRM |= target.registerSet[b].pimpl->opcodeOffset;
  Emit<uint8_t>(thing, modRM);
}

/*
 * NOTE(Isaac): `scale` may be 1, 2, 4 or 8. If left out, no SIB is created.
 */
static void EmitIndirectModRM(ElfThing* thing, CodegenTarget& target, reg dest, reg base, uint32_t displacement, reg index = NUM_REGISTERS, unsigned int scale = 0u)
{
  uint8_t modRM = 0u;
  modRM |= target.registerSet[dest].pimpl->opcodeOffset << 3u;

  if (scale == 0u)
  {
    modRM |= target.registerSet[base].pimpl->opcodeOffset;
  }
  else
  {
    modRM |= 0b100;
  }

  if (displacement == 0u)
  {
    modRM |= 0b00000000;  // NOTE(Isaac): we don't need a displacement
  }
  else if (displacement >= ((2u<<7u)-1u))
  {
    modRM |= 0b10000000;  // NOTE(Isaac): we need four bytes for the displacement
  }
  else
  {
    modRM |= 0b01000000;  // NOTE(Isaac): we only need one byte for the displacement
  }

  Emit<uint8_t>(thing, modRM);

  if (scale != 0u)
  {
    uint8_t sib = 0x0;

    // NOTE(Isaac): taking the base-2 log of the scale gives the correct bit sequence... because magic
    sib |= static_cast<uint8_t>(log2(scale)) << 6u;
    sib |= target.registerSet[index].pimpl->opcodeOffset << 3u;
    sib |= target.registerSet[base].pimpl->opcodeOffset;
    Emit<uint8_t>(thing, sib);
  }

  if (displacement > 0u)
  {
    if (displacement >= ((2u<<7u)-1u))
    {
      Emit<uint32_t>(thing, displacement);
    }
    else
    {
      Emit<uint8_t>(thing, static_cast<uint8_t>(displacement));
    }
  }
}

static void EmitExtensionModRM(ElfThing* thing, CodegenTarget& target, uint8_t extension, reg r)
{
  uint8_t modRM = 0b11000000;  // NOTE(Isaac): register-direct addressing mode
  modRM |= extension << 3u;
  modRM |= target.registerSet[r].pimpl->opcodeOffset;
  Emit<uint8_t>(thing, modRM);
}

static void Emit(ErrorState& errorState, ElfThing* thing, CodegenTarget& target, i instruction, ...)
{
  va_list args;
  va_start(args, instruction);

  switch (instruction)
  {
    case i::CMP_REG_REG:
    {
      reg op1 = static_cast<reg>(va_arg(args, int));
      reg op2 = static_cast<reg>(va_arg(args, int));

      Emit<uint8_t>(thing, 0x39);
      EmitRegisterModRM(thing, target, op1, op2);
    } break;

    case i::CMP_RAX_IMM32:
    {
      uint32_t imm = va_arg(args, uint32_t);

      Emit<uint8_t>(thing, 0x3D);
      Emit<uint32_t>(thing, imm);
    } break;

    case i::PUSH_REG:
    {
      reg r = static_cast<reg>(va_arg(args, int));
      Emit<uint8_t>(thing, 0x50 + target.registerSet[r].pimpl->opcodeOffset);
    } break;

    case i::POP_REG:
    {
      reg r = static_cast<reg>(va_arg(args, int));
      Emit<uint8_t>(thing, 0x58 + target.registerSet[r].pimpl->opcodeOffset);
    } break;

    case i::ADD_REG_REG:
    {
      reg dest  = static_cast<reg>(va_arg(args, int));
      reg src   = static_cast<reg>(va_arg(args, int));

      Emit<uint8_t>(thing, 0x48);
      Emit<uint8_t>(thing, 0x01);
      EmitRegisterModRM(thing, target, src, dest);
    } break;

    case i::SUB_REG_REG:
    {
      reg dest = static_cast<reg>(va_arg(args, int));
      reg src  = static_cast<reg>(va_arg(args, int));

      Emit<uint8_t>(thing, 0x48);
      Emit<uint8_t>(thing, 0x29);
      EmitRegisterModRM(thing, target, src, dest);
    } break;

    case i::MUL_REG_REG:
    {
      reg dest = static_cast<reg>(va_arg(args, int));
      reg src  = static_cast<reg>(va_arg(args, int));

      Emit<uint8_t>(thing, 0x48);
      Emit<uint8_t>(thing, 0x0f);
      Emit<uint8_t>(thing, 0xaf);
      EmitRegisterModRM(thing, target, src, dest);
    } break;

    case i::DIV_REG_REG:
    {
      // TODO(Isaac): division is apparently a PITA, so work out what the hell to do here
      RaiseError(errorState, ICE_GENERIC, "Division is actually physically impossible on the x64");
    } break;

    case i::XOR_REG_REG:
    {
      reg dest = static_cast<reg>(va_arg(args, int));
      reg src  = static_cast<reg>(va_arg(args, int));

      Emit<uint8_t>(thing, 0x48);
      Emit<uint8_t>(thing, 0x31);
      EmitRegisterModRM(thing, target, src, dest);
    } break;

    case i::ADD_REG_IMM32:
    {
      reg result    = static_cast<reg>(va_arg(args, int));
      uint32_t imm  = va_arg(args, uint32_t);

      Emit<uint8_t>(thing, 0x48);
      Emit<uint8_t>(thing, 0x81);
      EmitExtensionModRM(thing, target, 0u, result);
      Emit<uint32_t>(thing, imm);
    } break;

    case i::SUB_REG_IMM32:
    {
      reg result    = static_cast<reg>(va_arg(args, int));
      uint32_t imm  = va_arg(args, uint32_t);

      Emit<uint8_t>(thing, 0x48);
      Emit<uint8_t>(thing, 0x81);
      EmitExtensionModRM(thing, target, 5u, result);
      Emit<uint32_t>(thing, imm);
    } break;

    case i::MUL_REG_IMM32:
    {
      reg result    = static_cast<reg>(va_arg(args, int));
      uint32_t imm  = va_arg(args, uint32_t);

      if (imm >= 256u)
      {
        RaiseError(errorState, ICE_GENERIC, "Multiplication is only supported with byte-wide immediates");
      }

      Emit<uint8_t>(thing, 0x48);
      Emit<uint8_t>(thing, 0x6b);
      EmitRegisterModRM(thing, target, result, result);
      Emit<uint8_t>(thing, static_cast<uint8_t>(imm));
    } break;

    case i::DIV_REG_IMM32:
    {
      RaiseError(errorState, ICE_GENERIC, "Division is currently deemed impossible on the x64...");
    } break;

    case i::MOV_REG_REG:
    {
      reg dest = static_cast<reg>(va_arg(args, int));
      reg src  = static_cast<reg>(va_arg(args, int));

      Emit<uint8_t>(thing, 0x48);
      Emit<uint8_t>(thing, 0x89);
      EmitRegisterModRM(thing, target, src, dest);
    } break;

    case i::MOV_REG_IMM32:
    {
      reg dest = static_cast<reg>(va_arg(args, int));
      uint32_t imm = va_arg(args, uint32_t);

      Emit<uint8_t>(thing, 0xB8 + target.registerSet[dest].pimpl->opcodeOffset);
      Emit<uint32_t>(thing, imm);
    } break;

    case i::MOV_REG_IMM64:
    {
      reg dest = static_cast<reg>(va_arg(args, int));
      uint64_t imm = va_arg(args, uint64_t);

      Emit<uint8_t>(thing, 0x48);
      Emit<uint8_t>(thing, 0xB8 + target.registerSet[dest].pimpl->opcodeOffset);
      Emit<uint64_t>(thing, imm);
    } break;

    case i::MOV_REG_BASE_DISP:
    {
      reg dest = static_cast<reg>(va_arg(args, int));
      reg base = static_cast<reg>(va_arg(args, int));
      uint32_t displacement = va_arg(args, uint32_t);

      Emit<uint8_t>(thing, 0x48);
      Emit<uint8_t>(thing, 0x8B);
      EmitIndirectModRM(thing, target, dest, base, displacement);
    } break;

    case i::INC_REG:
    {
      reg r = static_cast<reg>(va_arg(args, int));

      Emit<uint8_t>(thing, 0xFF);
      EmitExtensionModRM(thing, target, 0u, r);
    } break;

    case i::DEC_REG:
    {
      reg r = static_cast<reg>(va_arg(args, int));

      Emit<uint8_t>(thing, 0xFF);
      EmitExtensionModRM(thing, target, 1u, r);
    } break;

    case i::NOT_REG:
    {
      reg r = static_cast<reg>(va_arg(args, int));

      Emit<uint8_t>(thing, 0xF7);
      EmitExtensionModRM(thing, target, 2u, r);
    } break;

    case i::NEG_REG:
    {
      reg r = static_cast<reg>(va_arg(args, int));

      Emit<uint8_t>(thing, 0xF7);
      EmitExtensionModRM(thing, target, 3u, r);
    } break;

    case i::CALL32:
    {
      uint32_t offset = va_arg(args, uint32_t);

      Emit<uint8_t>(thing, 0xE8);
      Emit<uint32_t>(thing, offset);
    } break;

    case i::INT_IMM8:
    {
      uint8_t intNumber = static_cast<uint8_t>(va_arg(args, int));

      Emit<uint8_t>(thing, 0xCD);
      Emit<uint8_t>(thing, intNumber);
    } break;

    case i::LEAVE:
    {
      Emit<uint8_t>(thing, 0xC9);
    } break;

    case i::RET:
    {
      Emit<uint8_t>(thing, 0xC3);
    } break;

    case i::JMP:
    {
      uint32_t relAddress = va_arg(args, uint32_t);

      Emit<uint8_t>(thing, 0xE9);
      Emit<uint32_t>(thing, relAddress);
    } break;

    case i::JE:
    case i::JNE:
    case i::JO:
    case i::JNO:
    case i::JS:
    case i::JNS:
    case i::JG:
    case i::JGE:
    case i::JL:
    case i::JLE:
    case i::JPE:
    case i::JPO:
    {
      //                               JE    JNE   JO    JNO   JS    JNS   JG    JGE   JL    JLE   JPE   JPO
      static const uint8_t jumps[] = { 0x84, 0x85, 0x80, 0x81, 0x88, 0x89, 0x8F, 0x8D, 0x8C, 0x8E, 0x8A, 0x8B };

      uint32_t relAddress = va_arg(args, uint32_t);
      Emit<uint8_t>(thing, 0x0F);
      Emit<uint8_t>(thing, jumps[static_cast<unsigned int>(instruction) - static_cast<unsigned int>(i::JE)]);
      Emit<uint32_t>(thing, relAddress);
    } break;
  }

  va_end(args);
}

#define E(...) \
  Emit(errorState, thing, target, __VA_ARGS__);

static void GenerateBootstrap(ElfFile& elf, CodegenTarget& target, ElfThing* thing, ParseResult& parse)
{
  ElfSymbol* entrySymbol = nullptr;
  ErrorState errorState(ErrorState::Type::GENERAL_STUFF);

  for (ThingOfCode* thing : parse.codeThings)
  {
    if (thing->type != ThingOfCode::Type::FUNCTION)
    {
      continue;
    }

    if (thing->attribs.isEntry)
    {
      entrySymbol = thing->symbol;
      break;
    }
  }

  if (!entrySymbol)
  {
    RaiseError(errorState, ERROR_NO_ENTRY_FUNCTION);
  }

  // Clearly mark the outermost stack frame
  E(i::XOR_REG_REG, RBP, RBP);

  // Call the entry point
  E(i::CALL32, 0x0);
  new ElfRelocation(elf, thing, thing->length - sizeof(uint32_t), ElfRelocation::Type::R_X86_64_PC32, entrySymbol, -0x4);

  // Call the SYS_EXIT system call
  // The return value of Main() should be in RAX
  E(i::MOV_REG_REG, RBX, RAX);
  E(i::MOV_REG_IMM32, RAX, 1u);
  E(i::INT_IMM8, 0x80);
}

struct CodeGenerator : AirPass<void>
{
  CodeGenerator(CodegenTarget& target, ElfFile& file, ElfThing* elfThing, ThingOfCode* code, ElfThing* rodataThing)
    :AirPass()
    ,target(target)
    ,file(file)
    ,elfThing(elfThing)
    ,code(code)
    ,rodataThing(rodataThing)
  {
  }
  ~CodeGenerator() { }

  CodegenTarget&  target;
  ElfFile&        file;
  ElfThing*       elfThing;
  ThingOfCode*    code;
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

#undef E
#define E(...) \
  Emit(code->errorState, elfThing, target, __VA_ARGS__);

void CodeGenerator::Visit(LabelInstruction* instruction, void*)
{
  /*
   * This doesn't correspond to a real instruction, so we don't emit anything.
   *
   * However, we do need to know where this label lies in the stream as it's emitted, so we can refer to it
   * while doing relocations later.
   */
  instruction->offset = elfThing->length;
}

void CodeGenerator::Visit(ReturnInstruction* instruction, void*)
{
  if (instruction->returnValue)
  {
    switch (instruction->returnValue->GetType())
    {
      case SlotType::INT_CONSTANT:
      {
        E(i::MOV_REG_IMM32, RAX, dynamic_cast<ConstantSlot<int>*>(instruction->returnValue)->value);
      } break;

      case SlotType::UNSIGNED_INT_CONSTANT:
      {
        E(i::MOV_REG_IMM32, RAX, dynamic_cast<ConstantSlot<unsigned int>*>(instruction->returnValue)->value);
      } break;

      case SlotType::FLOAT_CONSTANT:
      {
        // TODO: work out how floats work
      } break;

      case SlotType::BOOL_CONSTANT:
      {
        E(i::MOV_REG_IMM32, RAX, (dynamic_cast<ConstantSlot<bool>*>(instruction->returnValue) ? 1u : 0u));
      } break;

      case SlotType::STRING_CONSTANT:
      {
        E(i::MOV_REG_IMM64, RAX, 0x00);
        new ElfRelocation(file, elfThing, elfThing->length-sizeof(uint64_t), ElfRelocation::Type::R_X86_64_64,
                          rodataThing->symbol, dynamic_cast<ConstantSlot<StringConstant*>*>(instruction->returnValue)->value->offset);
      } break;

      case SlotType::VARIABLE:
      case SlotType::PARAMETER:
      case SlotType::TEMPORARY:
      case SlotType::RETURN_RESULT:
      {
        Assert(instruction->returnValue->IsColored(), "Vars etc. need to be in registers atm");
        E(i::MOV_REG_REG, RAX, instruction->returnValue->color);
      } break;

      case SlotType::MEMBER:
      {
        MemberSlot* returnValue = dynamic_cast<MemberSlot*>(instruction->returnValue);
        Assert(returnValue->parent->IsColored(), "Parent must be in a register");
        E(i::MOV_REG_BASE_DISP, RAX, returnValue->parent->color, returnValue->member->offset);
      } break;
    }
  }

  E(i::LEAVE);
  E(i::RET);
}

void CodeGenerator::Visit(JumpInstruction* instruction, void*)
{
  switch (instruction->condition)
  {
    /*
     * TODO: The instructions we actually need to emit here depend on whether the operands of the comparison
     * were unsigned or signed. We should take this into accound
     */
    case JumpInstruction::Condition::UNCONDITIONAL:       E(i::JMP, 0x00);  break;
    case JumpInstruction::Condition::IF_EQUAL:            E(i::JE,  0x00);  break;
    case JumpInstruction::Condition::IF_NOT_EQUAL:        E(i::JNE, 0x00);  break;
    case JumpInstruction::Condition::IF_OVERFLOW:         E(i::JO,  0x00);  break;
    case JumpInstruction::Condition::IF_NOT_OVERFLOW:     E(i::JNO, 0x00);  break;
    case JumpInstruction::Condition::IF_SIGN:             E(i::JS,  0x00);  break;
    case JumpInstruction::Condition::IF_NOT_SIGN:         E(i::JNS, 0x00);  break;
    case JumpInstruction::Condition::IF_GREATER:          E(i::JG,  0x00);  break;
    case JumpInstruction::Condition::IF_GREATER_OR_EQUAL: E(i::JGE, 0x00);  break;
    case JumpInstruction::Condition::IF_LESSER:           E(i::JL,  0x00);  break;
    case JumpInstruction::Condition::IF_LESSER_OR_EQUAL:  E(i::JLE, 0x00);  break;
    case JumpInstruction::Condition::IF_PARITY_EVEN:      E(i::JPE, 0x00);  break;
    case JumpInstruction::Condition::IF_PARITY_ODD:       E(i::JPO, 0x00);  break;
  }

  new ElfRelocation(file, elfThing, elfThing->length-sizeof(uint32_t), ElfRelocation::Type::R_X86_64_PC32, code->symbol, -0x4, instruction->label);
}

void CodeGenerator::Visit(MovInstruction* instruction, void*)
{
  switch (instruction->src->GetType())
  {
    case SlotType::INT_CONSTANT:
    {
      E(i::MOV_REG_IMM32, instruction->dest->color, dynamic_cast<ConstantSlot<int>*>(instruction->src)->value);
    } break;

    case SlotType::UNSIGNED_INT_CONSTANT:
    {
      E(i::MOV_REG_IMM32, instruction->dest->color, dynamic_cast<ConstantSlot<unsigned int>*>(instruction->src)->value);
    } break;

    case SlotType::FLOAT_CONSTANT:
    {
      // TODO
    } break;

    case SlotType::BOOL_CONSTANT:
    {
      E(i::MOV_REG_IMM32, instruction->dest->color, (dynamic_cast<ConstantSlot<bool>*>(instruction->src) ? 1u : 0u));
    } break;

    case SlotType::STRING_CONSTANT:
    {
      E(i::MOV_REG_IMM64, instruction->dest->color, 0x00);
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
      E(i::MOV_REG_REG, instruction->dest->color, instruction->src->color);
    } break;
  }
}

void CodeGenerator::Visit(CmpInstruction* instruction, void*)
{
  if (instruction->a->IsColored() && instruction->b->IsColored())
  {
    E(i::CMP_REG_REG, instruction->a->color, instruction->b->color);
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
        E(i::CMP_RAX_IMM32, dynamic_cast<ConstantSlot<unsigned int>*>(immediate)->value);
      } break;

      case SlotType::INT_CONSTANT:
      {
        E(i::CMP_RAX_IMM32, dynamic_cast<ConstantSlot<int>*>(immediate)->value);
      } break;

      case SlotType::FLOAT_CONSTANT:
      {
        // TODO
      } break;

      default:
      {
        RaiseError(code->errorState, ICE_UNHANDLED_SLOT_TYPE, "SlotType", "CodeGenerator::CmpInstruction");
      } break;
    }
  }
}

void CodeGenerator::Visit(UnaryOpInstruction* instruction, void*)
{
  Assert(instruction->result->IsColored(), "Result must be in a register");
  if (instruction->operand->IsConstant())
  {
    switch (instruction->operand->GetType())
    {
      case SlotType::UNSIGNED_INT_CONSTANT:
      {
        E(i::MOV_REG_IMM32, dynamic_cast<ConstantSlot<unsigned int>*>(instruction->operand)->value);
      } break;

      case SlotType::INT_CONSTANT:
      {
        E(i::MOV_REG_IMM32, dynamic_cast<ConstantSlot<int>*>(instruction->operand)->value);
      };

      case SlotType::FLOAT_CONSTANT:
      {
        // TODO
      } break;

      default:
      {
        RaiseError(code->errorState, ICE_UNHANDLED_SLOT_TYPE, "SlotType", "CodeGenerator::UnaryOpInstruction");
      }
    }
  }
  else
  {
    E(i::MOV_REG_REG, instruction->result->color, instruction->operand->color);
  }

  switch (instruction->op)
  {
    case UnaryOpInstruction::Operation::INCREMENT:    E(i::INC_REG, instruction->result->color);  break;
    case UnaryOpInstruction::Operation::DECREMENT:    E(i::DEC_REG, instruction->result->color);  break;
    case UnaryOpInstruction::Operation::NEGATE:       E(i::NEG_REG, instruction->result->color);  break;
    case UnaryOpInstruction::Operation::LOGICAL_NOT:  E(i::NOT_REG, instruction->result->color);  break;
  }
}

void CodeGenerator::Visit(BinaryOpInstruction* instruction, void*)
{
  Assert(instruction->result->IsColored(), "Result must be in a register");
  if (instruction->left->IsConstant())
  {
    switch (instruction->left->GetType())
    {
      case SlotType::UNSIGNED_INT_CONSTANT:
      {
        E(i::MOV_REG_IMM32, dynamic_cast<ConstantSlot<unsigned int>*>(instruction->left)->value);
      } break;

      case SlotType::INT_CONSTANT:
      {
        E(i::MOV_REG_IMM32, dynamic_cast<ConstantSlot<int>*>(instruction->left)->value);
      };

      case SlotType::FLOAT_CONSTANT:
      {
        // TODO
      } break;

      default:
      {
        RaiseError(code->errorState, ICE_UNHANDLED_SLOT_TYPE, "SlotType", "CodeGenerator::BinaryOpInstruction");
      } break;
    }
  }
  else
  {
    E(i::MOV_REG_REG, instruction->result->color, instruction->left->color);
  }

  if (instruction->right->IsColored())
  {
    switch (instruction->op)
    {
      case BinaryOpInstruction::Operation::ADD:       E(i::ADD_REG_REG, instruction->result->color, instruction->right->color); break;
      case BinaryOpInstruction::Operation::SUBTRACT:  E(i::SUB_REG_REG, instruction->result->color, instruction->right->color); break;
      case BinaryOpInstruction::Operation::MULTIPLY:  E(i::MUL_REG_REG, instruction->result->color, instruction->right->color); break;
      case BinaryOpInstruction::Operation::DIVIDE:    E(i::DIV_REG_REG, instruction->result->color, instruction->right->color); break;
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
          case BinaryOpInstruction::Operation::ADD:       E(i::ADD_REG_IMM32, instruction->result->color, slot->value); break;
          case BinaryOpInstruction::Operation::SUBTRACT:  E(i::SUB_REG_IMM32, instruction->result->color, slot->value); break;
          case BinaryOpInstruction::Operation::MULTIPLY:  E(i::MUL_REG_IMM32, instruction->result->color, slot->value); break;
          case BinaryOpInstruction::Operation::DIVIDE:    E(i::DIV_REG_IMM32, instruction->result->color, slot->value); break;
        }
      } break;

      case SlotType::INT_CONSTANT:
      {
        ConstantSlot<int>* slot = dynamic_cast<ConstantSlot<int>*>(instruction->right);
        switch (instruction->op)
        {
          case BinaryOpInstruction::Operation::ADD:       E(i::ADD_REG_IMM32, instruction->result->color, slot->value); break;
          case BinaryOpInstruction::Operation::SUBTRACT:  E(i::SUB_REG_IMM32, instruction->result->color, slot->value); break;
          case BinaryOpInstruction::Operation::MULTIPLY:  E(i::MUL_REG_IMM32, instruction->result->color, slot->value); break;
          case BinaryOpInstruction::Operation::DIVIDE:    E(i::DIV_REG_IMM32, instruction->result->color, slot->value); break;
        }
      } break;

      case SlotType::FLOAT_CONSTANT:
      {
        // TODO
      } break;

      default:
      {
        RaiseError(code->errorState, ICE_UNHANDLED_SLOT_TYPE, "SlotType", "CodeGenerator::BinaryOp");
      } break;
    }
  }
}

void CodeGenerator::Visit(CallInstruction* instruction, void*)
{
  #define SAVE_REG(reg)\
    if (IsColorInUseAtPoint(code, instruction, reg))\
    {\
      E(i::PUSH_REG, reg);\
    }

  #define RESTORE_REG(reg)\
    if (IsColorInUseAtPoint(code, instruction, reg))\
    {\
      E(i::POP_REG, reg);\
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

  E(i::CALL32, 0x00);
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

static ElfThing* Generate(ElfFile& file, CodegenTarget& target, ThingOfCode* code, ElfThing* rodataThing)
{
  // NOTE(Isaac): we don't generate empty functions
  if (!(code->airHead))
  {
    return nullptr;
  }

  ElfThing* elfThing = new ElfThing(GetSection(file, ".text"), code->symbol);

  // Enter a new stack frame
  E(i::PUSH_REG, RBP);
  E(i::MOV_REG_REG, RBP, RSP);

  // Emit the instructions for the body of the thing
  CodeGenerator codeGenerator(target, file, elfThing, code, rodataThing);
  
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
    E(i::LEAVE);
    E(i::RET);
  }

  return elfThing;
}

void Generate(const char* outputPath, CodegenTarget& target, ParseResult& result)
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
  for (char* file : result.filesToLink)
  {
    LinkObject(elf, file);
  }

  // Emit string constants into the .rodata thing
  unsigned int tail = 0u;
  for (StringConstant* constant : result.strings)
  {
    constant->offset = tail;

    for (char* c = constant->string;
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
  for (ThingOfCode* thing : result.codeThings)
  {
    thing->errorState = ErrorState(ErrorState::Type::CODE_GENERATION, thing);

    /*
     * If it's a prototype, we want to reference the symbol of an already loaded (hopefully) function.
     */
    if (thing->attribs.isPrototype)
    {
      for (ElfThing* elfThing : textSection->things)
      {
        if (strcmp(thing->mangledName, elfThing->symbol->name->str) == 0)
        {
          thing->symbol = elfThing->symbol;
        }
      }

      if (!(thing->symbol))
      {
        RaiseError(thing->errorState, ERROR_UNIMPLEMENTED_PROTOTYPE, thing->mangledName);
      }
    }
    else
    {
      thing->symbol = new ElfSymbol(elf, thing->mangledName, ElfSymbol::Binding::SYM_BIND_GLOBAL, ElfSymbol::Type::SYM_TYPE_FUNCTION, textSection->index, 0x00);
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
  for (ThingOfCode* thing : result.codeThings)
  {
    if (thing->attribs.isPrototype)
    {
      continue;
    }

    Generate(elf, target, thing, rodataThing);
  }

  WriteElf(elf, outputPath);
}
