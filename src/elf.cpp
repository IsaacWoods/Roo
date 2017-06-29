/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <elf.hpp>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstdarg>
#include <climits>
#include <error.hpp>

#define PROGRAM_HEADER_ENTRY_SIZE 0x38
#define SECTION_HEADER_ENTRY_SIZE 0x40
#define SYMBOL_TABLE_ENTRY_SIZE   0x18

// TODO XXX: get rid of this
template<>
void Free<elf_thing*>(elf_thing*& thing)
{
  free(thing->data);
  free(thing);
}
/*
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
*/

/*
 * NOTE(Isaac): the string is duplicated and freed separately of the passed string.
 */
static elf_string* CreateString(elf_file& elf, const char* str)
{
  elf_string* string = static_cast<elf_string*>(malloc(sizeof(elf_string)));
  string->offset = elf.stringTableTail;
  string->str = strdup(str);

  elf.stringTableTail += strlen(str) + 1u;

  elf.strings.push_back(string);
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
  elf.symbols.push_back(symbol);
  return symbol;
}

void CreateRelocation(elf_file& elf, elf_thing* thing, uint64_t offset, relocation_type type, elf_symbol* symbol,
                      int64_t addend, const LabelInstruction* label)
{
  elf_relocation* relocation = static_cast<elf_relocation*>(malloc(sizeof(elf_relocation)));
  relocation->thing   = thing;
  relocation->offset  = offset;
  relocation->type    = type;
  relocation->symbol  = symbol;
  relocation->addend  = addend;
  relocation->label   = label;

  elf.relocations.push_back(relocation);
}

elf_segment* CreateSegment(elf_file& elf, segment_type type, uint32_t flags, uint64_t address, uint64_t alignment,
                           bool isMappedDirectly)
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
  elf.segments.push_back(segment);
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

  // NOTE(Isaac): section indices begin at 1 because 0 is the special SHT_NULL section
  section->index = (elf.sections.size() > 0u ? elf.sections[elf.sections.size() - 1u]->index + 1u : 1u);

  elf.header.numSectionHeaderEntries++;
  elf.sections.push_back(section);
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

  elf.things.push_back(thing);
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
  for (elf_section* section : elf.sections)
  {
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
  for (elf_symbol* symbol : elf.symbols)
  {
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

  elf.mappings.push_back(mapping);
}

struct elf_object
{
  const char*                     path;
  FILE*                           f;
  std::vector<elf_section*>       sections;
  std::vector<elf_symbol*>        symbols;      // NOTE(Isaac): Underlying pointers should not be freed
  std::vector<elf_relocation*>    relocations;  // NOTE(Isaac): Underlying pointers should not be freed
  std::vector<elf_thing*>         things;       // NOTE(Isaac): Underlying pointers should not be freed

  elf_symbol**                    symbolRemaps;
  unsigned int                    numRemaps;
};

template<>
void Free<elf_object>(elf_object& object)
{
  fclose(object.f);
  free(object.symbolRemaps);
}

static elf_string* ExtractString(elf_file& elf, elf_object& object, const elf_section* stringTable, uint64_t stringOffset)
{
  Assert(stringTable, "Tried to extract string from non-existant string table in ELF object");

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

    object.sections.push_back(section);
  }

  uint16_t sectionWithNames;
  fseek(object.f, 0x3E, SEEK_SET);
  fread(&sectionWithNames, sizeof(uint16_t), 1, object.f);
  elf_section* stringTable = object.sections[sectionWithNames];

  for (elf_section* section : object.sections)
  {
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
    elf.symbols.push_back(symbol);
    object.symbols.push_back(symbol);
  }
}

static void ParseRelocationSection(error_state& errorState, elf_file& elf, elf_object& object, elf_section* section)
{
  const unsigned int RELOCATION_ENTRY_SIZE = 0x18;

  if (section->entrySize != RELOCATION_ENTRY_SIZE)
  {
    RaiseError(errorState, ERROR_WEIRD_LINKED_OBJECT, object.path, "Relocation section has weirdly sized entries");
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

    elf.relocations.push_back(relocation);
    object.relocations.push_back(relocation);
  }
}

