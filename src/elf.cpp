/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <elf.hpp>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdarg>
#include <climits>
#include <cassert>
#include <error.hpp>

#define PROGRAM_HEADER_ENTRY_SIZE 0x38
#define SECTION_HEADER_ENTRY_SIZE 0x40
#define SYMBOL_TABLE_ENTRY_SIZE   0x18

static const char* GetSectionTypeName(section_type type)
{
  switch (type)
  {
    case SHT_NULL:          return "SHT_NULL";
    case SHT_PROGBITS:      return "SHT_PROGBITS";
    case SHT_SYMTAB:        return "SHT_SYMTAB";
    case SHT_STRTAB:        return "SHT_STRTAB"; 
    case SHT_RELA:          return "SHT_RELA"; 
    case SHT_HASH:          return "SHT_HASH"; 
    case SHT_DYNAMIC:       return "SHT_DYNAMIC"; 
    case SHT_NOTE:          return "SHT_NOTE"; 
    case SHT_NOBITS:        return "SHT_NOBITS"; 
    case SHT_REL:           return "SHT_REL"; 
    case SHT_SHLIB:         return "SHT_SHLIB"; 
    case SHT_DYNSYM:        return "SHT_DYNSYM"; 
    case SHT_INIT_ARRAY:    return "SHT_INIT_ARRAY"; 
    case SHT_FINI_ARRAY:    return "SHT_FINI_ARRAY"; 
    case SHT_PREINIT_ARRAY: return "SHT_PREINIT_ARRAY"; 
    case SHT_GROUP:         return "SHT_GROUP"; 
    case SHT_SYMTAB_SHNDX:  return "SHT_SYMTAB_SHNDX"; 
    case SHT_LOOS:          return "SHT_LOOS"; 
    case SHT_HIOS:          return "SHT_HIOS"; 
    case SHT_LOPROC:        return "SHT_LOPROC"; 
    case SHT_HIPROC:        return "SHT_HIPROC"; 
    case SHT_LOUSER:        return "SHT_LOUSER"; 
    case SHT_HIUSER:        return "SHT_HIUSER"; 
  }

  return nullptr;
}

/*
 * NOTE(Isaac): the string is duplicated and freed separately of the passed string.
 */
static elf_string* CreateString(elf_file& elf, const char* str)
{
  elf_string* string = static_cast<elf_string*>(malloc(sizeof(elf_string)));
  string->offset = elf.stringTableTail;
  string->str = strdup(str);

  elf.stringTableTail += strlen(str) + 1u;

  Add<elf_string*>(elf.strings, string);
  return string;
}

/*
 * NOTE(Isaac): If `name == nullptr`, the symbol points towards the nulled entry of the string table.
 */
elf_symbol* CreateSymbol(elf_file& elf, const char* name, symbol_binding binding, symbol_type type,
                                uint16_t sectionIndex, uint64_t value)
{
  elf_symbol* symbol = static_cast<elf_symbol*>(malloc(sizeof(elf_symbol)));
  symbol->name = (name ? CreateString(elf, name) : nullptr);
  symbol->info = type;
  symbol->info |= binding << 4u;
  symbol->sectionIndex = sectionIndex;
  symbol->value = value;
  symbol->size = 0u;

  symbol->index = elf.numSymbols + 1u;

  // Set the `info` field of the symbol table to the index of the first GLOBAL symbol
  if (GetSection(elf, ".symtab")->info == 0u && binding == SYM_BIND_GLOBAL)
  {
    GetSection(elf, ".symtab")->info = elf.numSymbols;
  }

  elf.numSymbols++;
  Add<elf_symbol*>(elf.symbols, symbol);
  return symbol;
}

void CreateRelocation(elf_file& elf, elf_thing* thing, uint64_t offset, relocation_type type, elf_symbol* symbol, int64_t addend, const instruction_label* label)
{
  elf_relocation* relocation = static_cast<elf_relocation*>(malloc(sizeof(elf_relocation)));
  relocation->thing   = thing;
  relocation->offset  = offset;
  relocation->type    = type;
  relocation->symbol  = symbol;
  relocation->addend  = addend;
  relocation->label   = label;

  Add<elf_relocation*>(elf.relocations, relocation);
}

elf_segment* CreateSegment(elf_file& elf, segment_type type, uint32_t flags, uint64_t address, uint64_t alignment, bool isMappedDirectly)
{
  elf_segment* segment        = static_cast<elf_segment*>(malloc(sizeof(elf_segment)));
  segment->type               = type;
  segment->flags              = flags;
  segment->offset             = 0u;
  segment->virtualAddress     = address;
  segment->physicalAddress    = address;
  segment->alignment          = alignment;
  segment->size.map           = {};  // NOTE(Isaac): this will zero-initialise the whole union
  segment->isMappedDirectly   = isMappedDirectly;

  elf.header.numProgramHeaderEntries++;
  Add<elf_segment*>(elf.segments, segment);
  return segment;
}

