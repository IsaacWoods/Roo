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

// --- ELF stuff and things ---
struct elf_header
{
  uint16_t fileType;
  uint64_t entryPoint;
  uint64_t programHeaderOffset;
  uint64_t sectionHeaderOffset;
  uint16_t numProgramHeaderEntries;
  uint16_t numSectionHeaderEntries;
  uint16_t sectionWithSectionNames;
};

#define SEGMENT_ATTRIB_X        0x1         // NOTE(Isaac): marks the segment as executable
#define SEGMENT_ATTRIB_W        0x2         // NOTE(Isaac): marks the segment as writable
#define SEGMENT_ATTRIB_R        0x4         // NOTE(Isaac): marks the segment as readable
#define SEGMENT_ATTRIB_MASKOS   0x00FF0000  // NOTE(Isaac): environment-specific (nobody really knows :P)
#define SEGMENT_ATTRIB_MASKPROC 0xFF000000  // NOTE(Isaac): processor-specific (even fewer people know)

struct elf_segment
{
  enum segment_type : uint32_t
  {
    PT_NULL     = 0u,
    PT_LOAD     = 1u,
    PT_DYNAMIC  = 2u,
    PT_INTERP   = 3u,
    PT_NOTE     = 4u,
    PT_SHLIB    = 5u,
    PT_PHDR     = 6u,
    PT_TLS      = 7u,
    PT_LOOS     = 0x60000000,
    PT_HIOS     = 0x6FFFFFFF,
    PT_LOPROC   = 0x70000000,
    PT_HIPROC   = 0x7FFFFFFF
  } type;
  uint32_t flags;
  uint64_t offset;          // Offset of the first byte of the segment from the image
  uint64_t virtualAddress;
  uint64_t physicalAddress;
  uint64_t fileSize;        // Number of bytes in the file image of the segment (may be zero)
  uint64_t memorySize;      // Number of bytes in the memory image of the segment (may be zero)
  uint64_t alignment;       // NOTE(Isaac): `virtualAddress` should equal `offset`, modulo this `alignment`
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
  const uint16_t programHeaderEntrySize = 0x38;
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
/*0x20*/fwrite(&(header.programHeaderOffset), sizeof(uint64_t), 1, f);
/*0x28*/fwrite(&(header.sectionHeaderOffset), sizeof(uint64_t), 1, f);
/*0x30*/fputc(0x00,     f); // Specify some flags (undefined for x86_64)
        fputc(0x00,     f);
        fputc(0x00,     f);
        fputc(0x00,     f);
/*0x34*/fputc(64u,      f); // Specify the size of the header (64 bytes)
        fputc(0x00,     f);
/*0x36*/fwrite(&programHeaderEntrySize,           sizeof(uint16_t), 1, f);
/*0x38*/fwrite(&(header.numProgramHeaderEntries), sizeof(uint16_t), 1, f);
/*0x3A*/fwrite(&sectionHeaderEntrySize,           sizeof(uint16_t), 1, f);
/*0x3C*/fwrite(&(header.numSectionHeaderEntries), sizeof(uint16_t), 1, f);
/*0x3E*/fwrite(&(header.sectionWithSectionNames), sizeof(uint16_t), 1, f);
/*0x40*/
}

