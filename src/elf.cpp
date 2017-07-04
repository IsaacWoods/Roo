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

ElfString::ElfString(ElfFile& elf, const char* str)
  :offset(elf.stringTableTail)
  ,str(strdup(str))
{
  elf.stringTableTail += strlen(str)+1u;
  elf.strings.push_back(this);
}

ElfString::~ElfString()
{
  delete str;
}

/*
 * NOTE(Isaac): If `name == nullptr`, the symbol points towards the nulled entry of the string table.
 */
ElfSymbol::ElfSymbol(ElfFile& elf, const char* name, Binding binding, Type type, uint16_t sectionIndex, uint64_t value)
  :name(name ? new ElfString(elf, name) : nullptr)
  ,info((binding << 4u) | type)
  ,sectionIndex(sectionIndex)
  ,value(value)
  ,size(0u)
  ,index(elf.numSymbols + 1u)
{
  // Make sure the `info` field of the symbol table points to the index of the first GLOBAL symbol
  if (GetSection(elf, ".symtab")->info == 0u && binding == ElfSymbol::Binding::SYM_BIND_GLOBAL)
  {
    GetSection(elf, ".symtab")->info = elf.numSymbols;
  }

  elf.numSymbols++;
  elf.symbols.push_back(this);
}

ElfRelocation::ElfRelocation(ElfFile& elf, ElfThing* thing, uint64_t offset, Type type, ElfSymbol* symbol,
                             int64_t addend, const LabelInstruction* label)
  :thing(thing)
  ,offset(offset)
  ,type(type)
  ,symbol(symbol)
  ,addend(addend)
  ,label(label)
{
  elf.relocations.push_back(this);
}

ElfSegment::ElfSegment(ElfFile& elf, Type type, uint32_t flags, uint64_t address, uint64_t alignment,
                       bool isMappedDirectly)
  :type(type)
  ,flags(flags)
  ,offset(0u)
  ,virtualAddress(address)
  ,physicalAddress(address)
  ,alignment(alignment)
  ,size()
  ,isMappedDirectly(isMappedDirectly)
{
  elf.header.numProgramHeaderEntries++;
  elf.segments.push_back(this);
}

ElfSection::ElfSection(ElfFile& elf, const char* name, Type type, uint64_t alignment, bool addToElf)
  :name(name ? new ElfString(elf, name) : nullptr)
  ,type(type)
  ,flags(0u)
  ,address(0u)
  ,offset(0u)
  ,size(0u)
  ,link(0u)
  ,info(0u)
  ,alignment(alignment)
  ,entrySize(0u)
  // NOTE(Isaac): Section indices begin at 1 because 0 is the special SHT_NULL section
  ,index(elf.sections.size() > 0u ? elf.sections[elf.sections.size() - 1u]->index + 1u : 1u)
  ,things()
{
  if (addToElf)
  {
    elf.header.numSectionHeaderEntries++;
    elf.sections.push_back(this);
  }
}

#define INITIAL_DATA_SIZE 256u
ElfThing::ElfThing(ElfSection* section, ElfSymbol* symbol)
  :symbol(symbol)
  ,length(0u)
  ,capacity(INITIAL_DATA_SIZE)
  ,data(static_cast<uint8_t*>(malloc(sizeof(uint8_t) * INITIAL_DATA_SIZE)))
  ,fileOffset(0u)
  ,address(0x00)
{
  section->things.push_back(this);
}
#undef INITIAL_DATA_SIZE

ElfThing::~ElfThing()
{
  delete data;
}

ElfSection* GetSection(ElfFile& elf, const char* name)
{
  for (ElfSection* section : elf.sections)
  {
    if (strcmp(section->name->str, name) == 0)
    {
      return section;
    }
  }

  RaiseError(ICE_MISSING_ELF_SECTION, name);
  __builtin_unreachable();
}

ElfSymbol* GetSymbol(ElfFile& elf, const char* name)
{
  for (ElfSymbol* symbol : elf.symbols)
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

void MapSection(ElfFile& elf, ElfSegment* segment, ElfSection* section)
{
  ElfMapping mapping;
  mapping.segment = segment;
  mapping.section = section;

  elf.mappings.push_back(mapping);
}

struct ElfObject
{
  ~ElfObject()
  {
    fclose(f);
    delete symbolRemaps;
  }

  const char*                 path;
  FILE*                       f;
  std::vector<ElfSection*>    sections;
  std::vector<ElfSymbol*>     symbols;      // NOTE(Isaac): Underlying pointers should not be freed
  std::vector<ElfRelocation*> relocations;  // NOTE(Isaac): Underlying pointers should not be freed
  std::vector<ElfThing*>      things;       // NOTE(Isaac): Underlying pointers should not be freed

  ElfSymbol**                 symbolRemaps;
  unsigned int                numRemaps;
};

static ElfString* ExtractString(ElfFile& elf, ElfObject& object, const ElfSection* stringTable, uint64_t stringOffset)
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

  ElfString* string = new ElfString(elf, str);
  free(str);
  return string;
}

