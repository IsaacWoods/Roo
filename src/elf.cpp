/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <elf.hpp>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdarg>
#include <climits>
#include <cassert>

#define PROGRAM_HEADER_ENTRY_SIZE 0x38
#define SECTION_HEADER_ENTRY_SIZE 0x40
#define SYMBOL_TABLE_ENTRY_SIZE 0x18

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
  elf_segment* segment = static_cast<elf_segment*>(malloc(sizeof(elf_segment)));
  segment->type = type;
  segment->flags = flags;
  segment->offset = 0u;
  segment->virtualAddress = address;
  segment->physicalAddress = address;
  segment->alignment = alignment;
  segment->size.map = {};  // NOTE(Isaac): `map` is biggest, so this will correctly zero-initialise the union
  segment->isMappedDirectly = isMappedDirectly;

  elf.header.numProgramHeaderEntries++;
  Add<elf_segment*>(elf.segments, segment);
  return segment;
}

elf_section* CreateSection(elf_file& elf, const char* name, section_type type, uint64_t alignment)
{
  elf_section* section = static_cast<elf_section*>(malloc(sizeof(elf_section)));
  section->name = CreateString(elf, name);
  section->type = type;
  section->flags = 0u;
  section->address = 0u;
  section->offset = 0u;
  section->size = 0u;
  section->link = 0u;
  section->info = 0u;
  section->alignment = alignment;
  section->entrySize = 0u;

  // NOTE(Isaac): section indices begin at 1
  section->index = (elf.sections.tail ? (**elf.sections.tail)->index + 1u : 1u);

  elf.header.numSectionHeaderEntries++;
  Add<elf_section*>(elf.sections, section);
  return section;
}

#define INITIAL_DATA_SIZE 256u
elf_thing* CreateThing(elf_file& elf, elf_symbol* symbol)
{
  elf_thing* thing = static_cast<elf_thing*>(malloc(sizeof(elf_thing)));
  thing->symbol = symbol;
  thing->length = 0u;
  thing->capacity = INITIAL_DATA_SIZE;
  thing->data = static_cast<uint8_t*>(malloc(sizeof(uint8_t) * INITIAL_DATA_SIZE));
  thing->fileOffset = 0u;
  thing->address = 0x0;

  Add<elf_thing*>(elf.things, thing);
  return thing;
}

elf_thing* CreateRodataThing(elf_file& elf)
{
  elf_thing* thing = static_cast<elf_thing*>(malloc(sizeof(elf_thing)));
  thing->symbol = CreateSymbol(elf, nullptr, SYM_BIND_GLOBAL, SYM_TYPE_SECTION, GetSection(elf, ".rodata")->index, 0x00);
  thing->length = 0u;
  thing->capacity = INITIAL_DATA_SIZE;
  thing->data = static_cast<uint8_t*>(malloc(sizeof(uint8_t) * INITIAL_DATA_SIZE));
  thing->fileOffset = 0u;
  thing->address = 0x0;

  return thing;
}
#undef INITIAL_DATA_SIZE

elf_section* GetSection(elf_file& elf, const char* name)
{
  for (auto* sectionIt = elf.sections.first;
       sectionIt;
       sectionIt = sectionIt->next)
  {
    if (strcmp((**sectionIt)->name->str, name) == 0)
    {
      return **sectionIt;
    }
  }

  fprintf(stderr, "WARNING: Couldn't find section of name '%s'!\n", name);
  return nullptr;
}

