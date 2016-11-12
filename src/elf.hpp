/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <cstdio>
#include <common.hpp>
#include <ir.hpp>

enum elf_file_type : uint16_t
{
  ET_NONE   = 0x0,
  ET_REL    = 0x1,
  ET_EXEC   = 0x2,
  ET_DYN    = 0x3,
  ET_CORE   = 0x4,
  ET_LOOS   = 0xFE00,
  ET_HIOS   = 0xFEFF,
  ET_LOPROC = 0xFF00,
  ET_HIPROC = 0xFFFF
};

struct elf_header
{
  elf_file_type fileType;
  uint64_t      entryPoint;
  uint64_t      programHeaderOffset;
  uint64_t      sectionHeaderOffset;
  uint16_t      numProgramHeaderEntries;
  uint16_t      numSectionHeaderEntries;
  uint16_t      sectionWithSectionNames;
};

struct elf_string
{
  unsigned int  offset;
  const char*   str;
};

#define SEGMENT_ATTRIB_X        0x1         // Marks the segment as executable
#define SEGMENT_ATTRIB_W        0x2         // Marks the segment as writable
#define SEGMENT_ATTRIB_R        0x4         // Marks the segment as readable
#define SEGMENT_ATTRIB_MASKOS   0x00FF0000
#define SEGMENT_ATTRIB_MASKPROC 0xFF000000

enum segment_type : uint32_t
{
  PT_NULL,
  PT_LOAD,
  PT_DYNAMIC,
  PT_INTERP,
  PT_NOTE,
  PT_SHLIB,
  PT_PHDR,
  PT_TLS,
  PT_LOOS     = 0x60000000,
  PT_HIOS     = 0x6FFFFFFF,
  PT_LOPROC   = 0x70000000,
  PT_HIPROC   = 0x7FFFFFFF
};

struct elf_segment
{
  segment_type  type;
  uint32_t      flags;
  uint64_t      offset;
  uint64_t      virtualAddress;
  uint64_t      physicalAddress;
  uint64_t      fileSize;         // Number of bytes in the *file* allocated to this segment
  uint64_t      memorySize;       // Number of bytes in the *image* allocated to this segment
  uint64_t      alignment;        // NOTE(Isaac): `virtualAddress` should equal `offset`, modulo this alignment
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
  elf_string*   name;       // Offset in the string table to the section name
  section_type  type;
  uint64_t      flags;
  uint64_t      address;    // Virtual address of the beginning of the section in memory
  uint64_t      offset;     // Offset of the beginning of the section in the file
  uint64_t      size;       // NOTE(Isaac): for SHT_NOBITS, this is NOT the size in the file!
  uint32_t      link;       // Depends on section type
  uint32_t      info;       // Also depends on section type
  uint64_t      alignment;  // Required alignment of this section
  uint64_t      entrySize;  // Size of each section entry (if fixed, zero otherwise)

  unsigned int index;       // Index in the section header
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

enum symbol_type : uint8_t
{
  SYM_TYPE_NONE,
  SYM_TYPE_OBJECT,
  SYM_TYPE_FUNCTION,
  SYM_TYPE_SECTION,
};

struct elf_symbol
{
  elf_string* name;
  uint8_t     info;
  uint16_t    sectionIndex;
  uint64_t    value;
  uint64_t    size;
};

/*
 * Describes a piece of relocatable data that can be moved around and stuff.
 */
struct elf_thing
{
  elf_symbol* symbol;
  unsigned int length;
  unsigned int capacity;
  uint8_t* data;  // NOTE(Isaac): this should be `capacity` elements long

  unsigned int fileOffset;
  unsigned int address;
};

// NOTE(Isaac): do not call this directly!
template<typename T>
void Emit_(elf_thing* thing, T);

template<typename T>
void Emit(elf_thing* thing, T t)
{
  const unsigned int THING_EXPAND_CONSTANT = 2u;

  if ((thing->length + sizeof(T)) >= thing->capacity)
  {
    thing->capacity *= THING_EXPAND_CONSTANT;
    thing->data = static_cast<uint8_t*>(realloc(thing->data, thing->capacity));
  }

  Emit_(thing, t);
}

struct elf_symbol;

struct elf_file
{
  codegen_target*           target;
  elf_header                header;
  linked_list<elf_segment*> segments;
  linked_list<elf_section*> sections;
  linked_list<elf_thing*>   things;
  linked_list<elf_symbol*>  symbols;
  linked_list<elf_string*>  strings;
  unsigned int              stringTableTail; // Tail of the string table, relative to the start of the table
};

void CreateElf(elf_file& elf, codegen_target& target);
elf_symbol* CreateSymbol(elf_file& elf, const char* name, symbol_binding binding, symbol_type type, uint16_t sectionIndex, uint64_t value);
elf_thing* CreateThing(elf_file& elf, const char* name);
elf_segment* CreateSegment(elf_file& elf, segment_type type, uint64_t address, uint64_t alignment);
elf_section* CreateSection(elf_file& elf, const char* name, section_type type, uint64_t alignment);
elf_section* GetSection(elf_file& elf, const char* name);
void WriteElf(elf_file& elf, const char* path);