static void ParseSectionHeader(ElfFile& elf, ElfObject& object)
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
    /*
     * We don't actually know anything about the section yet, so just get some memory.
     * We also don't know where the string table is yet (because we're loading the section header) so we can't
     * load names yet.
     * NOTE(Isaac): we don't want to add this to the main ELF because we're loading it from another object!
     */
    ElfSection* section = new ElfSection(elf, nullptr, ElfSection::Type::SHT_HIUSER, 0x00, false);
    section->index = i; // NOTE(Isaac): this is the index in the *external object*, not our executable

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
  ElfSection* stringTable = object.sections[sectionWithNames];

  for (ElfSection* section : object.sections)
  {
    section->name = ExtractString(elf, object, stringTable, section->nameOffset);
  }
}

static void ParseSymbolTable(ErrorState& errorState, ElfFile& elf, ElfObject& object, ElfSection* table)
{
  if (table->entrySize != SYMBOL_TABLE_ENTRY_SIZE)
  {
    RaiseError(errorState, ERROR_WEIRD_LINKED_OBJECT, object.path, "Object has weirdly sized symbols");
  }
  
  unsigned int numSymbols = table->size / SYMBOL_TABLE_ENTRY_SIZE;
  object.numRemaps = numSymbols;
  object.symbolRemaps = static_cast<ElfSymbol**>(malloc(sizeof(ElfSymbol*) * numSymbols));
  memset(object.symbolRemaps, 0, sizeof(ElfSymbol*) * numSymbols);

  // NOTE(Isaac): start at 1 to skip the nulled symbol at the beginning
  for (unsigned int i = 1u;
       i < numSymbols;
       i++)
  {
    fseek(object.f, table->offset + i * SYMBOL_TABLE_ENTRY_SIZE, SEEK_SET);
    const ElfSection* stringTable = object.sections[table->link];
    ElfSymbol* symbol = new ElfSymbol(elf, nullptr, ElfSymbol::Binding::SYM_BIND_LOCAL, ElfSymbol::Type::SYM_TYPE_NONE, 0u, 0u);
    symbol->index = elf.numSymbols;
    uint32_t nameOffset;
  
    /*0x00*/fread(&nameOffset,              sizeof(uint32_t), 1, object.f);
    /*0x04*/fread(&(symbol->info),          sizeof(uint8_t) , 1, object.f);
    /*0x05*/fseek(object.f, ftell(object.f) + 0x01, SEEK_SET);
    /*0x06*/fread(&(symbol->sectionIndex),  sizeof(uint16_t), 1, object.f);
    /*0x08*/fread(&(symbol->value),         sizeof(uint64_t), 1, object.f);
    /*0x10*/fread(&(symbol->size),          sizeof(uint64_t), 1, object.f);
    /*0x18*/

    /*
     * NOTE(Isaac): skip file symbols (we don't want them in the file executable)
     */
    if ((symbol->info & 0xf) == ElfSymbol::Type::SYM_TYPE_FILE)
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

static void ParseRelocationSection(ErrorState& errorState, ElfFile& elf, ElfObject& object, ElfSection* section)
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

    uint64_t offset;
    uint64_t info;
    int64_t  addend;
    /*0x00*/fread(&offset,  sizeof(uint64_t), 1, object.f);
    /*0x08*/fread(&info,    sizeof(uint64_t), 1, object.f);
    /*0x10*/fread(&addend,  sizeof(int64_t) , 1, object.f);
    /*0x18*/

    ElfRelocation* relocation = new ElfRelocation(elf, nullptr, offset, static_cast<ElfRelocation::Type>(info & 0xFFFFFFFFL),
                                                  object.symbolRemaps[(info >> 32u) & 0xFFFFFFFFL], addend, nullptr);

    if (!(relocation->symbol))
    {
      RaiseError(errorState, ERROR_UNRESOLVED_SYMBOL, "unknown (from external relocation section)");
    }

    elf.relocations.push_back(relocation);
    object.relocations.push_back(relocation);
  }
}

