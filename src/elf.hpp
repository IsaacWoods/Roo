/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

/*
 * XXX: This is far more coupled to the rest of the compiler than I'd like (given that all we're doing here is
 * encoding and decoding ELF files, but this isn't a priority
 */

#include <cstdio>
#include <common.hpp>
#include <ir.hpp>
#include <air.hpp>
#include <codegen.hpp>

struct ElfFile;

enum ElfFileType : uint16_t
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

struct ElfHeader
{
  ElfFileType fileType;
  uint64_t    entryPoint;
  uint64_t    programHeaderOffset;
  uint64_t    sectionHeaderOffset;
  uint16_t    numProgramHeaderEntries;
  uint16_t    numSectionHeaderEntries;
  uint16_t    sectionWithSectionNames;
};

/*
 * The string passed to this is duplicated and then managed separately.
 */
struct ElfString
{
  ElfString(ElfFile& elf, const char* str);
  ~ElfString();

  unsigned int  offset;
  char*         str;
};

#define SEGMENT_ATTRIB_X        0x1         // Marks the segment as executable
#define SEGMENT_ATTRIB_W        0x2         // Marks the segment as writable
#define SEGMENT_ATTRIB_R        0x4         // Marks the segment as readable
#define SEGMENT_ATTRIB_MASKOS   0x00FF0000
#define SEGMENT_ATTRIB_MASKPROC 0xFF000000

struct ElfSegment
{
  enum Type : uint32_t
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

  ElfSegment(ElfFile& elf, Type type, uint32_t flags, uint64_t address, uint64_t alignment, bool isMappedDirectly = true);
  ~ElfSegment() { }

  Type      type;
  uint32_t  flags;
  uint64_t  offset;
  uint64_t  virtualAddress;
  uint64_t  physicalAddress;
  uint64_t  alignment;        // NOTE(Isaac): `virtualAddress` should equal `offset`, modulo this alignment

  union
  {
    struct
    {
      uint64_t inFile;
      uint64_t inImage;
    } map;            // NOTE(Isaac): valid if `isMappedDirectly` is false
    uint64_t inFile;  // NOTE(Isaac): valid if `isMappedDirectly` is true
  } size;

  /*
   * True if the size in the memory image is the same as in the file.
   * This isn't true for things like BSS segments that are expanded during loading.
   */
  bool isMappedDirectly;
};

#define SECTION_ATTRIB_W        0x1         // Marks the section as writable
#define SECTION_ATTRIB_A        0x2         // Marks the section to be allocated in the memory image
#define SECTION_ATTRIB_E        0x4         // Marks the section as executable
#define SECTION_ATTRIB_MASKOS   0x0F000000  // Environment-specific
#define SECTION_ATTRIB_MASKPROC 0xF0000000  // Processor-specific

struct ElfThing;
struct ElfSection
{
  enum Type : uint32_t
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

  ElfSection(ElfFile& elf, const char* name, Type type, uint64_t alignment, bool addtoElf=true);
  ~ElfSection() { }

  ElfString*    name;       // Offset in the string table to the section name
  Type          type;
  uint64_t      flags;
  uint64_t      address;    // Virtual address of the beginning of the section in memory
  uint64_t      offset;     // Offset of the beginning of the section in the file
  uint64_t      size;       // NOTE(Isaac): for SHT_NOBITS, this is NOT the size in the file!
  uint32_t      link;       // Depends on section type
  uint32_t      info;       // Also depends on section type
  uint64_t      alignment;  // Required alignment of this section
  uint64_t      entrySize;  // Size of each section entry (if fixed, zero otherwise)

  unsigned int  index;      // Index in the section header
  unsigned int  nameOffset; // NOTE(Isaac): Used when parsing external objects before we can parse the string table

  /*
   * For sections of type SHT_PROGBITS, this is the list of things that should be emitted into the section.
   */
  std::vector<ElfThing*> things;
};

struct ElfMapping
{
  ElfSegment* segment;
  ElfSection* section;
};

