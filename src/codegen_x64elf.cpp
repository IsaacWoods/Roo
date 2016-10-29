/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cstdint>
#include <cassert>
#include <cstdarg>
#include <common.hpp>
#include <air.hpp>

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

void FreeCodegenTarget(codegen_target& target)
{
  free(target.registerSet);
}

// --- ELF stuff and things ---
struct elf_header
{
  uint16_t fileType;
  uint64_t entryPoint;
  uint64_t sectionHeaderOffset;
  uint16_t numSectionHeaderEntries;
  uint16_t sectionWithSectionNames;
};

#define SECTION_ATTRIB_W        0x1         // NOTE(Isaac): marks the section as writable
#define SECTION_ATTRIB_A        0x2         // NOTE(Isaac): marks the section to be allocated in the memory image
#define SECTION_ATTRIB_E        0x4         // NOTE(Isaac): marks the section as executable
#define SECTION_ATTRIB_MASKOS   0x0F000000  // NOTE(Isaac): environment-specific
#define SECTION_ATTRIB_MASKPROC 0xF0000000  // NOTE(Isaac): processor-specific

struct elf_section
{
  uint32_t name;                        // Offset in the string table to the section name
  enum section_type : uint32_t
  {
    SHT_NULL            = 0u,
    SHT_PROGBITS        = 1u,
    SHT_SYMTAB          = 2u,
    SHT_STRTAB          = 3u,
    SHT_RELA            = 4u,
    SHT_HASH            = 5u,
    SHT_DYNAMIC         = 6u,
    SHT_NOTE            = 7u,
    SHT_NOBITS          = 8u,
    SHT_REL             = 9u,
    SHT_SHLIB           = 10u,
    SHT_DYNSYM          = 11u,
    SHT_INIT_ARRAY      = 14u,
    SHT_FINI_ARRAY      = 15u,
    SHT_PREINIT_ARRAY   = 16u,
    SHT_GROUP           = 17u,
    SHT_SYMTAB_SHNDX    = 18u,
    SHT_LOOS            = 0x60000000,
    SHT_HIOS            = 0x6FFFFFFF,
    SHT_LOPROC          = 0x70000000,
    SHT_HIPROC          = 0x7FFFFFFF,
    SHT_LOUSER          = 0x80000000,
    SHT_HIUSER          = 0x8FFFFFFF
  } type;
  uint64_t flags;
  uint64_t address;                     // Virtual address of the beginning of the section in memory
  uint64_t offset;                      // Offset of the beginning of the section in the file
  uint64_t size;                        // NOTE(Isaac): for SHT_NOBITS, this is NOT the size in the file!
  uint32_t link;                        // Index of an associated section
  uint32_t info;
  uint64_t addressAlignment;            // Required alignment of this section
  uint64_t entrySize;                   // Size of each section entry (if fixed, zero otherwise)
};

