/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <x64/emitter.hpp>
#include <cstdint>
#include <ctgmath>
#include <cstdarg>
#include <elf/elf.hpp>

/*
 * --- Mod/RM bytes ---
 * A ModR/M byte is used to encode how an opcode's instructions are laid out. It is optionally accompanied
 * by an SIB, a one-byte or four-byte displacement and/or a four-byte immediate value.
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
 * --- SIBs ---
 * An SIB (Scaled Index Byte) is used to specify an address of the form `[rax+rbx*4+7]
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
static void EmitRegisterModRM(ElfThing* thing, TargetMachine* target, Reg a, Reg b)
{
  uint8_t modRM = 0b11000000; // NOTE(Isaac): use the register-direct addressing mode
  modRM |= target->registerSet[a].pimpl->opcodeOffset << 3u;
  modRM |= target->registerSet[b].pimpl->opcodeOffset;
  Emit<uint8_t>(thing, modRM);
}

/*
 * NOTE(Isaac): `scale` may be 1, 2, 4 or 8. If left out, no SIB is created.
 */
static void EmitIndirectModRM(ElfThing* thing, TargetMachine* target, Reg reg, Reg base, uint32_t displacement,
                              Reg index = NUM_REGISTERS, unsigned int scale = 0u)
{
  uint8_t modRM = 0u;
  modRM |= target->registerSet[reg].pimpl->opcodeOffset << 3u;

  if (scale == 0u)
  {
    modRM |= target->registerSet[base].pimpl->opcodeOffset;
  }
  else
  {
    modRM |= 0b100;
  }

  if (displacement >= ((2u<<7u)-1u))
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
    sib |= target->registerSet[index].pimpl->opcodeOffset << 3u;
    sib |= target->registerSet[base].pimpl->opcodeOffset;
    Emit<uint8_t>(thing, sib);
  }

  if (displacement >= ((2u<<7u)-1u))
  {
    Emit<uint32_t>(thing, displacement);
  }
  else
  {
    Emit<uint8_t>(thing, static_cast<uint8_t>(displacement));
  }
}

static void EmitExtensionModRM(ElfThing* thing, TargetMachine* target, uint8_t extension, Reg r)
{
  uint8_t modRM = 0b11000000;  // NOTE(Isaac): register-direct addressing mode
  modRM |= extension << 3u;
  modRM |= target->registerSet[r].pimpl->opcodeOffset;
  Emit<uint8_t>(thing, modRM);
}