struct ElfSymbol
{
  enum Binding : uint8_t
  {
    SYM_BIND_LOCAL      = 0x0,
    SYM_BIND_GLOBAL     = 0x1,
    SYM_BIND_WEAK       = 0x2,
    SYM_BIND_LOOS       = 0xA,
    SYM_BIND_HIOS       = 0xC,
    SYM_BIND_LOPROC     = 0xD,
    SYM_BIND_HIGHPROC   = 0xF,
  };

  enum Type : uint8_t
  {
    SYM_TYPE_NONE,
    SYM_TYPE_OBJECT,
    SYM_TYPE_FUNCTION,
    SYM_TYPE_SECTION,
    SYM_TYPE_FILE,
    SYM_TYPE_LOOS,
    SYM_TYPE_HIOS,
    SYM_TYPE_LOPROC,
    SYM_TYPE_HIPROC,
  };

  ElfSymbol(ElfFile& elf, const char* name, Binding binding, Type type, uint16_t sectionIndex, uint64_t value);
  ~ElfSymbol() { }

  ElfString*    name;
  uint8_t       info;
  uint16_t      sectionIndex;
  uint64_t      value;
  uint64_t      size;

  unsigned int  index;   // Index of this symbol in the symbol table
};

/*
 * Describes a piece of relocatable data that can be moved around and stuff.
 */
struct ElfThing
{
  ElfThing(ElfSection* section, ElfSymbol* symbol);
  ~ElfThing();

  ElfSymbol*    symbol;
  unsigned int  length;
  unsigned int  capacity;
  uint8_t*      data;  // NOTE(Isaac): this should be `capacity` elements long

  unsigned int  fileOffset;
  unsigned int  address;
};

struct ElfRelocation
{
  enum Type : uint32_t
  {
    R_X86_64_64   = 1u,
    R_X86_64_PC32 = 2u,
    R_X86_64_32   = 10u,
  };

  ElfRelocation(ElfFile& elf, ElfThing* thing, uint64_t offset, Type type, ElfSymbol* symbol, int64_t addend, const LabelInstruction* label = nullptr);
  ~ElfRelocation() { }

  ElfThing*   thing;
  uint64_t    offset;   // NOTE(Isaac): this is relative to the beginning of `thing`
  Type        type;
  ElfSymbol*  symbol;

  /*
   * NOTE(Isaac): the final addend will be the sum of the offset from the label (if not null) and the constant
   */
  int64_t                 addend;
  const LabelInstruction* label;
};

// XXX(Isaac): do not call this directly!
template<typename T>
void Emit_(ElfThing* thing, T);

template<typename T>
void Emit(ElfThing* thing, T t)
{
  const unsigned int THING_EXPAND_CONSTANT = 2u;

  if ((thing->length + sizeof(T)) >= thing->capacity)
  {
    thing->capacity *= THING_EXPAND_CONSTANT;
    thing->data = static_cast<uint8_t*>(realloc(thing->data, thing->capacity));
  }

  Emit_(thing, t);
}

struct ElfFile
{
  ElfFile(TargetMachine& target, bool isRelocatable);
  ~ElfFile() { }

  bool                        isRelocatable;
  TargetMachine*              target;

  ElfHeader                   header;
  std::vector<ElfSegment*>    segments;
  std::vector<ElfSection*>    sections;
  std::vector<ElfSymbol*>     symbols;
  std::vector<ElfString*>     strings;
  std::vector<ElfMapping>     mappings;
  std::vector<ElfRelocation*> relocations;
  unsigned int                stringTableTail; // Tail of the string table, relative to the start of the table
  unsigned int                numSymbols;
};

ElfSection* GetSection(ElfFile& elf, const char* name);
ElfSymbol* GetSymbol(ElfFile& elf, const char* name);
void MapSection(ElfFile& elf, ElfSegment* segment, ElfSection* section);
void LinkObject(ElfFile& elf, const char* objectPath);
void WriteElf(ElfFile& elf, const char* path);
