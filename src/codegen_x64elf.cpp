/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <cstdio>
#include <cstdlib>
#include <cstdint>
#include <common.hpp>

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

void GenerateTextSection(elf_section& section, elf_header& header, parse_result& result)
{
  section.name = 0; // TODO
  section.type = elf_section::section_type::SHT_PROGBITS;
  section.flags = SECTION_ATTRIB_E | SECTION_ATTRIB_A;
  section.address = 0;
  section.offset = 0; // TODO
  section.size = 0; // TODO
  section.link = 0u; // TODO
  section.info = 0u; // TODO
  section.addressAlignment = 0x10;

  header.entryPoint = 0u; // TODO

  // Generate code for each function
  for (function_def* function = result.firstFunction;
       function;
       function = function->next)
  {
    printf("Generating object code for function: %s\n", function->name);
  }
}

void Generate(const char* outputPath, codegen_target& target, parse_result& result)
{
  FILE* file = fopen(outputPath, "wb");

  if (!file)
  {
    fprintf(stderr, "Failed to open output file: %s\n", outputPath);
    exit(1);
  }

  elf_header header;
  header.fileType = 0x02;
  header.programHeaderOffset = 0x40;
  header.numProgramHeaderEntries = 0u;
  header.numSectionHeaderEntries = 0u;

  // [0x40] Create a segment for .text
  fseek(file, 0x40, SEEK_SET);
  elf_segment segment;
  segment.type = elf_segment::segment_type::PT_LOAD;
  segment.flags = SEGMENT_ATTRIB_X | SEGMENT_ATTRIB_R;
  segment.offset = 0;// TODO
  segment.virtualAddress = 0x08048000;
  segment.physicalAddress = 0x08048000;
  segment.fileSize = 0;
  segment.memorySize = 0;
  segment.alignment = 0x1000;
  GenerateSegmentHeaderEntry(file, header, segment);

  // [0x78] Generate the object code that will be in the .text section
  elf_section textSection;
  GenerateTextSection(textSection, header, result);

  // [???] Create the string table
  // TODO
  header.sectionWithSectionNames = 0u; // TODO

  // [???] Create the section header
  header.sectionHeaderOffset = ftell(file);
  
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
  GenerateSectionHeaderEntry(file, header, nullSection);

  // Emit the text section header
  GenerateSectionHeaderEntry(file, header, textSection);

  // [0x00] Generate the ELF header
  fseek(file, 0x00, SEEK_SET);
  GenerateHeader(file, header);

  fclose(file);
}
