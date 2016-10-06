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
  uint16_t sectionNameIndexInSectionHeader;
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
  uint16_t alignment;       // NOTE(Isaac): `virtualAddress` should equal `offset`, modulo this `alignment`
};

static void GenerateHeader(FILE* f, elf_header& header)
{
  const uint16_t programHeaderEntrySize = 32u;  // TODO umm
  const uint16_t sectionHeaderEntrySize = 32u;  // TODO umm

/*0x00*/fputc(0x7F    , f); // Emit the 4 byte magic value
        fputc('E'     , f);
        fputc('L'     , f);
        fputc('F'     , f);
/*0x04*/fputc(2       , f); // Specify we are targetting a 64-bit system
/*0x05*/fputc(1       , f); // Specify we are targetting a little-endian system
/*0x06*/fputc(1       , f); // Specify that we are targetting the first version of ELF
/*0x07*/fputc(0x00    , f); // Specify that we are targetting the System-V ABI
/*0x08*/for (unsigned int i = 0x08;
             i < 0x10;
             i++)
        {
          fputc(0x00  , f); // Pad out EI_ABIVERSION and EI_PAD
        }
/*0x10*/fwrite(&(header.fileType), sizeof(uint16_t), 1, f);
/*0x12*/fputc(0x3E    , f); // Specify we are targetting the x86_64 ISA
        fputc(0x00    , f);
/*0x14*/fputc(0x01    , f); // Specify we are targetting the first version of ELF
        fputc(0x00    , f);
        fputc(0x00    , f);
        fputc(0x00    , f);
/*0x18*/fwrite(&(header.entryPoint), sizeof(uint64_t), 1, f);
/*0x20*/fwrite(&(header.programHeaderOffset), sizeof(uint64_t), 1, f);
/*0x28*/fwrite(&(header.sectionHeaderOffset), sizeof(uint64_t), 1, f);
/*0x30*/fputc(0x00    , f); // Specify some flags (undefined for x86_64)
        fputc(0x00    , f);
        fputc(0x00    , f);
        fputc(0x00    , f);
/*0x34*/fputc(64u     , f); // Specify the size of the header (64 bytes)
        fputc(0x00    , f);
/*0x36*/fwrite(&programHeaderEntrySize, sizeof(uint16_t), 1, f);
/*0x38*/fwrite(&(header.numProgramHeaderEntries), sizeof(uint16_t), 1, f);
/*0x3A*/fputc(0x10    , f); // Specify the size of an entry in the section header TODO
        fputc(0x00    , f);
/*0x3A*/fwrite(&sectionHeaderEntrySize, sizeof(uint16_t), 1, f);
/*0x3C*/fwrite(&(header.numSectionHeaderEntries), sizeof(uint16_t), 1, f);
/*0x3E*/fwrite(&(header.sectionNameIndexInSectionHeader), sizeof(uint16_t), 1, f);
}

static void GenerateSegment(FILE* f, elf_segment& segment)
{
/*0x00*/fwrite(&(segment.type), sizeof(uint32_t), 1, f);
/*0x04*/fwrite(&(segment.flags), sizeof(uint32_t), 1, f);
/*0x08*/fwrite(&(segment.offset), sizeof(uint64_t), 1, f);
/*0x10*/fwrite(&(segment.virtualAddress), sizeof(uint64_t), 1, f);
/*0x18*/fwrite(&(segment.physicalAddress), sizeof(uint64_t), 1, f);
/*0x20*/fwrite(&(segment.fileSize), sizeof(uint64_t), 1, f);
/*0x28*/fwrite(&(segment.memorySize), sizeof(uint64_t), 1, f);
/*0x30*/fwrite(&(segment.alignment), sizeof(uint16_t), 1, f);
/*0x32*/
}

void Generate(const char* outputPath, codegen_target& target, parse_result& result)
{
  FILE* file = fopen(outputPath, "wb");

  elf_header header;
  header.fileType = 0x02; // NOTE(Isaac): executable file
  header.entryPoint = 0x63; // random
  header.programHeaderOffset = 0x40;  // NOTE(Isaac): right after the header
  header.sectionHeaderOffset = 0x60; // random
  header.numProgramHeaderEntries = 0x0; // TODO
  header.numSectionHeaderEntries = 0x4; // TODO
  header.sectionNameIndexInSectionHeader = 0x0; // TODO
  GenerateHeader(file, header);

  fclose(file);
}