elf_section* CreateSection(elf_file& elf, const char* name, section_type type, uint64_t alignment)
{
  elf_section* section  = static_cast<elf_section*>(malloc(sizeof(elf_section)));
  section->name         = CreateString(elf, name);
  section->type         = type;
  section->flags        = 0u;
  section->address      = 0u;
  section->offset       = 0u;
  section->size         = 0u;
  section->link         = 0u;
  section->info         = 0u;
  section->alignment    = alignment;
  section->entrySize    = 0u;

  // NOTE(Isaac): section indices begin at 1
  section->index = (elf.sections.size > 0u ? elf.sections[elf.sections.size - 1u]->index + 1u : 1u);

  elf.header.numSectionHeaderEntries++;
  Add<elf_section*>(elf.sections, section);
  return section;
}

#define INITIAL_DATA_SIZE 256u
elf_thing* CreateThing(elf_file& elf, elf_symbol* symbol)
{
  elf_thing* thing    = static_cast<elf_thing*>(malloc(sizeof(elf_thing)));
  thing->symbol       = symbol;
  thing->length       = 0u;
  thing->capacity     = INITIAL_DATA_SIZE;
  thing->data         = static_cast<uint8_t*>(malloc(sizeof(uint8_t) * INITIAL_DATA_SIZE));
  thing->fileOffset   = 0u;
  thing->address      = 0x0;

  Add<elf_thing*>(elf.things, thing);
  return thing;
}

elf_thing* CreateRodataThing(elf_file& elf)
{
  elf_thing* thing    = static_cast<elf_thing*>(malloc(sizeof(elf_thing)));
  thing->symbol       = CreateSymbol(elf, nullptr, SYM_BIND_GLOBAL, SYM_TYPE_SECTION, GetSection(elf, ".rodata")->index, 0x00);
  thing->length       = 0u;
  thing->capacity     = INITIAL_DATA_SIZE;
  thing->data         = static_cast<uint8_t*>(malloc(sizeof(uint8_t) * INITIAL_DATA_SIZE));
  thing->fileOffset   = 0u;
  thing->address      = 0x0;

  return thing;
}
#undef INITIAL_DATA_SIZE

elf_section* GetSection(elf_file& elf, const char* name)
{
  for (auto* it = elf.sections.head;
       it < elf.sections.tail;
       it++)
  {
    elf_section* section = *it;
    if (strcmp(section->name->str, name) == 0)
    {
      return section;
    }
  }

  fprintf(stderr, "WARNING: Couldn't find section of name '%s'!\n", name);
  return nullptr;
}

elf_symbol* GetSymbol(elf_file& elf, const char* name)
{
  for (auto* it = elf.symbols.head;
       it < elf.symbols.tail;
       it++)
  {
    elf_symbol* symbol = *it;

    if (!(symbol->name))
    {
      continue;
    }

    if (strcmp(symbol->name->str, name) == 0)
    {
      return symbol;
    }
  }

  fprintf(stderr, "WARNING: Couldn't find symbol of name: '%s'!\n", name);
  return nullptr;
}

void MapSection(elf_file& elf, elf_segment* segment, elf_section* section)
{
  elf_mapping mapping;
  mapping.segment = segment;
  mapping.section = section;

  Add<elf_mapping>(elf.mappings, mapping);
}

struct elf_object
{
  const char*               path;
  FILE*                     f;
  vector<elf_section*>      sections;
  vector<elf_symbol*>       symbols;      // NOTE(Isaac): contents should not be freed
  vector<elf_relocation*>   relocations;  // NOTE(Isaac): contents should not be freed
  vector<elf_thing*>        things;       // NOTE(Isaac): contents should not be freed

  elf_symbol**              symbolRemaps;
  unsigned int              numRemaps;
};

template<>
void Free<elf_object>(elf_object& object)
{
  fclose(object.f);
  FreeVector<elf_section*>(object.sections);
  DetachVector<elf_symbol*>(object.symbols);
  DetachVector<elf_relocation*>(object.relocations);
  DetachVector<elf_thing*>(object.things);
  free(object.symbolRemaps);
}

static elf_string* ExtractString(elf_file& elf, elf_object& object, const elf_section* stringTable, uint64_t stringOffset)
{
  assert(stringTable);

  if (stringOffset == 0u)
  {
    return nullptr;
  }

  fseek(object.f, stringTable->offset + stringOffset, SEEK_SET);
  char buffer[1024u];
  unsigned int length = 0u;

  for (char c = fgetc(object.f);
       c;
       c = fgetc(object.f))
  {
    buffer[length++] = c;
  }

  buffer[length++] = '\0';
  char* str = static_cast<char*>(malloc(sizeof(char) * length));
  memcpy(str, buffer, sizeof(char) * length);

  elf_string* string = CreateString(elf, str);
  free(str);
  return string;
}