static void GenerateHeader(FILE* f, elf_header& header)
{
  const uint16_t sectionHeaderEntrySize = 0x40;

/*0x00*/fputc(0x7F,     f); // Emit the 4 byte magic value
        fputc('E',      f);
        fputc('L',      f);
        fputc('F',      f);
/*0x04*/fputc(2,        f); // Specify we are targetting a 64-bit system
/*0x05*/fputc(1,        f); // Specify we are targetting a little-endian system
/*0x06*/fputc(1,        f); // Specify that we are targetting the first version of ELF
/*0x07*/fputc(0x00,     f); // Specify that we are targetting the System-V ABI
/*0x08*/for (unsigned int i = 0x08;
             i < 0x10;
             i++)
        {
          fputc(0x00,   f); // Pad out EI_ABIVERSION and EI_PAD
        }
/*0x10*/fwrite(&(header.fileType), sizeof(uint16_t), 1, f);
/*0x12*/fputc(0x3E,     f); // Specify we are targetting the x86_64 ISA
        fputc(0x00,     f);
/*0x14*/fputc(0x01,     f); // Specify we are targetting the first version of ELF
        fputc(0x00,     f);
        fputc(0x00,     f);
        fputc(0x00,     f);
/*0x18*/fwrite(&(header.entryPoint), sizeof(uint64_t), 1, f);
/*0x20*/fputc(0x00,     f); // Offset to the program header - there isn't one
        fputc(0x00,     f);
        fputc(0x00,     f);
        fputc(0x00,     f);
        fputc(0x00,     f);
        fputc(0x00,     f);
        fputc(0x00,     f);
        fputc(0x00,     f);
/*0x28*/fwrite(&(header.sectionHeaderOffset), sizeof(uint64_t), 1, f);
/*0x30*/fputc(0x00,     f); // Specify some flags (undefined for x86_64)
        fputc(0x00,     f);
        fputc(0x00,     f);
        fputc(0x00,     f);
/*0x34*/fputc(64u,      f); // Specify the size of the header (64 bytes)
        fputc(0x00,     f);
/*0x36*/fputc(0x00,     f); // Specify the size of a program header entry as 0
        fputc(0x00,     f);
/*0x38*/fputc(0x00,     f); // Specify that there are 0 program header entries
        fputc(0x00,     f);
/*0x3A*/fwrite(&sectionHeaderEntrySize,           sizeof(uint16_t), 1, f);
/*0x3C*/fwrite(&(header.numSectionHeaderEntries), sizeof(uint16_t), 1, f);
/*0x3E*/fwrite(&(header.sectionWithSectionNames), sizeof(uint16_t), 1, f);
/*0x40*/
}

static void GenerateSectionHeaderEntry(FILE* f, elf_header& header, elf_section& section)
{
  header.numSectionHeaderEntries++;

/*n + */
/*0x00*/fwrite(&(section.name),             sizeof(uint32_t), 1, f);
/*0x04*/fwrite(&(section.type),             sizeof(uint32_t), 1, f);
/*0x08*/fwrite(&(section.flags),            sizeof(uint64_t), 1, f);
/*0x10*/fwrite(&(section.address),          sizeof(uint64_t), 1, f);
/*0x18*/fwrite(&(section.offset),           sizeof(uint64_t), 1, f);
/*0x20*/fwrite(&(section.size),             sizeof(uint64_t), 1, f);
/*0x28*/fwrite(&(section.link),             sizeof(uint32_t), 1, f);
/*0x2C*/fwrite(&(section.info),             sizeof(uint32_t), 1, f);
/*0x30*/fwrite(&(section.addressAlignment), sizeof(uint64_t), 1, f);
/*0x38*/fwrite(&(section.entrySize),        sizeof(uint64_t), 1, f);
/*0x40*/
}

