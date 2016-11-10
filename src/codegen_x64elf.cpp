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
  type->name = static_cast<char*>(malloc(sizeof(char) * strlen(name)));
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

void EmitText(elf_file& elf, codegen_target& target, parse_result& result)
{
/*  InitSection(generator, generator.text, ".text", SHT_PROGBITS, 0x10);
  generator.text.flags = SECTION_ATTRIB_E | SECTION_ATTRIB_A;
  generator.text.offset = ftell(generator.f);*/

  #define EMIT(...) \
    /*generator.text.size += */Emit(thing, target, __VA_ARGS__);

  // Generate code for each function
  for (auto* functionIt = result.functions.first;
       functionIt;
       functionIt = functionIt->next)
  {
    if (GetAttrib(**functionIt, function_attrib::attrib_type::PROTOTYPE))
    {
      continue;
    }

    printf("Generating object code for function: %s\n", (**functionIt)->name);
    assert((**functionIt)->air);
    assert((**functionIt)->air->code);

    if (GetAttrib(**functionIt, function_attrib::attrib_type::ENTRY))
    {
      printf("Found program entry point: %s!\n", (**functionIt)->name);
      // TODO: yeah this doesn't actually do anything yet
    }

    // Create the symbol for the function
    // TODO: decide whether each should have a local or global binding
/*    unsigned int offset = ftell(generator.f) - generator.text.offset;
    CreateSymbol(generator, MangleFunctionName(**functionIt), SYM_BIND_GLOBAL, SYM_TYPE_FUNCTION, TEXT_INDEX, offset, 0u);*/

    // Create a `elf_thing` for the function
    elf_thing* thing = CreateThing(elf, MangleFunctionName((**functionIt)));

    for (air_instruction* instruction = (**functionIt)->air->code;
         instruction;
         instruction = instruction->next)
    {
      switch (instruction->type)
      {
        case I_ENTER_STACK_FRAME:
        {
          EMIT(i::PUSH_REG, RBP);
          EMIT(i::MOV_REG_REG, RBP, RSP);
        } break;

        case I_LEAVE_STACK_FRAME:
        {
          EMIT(i::LEAVE);
          EMIT(i::RET);
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
            EMIT(i::MOV_REG_IMM32, mov.dest->color, mov.src->payload.i);
          }
          else if (mov.src->type == slot::slot_type::STRING_CONSTANT)
          {
            EMIT(i::MOV_REG_IMM64, mov.dest->color, 0x0);
//            CreateRelocation(generator, ftell(generator.f) - generator.text.offset - sizeof(uint64_t), R_X86_64_64, generator.rodataSymbol, mov.src->payload.string->offset);
          }
          else
          {
            // NOTE(Isaac): if we're here, `src` should be colored
            assert(mov.src->color != -1);

            EMIT(i::MOV_REG_REG, mov.dest->color, mov.src->color);
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
            EMIT(i::MOV_REG_REG, triple.result->color, triple.left->color);
          }
          else
          {
            EMIT(i::MOV_REG_IMM32, triple.result->color, triple.left->payload.i);
          }

          if (triple.right->shouldBeColored)
          {
            EMIT(i::ADD_REG_REG, triple.result->color, triple.right->color);
          }
          else
          {
            EMIT(i::ADD_REG_IMM32, triple.result->color, triple.right->payload.i);
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
          EMIT(i::CALL32, 0x0);
          // TODO: find the correct symbol for the function symbol in the table
          // TODO: not entirely sure why we need an addend of -4, but all the relocations were off by 4 for some reason so meh...?
//          CreateRelocation(generator, ftell(generator.f) - generator.text.offset - sizeof(uint32_t), R_X86_64_PC32, 2u, -4);
        } break;

        case I_NUM_INSTRUCTIONS:
        {
          fprintf(stderr, "Tried to generate code for AIR instruction of type `I_NUM_INSTRUCTIONS`!\n");
          exit(1);
        }
      }
    }
  }
#undef EMIT
}