static void GenerateSegmentHeaderEntry(FILE* f, elf_header& header, elf_segment& segment)
{
  header.numProgramHeaderEntries++;

/*n + */
/*0x00*/fwrite(&(segment.type),             sizeof(uint32_t), 1, f);
/*0x04*/fwrite(&(segment.flags),            sizeof(uint32_t), 1, f);
/*0x08*/fwrite(&(segment.offset),           sizeof(uint64_t), 1, f);
/*0x10*/fwrite(&(segment.virtualAddress),   sizeof(uint64_t), 1, f);
/*0x18*/fwrite(&(segment.physicalAddress),  sizeof(uint64_t), 1, f);
/*0x20*/fwrite(&(segment.fileSize),         sizeof(uint64_t), 1, f);
/*0x28*/fwrite(&(segment.memorySize),       sizeof(uint64_t), 1, f);
/*0x30*/fwrite(&(segment.alignment),        sizeof(uint64_t), 1, f);
/*0x38*/
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
 * (...) - denotes bytes that follow the opcode
 */
enum class i : uint8_t
{
  PUSH_REG        = 0x50,       // +r
  MOV_REG_REG     = 0x89,       // [opcodeSize] (ModR/M)
  RET             = 0xC3,
  LEAVE           = 0xC9,
};

/*
 * ModR/M bytes are used to encode an instruction's operands, when there are only two and they are direct
 * registers or effective memory addresses.
 *
 *   7                           0
 * +---+---+---+---+---+---+---+---+
 * |  mod  |    dest   |    src    |
 * +---+---+---+---+---+---+---+---+
 *
 * `mod`  : register-direct addressing mode is used when this field is b11, indirect addressing is used otherwise
 * `dest` : opcode offset of the destination register
 * `src`  : opcode offset of the source register
 */
static inline uint8_t CreateModRM(codegen_target& target, reg dest, reg src)
{
  uint8_t result = 0b11000000; // NOTE(Isaac): use the register-direct addressing mode
  result |= target.registerSet[dest].pimpl->opcodeOffset << 3u;
  result |= target.registerSet[src].pimpl->opcodeOffset;
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
    case i::PUSH_REG:
    {
      reg r = static_cast<reg>(va_arg(args, int));
      EMIT(static_cast<uint8_t>(i::PUSH_REG) + target.registerSet[r].pimpl->opcodeOffset);
    } break;

    case i::MOV_REG_REG:
    {
      reg dest = static_cast<reg>(va_arg(args, int));
      reg src  = static_cast<reg>(va_arg(args, int));

      // TODO: `48` as a prefix is used to signify we are moving a 64-bit value into a 64-bit register. We should probably check this is the case.
      EMIT(0x48);
      EMIT(static_cast<uint8_t>(i::MOV_REG_REG));
      EMIT(CreateModRM(target, dest, src));
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
  #undef EMIT
}

void GenerateTextSection(FILE* file, codegen_target& target, elf_section& section, elf_header& header,
                         parse_result& result, uint64_t virtualAddress)
{
  section.type = elf_section::section_type::SHT_PROGBITS;
  section.flags = SECTION_ATTRIB_E | SECTION_ATTRIB_A;
  section.address = virtualAddress;
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
      header.entryPoint = virtualAddress + (ftell(file) - section.offset);
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

        } break;

        case I_CMP:
        {

        } break;

        case I_ADD:
        {

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
  header.fileType = 0x02;
  header.programHeaderOffset = 0x40;
  header.numProgramHeaderEntries = 0u;
  header.numSectionHeaderEntries = 0u;

  // Segment for .text
  elf_segment textSegment;
  textSegment.type = elf_segment::segment_type::PT_LOAD;
  textSegment.flags = SEGMENT_ATTRIB_X | SEGMENT_ATTRIB_R;
  textSegment.virtualAddress = 0x08048000;
  textSegment.physicalAddress = 0x08048000;
  textSegment.alignment = 0x1000;

  // [0x78] Generate the object code that will be in the .text section
  fseek(f, 0x78, SEEK_SET);
  elf_section textSection;
  GenerateTextSection(f, target, textSection, header, result, textSegment.virtualAddress);

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

  // [0x40] Generate the program header
  fseek(f, 0x40, SEEK_SET);

  // Finish off the .text segment definition and emit it
  textSegment.offset = textSection.offset;
  textSegment.fileSize = textSection.size;
  textSegment.memorySize = textSection.size;
  GenerateSegmentHeaderEntry(f, header, textSegment);

  // [0x00] Generate the ELF header
  fseek(f, 0x00, SEEK_SET);
  GenerateHeader(f, header);

  fclose(f);
}
