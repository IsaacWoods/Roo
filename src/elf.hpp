/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <cstdio>
#include <common.hpp>

enum elf_file_type : uint16_t
{
  
};

struct elf_header
{
  elf_file_type fileType;
  uint64_t      entryPoint;
  uint64_t      sectionHeaderOffset;
  uint16_t      numSectionHeaderEntries;
  uint16_t      sectionWithSectionNames;
};

#define SECTION_ATTRIB_W        0x1         // Marks the section as writable
#define SECTION_ATTRIB_A        0x2         // Marks the section to be allocated in the memory image
#define SECTION_ATTRIB_E        0x4         // Marks the section as executable
#define SECTION_ATTRIB_MASKOS   0x0F000000  // Environment-specific
#define SECTION_ATTRIB_MASKPROC 0xF0000000  // Processor-specific

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
};

struct elf_section
{
  uint32_t      name;               // Offset in the string table to the section name
  section_type  type;
  uint64_t      flags;
  uint64_t      address;            // Virtual address of the beginning of the section in memory
  uint64_t      offset;             // Offset of the beginning of the section in the file
  uint64_t      size;               // NOTE(Isaac): for SHT_NOBITS, this is NOT the size in the file!
  uint32_t      link;               // Depends on section type
  uint32_t      info;               // Also depends on section type
  uint64_t      addressAlignment;   // Required alignment of this section
  uint64_t      entrySize;          // Size of each section entry (if fixed, zero otherwise)
};

struct elf_relocation
{
  uint64_t  offset;
  uint64_t  info;
  int64_t   addend;
};

enum symbol_binding : uint8_t
{
  SYM_BIND_LOCAL      = 0x0,
  SYM_BIND_GLOBAL     = 0x1,
  SYM_BIND_WEAK       = 0x2,
  SYM_BIND_LOOS       = 0xA,
  SYM_BIND_HIOS       = 0xC,
  SYM_BIND_LOPROC     = 0xD,
  SYM_BIND_HIGHPROC   = 0xF,
};

/*
 * Describes a piece of relocatable data that can be moved around and stuff.
 */
struct elf_thing
{
  unsigned int length;
  unsigned int capacity;
  uint8_t* data;  // NOTE(Isaac): this should be `capacity` elements long

  unsigned int fileOffset;
  unsigned int address;
};

struct elf_symbol;

struct elf_file
{
  elf_header header;
  linked_list<elf_thing*> things;
  linked_list<elf_symbol*> symbols;
  linked_list<const char*> strings;

  unsigned int stringTableTail; // Tail of the string table, relative to the start of the table
};

void CreateElf(elf_file& elf);
void LinkElf(elf_file& elf);
void FreeElf(elf_file& elf);