void LinkObject(ElfFile& elf, const char* objectPath)
{
  ErrorState errorState(ErrorState::Type::LINKING);

  ElfObject object;
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
  for (ElfSection* section : object.sections)
  {
    switch (section->type)
    {
      case ElfSection::Type::SHT_SYMTAB:
      {
        ParseSymbolTable(errorState, elf, object, section);
      } break;

      case ElfSection::Type::SHT_REL:
      {
        RaiseError(errorState, ICE_GENERIC, "SHT_REL sections are not supported, use SHT_RELA sections instead");
      } break;

      case ElfSection::Type::SHT_RELA:
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
  ElfSection* text = nullptr;
  for (ElfSection* section : object.sections)
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
  std::vector<ElfSymbol*> functionSymbols;

  // Extract functions from the symbol table and their code from .text
  for (ElfSymbol* symbol : object.symbols)
  {
    /*
     * NOTE(Isaac): NASM refuses to emit symbols for functions with the correct type,
     * so assume that symbols with no type that are in .text are also functions
     */
    if (((symbol->info & 0xf) == ElfSymbol::Type::SYM_TYPE_FUNCTION) ||
        ((symbol->info & 0xf) == ElfSymbol::Type::SYM_TYPE_NONE && symbol->sectionIndex == text->index))
    {
      functionSymbols.push_back(symbol);
    }
  }

  // Sort the linked list by the symbols' offsets into the section
  std::sort(functionSymbols.begin(), functionSymbols.end(), [](ElfSymbol*& a, ElfSymbol*& b)
    {
      return (a->value < b->value);
    });

  // Extract functions from the external object and link them into ourselves
  for (auto it = functionSymbols.begin();
       it < functionSymbols.end();
       it++)
  {
    ElfSymbol* symbol = *it;
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
    ElfSymbol* thingSymbol = new ElfSymbol(elf, symbol->name->str, ElfSymbol::Binding::SYM_BIND_GLOBAL,
                                           ElfSymbol::Type::SYM_TYPE_FUNCTION, GetSection(elf, ".text")->index, 0u);
    ElfThing* thing = new ElfThing(GetSection(elf, ".text"), thingSymbol);
                                                      
    delete thing->data;
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
    object.things.push_back(thing);

    /*
     * We create a new symbol for the function, so remove the old one.
     * NOTE(Isaac): This must be stable, because the symbols are sorted by offset.
     */
    elf.symbols.erase(std::remove(elf.symbols.begin(), elf.symbols.end(), symbol), elf.symbols.end());
  }

  // Complete the relocations loaded from the external object
  for (ElfRelocation* relocation : object.relocations)
  {
    for (ElfThing* thing : object.things)
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
void Emit_<uint8_t>(ElfThing* thing, uint8_t byte)
{
  thing->data[thing->length++] = byte;
}

template<>
void Emit_<uint32_t>(ElfThing* thing, uint32_t i)
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
void Emit_<uint64_t>(ElfThing* thing, uint64_t i)
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

static void EmitHeader(FILE* f, ElfHeader& header)
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

static void EmitProgramEntry(FILE* f, ElfSegment* segment)
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

static void EmitSectionEntry(FILE* f, ElfSection* section)
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

static void EmitSymbolTable(FILE* f, ElfFile& elf, const std::vector<ElfSymbol*>& symbols)
{
  // Emit an empty symbol table entry, because the standard says so
  GetSection(elf, ".symtab")->size += SYMBOL_TABLE_ENTRY_SIZE;
  for (unsigned int i = 0u;
       i < SYMBOL_TABLE_ENTRY_SIZE;
       i++)
  {
    fputc(0x0, f);
  }

  for (ElfSymbol* symbol : symbols)
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

static void EmitStringTable(FILE* f, ElfFile& elf, const std::vector<ElfString*>& strings)
{
  // Lead with a null terminator to mark the null-string
  fputc('\0', f);
  GetSection(elf, ".strtab")->size = 1u;

  for (ElfString* string : strings)
  {
    GetSection(elf, ".strtab")->size += strlen(string->str) + 1u;

    // NOTE(Isaac): add 1 to also write the included null-terminator
    fwrite(string->str, sizeof(char), strlen(string->str) + 1u, f);
  }
}

static void EmitThing(FILE* f, ElfThing* thing, ElfSection* section)
{
  section->size += thing->length;

  /*
   * For now, set the symbol's value relative to the start of the section, since we don't know the address yet
   */
  thing->symbol->value  = ftell(f) - section->offset;
  thing->symbol->size   = thing->length;
  thing->fileOffset     = ftell(f);
  thing->address        = section->address + (thing->fileOffset - section->offset);

  fwrite(thing->data, sizeof(uint8_t), thing->length, f);
}

/*
 * This resolves the symbols in the symbol table that are actually referencing symbols
 * that haven't been linked yet.
 */
static void ResolveUndefinedSymbols(ErrorState& errorState, ElfFile& elf)
{
  /*
   * NOTE(Isaac): We can't start the second loop from the next symbol (to make it O(n log n)) because we can
   * only coalesce the undefined symbol with its matching definition, not the other way around!
   */
  for (auto it = elf.symbols.begin();
       it < elf.symbols.end();
       it++)
  {
    ElfSymbol* symbol = *it;
    bool symbolResolved = false;

    // No work needs to be done for defined symbols
    if (!(symbol->name) || symbol->sectionIndex != 0u)
    {
      continue;
    }

    for (ElfSymbol* otherSymbol : elf.symbols)
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
        for (ElfRelocation* relocation : elf.relocations)
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

const char* GetRelocationTypeName(ElfRelocation::Type type)
{
  switch (type)
  {
    case ElfRelocation::Type::R_X86_64_64:    return "R_X86_64_64";
    case ElfRelocation::Type::R_X86_64_PC32:  return "R_X86_64_PC32";
    case ElfRelocation::Type::R_X86_64_32:    return "R_X86_64_32";
  }

  __builtin_unreachable();
}

static void CompleteRelocations(ErrorState& errorState, FILE* f, ElfFile& elf)
{
  long int currentPosition = ftell(f);

  for (ElfRelocation* relocation : elf.relocations)
  {
    Assert(relocation->thing, "Relocation trying to be applied to a nullptr ElfThing");
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
      case ElfRelocation::Type::R_X86_64_64:     // S + A
      {
        uint64_t value = relocation->symbol->value + addend;
        fwrite(&value, sizeof(uint64_t), 1, f);
      } break;

      case ElfRelocation::Type::R_X86_64_PC32:   // S + A - P
      {
        uint32_t relocationPos = GetSection(elf, ".text")->address + target - GetSection(elf, ".text")->offset;
        uint32_t value = (relocation->symbol->value + addend) - relocationPos;
        fwrite(&value, sizeof(uint32_t), 1, f);
      } break;

      case ElfRelocation::Type::R_X86_64_32:     // S + A
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

static void MapSectionsToSegments(ElfFile& elf)
{
  for (ElfMapping& mapping : elf.mappings)
  {
    ElfSegment* segment = mapping.segment;
    ElfSection* section = mapping.section;

    Assert(section->type != ElfSection::Type::SHT_NOBITS, "We can't tell the size of SHT_NOBITS sections, because its file size is a lie");

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

ElfFile::ElfFile(CodegenTarget& target, bool isRelocatable)
  :isRelocatable(isRelocatable)
  ,target(&target)
  ,header()
  ,segments()
  ,symbols()
  ,strings()
  ,mappings()
  ,relocations()
  ,stringTableTail(1u)    // The ELF standard requires us to have a null byte at the beginning of string tables
  ,numSymbols(0u)
{
  header.fileType                 = (isRelocatable ? ET_REL : ET_EXEC);
  header.entryPoint               = 0x0;
  header.programHeaderOffset      = 0x0;
  header.sectionHeaderOffset      = 0x0;
  header.numProgramHeaderEntries  = 0u;
  header.numSectionHeaderEntries  = 0u;
  header.sectionWithSectionNames  = 0u;
}

void WriteElf(ElfFile& elf, const char* path)
{
  FILE* f = fopen(path, "wb");
  ErrorState errorState(ErrorState::Type::LINKING);

  if (!f)
  {
    RaiseError(errorState, ERROR_INVALID_EXECUTABLE, path);
  }

  ResolveUndefinedSymbols(errorState, elf);

  // Leave space for the ELF header
  fseek(f, 0x40, SEEK_SET);

  // --- Emit all the things ---
  for (ElfSection* section : elf.sections)
  {
    if (section->type != ElfSection::Type::SHT_PROGBITS)
    {
      continue;
    }

    section->offset = ftell(f);

    for (ElfThing* thing : section->things)
    {
      EmitThing(f, thing, section);
    }
  }

  MapSectionsToSegments(elf);

  // --- Recalculate symbol values ---
  for (ElfSection* section : elf.sections)
  {
    if (section->type != ElfSection::Type::SHT_PROGBITS)
    {
      continue;
    }

    /*
     * If this is a relocatable, we want to emit symbol values relative to their section's *offset*
     * If this is an executable, they should be relative to the final *virtual address*
     */
    uint64_t symbolOffset = (elf.isRelocatable ? 0u : section->address);
    for (ElfThing* thing : section->things)
    {
      thing->symbol->value += symbolOffset;
    }
  }

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
    ElfSymbol* startSymbol = GetSymbol(elf, "_start");
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

  for (ElfSection* section : elf.sections)
  {
    EmitSectionEntry(f, section);
  }

  // --- Emit the program header ---
  if (elf.segments.size() > 0u)
  {
    elf.header.programHeaderOffset = ftell(f);
    for (ElfSegment* segment : elf.segments)
    {
      EmitProgramEntry(f, segment);
    }
  }

  // --- Emit the ELF header ---
  fseek(f, 0x0, SEEK_SET);
  EmitHeader(f, elf.header);

  fclose(f);
}