static uint32_t EmitStringTableEntry(FILE* f, elf_section& section, const char* str)
{
  // NOTE(Isaac): index 0 is the leading null-terminator and so is non-existant
  static uint32_t stringIndex = 1u;

  for (unsigned int i = 0u;
       i < strlen(str);
       i++)
  {
    fputc(str[i], f);
    section.size++;
  }

  // Add a null-terminator
  fputc('\0', f);
  section.size++;
  
  return stringIndex++;
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
  RET             = 0xC3,
  LEAVE           = 0xC9,
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
 * `reg` : opcode offset of the destination register
 * `r/m` : opcode offset of the source register, optionally with a displacement (as specified by `mod`)
 */
static inline uint8_t CreateRegisterModRM(codegen_target& target, reg dest, reg src)
{
  uint8_t result = 0b11000000; // NOTE(Isaac): use the register-direct addressing mode
  result |= target.registerSet[dest].pimpl->opcodeOffset << 3u;
  result |= target.registerSet[src].pimpl->opcodeOffset;
  return result;
}

static inline uint8_t CreateExtensionModRM(codegen_target& target, uint8_t extension, reg r)
{
  uint8_t result = 0b11000000;  // NOTE(Isaac): register-direct addressing mode
  result |= extension << 3u;
  result |= target.registerSet[r].pimpl->opcodeOffset;
  return result;
}

/*
 * NOTE(Isaac): returns the number of bytes emitted
 */
static uint64_t Emit(FILE* f, codegen_target& target, i instruction, ...)
{
  va_list args;
  va_start(args, instruction);
  uint64_t numBytesEmitted = 0u;

  #define EMIT(thing) \
    fputc(thing, f); \
    numBytesEmitted++;

  switch (instruction)
  {
    case i::ADD_REG_REG:
    {
      reg dest  = static_cast<reg>(va_arg(args, int));
      reg src   = static_cast<reg>(va_arg(args, int));
      
      EMIT(0x48);
      EMIT(static_cast<uint8_t>(i::ADD_REG_REG));
      EMIT(CreateRegisterModRM(target, dest, src));
    } break;

    case i::PUSH_REG:
    {
      reg r = static_cast<reg>(va_arg(args, int));
      EMIT(static_cast<uint8_t>(i::PUSH_REG) + target.registerSet[r].pimpl->opcodeOffset);
    } break;

    case i::ADD_REG_IMM32:
    {
      reg result    = static_cast<reg>(va_arg(args, int));
      uint32_t imm  = va_arg(args, uint32_t);

      EMIT(0x48);   // Specify we are moving 64-bit values around
      EMIT(static_cast<uint8_t>(i::ADD_REG_IMM32));
      EMIT(CreateExtensionModRM(target, 0u, result));
      fwrite(&imm, sizeof(uint32_t), 1, f);
      numBytesEmitted += 4u;
    } break;

    case i::MOV_REG_REG:
    {
      reg dest = static_cast<reg>(va_arg(args, int));
      reg src  = static_cast<reg>(va_arg(args, int));

      EMIT(0x48);
      EMIT(static_cast<uint8_t>(i::MOV_REG_REG));
      EMIT(CreateRegisterModRM(target, dest, src));
    } break;

    case i::MOV_REG_IMM32:
    {
      reg dest = static_cast<reg>(va_arg(args, int));
      uint32_t imm = va_arg(args, uint32_t);

//      EMIT(0x48);
      EMIT(static_cast<uint8_t>(i::MOV_REG_IMM32) + target.registerSet[dest].pimpl->opcodeOffset);
      fwrite(&imm, sizeof(uint32_t), 1, f);
      numBytesEmitted += 4u;
    } break;

    // NOTE(Isaac): these are the single-opcode instructions with no extra crap
    case i::RET:
    case i::LEAVE:
    {
      EMIT(static_cast<uint8_t>(instruction));
    } break;
  }

  va_end(args);
  return numBytesEmitted;
}

void GenerateBootstrap(FILE* f, elf_section& textSection, elf_header& header, uint64_t entryFunctionOffset)
{
  header.entryPoint = ftell(f);
  unsigned numBytesEmitted = 0u;

  // xor ebp, ebp
  EMIT(0x31);
  EMIT(0xED);

  // call {the entry function}
  EMIT(0xE8);
  uint32_t relativeAddressToEntryFunction = (uint32_t)(entryFunctionOffset - (ftell(f) + 4u));
  fwrite(&relativeAddressToEntryFunction, sizeof(uint32_t), 1, f);
  numBytesEmitted += 4u;

  // mov rax, 1
  EMIT(0xB8);
  EMIT(0x01);
  EMIT(0x00);
  EMIT(0x00);
  EMIT(0x00);

  // xor ebx, ebx
  EMIT(0x31);
  EMIT(0xDB);

  // int 0x80
  EMIT(0xCD);
  EMIT(0x80);

  textSection.size += numBytesEmitted;
}
#undef EMIT

void GenerateTextSection(FILE* file, codegen_target& target, elf_section& section, parse_result& result)
{
  section.type = elf_section::section_type::SHT_PROGBITS;
  section.flags = SECTION_ATTRIB_E | SECTION_ATTRIB_A;
  section.address = 0u;
  section.offset = ftell(file);
  section.size = 0u;
  section.link = 0u;
  section.info = 0u;
  section.addressAlignment = 0x10;
  section.entrySize = 0x00;

  // Generate code for each function
  for (auto* functionIt = result.functions.first;
       functionIt;
       functionIt = functionIt->next)
  {
    printf("Generating object code for function: %s\n", (**functionIt)->name);
    assert((**functionIt)->air);
    assert((**functionIt)->air->code);

    // NOTE(Isaac): Check if this is the program's entry point
    if ((**functionIt)->attribMask & function_attribs::ENTRY)
    {
      printf("Found program entry point: %s!\n", (**functionIt)->name);
      // TODO: yeah this doesn't actually do anything yet
    }

    for (air_instruction* instruction = (**functionIt)->air->code;
         instruction;
         instruction = instruction->next)
    {
      switch (instruction->type)
      {
        case I_ENTER_STACK_FRAME:
        {
          section.size += Emit(file, target, i::PUSH_REG, RBP);
          section.size += Emit(file, target, i::MOV_REG_REG, RBP, RSP);
        } break;

        case I_LEAVE_STACK_FRAME:
        {
          section.size += Emit(file, target, i::LEAVE);
          section.size += Emit(file, target, i::RET);
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
            section.size += Emit(file, target, i::MOV_REG_IMM32, mov.dest->color, mov.src->payload.i);
          }
          else
          {
            section.size += Emit(file, target, i::MOV_REG_REG, mov.dest->color, mov.src->color);
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
            section.size += Emit(file, target, i::MOV_REG_REG, triple.result->color, triple.left->color);
          }
          else
          {
            section.size += Emit(file, target, i::MOV_REG_IMM32, triple.result->color, triple.left->payload.i);
          }

          if (triple.right->shouldBeColored)
          {
            section.size += Emit(file, target, i::ADD_REG_REG, triple.result->color, triple.right->color);
          }
          else
          {
            section.size += Emit(file, target, i::ADD_REG_IMM32, triple.result->color, triple.right->payload.i);
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

        case I_NUM_INSTRUCTIONS:
        {
          fprintf(stderr, "Tried to generate code for AIR instruction of type `I_NUM_INSTRUCTIONS`!\n");
          exit(1);
        }
      }
    }
  }
}

void Generate(const char* outputPath, codegen_target& target, parse_result& result)
{
  FILE* f = fopen(outputPath, "wb");

  if (!f)
  {
    fprintf(stderr, "Failed to open output file: %s\n", outputPath);
    exit(1);
  }

  elf_header header;
  header.fileType = 0x01;
  header.numSectionHeaderEntries = 0u;

  // [0x40] Generate the object code that will be in the .text section
  fseek(f, 0x40, SEEK_SET);
  elf_section textSection;
  GenerateTextSection(f, target, textSection, result);

  // [???] Create the string table
  elf_section stringTableSection;
  stringTableSection.name = 0u;
  stringTableSection.type = elf_section::section_type::SHT_STRTAB;
  stringTableSection.flags = 0u;
  stringTableSection.address = 0u;
  stringTableSection.offset = ftell(f);
  stringTableSection.size = 1u; // NOTE(Isaac): 1 because of the leading null-terminator
  stringTableSection.link = 0u;
  stringTableSection.info = 0u;
  stringTableSection.addressAlignment = 1u;
  stringTableSection.entrySize = 0u;
  header.sectionWithSectionNames = 2u; // NOTE(Isaac): this is hardcoded

  // NOTE(Isaac): start with a leading null-terminator
  fputc('\0', f);
  textSection.name = EmitStringTableEntry(f, stringTableSection, ".text");

  // [???] Create the section header
  header.sectionHeaderOffset = ftell(f);
  
  // Create a sentinel null section at index 0
  elf_section nullSection;
  nullSection.name = 0;
  nullSection.type = elf_section::section_type::SHT_NULL;
  nullSection.flags = 0;
  nullSection.address = 0;
  nullSection.offset = 0;
  nullSection.size = 0u;
  nullSection.link = 0u;
  nullSection.info = 0u;
  nullSection.addressAlignment = 0u;
  nullSection.entrySize = 0u;
  GenerateSectionHeaderEntry(f, header, nullSection);

  GenerateSectionHeaderEntry(f, header, textSection);
  GenerateSectionHeaderEntry(f, header, stringTableSection);

  // [0x00] Generate the ELF header
  fseek(f, 0x00, SEEK_SET);
  GenerateHeader(f, header);

  fclose(f);
}
