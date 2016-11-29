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
#include <common.hpp>
#include <ir.hpp>
#include <air.hpp>
#include <elf.hpp>

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
  R15
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
  type->size = size;

  return type;
}

void InitCodegenTarget(parse_result& parseResult, codegen_target& target)
{
  target.name = "x64_elf";
  target.numRegisters = 16u;
  target.registerSet = static_cast<register_def*>(malloc(sizeof(register_def) * target.numRegisters));

  target.numIntParamColors = 6u;
  target.intParamColors = static_cast<unsigned int*>(malloc(sizeof(unsigned int) * target.numIntParamColors));
  target.intParamColors[0] = RDI;
  target.intParamColors[1] = RSI;
  target.intParamColors[2] = RDX;
  target.intParamColors[3] = RCX;
  target.intParamColors[4] = R8;
  target.intParamColors[5] = R9;

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
  AddToLinkedList<type_def*>(parseResult.types, CreateInbuiltType("int",    4u));
  AddToLinkedList<type_def*>(parseResult.types, CreateInbuiltType("uint",   4u));
  AddToLinkedList<type_def*>(parseResult.types, CreateInbuiltType("float",  4u));
  AddToLinkedList<type_def*>(parseResult.types, CreateInbuiltType("u8",     1u));
  AddToLinkedList<type_def*>(parseResult.types, CreateInbuiltType("s8",     1u));
  AddToLinkedList<type_def*>(parseResult.types, CreateInbuiltType("u16",    2u));
  AddToLinkedList<type_def*>(parseResult.types, CreateInbuiltType("s16",    2u));
  AddToLinkedList<type_def*>(parseResult.types, CreateInbuiltType("u32",    4u));
  AddToLinkedList<type_def*>(parseResult.types, CreateInbuiltType("s32",    4u));
  AddToLinkedList<type_def*>(parseResult.types, CreateInbuiltType("u64",    8u));
  AddToLinkedList<type_def*>(parseResult.types, CreateInbuiltType("s64",    8u));
}

template<>
void Free<codegen_target>(codegen_target& target)
{
  free(target.registerSet);
  free(target.intParamColors);
}

/*
 * +r - add an register opcode offset to the primary opcode
 * [...] - denotes a prefix byte
 * (...) - denotes bytes that follow the opcode, in order
 */
enum class i : uint8_t
{
  ADD_REG_REG     = 0x01,       // [opcodeSize] (ModR/M)
  PUSH_REG        = 0x50,       // +r
  ADD_REG_IMM32   = 0x81,       // [opcodeSize] (ModR/M [extension]) (4-byte immediate)
  MOV_REG_REG     = 0x89,       // [opcodeSize] (ModR/M)
  MOV_REG_IMM32   = 0xB8,       // +r (4-byte immediate)
  MOV_REG_IMM64         ,       // NOTE(Isaac): same opcode as MOV_REG_IMM32      [immSize] +r (8-byte immedite)
  RET             = 0xC3,
  LEAVE           = 0xC9,
  CALL32          = 0xE8,       // (4-byte offset to RIP)
};

/*
 * ModR/M bytes are used to encode an instruction's operands, when there are only two and they are direct
 * registers or effective memory addresses.
 *
 * 7                               0
 * +---+---+---+---+---+---+---+---+
 * |  mod  |    reg    |    r/m    |
 * +---+---+---+---+---+---+---+---+
 *
 * `mod` : register-direct addressing mode is used when this field is b11, indirect addressing is used otherwise
 * `reg` : opcode offset of the destination or source register (depending on instruction)
 * `r/m` : opcode offset of the other register, optionally with a displacement (as specified by `mod`)
 */
static inline uint8_t CreateRegisterModRM(codegen_target& target, reg regOne, reg regTwo)
{
  uint8_t result = 0b11000000; // NOTE(Isaac): use the register-direct addressing mode
  result |= target.registerSet[regOne].pimpl->opcodeOffset << 3u;
  result |= target.registerSet[regTwo].pimpl->opcodeOffset;
  return result;
}

static inline uint8_t CreateExtensionModRM(codegen_target& target, uint8_t extension, reg r)
{
  uint8_t result = 0b11000000;  // NOTE(Isaac): register-direct addressing mode
  result |= extension << 3u;
  result |= target.registerSet[r].pimpl->opcodeOffset;
  return result;
}