static void ParseSectionHeader(elf_file& elf, elf_object& object)
{
  uint64_t sectionHeaderOffset;
  fseek(object.f, 0x28, SEEK_SET);
  fread(&sectionHeaderOffset, sizeof(uint64_t), 1, object.f);

  uint16_t numSectionHeaders;
  fseek(object.f, 0x3C, SEEK_SET);
  fread(&numSectionHeaders, sizeof(uint16_t), 1, object.f);

  fseek(object.f, sectionHeaderOffset, SEEK_SET);

  for (unsigned int i = 0u;
       i < numSectionHeaders;
       i++)
  {
    elf_section* section = static_cast<elf_section*>(malloc(sizeof(elf_section)));
    section->index = i; // NOTE(Isaac): this is the index in the *external object*, not our executable
 
    /*
     * NOTE(Isaac): We don't know where the string table is yet, so we can't load the section names for now
     */
    section->name = nullptr;

    /*0x00*/fread(&(section->nameOffset), sizeof(uint32_t), 1, object.f);
    /*0x04*/fread(&(section->type),       sizeof(uint32_t), 1, object.f);
    /*0x08*/fread(&(section->flags),      sizeof(uint64_t), 1, object.f);
    /*0x10*/fread(&(section->address),    sizeof(uint64_t), 1, object.f);
    /*0x18*/fread(&(section->offset),     sizeof(uint64_t), 1, object.f);
    /*0x20*/fread(&(section->size),       sizeof(uint64_t), 1, object.f);
    /*0x28*/fread(&(section->link),       sizeof(uint32_t), 1, object.f);
    /*0x2C*/fread(&(section->info),       sizeof(uint32_t), 1, object.f);
    /*0x30*/fread(&(section->alignment),  sizeof(uint64_t), 1, object.f);
    /*0x38*/fread(&(section->entrySize),  sizeof(uint64_t), 1, object.f);
    /*0x40*/

    Add<elf_section*>(object.sections, section);
  }

  uint16_t sectionWithNames;
  fseek(object.f, 0x3E, SEEK_SET);
  fread(&sectionWithNames, sizeof(uint16_t), 1, object.f);
  elf_section* stringTable = object.sections[sectionWithNames];

  for (auto* it = object.sections.head;
       it < object.sections.tail;
       it++)
  {
    elf_section* section = *it;
    section->name = ExtractString(elf, object, stringTable, section->nameOffset);
  }
}

static void ParseSymbolTable(error_state& errorState, elf_file& elf, elf_object& object, elf_section* table)
{
  if (table->entrySize != SYMBOL_TABLE_ENTRY_SIZE)
  {
    RaiseError(errorState, ERROR_WEIRD_LINKED_OBJECT, object.path, "Object has weirdly sized symbols");
  }
  
  unsigned int numSymbols = table->size / SYMBOL_TABLE_ENTRY_SIZE;
  object.numRemaps = numSymbols;
  object.symbolRemaps = static_cast<elf_symbol**>(malloc(sizeof(elf_symbol*) * numSymbols));
  memset(object.symbolRemaps, 0, sizeof(elf_symbol*) * numSymbols);

  // NOTE(Isaac): start at 1 to skip the nulled symbol at the beginning
  for (unsigned int i = 1u;
       i < numSymbols;
       i++)
  {
    fseek(object.f, table->offset + i * SYMBOL_TABLE_ENTRY_SIZE, SEEK_SET);
    const elf_section* stringTable = object.sections[table->link];
    elf_symbol* symbol = static_cast<elf_symbol*>(malloc(sizeof(elf_symbol)));
    uint32_t nameOffset;
  
    /*0x00*/fread(&nameOffset,              sizeof(uint32_t), 1, object.f);
    /*0x04*/fread(&(symbol->info),          sizeof(uint8_t) , 1, object.f);
    /*0x05*/fseek(object.f, ftell(object.f) + 0x01, SEEK_SET);
    /*0x06*/fread(&(symbol->sectionIndex),  sizeof(uint16_t), 1, object.f);
    /*0x08*/fread(&(symbol->value),         sizeof(uint64_t), 1, object.f);
    /*0x10*/fread(&(symbol->size),          sizeof(uint64_t), 1, object.f);
    /*0x18*/
  
    symbol->index = elf.numSymbols;

    /*
     * NOTE(Isaac): skip file symbols (we don't want them in the file executable)
     */
    if ((symbol->info & 0xf) == SYM_TYPE_FILE)
    {
      free(symbol);
      continue;
    }
  
    symbol->name = ExtractString(elf, object, stringTable, nameOffset);
  
    // Emit a remap from the index of the symbol in the relocatable object to the index in our symbol table
    object.symbolRemaps[i] = symbol;

    elf.numSymbols++;
    Add<elf_symbol*>(elf.symbols, symbol);
    Add<elf_symbol*>(object.symbols, symbol);
  }
}