void Emit(ErrorState& errorState, ElfThing* thing, TargetMachine* target, I instruction, ...)
{
  va_list args;
  va_start(args, instruction);

  switch (instruction)
  {
    case I::CMP_REG_REG:
    {
      Reg op1 = static_cast<Reg>(va_arg(args, int));
      Reg op2 = static_cast<Reg>(va_arg(args, int));

      Emit<uint8_t>(thing, 0x39);
      EmitRegisterModRM(thing, target, op1, op2);
    } break;

    case I::CMP_RAX_IMM32:
    {
      uint32_t imm = va_arg(args, uint32_t);

      Emit<uint8_t>(thing, 0x3D);
      Emit<uint32_t>(thing, imm);
    } break;

    case I::PUSH_REG:
    {
      Reg r = static_cast<Reg>(va_arg(args, int));
      Emit<uint8_t>(thing, 0x50 + target->registerSet[r].pimpl->opcodeOffset);
    } break;

    case I::POP_REG:
    {
      Reg r = static_cast<Reg>(va_arg(args, int));
      Emit<uint8_t>(thing, 0x58 + target->registerSet[r].pimpl->opcodeOffset);
    } break;

    case I::ADD_REG_REG:
    {
      Reg dest  = static_cast<Reg>(va_arg(args, int));
      Reg src   = static_cast<Reg>(va_arg(args, int));

      Emit<uint8_t>(thing, 0x48);
      Emit<uint8_t>(thing, 0x01);
      EmitRegisterModRM(thing, target, src, dest);
    } break;

    case I::SUB_REG_REG:
    {
      Reg dest = static_cast<Reg>(va_arg(args, int));
      Reg src  = static_cast<Reg>(va_arg(args, int));

      Emit<uint8_t>(thing, 0x48);
      Emit<uint8_t>(thing, 0x29);
      EmitRegisterModRM(thing, target, src, dest);
    } break;

    case I::MUL_REG_REG:
    {
      Reg dest = static_cast<Reg>(va_arg(args, int));
      Reg src  = static_cast<Reg>(va_arg(args, int));

      Emit<uint8_t>(thing, 0x48);
      Emit<uint8_t>(thing, 0x0f);
      Emit<uint8_t>(thing, 0xaf);
      EmitRegisterModRM(thing, target, src, dest);
    } break;

    case I::DIV_REG_REG:
    {
      // TODO(Isaac): division is apparently a PITA, so work out what the hell to do here
      RaiseError(errorState, ICE_GENERIC, "Division is actually physically impossible on the x64");
    } break;

    case I::XOR_REG_REG:
    {
      Reg dest = static_cast<Reg>(va_arg(args, int));
      Reg src  = static_cast<Reg>(va_arg(args, int));

      Emit<uint8_t>(thing, 0x48);
      Emit<uint8_t>(thing, 0x31);
      EmitRegisterModRM(thing, target, src, dest);
    } break;

    case I::ADD_REG_IMM32:
    {
      Reg result    = static_cast<Reg>(va_arg(args, int));
      uint32_t imm  = va_arg(args, uint32_t);

      Emit<uint8_t>(thing, 0x48);
      Emit<uint8_t>(thing, 0x81);
      EmitExtensionModRM(thing, target, 0u, result);
      Emit<uint32_t>(thing, imm);
    } break;

    case I::SUB_REG_IMM32:
    {
      Reg result    = static_cast<Reg>(va_arg(args, int));
      uint32_t imm  = va_arg(args, uint32_t);

      Emit<uint8_t>(thing, 0x48);
      Emit<uint8_t>(thing, 0x81);
      EmitExtensionModRM(thing, target, 5u, result);
      Emit<uint32_t>(thing, imm);
    } break;

    case I::MUL_REG_IMM32:
    {
      Reg result    = static_cast<Reg>(va_arg(args, int));
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

    case I::DIV_REG_IMM32:
    {
      RaiseError(errorState, ICE_GENERIC, "Division is currently deemed impossible on the x64...");
    } break;

    case I::MOV_REG_REG:
    {
      Reg dest = static_cast<Reg>(va_arg(args, int));
      Reg src  = static_cast<Reg>(va_arg(args, int));

      Emit<uint8_t>(thing, 0x48);
      Emit<uint8_t>(thing, 0x89);
      EmitRegisterModRM(thing, target, src, dest);
    } break;

    case I::MOV_REG_IMM32:
    {
      Reg dest = static_cast<Reg>(va_arg(args, int));
      uint32_t imm = va_arg(args, uint32_t);

      Emit<uint8_t>(thing, 0xB8 + target->registerSet[dest].pimpl->opcodeOffset);
      Emit<uint32_t>(thing, imm);
    } break;

    case I::MOV_REG_IMM64:
    {
      Reg dest = static_cast<Reg>(va_arg(args, int));
      uint64_t imm = va_arg(args, uint64_t);

      Emit<uint8_t>(thing, 0x48);
      Emit<uint8_t>(thing, 0xB8 + target->registerSet[dest].pimpl->opcodeOffset);
      Emit<uint64_t>(thing, imm);
    } break;

    case I::MOV_REG_BASE_DISP:
    {
      Reg dest = static_cast<Reg>(va_arg(args, int));
      Reg base = static_cast<Reg>(va_arg(args, int));
      uint32_t displacement = va_arg(args, uint32_t);

      Emit<uint8_t>(thing, 0x48);
      Emit<uint8_t>(thing, 0x8B);
      EmitIndirectModRM(thing, target, dest, base, displacement);
    } break;

    case I::MOV_BASE_DISP_IMM32:
    {
      Reg base = static_cast<Reg>(va_arg(args, int));
      uint32_t displacement = va_arg(args, uint32_t);
      uint32_t imm = va_arg(args, uint32_t);

      Emit<uint8_t>(thing, 0xC7);
      EmitIndirectModRM(thing, target, (Reg)0u, base, displacement);
      Emit<uint32_t>(thing, imm);
    } break;

    case I::MOV_BASE_DISP_IMM64:
    {
      Reg base = static_cast<Reg>(va_arg(args, int));
      uint32_t displacement = va_arg(args, uint32_t);
      uint64_t imm = va_arg(args, uint64_t);

      Emit<uint8_t>(thing, 0x48);
      Emit<uint8_t>(thing, 0xC7);
      EmitIndirectModRM(thing, target, (Reg)0u, base, displacement);
      Emit<uint64_t>(thing, imm);
    } break;

    case I::MOV_BASE_DISP_REG:
    {
      Reg base = static_cast<Reg>(va_arg(args, int));
      uint32_t displacement = va_arg(args, uint32_t);
      Reg src = static_cast<Reg>(va_arg(args, int));

      Emit<uint8_t>(thing, 0x48);
      Emit<uint8_t>(thing, 0x89);
      EmitIndirectModRM(thing, target, src, base, displacement);
    } break;

    case I::INC_REG:
    {
      Reg r = static_cast<Reg>(va_arg(args, int));

      Emit<uint8_t>(thing, 0xFF);
      EmitExtensionModRM(thing, target, 0u, r);
    } break;

    case I::DEC_REG:
    {
      Reg r = static_cast<Reg>(va_arg(args, int));

      Emit<uint8_t>(thing, 0xFF);
      EmitExtensionModRM(thing, target, 1u, r);
    } break;

    case I::NOT_REG:
    {
      Reg r = static_cast<Reg>(va_arg(args, int));

      Emit<uint8_t>(thing, 0xF7);
      EmitExtensionModRM(thing, target, 2u, r);
    } break;

    case I::NEG_REG:
    {
      Reg r = static_cast<Reg>(va_arg(args, int));

      Emit<uint8_t>(thing, 0xF7);
      EmitExtensionModRM(thing, target, 3u, r);
    } break;

    case I::CALL32:
    {
      uint32_t offset = va_arg(args, uint32_t);

      Emit<uint8_t>(thing, 0xE8);
      Emit<uint32_t>(thing, offset);
    } break;

    case I::INT_IMM8:
    {
      uint8_t intNumber = static_cast<uint8_t>(va_arg(args, int));

      Emit<uint8_t>(thing, 0xCD);
      Emit<uint8_t>(thing, intNumber);
    } break;

    case I::LEAVE:
    {
      Emit<uint8_t>(thing, 0xC9);
    } break;

    case I::RET:
    {
      Emit<uint8_t>(thing, 0xC3);
    } break;

    case I::JMP:
    {
      uint32_t relAddress = va_arg(args, uint32_t);

      Emit<uint8_t>(thing, 0xE9);
      Emit<uint32_t>(thing, relAddress);
    } break;

    case I::JE:
    case I::JNE:
    case I::JO:
    case I::JNO:
    case I::JS:
    case I::JNS:
    case I::JG:
    case I::JGE:
    case I::JL:
    case I::JLE:
    case I::JPE:
    case I::JPO:
    {
      //                               JE    JNE   JO    JNO   JS    JNS   JG    JGE   JL    JLE   JPE   JPO
      static const uint8_t jumps[] = { 0x84, 0x85, 0x80, 0x81, 0x88, 0x89, 0x8F, 0x8D, 0x8C, 0x8E, 0x8A, 0x8B };

      uint32_t relAddress = va_arg(args, uint32_t);
      Emit<uint8_t>(thing, 0x0F);
      Emit<uint8_t>(thing, jumps[static_cast<unsigned int>(instruction) - static_cast<unsigned int>(I::JE)]);
      Emit<uint32_t>(thing, relAddress);
    } break;
  }

  va_end(args);
}