static void Emit(elf_thing* thing, codegen_target& target, i instruction, ...)
{
  va_list args;
  va_start(args, instruction);

  switch (instruction)
  {
    case i::ADD_REG_REG:
    {
      reg dest  = static_cast<reg>(va_arg(args, int));
      reg src   = static_cast<reg>(va_arg(args, int));

      Emit<uint8_t>(thing, 0x48);
      Emit<uint8_t>(thing, static_cast<uint8_t>(i::ADD_REG_REG));
      Emit<uint8_t>(thing, CreateRegisterModRM(target, dest, src));
    } break;

    case i::PUSH_REG:
    {
      reg r = static_cast<reg>(va_arg(args, int));
      Emit<uint8_t>(thing, static_cast<uint8_t>(i::PUSH_REG) + target.registerSet[r].pimpl->opcodeOffset);
    } break;

    case i::ADD_REG_IMM32:
    {
      reg result    = static_cast<reg>(va_arg(args, int));
      uint32_t imm  = va_arg(args, uint32_t);

      Emit<uint8_t>(thing, 0x48);
      Emit<uint8_t>(thing, static_cast<uint8_t>(i::ADD_REG_IMM32));
      Emit<uint8_t>(thing, CreateExtensionModRM(target, 0u, result));
      Emit<uint32_t>(thing, imm);
    } break;

    case i::MOV_REG_REG:
    {
      reg dest = static_cast<reg>(va_arg(args, int));
      reg src  = static_cast<reg>(va_arg(args, int));

      Emit<uint8_t>(thing, 0x48);
      Emit<uint8_t>(thing, static_cast<uint8_t>(i::MOV_REG_REG));
      Emit<uint8_t>(thing, CreateRegisterModRM(target, src, dest));
    } break;

    case i::MOV_REG_IMM32:
    {
      reg dest = static_cast<reg>(va_arg(args, int));
      uint32_t imm = va_arg(args, uint32_t);

      Emit<uint8_t>(thing, static_cast<uint8_t>(i::MOV_REG_IMM32) + target.registerSet[dest].pimpl->opcodeOffset);
      Emit<uint32_t>(thing, imm);
    } break;

    case i::MOV_REG_IMM64:
    {
      reg dest = static_cast<reg>(va_arg(args, int));
      uint64_t imm = va_arg(args, uint64_t);

      Emit<uint8_t>(thing, 0x48);
      // NOTE(Isaac): use the opcode from `MOV_REG_IMM32` because they wouldn't all fit in the enum
      Emit<uint8_t>(thing, static_cast<uint8_t>(i::MOV_REG_IMM32) + target.registerSet[dest].pimpl->opcodeOffset);
      Emit<uint64_t>(thing, imm);
    } break;

    case i::CALL32:
    {
      uint32_t offset = va_arg(args, uint32_t);

      Emit<uint8_t>(thing, static_cast<uint8_t>(i::CALL32));
      Emit<uint32_t>(thing, offset);
    } break;

    // NOTE(Isaac): these are the single-opcode instructions with no extra crap
    case i::RET:
    case i::LEAVE:
    {
      Emit<uint8_t>(thing, static_cast<uint8_t>(instruction));
    } break;
  }

  va_end(args);
}