static void ParseRelocationSection(error_state& errorState, elf_file& elf, elf_object& object, elf_section* section)
{
  const unsigned int RELOCATION_ENTRY_SIZE = 0x18;

  if (section->entrySize != RELOCATION_ENTRY_SIZE)
  {
    RaiseError(errorState, ERROR_WEIRD_LINKED_OBJECT, object.path);
  }

  unsigned int numRelocations = section->size / section->entrySize;

  for (unsigned int i = 0u;
       i < numRelocations;
       i++)
  {
    fseek(object.f, section->offset + i * RELOCATION_ENTRY_SIZE, SEEK_SET);
    elf_relocation* relocation = static_cast<elf_relocation*>(malloc(sizeof(elf_relocation)));
    relocation->thing = nullptr;

    uint64_t info;
    /*0x00*/fread(&(relocation->offset),  sizeof(uint64_t), 1, object.f);
    /*0x08*/fread(&info,                  sizeof(uint64_t), 1, object.f);
    /*0x10*/fread(&(relocation->addend),  sizeof(int64_t) , 1, object.f);
    /*0x18*/

    relocation->type = static_cast<relocation_type>(info & 0xffffffffL);
    relocation->symbol = object.symbolRemaps[(info >> 32u) & 0xffffffffL];

    if (!(relocation->symbol))
    {
      RaiseError(errorState, ERROR_UNRESOLVED_SYMBOL, "unknown (from external relocation section)");
    }

    Add<elf_relocation*>(elf.relocations, relocation);
    Add<elf_relocation*>(object.relocations, relocation);
  }
}

void LinkObject(elf_file& elf, const char* objectPath)
{
  error_state errorState = CreateErrorState(LINKING);

  elf_object object;
  object.path = objectPath;
  object.f = fopen(objectPath, "rb");
  InitVector<elf_section*>(object.sections);
  InitVector<elf_symbol*>(object.symbols);
  InitVector<elf_relocation*>(object.relocations);
  InitVector<elf_thing*>(object.things);

  if (!(object.f))
  {
    RaiseError(errorState, ERROR_WEIRD_LINKED_OBJECT, objectPath, "Failed to open file");
  }

  #define CONSUME(byte) \
    if (fgetc(object.f) != byte) \
    { \
      RaiseError(errorState, ERROR_WEIRD_LINKED_OBJECT, objectPath); \
    }

  // Check the magic
  CONSUME(0x7F);
  CONSUME('E');
  CONSUME('L');
  CONSUME('F');

  // Check that it's a relocatable object
  uint16_t fileType;
  fseek(object.f, 0x10, SEEK_SET);
  fread(&fileType, sizeof(uint16_t), 1, object.f);

  if (fileType != ET_REL)
  {
    RaiseError(errorState, ERROR_WEIRD_LINKED_OBJECT, objectPath);
  }

  // Parse the section header
  ParseSectionHeader(elf, object);
  for (auto* it = object.sections.head;
       it < object.sections.tail;
       it++)
  {
    elf_section* section = *it;

    switch (section->type)
    {
      case SHT_SYMTAB:
      {
        ParseSymbolTable(errorState, elf, object, section);
      } break;

      case SHT_REL:
      {
        RaiseError(errorState, ICE_GENERIC, "SHT_REL sections are not supported, use SHT_RELA sections instead");
      } break;

      case SHT_RELA:
      {
        ParseRelocationSection(errorState, elf, object, section);
      } break;

      default:
      {
//        RaiseError(errorState, NOTE_IGNORED_ELF_SECTION, GetSectionTypeName(section->type));
      } break;
    }
  }

  // Find the .text section
  elf_section* text = nullptr;
  for (auto* it = object.sections.head;
       it < object.sections.tail;
       it++)
  {
    elf_section* section = *it;
    if (!(section->name))
    {
      continue;
    }

    if (strcmp(section->name->str, ".text") == 0)
    {
      text = section;
      break;
    }
  }

  /*
   * NOTE(Isaac): this *borrows* symbols from the actual symbol table - don't free it, just detach it!
   */
  vector<elf_symbol*> functionSymbols;
  InitVector<elf_symbol*>(functionSymbols);

  // Extract functions from the symbol table and their code from .text
  for (auto* it = object.symbols.head;
       it < object.symbols.tail;
       it++)
  {
    elf_symbol* symbol = *it;;

    /*
     * NOTE(Isaac): NASM refuses to emit symbols for functions with the correct type,
     * so assume that symbols with no type that are in .text are also functions
     */
    if (((symbol->info & 0xf) == SYM_TYPE_FUNCTION) ||
        ((symbol->info & 0xf) == SYM_TYPE_NONE && symbol->sectionIndex == text->index))
    {
      Add<elf_symbol*>(functionSymbols, symbol);
    }
  }

  // Sort the linked list by the symbols' offsets into .text
  SortVector<elf_symbol*>(functionSymbols, [](elf_symbol*& a, elf_symbol*& b)
    {
      return (a->value < b->value);
    });

  // Extract functions from the external object and link them into ourselves
  for (auto* it = functionSymbols.head;
       it < functionSymbols.tail;
       it++)
  {
    elf_symbol* symbol = *it;
    assert(symbol->name); // NOTE(Isaac): functions should probably always have names

    if ((it + 1) < functionSymbols.tail)
    {
      symbol->size = (*(it + 1))->value - symbol->value;
    }
    else
    {
      symbol->size = text->size - symbol->value;
    }

    // Create a thing for the extracted function
    elf_thing* thing  = static_cast<elf_thing*>(malloc(sizeof(elf_thing)));
    thing->symbol     = CreateSymbol(elf, symbol->name->str, SYM_BIND_GLOBAL, SYM_TYPE_FUNCTION, GetSection(elf, ".text")->index, 0u);
    thing->length     = symbol->size;
    thing->capacity   = symbol->size;
    thing->data       = static_cast<uint8_t*>(malloc(sizeof(uint8_t) * symbol->size));
    thing->fileOffset = 0u;
    thing->address    = symbol->value;  // NOTE(Isaac): this is the original offset from the file

    /*
     * NOTE(Isaac): The symbol's value currently points to the offset in the external object's .text section
     */
    fseek(object.f, text->offset + symbol->value, SEEK_SET);
    fread(thing->data, sizeof(uint8_t), symbol->size, object.f);
    Add<elf_thing*>(elf.things, thing);
    Add<elf_thing*>(object.things, thing);

    /*
     * We create a new symbol for the function, so remove the old one
     */
    StableRemove<elf_symbol*>(elf.symbols, symbol);
  }

  // Complete the relocations loaded from the external object
  for (auto* it = object.relocations.head;
       it < object.relocations.tail;
       it++)
  {
    elf_relocation* relocation = *it;

    for (auto* thingIt = object.things.head;
         thingIt < object.things.tail;
         thingIt++)
    {
      elf_thing* thing = *thingIt;

      // NOTE(Isaac): the address should still be the offset in the external object's .text section
      if ((relocation->offset >= thing->address) &&
          (relocation->offset < thing->address + thing->length))
      {
        relocation->thing = thing;
        continue;
      }
    }
  }

  DetachVector<elf_symbol*>(functionSymbols);
  Free<elf_object>(object);
  #undef CONSUME
}

