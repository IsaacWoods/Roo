/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cassert>
#include <cstdarg>
#include <climits>
#include <ctgmath>
#include <common.hpp>
#include <ir.hpp>
#include <air.hpp>
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

static type_def* CreateInbuiltType(const char* name, unsigned int size)
{
  type_def* type = static_cast<type_def*>(malloc(sizeof(type_def)));
  type->name = static_cast<char*>(malloc(sizeof(char) * strlen(name) + 1u));
  strcpy(type->name, name);
  InitVector<variable_def*>(type->members);
  type->size = size;

  return type;
}

void InitCodegenTarget(parse_result& parseResult, codegen_target& target)
{
  target.name = "x64_elf";
  target.numRegisters = 16u;
  target.registerSet = static_cast<register_def*>(malloc(sizeof(register_def) * target.numRegisters));
  target.generalRegisterSize = 8u;

  target.numIntParamColors = 6u;
  target.intParamColors = static_cast<unsigned int*>(malloc(sizeof(unsigned int) * target.numIntParamColors));
  target.intParamColors[0] = RDI;
  target.intParamColors[1] = RSI;
  target.intParamColors[2] = RDX;
  target.intParamColors[3] = RCX;
  target.intParamColors[4] = R8;
  target.intParamColors[5] = R9;

  target.functionReturnColor = RAX;

  #define REGISTER(index, name, usage, modRMOffset) \
    target.registerSet[index] = register_def{usage, name, static_cast<register_pimpl*>(malloc(sizeof(register_pimpl)))}; \
    target.registerSet[index].pimpl->opcodeOffset = modRMOffset;

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

  // Add inbuilt types
  Add<type_def*>(parseResult.types, CreateInbuiltType("int",    4u));
  Add<type_def*>(parseResult.types, CreateInbuiltType("uint",   4u));
  Add<type_def*>(parseResult.types, CreateInbuiltType("float",  4u));
  Add<type_def*>(parseResult.types, CreateInbuiltType("u8",     1u));
  Add<type_def*>(parseResult.types, CreateInbuiltType("s8",     1u));
  Add<type_def*>(parseResult.types, CreateInbuiltType("u16",    2u));
  Add<type_def*>(parseResult.types, CreateInbuiltType("s16",    2u));
  Add<type_def*>(parseResult.types, CreateInbuiltType("u32",    4u));
  Add<type_def*>(parseResult.types, CreateInbuiltType("s32",    4u));
  Add<type_def*>(parseResult.types, CreateInbuiltType("u64",    8u));
  Add<type_def*>(parseResult.types, CreateInbuiltType("s64",    8u));
  Add<type_def*>(parseResult.types, CreateInbuiltType("char",   1u));
}

template<>
void Free<codegen_target>(codegen_target& target)
{
  for (unsigned int i = 0u;
       i < target.numRegisters;
       i++)
  {
    free(target.registerSet[i].pimpl);
  }

  free(target.registerSet);
  free(target.intParamColors);
}

/*
 * This is used by the AIR generation system to allow us to deal with all the weird bits of the x86_64 ISA.
 */
