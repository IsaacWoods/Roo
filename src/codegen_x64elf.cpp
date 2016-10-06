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

static void GenerateHeader(FILE* f, elf_header& header)
{
  // --- Generate ELF Header ---
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
/*0x36*/fputc(0x10    , f); // Specify the size of an entry in the program header TODO
        fputc(0x00    , f);
/*0x38*/fwrite(&(header.numProgramHeaderEntries), sizeof(uint16_t), 1, f);
/*0x3A*/fputc(0x10    , f); // Specify the size of an entry in the section header TODO
        fputc(0x00    , f);
/*0x3C*/fwrite(&(header.numSectionHeaderEntries), sizeof(uint16_t), 1, f);
/*0x3E*/fwrite(&(header.sectionNameIndexInSectionHeader), sizeof(uint16_t), 1, f);

  // --- Generate Program Header ---
  // TODO

  // --- Generate Section Header
  // TODO
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