template<>
void Emit_<uint8_t>(elf_thing* thing, uint8_t byte)
{
  thing->data[thing->length++] = byte;
}

template<>
void Emit_<uint32_t>(elf_thing* thing, uint32_t i)
{
  thing->data[thing->length++] = (i >> 0u)  & 0xff;
  thing->data[thing->length++] = (i >> 8u)  & 0xff;
  thing->data[thing->length++] = (i >> 16u) & 0xff;
  thing->data[thing->length++] = (i >> 24u) & 0xff;
}

template<>
void Emit_<uint64_t>(elf_thing* thing, uint64_t i)
{
  thing->data[thing->length++] = (i >> 0u)  & 0xff;
  thing->data[thing->length++] = (i >> 8u)  & 0xff;
  thing->data[thing->length++] = (i >> 16u) & 0xff;
  thing->data[thing->length++] = (i >> 24u) & 0xff;
  thing->data[thing->length++] = (i >> 32u) & 0xff;
  thing->data[thing->length++] = (i >> 40u) & 0xff;
  thing->data[thing->length++] = (i >> 48u) & 0xff;
  thing->data[thing->length++] = (i >> 56u) & 0xff;
}

static void EmitHeader(FILE* f, elf_header& header)
{
  /*
   * NOTE(Isaac): these are already defines, but `fwrite` is stupid, so it has to be a lvalue too
   */
  const uint16_t programHeaderEntrySize = PROGRAM_HEADER_ENTRY_SIZE;
  const uint16_t sectionHeaderEntrySize = SECTION_HEADER_ENTRY_SIZE;

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

static void EmitProgramEntry(FILE* f, elf_segment* segment)
{
/*n + */
/*0x00*/fwrite(&(segment->type),              sizeof(uint32_t), 1, f);
/*0x04*/fwrite(&(segment->flags),             sizeof(uint32_t), 1, f);
/*0x08*/fwrite(&(segment->offset),            sizeof(uint64_t), 1, f);
/*0x10*/fwrite(&(segment->virtualAddress),    sizeof(uint64_t), 1, f);
/*0x18*/fwrite(&(segment->physicalAddress),   sizeof(uint64_t), 1, f);
  if (segment->isMappedDirectly)
  {
/*0x20*/fwrite(&(segment->size.inFile),       sizeof(uint64_t), 1, f);
/*0x28*/fwrite(&(segment->size.inFile),       sizeof(uint64_t), 1, f);
  }
  else
  {
/*0x20*/fwrite(&(segment->size.map.inFile),   sizeof(uint64_t), 1, f);
/*0x28*/fwrite(&(segment->size.map.inImage),  sizeof(uint64_t), 1, f);
  }
/*0x30*/fwrite(&(segment->alignment),         sizeof(uint64_t), 1, f);
/*0x38*/
}

static void EmitSectionEntry(FILE* f, elf_section* section)
{
/*n + */
/*0x00*/fwrite(&(section->name->offset),  sizeof(uint32_t), 1, f);
/*0x04*/fwrite(&(section->type),          sizeof(uint32_t), 1, f);
/*0x08*/fwrite(&(section->flags),         sizeof(uint64_t), 1, f);
/*0x10*/fwrite(&(section->address),       sizeof(uint64_t), 1, f);
/*0x18*/fwrite(&(section->offset),        sizeof(uint64_t), 1, f);
/*0x20*/fwrite(&(section->size),          sizeof(uint64_t), 1, f);
/*0x28*/fwrite(&(section->link),          sizeof(uint32_t), 1, f);
/*0x2C*/fwrite(&(section->info),          sizeof(uint32_t), 1, f);
/*0x30*/fwrite(&(section->alignment),     sizeof(uint64_t), 1, f);
/*0x38*/fwrite(&(section->entrySize),     sizeof(uint64_t), 1, f);
/*0x40*/
}

static void EmitSymbolTable(FILE* f, elf_file& elf, vector<elf_symbol*>& symbols)
{
  // Emit an empty symbol table entry, because the standard says so
  GetSection(elf, ".symtab")->size += SYMBOL_TABLE_ENTRY_SIZE;
  for (unsigned int i = 0u;
       i < SYMBOL_TABLE_ENTRY_SIZE;
       i++)
  {
    fputc(0x0, f);
  }

  for (auto* symbolIt = symbols.head;
       symbolIt < symbols.tail;
       symbolIt++)
  {
    elf_symbol* symbol = *symbolIt;
    GetSection(elf, ".symtab")->size += SYMBOL_TABLE_ENTRY_SIZE;

/*n + */
    if (symbol->name)
    {
/*0x00*/fwrite(&(symbol->name->offset), sizeof(uint32_t), 1, f);
    }
    else
    {
/*0x00*/fputc(0x00, f);
/*0x01*/fputc(0x00, f);
/*0x02*/fputc(0x00, f);
/*0x03*/fputc(0x00, f);
    }
/*0x04*/fwrite(&(symbol->info),         sizeof(uint8_t) , 1, f);
/*0x05*/fputc(0x00, f);   // NOTE(Isaac): the `st_other` field, which is marked as reserved
/*0x06*/fwrite(&(symbol->sectionIndex), sizeof(uint16_t), 1, f);
/*0x08*/fwrite(&(symbol->value),        sizeof(uint64_t), 1, f);
/*0x10*/fwrite(&(symbol->size),         sizeof(uint64_t), 1, f);
/*0x18*/
  }
}

static void EmitStringTable(FILE* f, elf_file& elf, vector<elf_string*>& strings)
{
  // Lead with a null terminator to mark the null-string
  fputc('\0', f);
  GetSection(elf, ".strtab")->size = 1u;

  for (auto* it = strings.head;
       it < strings.tail;
       it++)
  {
    const char* string = (*it)->str;
    GetSection(elf, ".strtab")->size += strlen(string) + 1u;

    // NOTE(Isaac): add 1 to also write the included null-terminator
    fwrite(string, sizeof(char), strlen(string) + 1u, f);
  }
}

static void EmitThing(FILE* f, elf_file& elf, elf_thing* thing, elf_section* section = nullptr)
{
  if (!section)
  {
    section = GetSection(elf, ".text");
  }

  section->size += thing->length;

  /*
   * For now, set the symbol's value relative to the start of the section, since we don't know the address yet
   */
  thing->symbol->value = ftell(f) - section->offset;
  thing->symbol->size = thing->length;
  thing->fileOffset = ftell(f);
  thing->address = section->address + (thing->fileOffset - section->offset);

  fwrite(thing->data, sizeof(uint8_t), thing->length, f);
}

/*
 * This resolves the symbols in the symbol table that are actually referencing symbols
 * that haven't been linked yet.
 */
static void ResolveUndefinedSymbols(error_state& errorState, elf_file& elf)
{
  for (auto* it = elf.symbols.head;
       it < elf.symbols.tail;
       it++)
  {
    elf_symbol* symbol = *it;
    bool symbolResolved = false;

    // No work needs to be done for defined symbols
    if (!(symbol->name) || symbol->sectionIndex != 0u)
    {
      continue;
    }

    for (auto* otherIt = elf.symbols.head;
         otherIt < elf.symbols.tail;
         otherIt++)
    {
      elf_symbol* otherSymbol = *otherIt;

      // NOTE(Isaac): don't coalesce with yourself!
      if ((symbol == otherSymbol) || !(otherSymbol->name))
      {
        continue;
      }

      if (strcmp(symbol->name->str, otherSymbol->name->str) == 0u)
      {
        // Coalesce the symbols!
        StableRemove<elf_symbol*>(elf.symbols, symbol);
        symbolResolved = true;

        // Point relocations that refer to the undefined symbol to its defined partner
        for (auto* relocationIt = elf.relocations.head;
             relocationIt < elf.relocations.tail;
             relocationIt++)
        {
          elf_relocation* relocation = *relocationIt;

          if (relocation->symbol->index == symbol->index)
          {
            relocation->symbol = otherSymbol;
          }
        }

        break;
      }
    }

    if (!symbolResolved)
    {
      RaiseError(errorState, ERROR_UNRESOLVED_SYMBOL, symbol->name->str);
    }
  }
}

const char* GetRelocationTypeName(relocation_type type)
{
  switch (type)
  {
    case R_X86_64_64:
    {
      return "R_X86_64_64";
    } break;

    case R_X86_64_PC32:
    {
      return "R_X86_64_PC32";
    } break;

    case R_X86_64_32:
    {
      return "R_X86_64_32";
    } break;

    default:
    {
      error_state errorState = CreateErrorState(GENERAL_STUFF);
      RaiseError(errorState, ICE_GENERIC, "Unhandled relocation type in GetRelocationTypeName");
    } break;
  }

  return nullptr;
}

static void CompleteRelocations(error_state& errorState, FILE* f, elf_file& elf)
{
  long int currentPosition = ftell(f);

  for (auto* it = elf.relocations.head;
       it < elf.relocations.tail;
       it++)
  {
    elf_relocation* relocation = *it;
    assert(relocation->thing);
    assert(relocation->symbol);

    // Go to the correct position in the ELF file to apply the relocation
    uint64_t target = relocation->thing->fileOffset + relocation->offset;
    fseek(f, target, SEEK_SET);

    int64_t addend = relocation->addend;

    if (relocation->label)
    {
      addend += static_cast<int64_t>(relocation->label->offset);
    }

    switch (relocation->type)
    {
      case R_X86_64_64:     // S + A
      {
        uint64_t value = relocation->symbol->value + addend;
        fwrite(&value, sizeof(uint64_t), 1, f);
      } break;

      case R_X86_64_PC32:   // S + A - P
      {
        uint32_t relocationPos = GetSection(elf, ".text")->address + target - GetSection(elf, ".text")->offset;
        uint32_t value = (relocation->symbol->value + addend) - relocationPos;
        fwrite(&value, sizeof(uint32_t), 1, f);
      } break;

      case R_X86_64_32:     // S + A
      {
        uint32_t value = relocation->symbol->value + addend;
        fwrite(&value, sizeof(uint32_t), 1, f);
      } break;

      default:
      {
        RaiseError(errorState, ICE_UNHANDLED_RELOCATION, GetRelocationTypeName(relocation->type));
      }
    }
  }

  fseek(f, currentPosition, SEEK_SET);
}

static void MapSectionsToSegments(elf_file& elf)
{
  for (auto* it = elf.mappings.head;
       it < elf.mappings.tail;
       it++)
  {
    elf_segment* segment = it->segment;
    elf_section* section = it->section;

    /*
     * Atm, we can't tell the size of a SHT_NOBITS section, because it's size in the file is a lie
     */
    assert(section->type != SHT_NOBITS);

    if (segment->isMappedDirectly)
    {
      section->address = segment->virtualAddress + segment->size.inFile;
      segment->size.inFile += section->size;
    }
    else
    {
      section->address = segment->virtualAddress + segment->size.map.inFile;
      segment->size.map.inFile += section->size;
      segment->size.map.inImage += section->size;
    }
  }
}

void CreateElf(elf_file& elf, codegen_target& target, bool isRelocatable)
{
  elf.isRelocatable = isRelocatable;
  elf.target = &target;

  InitVector<elf_thing*>(elf.things);
  InitVector<elf_symbol*>(elf.symbols);
  InitVector<elf_segment*>(elf.segments);
  InitVector<elf_section*>(elf.sections);
  InitVector<elf_string*>(elf.strings);
  InitVector<elf_mapping>(elf.mappings);
  InitVector<elf_relocation*>(elf.relocations);
  elf.stringTableTail = 1u;
  elf.numSymbols = 0u;
  elf.rodataThing = nullptr;

  elf.header.fileType = (isRelocatable ? ET_REL : ET_EXEC);
  elf.header.entryPoint = 0x0;
  elf.header.programHeaderOffset = 0x0;
  elf.header.sectionHeaderOffset = 0x0;
  elf.header.numProgramHeaderEntries = 0u;
  elf.header.numSectionHeaderEntries = 0u;
  elf.header.sectionWithSectionNames = 0u;
}

void WriteElf(elf_file& elf, const char* path)
{
  FILE* f = fopen(path, "wb");
  error_state errorState = CreateErrorState(LINKING);

  if (!f)
  {
    RaiseError(errorState, ERROR_INVALID_EXECUTABLE, path);
  }

  ResolveUndefinedSymbols(errorState, elf);

  // Leave space for the ELF header
  fseek(f, 0x40, SEEK_SET);

  // --- Emit all the things ---
  GetSection(elf, ".text")->offset = ftell(f);

  for (auto* it = elf.things.head;
       it < elf.things.tail;
       it++)
  {
    EmitThing(f, elf, *it);
  }

  // Manually emit the .rodata thing
  if (elf.rodataThing)
  {
    GetSection(elf, ".rodata")->offset = ftell(f);
    EmitThing(f, elf, elf.rodataThing, GetSection(elf, ".rodata"));
  }

  MapSectionsToSegments(elf);

  // Recalculate symbol values
  uint64_t textAddress = GetSection(elf, ".text")->address;
  for (auto* it = elf.things.head;
       it < elf.things.tail;
       it++)
  {
    (*it)->symbol->value += textAddress;
  }

  // Calculate virtual address of .rodata symbol
  elf.rodataThing->symbol->value = GetSection(elf, ".rodata")->address;

  // --- Emit the string table ---
  GetSection(elf, ".strtab")->offset = ftell(f);
  elf.header.sectionWithSectionNames = GetSection(elf, ".strtab")->index;
  EmitStringTable(f, elf, elf.strings);

  // --- Emit the symbol table ---
  GetSection(elf, ".symtab")->offset = ftell(f);
  EmitSymbolTable(f, elf, elf.symbols);

  // --- Do all the relocations and find the entry point ---
  CompleteRelocations(errorState, f, elf);

  if (!(elf.isRelocatable))
  {
    elf_symbol* startSymbol = GetSymbol(elf, "_start");
    if (!startSymbol)
    {
      RaiseError(errorState, ERROR_NO_START_SYMBOL);
    }
    elf.header.entryPoint = startSymbol->value;
  }

  // --- Emit the section header ---
  elf.header.sectionHeaderOffset = ftell(f);

  // Emit an empty section header entry for reasons
  elf.header.numSectionHeaderEntries++;
  for (unsigned int i = 0u;
       i < SECTION_HEADER_ENTRY_SIZE;
       i++)
  {
    fputc(0x00, f);
  }

  for (auto* it = elf.sections.head;
       it < elf.sections.tail;
       it++)
  {
    EmitSectionEntry(f, *it);
  }

  // --- Emit the program header ---
  elf.header.programHeaderOffset = ftell(f);
  for (auto* it = elf.segments.head;
       it < elf.segments.tail;
       it++)
  {
    EmitProgramEntry(f, *it);
  }

  // --- Emit the ELF header ---
  fseek(f, 0x0, SEEK_SET);
  EmitHeader(f, elf.header);

  fclose(f);
}

template<>
void Free<elf_thing*>(elf_thing*& thing)
{
  free(thing->data);
  free(thing);
}

template<>
void Free<elf_mapping>(elf_mapping& /*mapping*/)
{
}

template<>
void Free<elf_relocation*>(elf_relocation*& relocation)
{
  free(relocation);
}

template<>
void Free<elf_string*>(elf_string*& string)
{
  free(string->str);
  free(string);
}

template<>
void Free<elf_symbol*>(elf_symbol*& symbol)
{
  // NOTE(Isaac): don't free the name
  free(symbol);
}

template<>
void Free<elf_segment*>(elf_segment*& segment)
{
  free(segment);
}

template<>
void Free<elf_section*>(elf_section*& section)
{
  // NOTE(Isaac): don't free the name, it's added to the list of strings and would be freed twice!
  free(section);
}

template<>
void Free<elf_file>(elf_file& elf)
{
  FreeVector<elf_segment*>(elf.segments);
  FreeVector<elf_section*>(elf.sections);
  FreeVector<elf_thing*>(elf.things);
  FreeVector<elf_symbol*>(elf.symbols);
  FreeVector<elf_string*>(elf.strings);
  FreeVector<elf_mapping>(elf.mappings);
  FreeVector<elf_relocation*>(elf.relocations);
  Free<elf_thing*>(elf.rodataThing);
}