elf_symbol* GetSymbol(elf_file& elf, const char* name)
{
  for (auto* it = elf.symbols.first;
       it;
       it = it->next)
  {
    elf_symbol* symbol = **it;

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
  FILE*                         f;
  linked_list<elf_section*>     sections;
  linked_list<elf_symbol*>      symbols;      // NOTE(Isaac): should be *unlinked*
  linked_list<elf_relocation*>  relocations;  // NOTE(Isaac): should be *unlinked*
  linked_list<elf_thing*>       things;       // NOTE(Isaac): should be *unlinked*

  elf_symbol**                  symbolRemaps;
  unsigned int                  numRemaps;
};

template<>
void Free<elf_object>(elf_object& object)
{
  fclose(object.f);
  FreeLinkedList<elf_section*>(object.sections);
  UnlinkLinkedList<elf_symbol*>(object.symbols);
  UnlinkLinkedList<elf_relocation*>(object.relocations);
  UnlinkLinkedList<elf_thing*>(object.things);
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

  for (auto* sectionIt = object.sections.first;
       sectionIt;
       sectionIt = sectionIt->next)
  {
    (**sectionIt)->name = ExtractString(elf, object, stringTable, (**sectionIt)->nameOffset);
  }
}

static void ParseSymbolTable(elf_file& elf, elf_object& object, elf_section* table)
{
  if (table->entrySize != SYMBOL_TABLE_ENTRY_SIZE)
  {
    fprintf(stderr, "FATAL: External object has weirdly-sized symbols!\n");
    Crash();
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

static void ParseRelocationSection(elf_file& elf, elf_object& object, elf_section* section)
{
  const unsigned int RELOCATION_ENTRY_SIZE = 0x18;

  if (section->entrySize != RELOCATION_ENTRY_SIZE)
  {
    fprintf(stderr, "FATAL: External object has weirdly-sized relocations!\n");
    Crash();
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
      printf("FATAL: Failed to find resolve symbol used in an external relocation!\n");
      Crash();
    }

    Add<elf_relocation*>(elf.relocations, relocation);
    Add<elf_relocation*>(object.relocations, relocation);
  }
}

void LinkObject(elf_file& elf, const char* objectPath)
{
  elf_object object;
  object.f = fopen(objectPath, "rb");
  CreateLinkedList<elf_section*>(object.sections);
  CreateLinkedList<elf_symbol*>(object.symbols);
  CreateLinkedList<elf_relocation*>(object.relocations);
  CreateLinkedList<elf_thing*>(object.things);

  if (!(object.f))
  {
    fprintf(stderr, "FATAL: failed to open object to link against: '%s'!\n", objectPath);
    Crash();
  }

  #define CONSUME(byte) \
    if (fgetc(object.f) != byte) \
    { \
      fprintf(stderr, "FATAL: Object file (%s) does not match expected format!\n", objectPath); \
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
    fprintf(stderr, "FATAL: Can only link relocatable object files!\n");
    Crash();
  }

  // Parse the section header
  ParseSectionHeader(elf, object);
  for (auto* sectionIt = object.sections.first;
       sectionIt;
       sectionIt = sectionIt->next)
  {
    switch ((**sectionIt)->type)
    {
      case SHT_SYMTAB:
      {
        ParseSymbolTable(elf, object, **sectionIt);
      } break;

      case SHT_REL:
      {
        fprintf(stderr, "FATAL: SHT_REL sections are not supported, please emit SHT_RELA sections instead!\n");
        Crash();
      } break;

      case SHT_RELA:
      {
        ParseRelocationSection(elf, object, **sectionIt);
      } break;

      default:
      {
      } break;
    }
  }

  // Find the .text section
  elf_section* text = nullptr;
  for (auto* sectionIt = object.sections.first;
       sectionIt;
       sectionIt = sectionIt->next)
  {
    if (!((**sectionIt)->name))
    {
      continue;
    }

    if (strcmp((**sectionIt)->name->str, ".text") == 0)
    {
      text = **sectionIt;
      break;
    }
  }

  /*
   * NOTE(Isaac): this *borrows* symbols from the actual symbol table - don't free it, just unlink it!
   */
  linked_list<elf_symbol*> functionSymbols;
  CreateLinkedList<elf_symbol*>(functionSymbols);

  // Extract functions from the symbol table and their code from .text
  for (auto* symbolIt = object.symbols.first;
       symbolIt;
       symbolIt = symbolIt->next)
  {
    elf_symbol* symbol = **symbolIt;

    /*
     * NOTE(Isaac): NASM refuses to emit symbols for functions with the correct type,
     * so assume that symbols with no type that are in .text are also functions
     */
    if ((symbol->info & 0xf) == SYM_TYPE_FUNCTION ||
        ((symbol->info & 0xf) == SYM_TYPE_NONE && symbol->sectionIndex == text->index))
    {
      Add<elf_symbol*>(functionSymbols, symbol);
    }
  }

  // Sort the linked list by the symbols' values (offsets into .text)
  SortLinkedList<elf_symbol*>(functionSymbols, [](elf_symbol*& a, elf_symbol*& b)
    {
      return (a->value < b->value);
    });

  // Extract functions from the external object and link them into ourselves
  for (auto* symbolIt = functionSymbols.first;
       symbolIt;
       symbolIt = symbolIt->next)
  {
    elf_symbol* symbol = **symbolIt;
    assert(symbol->name); // NOTE(Isaac): functions should probably always have names

    if (symbolIt->next)
    {
      symbol->size = (**(symbolIt->next))->value - symbol->value;
    }
    else
    {
      symbol->size = text->size - symbol->value;
    }

    printf("Extracting function from external object '%s' from 0x%lx\n", symbol->name->str, symbol->value);

    elf_thing* thing = static_cast<elf_thing*>(malloc(sizeof(elf_thing)));
    thing->symbol = CreateSymbol(elf, symbol->name->str, SYM_BIND_GLOBAL, SYM_TYPE_FUNCTION, GetSection(elf, ".text")->index, 0u);
    thing->length = symbol->size;
    thing->capacity = symbol->size;
    thing->data = static_cast<uint8_t*>(malloc(sizeof(uint8_t) * symbol->size));
    thing->fileOffset = 0u;
    thing->address = symbol->value;  // NOTE(Isaac): Before it's linked, this is the original offset from the file

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
    Remove<elf_symbol*>(elf.symbols, symbol);
  }

  // Complete the relocations loaded from the external object
  for (auto* it = object.relocations.first;
       it;
       it = it->next)
  {
    elf_relocation* relocation = **it;

    for (auto* thingIt = object.things.first;
         thingIt;
         thingIt = thingIt->next)
    {
      elf_thing* thing = **thingIt;

      // NOTE(Isaac): the address should still be the offset in the external object's .text section
      if ((relocation->offset >= thing->address) &&
          (relocation->offset < thing->address + thing->length))
      {
        relocation->thing = thing;
        continue;
      }
    }
  }

  UnlinkLinkedList<elf_symbol*>(functionSymbols);
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

static void EmitSymbolTable(FILE* f, elf_file& elf, linked_list<elf_symbol*>& symbols)
{
  // Emit an empty symbol table entry, because the standard says so
  GetSection(elf, ".symtab")->size += SYMBOL_TABLE_ENTRY_SIZE;
  for (unsigned int i = 0u;
       i < SYMBOL_TABLE_ENTRY_SIZE;
       i++)
  {
    fputc(0x0, f);
  }

  for (auto* symbolIt = symbols.first;
       symbolIt;
       symbolIt = symbolIt->next)
  {
    elf_symbol* symbol = **symbolIt;
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

static void EmitStringTable(FILE* f, elf_file& elf, linked_list<elf_string*>& strings)
{
  // Lead with a null terminator to mark the null-string
  fputc('\0', f);
  GetSection(elf, ".strtab")->size = 1u;

  for (auto* it = strings.first;
       it;
       it = it->next)
  {
    GetSection(elf, ".strtab")->size += strlen((**it)->str) + 1u;

    for (const char* c = (**it)->str;
         *c;
         c++)
    {
      fputc(*c, f);
    }

    fputc('\0', f);
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

  for (unsigned int i = 0u;
       i < thing->length;
       i++)
  {
    fputc(thing->data[i], f);
  }
}

/*
 * This resolves the symbols in the symbol table that are actually referencing symbols
 * that haven't been linked yet.
 */
static void ResolveUndefinedSymbols(elf_file& elf)
{
  for (auto* symbolIt = elf.symbols.first;
       symbolIt;
       symbolIt = symbolIt->next)
  {
    // No work needs to be done for defined
    if (!((**symbolIt)->name) || (**symbolIt)->sectionIndex != 0u)
    {
      continue;
    }

    elf_symbol* undefinedSymbol = **symbolIt;
    bool symbolResolved = false;

    for (auto* otherSymbolIt = elf.symbols.first;
         otherSymbolIt;
         otherSymbolIt = otherSymbolIt->next)
    {
      // NOTE(Isaac): don't try and coalesce with yourself!
      if ((undefinedSymbol == **otherSymbolIt) || !((**otherSymbolIt)->name))
      {
        continue;
      }

      if (strcmp(undefinedSymbol->name->str, (**otherSymbolIt)->name->str) == 0u)
      {
        // Coalesce the symbols!
        elf_symbol* partner = **otherSymbolIt;
        Remove<elf_symbol*>(elf.symbols, undefinedSymbol);
        symbolResolved = true;

        // Point relocations that refer to the undefined symbol to its defined partner
        for (auto* relocationIt = elf.relocations.first;
             relocationIt;
             relocationIt = relocationIt->next)
        {
          elf_relocation* relocation = **relocationIt;

          if (relocation->symbol->index == undefinedSymbol->index)
          {
            relocation->symbol = partner;
          }
        }

        break;
      }
    }

    if (!symbolResolved)
    {
      // We can't find a matching symbol - throw an error
      fprintf(stderr, "FATAL: Failed to resolve symbol during linking: '%s'!\n", undefinedSymbol->name->str);
      Crash();
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
      return "!!!UNHANDLED RELOCATION TYPE IN GetRelocationTypeName!!!";
    } break;
  }
}

static void CompleteRelocations(FILE* f, elf_file& elf)
{
  long int currentPosition = ftell(f);

  for (auto* relocationIt = elf.relocations.first;
       relocationIt;
       relocationIt = relocationIt->next)
  {
    elf_relocation* relocation = **relocationIt;
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
        fprintf(stderr, "FATAL: Unhandled relocation->type (%s)!\n", GetRelocationTypeName(relocation->type));
        Crash();
      }
    }
  }

  fseek(f, currentPosition, SEEK_SET);
}

static void MapSectionsToSegments(elf_file& elf)
{
  for (auto* mappingIt = elf.mappings.first;
       mappingIt;
       mappingIt = mappingIt->next)
  {
    elf_segment* segment = (**mappingIt).segment;
    elf_section* section = (**mappingIt).section;

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

void CreateElf(elf_file& elf, codegen_target& target)
{
  elf.target = &target;
  CreateLinkedList<elf_thing*>(elf.things);
  CreateLinkedList<elf_symbol*>(elf.symbols);
  CreateLinkedList<elf_segment*>(elf.segments);
  CreateLinkedList<elf_section*>(elf.sections);
  CreateLinkedList<elf_string*>(elf.strings);
  CreateLinkedList<elf_mapping>(elf.mappings);
  CreateLinkedList<elf_relocation*>(elf.relocations);
  elf.stringTableTail = 1u;
  elf.numSymbols = 0u;
  elf.rodataThing = nullptr;

  elf.header.fileType = ET_EXEC;
  elf.header.entryPoint = 0x0;
  elf.header.programHeaderOffset = 0x0;
  elf.header.sectionHeaderOffset = 0x0;
  elf.header.numProgramHeaderEntries = 0u;
  elf.header.numSectionHeaderEntries = 0u;
  elf.header.sectionWithSectionNames = 0u;
}

template<> void Free<int>(int&) {}
void CompleteElf(elf_file& elf)
{
  ResolveUndefinedSymbols(elf);
}

void WriteElf(elf_file& elf, const char* path)
{
  FILE* f = fopen(path, "wb");

  if (!f)
  {
    fprintf(stderr, "FATAL: unable to create executable at path: %s\n", path);
    Crash();
  }

  // Leave space for the ELF header
  fseek(f, 0x40, SEEK_SET);

  // --- Emit all the things ---
  GetSection(elf, ".text")->offset = ftell(f);

  for (auto* thingIt = elf.things.first;
       thingIt;
       thingIt = thingIt->next)
  {
    EmitThing(f, elf, **thingIt);
  }

  // Manually emit the .rodata thing
  if (elf.rodataThing)
  {
    GetSection(elf, ".rodata")->offset = ftell(f);
    EmitThing(f, elf, elf.rodataThing, GetSection(elf, ".rodata"));
  }

  // --- Map sections to segments ---
  MapSectionsToSegments(elf);

  // Recalculate symbol values
  uint64_t textAddress = GetSection(elf, ".text")->address;
  for (auto* it = elf.things.first;
       it;
       it = it->next)
  {
    (**it)->symbol->value += textAddress;
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
  CompleteRelocations(f, elf);

  elf_symbol* startSymbol = GetSymbol(elf, "_start");
  if (!startSymbol)
  {
    fprintf(stderr, "FATAL: Can't find a '_start' symbol to enter into!\n");
    Crash();
  }
  elf.header.entryPoint = startSymbol->value;

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

  for (auto* sectionIt = elf.sections.first;
       sectionIt;
       sectionIt = sectionIt->next)
  {
    EmitSectionEntry(f, **sectionIt);
  }

  // --- Emit the program header ---
  elf.header.programHeaderOffset = ftell(f);

  for (auto* segmentIt = elf.segments.first;
       segmentIt;
       segmentIt = segmentIt->next)
  {
    EmitProgramEntry(f, **segmentIt);
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
  FreeLinkedList<elf_segment*>(elf.segments);
  FreeLinkedList<elf_section*>(elf.sections);
  FreeLinkedList<elf_thing*>(elf.things);
  FreeLinkedList<elf_symbol*>(elf.symbols);
  FreeLinkedList<elf_string*>(elf.strings);
  FreeLinkedList<elf_mapping>(elf.mappings);
  FreeLinkedList<elf_relocation*>(elf.relocations);
  Free<elf_thing*>(elf.rodataThing);
}