void LinkObject(elf_file& elf, const char* objectPath)
{
  error_state errorState = CreateErrorState(LINKING);

  elf_object object;
  object.path = objectPath;
  object.f = fopen(objectPath, "rb");

  if (!(object.f))
  {
    RaiseError(errorState, ERROR_WEIRD_LINKED_OBJECT, objectPath, "Couldn't open file");
  }

  #define CONSUME(byte) \
    if (fgetc(object.f) != byte) \
    { \
      RaiseError(errorState, ERROR_WEIRD_LINKED_OBJECT, objectPath, "Does not follow format"); \
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
    RaiseError(errorState, ERROR_WEIRD_LINKED_OBJECT, objectPath, "File type is not a relocatable");
  }

  // Parse the section header
  ParseSectionHeader(elf, object);
  for (elf_section* section : object.sections)
  {
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
  for (elf_section* section : object.sections)
  {
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
  std::vector<elf_symbol*> functionSymbols;

  // Extract functions from the symbol table and their code from .text
  for (elf_symbol* symbol : object.symbols)
  {
    /*
     * NOTE(Isaac): NASM refuses to emit symbols for functions with the correct type,
     * so assume that symbols with no type that are in .text are also functions
     */
    if (((symbol->info & 0xf) == SYM_TYPE_FUNCTION) ||
        ((symbol->info & 0xf) == SYM_TYPE_NONE && symbol->sectionIndex == text->index))
    {
      functionSymbols.push_back(symbol);
      printf("Extracting symbol with name: %s\n", symbol->name->str);
    }
  }

  // Sort the linked list by the symbols' offsets into the section
  std::sort(functionSymbols.begin(), functionSymbols.end(), [](elf_symbol*& a, elf_symbol*& b)
    {
      return (a->value < b->value);
    });

  // Extract functions from the external object and link them into ourselves
  for (auto it = functionSymbols.begin();
       it < functionSymbols.end();
       it++)
  {
    elf_symbol* symbol = *it;
    Assert(symbol->name, "Extracted functions should have names");

    /*
     * Annoying assemblers/compilers like NASM neglect to actually include the sizes of symbols, so we have
     * to work them out ourselves.
     */
    if (symbol->size == 0u)
    {
      if (std::next(it) < functionSymbols.end())
      {
        /*
         * The size is the difference between the offset of this symbol and the next one
         */
        symbol->size = (*std::next(it))->value - symbol->value;
      }
      else
      {
        /*
         * Or the difference between the offset of this one and the end of the section, for the last symbol.
         */
        symbol->size = text->size - symbol->value;
      }
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
    elf.things.push_back(thing);
    object.things.push_back(thing);

    /*
     * We create a new symbol for the function, so remove the old one.
     * NOTE(Isaac): This must be stable, because the symbols are sorted by offset.
     */
    elf.symbols.erase(std::remove(elf.symbols.begin(), elf.symbols.end(), symbol), elf.symbols.end());
  }

  // Complete the relocations loaded from the external object
  for (elf_relocation* relocation : object.relocations)
  {
    for (elf_thing* thing : object.things)
    {
      // NOTE(Isaac): the address should still be the offset in the external object's .text section
      if ((relocation->offset >= thing->address) &&
          (relocation->offset < thing->address + thing->length))
      {
        relocation->thing = thing;
        continue;
      }
    }
  }
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
  /*
   * This encodes a little-endian representation, and will need to be extended if we need to allow
   * changing the endianness of the ELF.
   */
  thing->data[thing->length++] = (i >> 0u)  & 0xff;
  thing->data[thing->length++] = (i >> 8u)  & 0xff;
  thing->data[thing->length++] = (i >> 16u) & 0xff;
  thing->data[thing->length++] = (i >> 24u) & 0xff;
}

template<>
void Emit_<uint64_t>(elf_thing* thing, uint64_t i)
{
  /*
   * This encodes a little-endian representation, and will need to be extended if we need to allow
   * changing the endianness of the ELF.
   */
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
   * If there aren't any program/section headers (respectively), we should emit 0 instead of the actual size of
   * one of the entries.
   */
  const uint16_t programHeaderEntrySize = (header.numProgramHeaderEntries > 0u ? PROGRAM_HEADER_ENTRY_SIZE : 0u);
  const uint16_t sectionHeaderEntrySize = (header.numSectionHeaderEntries > 0u ? SECTION_HEADER_ENTRY_SIZE : 0u);

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
/*0x10*/fwrite(&(header.fileType),                sizeof(uint16_t), 1, f);
/*0x12*/fputc(0x3E,     f); // Specify we are targetting the x86_64 ISA
        fputc(0x00,     f);
/*0x14*/fputc(0x01,     f); // Specify we are targetting the EV_CURRENT version of the ELF object file format
        fputc(0x00,     f);
        fputc(0x00,     f);
        fputc(0x00,     f);
/*0x18*/fwrite(&(header.entryPoint),              sizeof(uint64_t), 1, f);
/*0x20*/fwrite(&(header.programHeaderOffset),     sizeof(uint64_t), 1, f);
/*0x28*/fwrite(&(header.sectionHeaderOffset),     sizeof(uint64_t), 1, f);
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

static void EmitSymbolTable(FILE* f, elf_file& elf, const std::vector<elf_symbol*>& symbols)
{
  // Emit an empty symbol table entry, because the standard says so
  GetSection(elf, ".symtab")->size += SYMBOL_TABLE_ENTRY_SIZE;
  for (unsigned int i = 0u;
       i < SYMBOL_TABLE_ENTRY_SIZE;
       i++)
  {
    fputc(0x0, f);
  }

  for (elf_symbol* symbol : symbols)
  {
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

static void EmitStringTable(FILE* f, elf_file& elf, const std::vector<elf_string*>& strings)
{
  // Lead with a null terminator to mark the null-string
  fputc('\0', f);
  GetSection(elf, ".strtab")->size = 1u;

  for (elf_string* string : strings)
  {
    GetSection(elf, ".strtab")->size += strlen(string->str) + 1u;

    // NOTE(Isaac): add 1 to also write the included null-terminator
    fwrite(string->str, sizeof(char), strlen(string->str) + 1u, f);
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
  /*
   * NOTE(Isaac): We can't start the second loop from the next symbol (to make it O(n log n)) because we can
   * only coalesce the undefined symbol with its matching definition, not the other way around!
   */
  for (auto it = elf.symbols.begin();
       it < elf.symbols.end();
       it++)
  {
    elf_symbol* symbol = *it;
    bool symbolResolved = false;

    // No work needs to be done for defined symbols
    if (!(symbol->name) || symbol->sectionIndex != 0u)
    {
      continue;
    }

    for (elf_symbol* otherSymbol : elf.symbols)
    {
      // NOTE(Isaac): don't coalesce with yourself!
      if ((symbol == otherSymbol) || !(otherSymbol->name))
      {
        continue;
      }

      if (strcmp(symbol->name->str, otherSymbol->name->str) == 0u)
      {
        // Coalesce the symbols!
        elf.symbols.erase(it);
        symbolResolved = true;

        // Point relocations that refer to the undefined symbol to its defined partner
        for (elf_relocation* relocation : elf.relocations)
        {
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
    case R_X86_64_64:   return "R_X86_64_64";
    case R_X86_64_PC32: return "R_X86_64_PC32";
    case R_X86_64_32:   return "R_X86_64_32";

    default:
    {
      RaiseError(ICE_GENERIC, "Unhandled relocation type in GetRelocationTypeName");
    } break;
  }

  return nullptr;
}

static void CompleteRelocations(error_state& errorState, FILE* f, elf_file& elf)
{
  long int currentPosition = ftell(f);

  for (elf_relocation* relocation : elf.relocations)
  {
    Assert(relocation->thing, "Relocation trying to be applied to a nullptr elf_thing");
    Assert(relocation->symbol, "Relocation has a nullptr symbol");

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
  for (elf_mapping& mapping : elf.mappings)
  {
    elf_segment* segment = mapping.segment;
    elf_section* section = mapping.section;

    Assert(section->type != SHT_NOBITS, "We can't tell the size of SHT_NOBITS sections, because its file size is a lie");

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

elf_file::elf_file(CodegenTarget& target, bool isRelocatable)
  :isRelocatable(isRelocatable)
  ,target(&target)
  ,header()
  ,segments()
  ,things()
  ,symbols()
  ,strings()
  ,mappings()
  ,relocations()
  ,stringTableTail(1u)
  ,numSymbols(0u)
  ,rodataThing(nullptr)
{
  header.fileType                 = (isRelocatable ? ET_REL : ET_EXEC);
  header.entryPoint               = 0x0;
  header.programHeaderOffset      = 0x0;
  header.sectionHeaderOffset      = 0x0;
  header.numProgramHeaderEntries  = 0u;
  header.numSectionHeaderEntries  = 0u;
  header.sectionWithSectionNames  = 0u;
}

elf_file::~elf_file()
{
  Free<elf_thing*>(rodataThing);
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

  for (elf_thing* thing : elf.things)
  {
    EmitThing(f, elf, thing);
  }

  // Manually emit the .rodata thing
  if (elf.rodataThing)
  {
    GetSection(elf, ".rodata")->offset = ftell(f);
    EmitThing(f, elf, elf.rodataThing, GetSection(elf, ".rodata"));
  }

  MapSectionsToSegments(elf);

  // --- Recalculate symbol values ---
  /*
   * If this is a relocatable, we want to emit symbol values relative to their section's *offset*
   * If this is an executable, they should be relative to the final *virtual address*
   */
  uint64_t symbolOffset = (elf.isRelocatable ? 0u : GetSection(elf, ".text")->address);
  for (elf_thing* thing : elf.things)
  {
    thing->symbol->value += symbolOffset;
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

  // We need empty section header entry for reasons (because the spec says so)
  elf.header.numSectionHeaderEntries++;
  for (unsigned int i = 0u;
       i < SECTION_HEADER_ENTRY_SIZE;
       i++)
  {
    fputc(0x00, f);
  }

  for (elf_section* section : elf.sections)
  {
    EmitSectionEntry(f, section);
  }

  // --- Emit the program header ---
  if (elf.segments.size() > 0u)
  {
    elf.header.programHeaderOffset = ftell(f);
    for (elf_segment* segment : elf.segments)
    {
      EmitProgramEntry(f, segment);
    }
  }

  // --- Emit the ELF header ---
  fseek(f, 0x0, SEEK_SET);
  EmitHeader(f, elf.header);

  fclose(f);
}

// TODO: get rid of all this
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