void Generate(const char* outputPath, codegen_target& target, parse_result& result)
{
  elf_file elf;
  CreateElf(elf);

  CreateSection(elf, ".text", SHT_PROGBITS, 0x10);
  CreateSection(elf, ".strtab", SHT_STRTAB, 0x04);

  WriteElf(elf, outputPath);
  Free<elf_file>(elf);

/*
  generator.header.fileType = 0x01;
  generator.header.numSectionHeaderEntries = 0u;
  generator.header.entryPoint = 0x00;
  generator.header.sectionWithSectionNames = SECTION_NAME_TABLE_INDEX;

  // --- The .rodata section ---
  InitSection(generator, generator.rodata, ".rodata", SHT_PROGBITS, 0x4);
  generator.rodata.flags = SECTION_ATTRIB_A;

  // Add a symbol to the .rodata section so we can add relocations relative to it
  generator.rodataSymbol = CreateSymbol(generator, nullptr, SYM_BIND_LOCAL, SYM_TYPE_SECTION, RODATA_INDEX, 0, 0);

  // --- The symbol table ---
  InitSection(generator, generator.symbolTable, ".symtab", SHT_SYMTAB, 0x04);
  generator.symbolTable.link = STRING_TABLE_INDEX;
  generator.symbolTable.info = UINT_MAX;
  generator.symbolTable.entrySize = 0x18;

  // --- The string table of symbols ---
  InitSection(generator, generator.stringTable, ".strtab", SHT_STRTAB, 0x01);
  generator.stringTable.size = 1u; // NOTE(Isaac): 1 because of the leading null-terminator

  // --- The string table of section names ---
  InitSection(generator, generator.sectionNameTable, ".shstrtab", SHT_STRTAB, 0x01);
  generator.sectionNameTable.size = 1u;

  // --- The relocation section for .text ---
  InitSection(generator, generator.relaText, ".rela.text", SHT_RELA, 0x04);
  generator.relaText.link = SYMBOL_TABLE_INDEX;
  generator.relaText.info = TEXT_INDEX;
  generator.relaText.entrySize = 0x18;

  // NOTE(Isaac): leave space for the ELF header
  fseek(generator.f, 0x40, SEEK_SET);

  // [0x40] Generate .rodata
  generator.rodata.offset = ftell(generator.f);

  for (auto* stringIt = result.strings.first;
       stringIt;
       stringIt = stringIt->next)
  {
    (**stringIt)->offset = ftell(generator.f) - generator.rodata.offset;

    for (const char* c = (**stringIt)->string;
         *c;
         c++)
    {
      fputc(*c, generator.f);
      generator.rodata.size++;
    }

    fputc('\0', generator.f);
    generator.rodata.size++;
  }

  // [???] Generate the object code that will be in the .text section
  EmitText(generator, result);
  generator.text.name = CreateString(generator.sectionNames, ".text");

  // [???] Emit .rela.text
  generator.relaText.offset = ftell(generator.f);
  
  for (auto* relocationIt = generator.textRelocations.first;
       relocationIt;
       relocationIt = relocationIt->next)
  {*/
///*n + */
///*0x00*/fwrite(&((**relocationIt).offset), sizeof(uint64_t), 1, generator.f);
///*0x08*/fwrite(&((**relocationIt).info), sizeof(uint64_t), 1, generator.f);
///*0x10*/fwrite(&((**relocationIt).addend), sizeof(int64_t), 1, generator.f);
///*0x18*/
/*
    generator.relaText.size += 0x18;
  }

  // [???] Emit the symbol table
  generator.symbolTable.offset = ftell(generator.f);

  // Emit a first, STN_UNDEF symbol
  generator.symbolTable.size += 0x18;
  for (unsigned int i = 0u;
       i < 0x18;
       i++)
  {
    fputc(0x00, generator.f);
  }

  for (auto* symbolIt = generator.symbols.first;
       symbolIt;
       symbolIt = symbolIt->next)
  {*/
///*n + */
///*0x00*/fwrite(&((**symbolIt).name), sizeof(uint32_t), 1, generator.f);
///*0x04*/fwrite(&((**symbolIt).info), sizeof(uint8_t), 1, generator.f);
///*0x05*/fputc(0x00, generator.f);
///*0x06*/fwrite(&((**symbolIt).sectionIndex), sizeof(uint16_t), 1, generator.f);
///*0x08*/fwrite(&((**symbolIt).definitionOffset), sizeof(uint64_t), 1, generator.f);
///*0x10*/fwrite(&((**symbolIt).size), sizeof(uint64_t), 1, generator.f);
///*0x18*/
/*
    generator.symbolTable.size += 0x18;
  }*/
/*
  // [???] Emit the string table of section names
  generator.sectionNameTable.offset = ftell(generator.f);
  fputc('\0', generator.f);   // NOTE(Isaac): start with a leading null-terminator

  for (auto* stringIt = generator.sectionNames.first;
       stringIt;
       stringIt = stringIt->next)
  {
    for (unsigned int i = 0u;
         i < strlen((**stringIt).str);
         i++)
    {
      fputc((**stringIt).str[i], generator.f);
      generator.sectionNameTable.size++;
    }

    // Add a null-terminator
    fputc('\0', generator.f);
    generator.sectionNameTable.size++;
  }

  // [???] Emit the string table
  generator.stringTable.offset = ftell(generator.f);
  fputc('\0', generator.f);   // NOTE(Isaac): start with a leading null-terminator

  for (auto* stringIt = generator.strings.first;
       stringIt;
       stringIt = stringIt->next)
  {
    for (unsigned int i = 0u;
         i < strlen((**stringIt).str);
         i++)
    {
      fputc((**stringIt).str[i], generator.f);
      generator.stringTable.size++;
    }

    // Add a null-terminator
    fputc('\0', generator.f);
    generator.stringTable.size++;
  }

  // [???] Create the section header
  generator.header.sectionHeaderOffset = ftell(generator.f);
  
  // Create a sentinel null section at index 0
  generator.header.numSectionHeaderEntries = 1u;
  for (unsigned int i = 0u;
       i < SECTION_HEADER_ENTRY_SIZE;
       i++)
  {
    fputc(0x00, generator.f);
  }

  EmitSectionEntry(generator, generator.rodata);            // [1]
  EmitSectionEntry(generator, generator.text);              // [2]
  EmitSectionEntry(generator, generator.symbolTable);       // [3]
  EmitSectionEntry(generator, generator.sectionNameTable);  // [4]
  EmitSectionEntry(generator, generator.stringTable);       // [5]
  EmitSectionEntry(generator, generator.relaText);          // [6]*/
}