elf_thing* GenerateFunction(elf_file& elf, codegen_target& target, function_def* function)
{
  printf("Generating object code for function: %s\n", function->name);
  assert(function->air);
  assert(function->air->code);

  #define E(...) \
    Emit(thing, target, __VA_ARGS__);

  elf_thing* thing = CreateThing(elf, MangleFunctionName(function));

  if (GetAttrib(function, function_attrib::attrib_type::ENTRY))
  {
    printf("Found program entry point: %s!\n", function->name);
    // TODO: yeah this doesn't actually do anything yet
  }

  for (air_instruction* instruction = function->air->code;
       instruction;
       instruction = instruction->next)
  {
    switch (instruction->type)
    {
      case I_ENTER_STACK_FRAME:
      {
        E(i::PUSH_REG, RBP);
        E(i::MOV_REG_REG, RBP, RSP);
      } break;

      case I_LEAVE_STACK_FRAME:
      {
        E(i::LEAVE);
        E(i::RET);
      } break;

      case I_RETURN:
      {

      } break;

      case I_JUMP:
      {

      } break;

      case I_MOV:
      {
        mov_instruction& mov = instruction->payload.mov;

        if (mov.src->type == slot::slot_type::INT_CONSTANT)
        {
          E(i::MOV_REG_IMM32, mov.dest->color, mov.src->payload.i);
        }
        else if (mov.src->type == slot::slot_type::STRING_CONSTANT)
        {
          E(i::MOV_REG_IMM64, mov.dest->color, 0x0);
//          CreateRelocation(elf, thing, thing.length - sizeof(uint64_t), R_X86_64_64, generator.rodataSymbol, mov.src->payload.string->offset);
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

      } break;

      case I_ADD:
      {
        slot_triple& triple = instruction->payload.slotTriple;

        if (triple.left->shouldBeColored) // NOTE(Isaac): this effectively checks if it's gonna be in a register
        {
          E(i::MOV_REG_REG, triple.result->color, triple.left->color);
        }
        else
        {
          E(i::MOV_REG_IMM32, triple.result->color, triple.left->payload.i);
        }

        if (triple.right->shouldBeColored)
        {
          E(i::ADD_REG_REG, triple.result->color, triple.right->color);
        }
        else
        {
          E(i::ADD_REG_IMM32, triple.result->color, triple.right->payload.i);
        }
      } break;

      case I_SUB:
      {

      } break;

      case I_MUL:
      {

      } break;

      case I_DIV:
      {

      } break;

      case I_NEGATE:
      {

      } break;

      case I_CALL:
      {
        E(i::CALL32, 0x0);
        // TODO: find the correct symbol for the function symbol in the table
        // TODO: not entirely sure why we need an addend of -4, but all the relocations were off by 4 for some reason so meh...?
        // TODO: we definitely need a better way of finding symbols...
//        CreateRelocation(elf, thing, thing->length - sizeof(uint32_t), R_X86_64_PC32, 1u, -4);
      } break;

      case I_NUM_INSTRUCTIONS:
      {
        fprintf(stderr, "Tried to generate code for AIR instruction of type `I_NUM_INSTRUCTIONS`!\n");
        exit(1);
      }
    }
  }

  return thing;
}

void Generate(const char* outputPath, codegen_target& target, parse_result& result)
{
  elf_file elf;
  CreateElf(elf, target);

  elf_segment* loadSegment = CreateSegment(elf, PT_LOAD, SEGMENT_ATTRIB_X | SEGMENT_ATTRIB_R, 0x400000, 0x200000);

  CreateSection(elf, ".text", SHT_PROGBITS, 0x10)->flags = SECTION_ATTRIB_A | SECTION_ATTRIB_E;
//  CreateSection(elf, ".rodata", SHT_PROGBITS, 0x04)->flags = SECTION_ATTRIB_A;
  CreateSection(elf, ".strtab", SHT_STRTAB, 0x04);
  CreateSection(elf, ".symtab", SHT_SYMTAB, 0x04)->link = GetSection(elf, ".strtab")->index;

  GetSection(elf, ".text")->address = 0x400040;
  GetSection(elf, ".symtab")->entrySize = 0x18;

  // Create a symbol to reference the .rodata section using
  elf.rodataThing = CreateThing(elf, nullptr);
//  elf.rodataThing->symbol = CreateSymbol(elf, nullptr, SYM_BIND_GLOBAL, SYM_TYPE_SECTION, GetSection(elf, ".rodata")->index, 0x0);
  
//  MapSection(elf, loadSegment, GetSection(elf, ".text"));
  loadSegment->offset = 0x00;
  loadSegment->fileSize = 0x9b;
  loadSegment->memorySize = 0x9b;
//  MapSection(elf, loadSegment, GetSection(elf, ".rodata"));

  // Generate an `elf_thing` for each function
  for (auto* functionIt = result.functions.first;
       functionIt;
       functionIt = functionIt->next)
  {
    if (GetAttrib(**functionIt, function_attrib::attrib_type::PROTOTYPE))
    {
      continue;
    }

    AddToLinkedList<elf_thing*>(elf.things, GenerateFunction(elf, target, **functionIt));
  }

  // --- TEMPORARY TESTING STUFF AND THINGS ---
  LinkObject(elf, "./std/bootstrap.o");
  // ---

  CompleteElf(elf);
  WriteElf(elf, outputPath);
  Free<elf_file>(elf);
}