void PrecolorInstruction(codegen_target& /*target*/, air_instruction* instruction)
{
  error_state errorState = CreateErrorState(GENERAL_STUFF);

  switch (instruction->type)
  {
    case I_RETURN:
    {
    } break;

    case I_JUMP:
    {
    } break;

    case I_MOV:
    {
    } break;

    case I_CMP:
    {
      /*
       * NOTE(Isaac): We should be able to assume that they're not both immediate values, because that should be
       * dealt with by the constant folder.
       */
      if (instruction->slotPair.left->type == SIGNED_INT_CONSTANT   ||
          instruction->slotPair.left->type == UNSIGNED_INT_CONSTANT ||
          instruction->slotPair.left->type == FLOAT_CONSTANT        ||
          instruction->slotPair.left->type == STRING_CONSTANT)
      {
        instruction->slotPair.right->color = RAX;
      }
      else if ( instruction->slotPair.right->type == SIGNED_INT_CONSTANT  ||
                instruction->slotPair.right->type == UNSIGNED_INT_CONSTANT ||
                instruction->slotPair.right->type == FLOAT_CONSTANT        ||
                instruction->slotPair.right->type == STRING_CONSTANT)
      {
        instruction->slotPair.left->color = RAX;
      }
    } break;

    case I_BINARY_OP:
    {
      switch (instruction->binaryOp.operation)
      {
        case binary_op_i::op::ADD_I:
        {
        } break;

        case binary_op_i::op::SUB_I:
        {
        } break;

        case binary_op_i::op::MUL_I:
        {
        } break;

        case binary_op_i::op::DIV_I:
        {
          printf("Precoloring DIV_I operator\n");
        } break;

        default:
        {
          // TODO: get a string of the actual operator in the binary_op_i
          RaiseError(errorState, ICE_UNHANDLED_OPERATOR, "operator", "PrecolorInstruction:X64:I_BINARY_OP");
        } break;
      }
    } break;

    case I_INC:
    {
    } break;

    case I_DEC:
    {
    } break;

    case I_CALL:
    {
    } break;

    case I_LABEL:
    {
    } break;

    case I_NUM_INSTRUCTIONS:
    {
      RaiseError(errorState, ICE_UNHANDLED_INSTRUCTION_TYPE, GetInstructionName(instruction), "PrecolorInstruction:X86_64");
    }
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
static void EmitRegisterModRM(elf_thing* thing, codegen_target& target, reg a, reg b)
{
  uint8_t modRM = 0b11000000; // NOTE(Isaac): use the register-direct addressing mode
  modRM |= target.registerSet[a].pimpl->opcodeOffset << 3u;
  modRM |= target.registerSet[b].pimpl->opcodeOffset;
  Emit<uint8_t>(thing, modRM);
}

/*
 * NOTE(Isaac): `scale` may be 1, 2, 4 or 8. If left out, no SIB is created.
 */
static void EmitIndirectModRM(elf_thing* thing, codegen_target& target, reg dest, reg base, uint32_t displacement, reg index = NUM_REGISTERS, unsigned int scale = 0u)
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

static void EmitExtensionModRM(elf_thing* thing, codegen_target& target, uint8_t extension, reg r)
{
  uint8_t modRM = 0b11000000;  // NOTE(Isaac): register-direct addressing mode
  modRM |= extension << 3u;
  modRM |= target.registerSet[r].pimpl->opcodeOffset;
  Emit<uint8_t>(thing, modRM);
}

static void Emit(error_state& errorState, elf_thing* thing, codegen_target& target, i instruction, ...)
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

static void GenerateBootstrap(elf_file& elf, codegen_target& target, elf_thing* thing, parse_result& parse)
{
  elf_symbol* entrySymbol = nullptr;
  error_state errorState = CreateErrorState(GENERAL_STUFF);

  for (auto* it = parse.codeThings.head;
       it < parse.codeThings.tail;
       it++)
  {
    thing_of_code* thing = *it;

    if (thing->type != thing_type::FUNCTION)
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
  CreateRelocation(elf, thing, thing->length - sizeof(uint32_t), R_X86_64_PC32, entrySymbol, -0x4);

  // Call the SYS_EXIT system call
  E(i::MOV_REG_IMM32, RAX, 1u);
  E(i::XOR_REG_REG, RBX, RBX);
  E(i::INT_IMM8, 0x80);
}

#undef E
#define E(...) \
  Emit(code->errorState, thing, target, __VA_ARGS__);

static elf_thing* Generate(elf_file& elf, codegen_target& target, thing_of_code* code)
{
  // NOTE(Isaac): we don't generate empty functions
  if (!(code->airHead))
  {
    return nullptr;
  }

  elf_thing* thing = CreateThing(elf, code->symbol);

  // Enter a new stack frame
  E(i::PUSH_REG, RBP);
  E(i::MOV_REG_REG, RBP, RSP);

  for (air_instruction* instruction = code->airHead;
       instruction;
       instruction = instruction->next)
  {
    switch (instruction->type)
    {
      case I_RETURN:
      {
        if (instruction->slot)
        {
          switch (instruction->slot->type)
          {
            case slot_type::SIGNED_INT_CONSTANT:
            {
              E(i::MOV_REG_IMM32, RAX, instruction->slot->i);
            } break;

            case slot_type::UNSIGNED_INT_CONSTANT:
            {
              E(i::MOV_REG_IMM32, RAX, instruction->slot->u);
            } break;

            case slot_type::STRING_CONSTANT:
            {
              E(i::MOV_REG_IMM64, RAX, 0x0);
              CreateRelocation(elf, thing, thing->length - sizeof(uint64_t), R_X86_64_64, elf.rodataThing->symbol, instruction->slot->string->offset);
            } break;

            case slot_type::VARIABLE:
            case slot_type::PARAMETER:
            case slot_type::TEMPORARY:
            case slot_type::RETURN_RESULT:
            {
              assert(instruction->slot->color != -1);
              E(i::MOV_REG_REG, RAX, instruction->slot->color);
            } break;

            case slot_type::MEMBER:
            {
              assert(instruction->slot->member.parent->color != -1);
              E(i::MOV_REG_BASE_DISP, RAX, instruction->slot->member.parent->color, instruction->slot->member.memberVar->offset);
            } break;

            default:
            {
              RaiseError(code->errorState, ICE_UNHANDLED_SLOT_TYPE, GetSlotString(instruction->slot), "Generate_X64::I_RETURN");
            } break;
          }
        }

        E(i::LEAVE);
        E(i::RET);
      } break;

      case I_JUMP:
      {
        jump_i& jump = instruction->jump;

        switch (jump.cond)
        {
          // TODO: the instructions we use for greater, greater or equal, less and less or equal depend
          // on whether the operands are signed or unsigned - take this into account
          
          /*
           * NOTE(Isaac): Because we're jumping to a label we don't have an address for yet,
           * we're emitting 0 and adding a relocation to do it later
           */
          case jump_i::condition::UNCONDITIONAL:        E(i::JMP,   0x00); break;
          case jump_i::condition::IF_EQUAL:             E(i::JE,    0x00); break;
          case jump_i::condition::IF_NOT_EQUAL:         E(i::JNE,   0x00); break;
          case jump_i::condition::IF_OVERFLOW:          E(i::JO,    0x00); break;
          case jump_i::condition::IF_NOT_OVERFLOW:      E(i::JNO,   0x00); break;
          case jump_i::condition::IF_SIGN:              E(i::JS,    0x00); break;
          case jump_i::condition::IF_NOT_SIGN:          E(i::JNS,   0x00); break;
          case jump_i::condition::IF_GREATER:           E(i::JG,    0x00); break;
          case jump_i::condition::IF_GREATER_OR_EQUAL:  E(i::JGE,   0x00); break;
          case jump_i::condition::IF_LESSER:            E(i::JL,    0x00); break;
          case jump_i::condition::IF_LESSER_OR_EQUAL:   E(i::JLE,   0x00); break;
          case jump_i::condition::IF_PARITY_EVEN:       E(i::JPE,   0x00); break;
          case jump_i::condition::IF_PARITY_ODD:        E(i::JPO,   0x00); break;
        }

        CreateRelocation(elf, thing, thing->length - sizeof(uint32_t), R_X86_64_PC32, thing->symbol, -0x4, instruction->jump.label);
      } break;

      case I_MOV:
      {
        mov_i& mov = instruction->mov;

        if (mov.src->type == slot_type::SIGNED_INT_CONSTANT)
        {
          E(i::MOV_REG_IMM32, mov.dest->color, mov.src->i);
        }
        else if (mov.src->type == slot_type::UNSIGNED_INT_CONSTANT)
        {
          E(i::MOV_REG_IMM32, mov.dest->color, mov.src->u);
        }
        else if (mov.src->type == slot_type::STRING_CONSTANT)
        {
          E(i::MOV_REG_IMM64, mov.dest->color, 0x0);
          CreateRelocation(elf, thing, thing->length - sizeof(uint64_t), R_X86_64_64, elf.rodataThing->symbol, mov.src->string->offset);
        }
        else
        {
          // NOTE(Isaac): if we're here, `src` should be colored
          assert(mov.src->color != -1);

          E(i::MOV_REG_REG, mov.dest->color, mov.src->color);
        }
      } break;

      case I_CMP:
      {
        slot_pair& pair = instruction->slotPair;

        if ((pair.left->color != -1) && (pair.right->color != -1))
        {
          E(i::CMP_REG_REG, pair.left->color, pair.right->color);
        }
        else
        {
          // TODO: precolor compare instructions with immediates, because the variable needs to be in RAX
          if (pair.left->color != -1)
          {
            if (pair.left->color != RAX)
            {
              RaiseError(code->errorState, ICE_GENERIC, "There's only an x86 instruction for comparing an immediate with RAX!");
            }

            E(i::CMP_RAX_IMM32, pair.right->i);
          }
        }
      } break;

      case I_BINARY_OP:
      {
        binary_op_i& op = instruction->binaryOp;

        if (op.left->color != -1)
        {
          E(i::MOV_REG_REG, op.result->color, op.left->color);
        }
        else
        {
          E(i::MOV_REG_IMM32, op.result->color, op.left->i);
        }

        if (op.right->color != -1)
        {
          switch (op.operation)
          {
            case binary_op_i::op::ADD_I: E(i::ADD_REG_REG, op.result->color, op.right->color); break;
            case binary_op_i::op::SUB_I: E(i::SUB_REG_REG, op.result->color, op.right->color); break;
            case binary_op_i::op::MUL_I: E(i::MUL_REG_REG, op.result->color, op.right->color); break;
            case binary_op_i::op::DIV_I: E(i::DIV_REG_REG, op.result->color, op.right->color); break;
          }
        }
        else
        {
          switch (op.operation)
          {
            case binary_op_i::op::ADD_I: E(i::ADD_REG_IMM32, op.result->color, op.right->i); break;
            case binary_op_i::op::SUB_I: E(i::SUB_REG_IMM32, op.result->color, op.right->i); break;
            case binary_op_i::op::MUL_I: E(i::MUL_REG_IMM32, op.result->color, op.right->i); break;
            case binary_op_i::op::DIV_I: E(i::DIV_REG_IMM32, op.result->color, op.right->i); break;
          }
        }
      } break;

      case I_INC:
      {
        assert(instruction->slot->color != -1);
        E(i::INC_REG, instruction->slot->color);
      } break;

      case I_DEC:
      {
        assert(instruction->slot->color != -1);
        E(i::DEC_REG, instruction->slot->color);
      } break;

      case I_CALL:
      {

        #define SAVE_REG(reg) \
          if (IsColorInUseAtPoint(code, instruction, reg)) \
          { \
            E(i::PUSH_REG, reg); \
          }

        #define RESTORE_REG(reg) \
          if (IsColorInUseAtPoint(code, instruction, reg)) \
          { \
            E(i::POP_REG, reg); \
          }

        /*
         * These are the registers that must be saved by the caller if it cares about their contents
         * NOTE(Isaac): While RSP is caller-saved, we don't care about its contents
         */
        SAVE_REG(RAX)
        SAVE_REG(RCX)
        SAVE_REG(RDX)
        SAVE_REG(RSI)
        SAVE_REG(RDI)
        SAVE_REG(R8)
        SAVE_REG(R9)
        SAVE_REG(R10)
        SAVE_REG(R11)

        // NOTE(Isaac): yeah I don't know why we need an addend of -0x4 in the relocation, but we do (probably should work that out)
        E(i::CALL32, 0x0);
        CreateRelocation(elf, thing, thing->length - sizeof(uint32_t), R_X86_64_PC32, instruction->call->symbol, -0x4);

        // NOTE(Isaac): We restore the saved registers in the reverse order to match the stack's layout
        RESTORE_REG(R11)
        RESTORE_REG(R10)
        RESTORE_REG(R9)
        RESTORE_REG(R8)
        RESTORE_REG(RDI)
        RESTORE_REG(RSI)
        RESTORE_REG(RDX)
        RESTORE_REG(RCX)
        RESTORE_REG(RAX)

        #undef SAVE_REGISTER
        #undef RESTORE_REGISTER
      } break;

      case I_LABEL:
      {
        instruction->label->offset = thing->length;
      } break;

      case I_NUM_INSTRUCTIONS:
      {
        RaiseError(code->errorState, ICE_GENERIC, "AIR instruction of type I_NUM_INSTRUCTIONS in code generator");
      }
    }
  }

  // If we should auto-return, leave the stack frame and return
  if (code->shouldAutoReturn)
  {
    E(i::LEAVE);
    E(i::RET);
  }

  return thing;
}

void Generate(const char* outputPath, codegen_target& target, parse_result& result)
{
  elf_file elf;
  CreateElf(elf, target);

  elf_segment* loadSegment = CreateSegment(elf, PT_LOAD, SEGMENT_ATTRIB_X | SEGMENT_ATTRIB_R, 0x400000, 0x200000);
  loadSegment->offset = 0x00;
  loadSegment->size.inFile = 0x40;  // NOTE(Isaac): set the tail to the end of the ELF header

  CreateSection(elf, ".text",   SHT_PROGBITS, 0x10)->flags = SECTION_ATTRIB_A | SECTION_ATTRIB_E;
  CreateSection(elf, ".rodata", SHT_PROGBITS, 0x04)->flags = SECTION_ATTRIB_A;
  CreateSection(elf, ".strtab", SHT_STRTAB,   0x04);
  CreateSection(elf, ".symtab", SHT_SYMTAB,   0x04);

  GetSection(elf, ".symtab")->link = GetSection(elf, ".strtab")->index;
  GetSection(elf, ".symtab")->entrySize = 0x18;

  MapSection(elf, loadSegment, GetSection(elf, ".text"));
  MapSection(elf, loadSegment, GetSection(elf, ".rodata"));

  // Create a symbol to reference the .rodata section with
  elf.rodataThing = CreateRodataThing(elf);

  // --- TEMPORARY TESTING STUFF AND THINGS ---
  LinkObject(elf, "./std/io.o");
  // ---
 
  // TODO: link external object files we need here

  // Emit string constants into the .rodata thing
  unsigned int tail = 0u;
  for (auto* it = result.strings.head;
       it < result.strings.tail;
       it++)
  {
    string_constant* constant = *it;
    constant->offset = tail;

    for (char* c = constant->string;
         *c;
         c++)
    {
      Emit<uint8_t>(elf.rodataThing, *c);
      tail++;    
    }

    // Add a null terminator
    Emit<uint8_t>(elf.rodataThing, '\0');
    tail++;
  }

  // --- Generate error states and symbols for things of code ---
  for (auto* it = result.codeThings.head;
       it < result.codeThings.tail;
       it++)
  {
    thing_of_code* thing = *it;
    thing->errorState = CreateErrorState(CODE_GENERATION, thing);

    /*
     * If it's a prototype, we want to reference the symbol of an already loaded (hopefully) function.
     */
    if (thing->attribs.isPrototype)
    {
      for (auto* thingIt = elf.things.head;
           thingIt < elf.things.tail;
           thingIt++)
      {
        elf_thing* elfThing = *thingIt;

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
      thing->symbol = CreateSymbol(elf, thing->mangledName, SYM_BIND_GLOBAL, SYM_TYPE_FUNCTION, GetSection(elf, ".text")->index, 0x00);
    }
  }

  // --- Create a thing for the bootstrap ---
  elf_symbol* bootstrapSymbol = CreateSymbol(elf, "_start", SYM_BIND_GLOBAL, SYM_TYPE_FUNCTION, GetSection(elf, ".text")->index, 0x00);
  elf_thing* bootstrapThing = CreateThing(elf, bootstrapSymbol);
  GenerateBootstrap(elf, target, bootstrapThing, result);

  // --- Generate `elf_thing`s for each thing of code ---
  for (auto* it = result.codeThings.head;
       it < result.codeThings.tail;
       it++)
  {
    thing_of_code* thing = *it;

    if (thing->attribs.isPrototype)
    {
      continue;
    }

    Generate(elf, target, thing);
  }

  WriteElf(elf, outputPath);
  Free<elf_file>(elf);
}
