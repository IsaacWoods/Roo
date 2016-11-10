/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <elf.hpp>
#include <cstdio>
#include <cstring>
#include <cstdlib>

// TODO: make these real values
#define PROGRAM_HEADER_ENTRY_SIZE 0x52
#define SECTION_HEADER_ENTRY_SIZE 0x40

static elf_string* CreateString(elf_file& elf, const char* str)
{
  elf_string* string = static_cast<elf_string*>(malloc(sizeof(elf_string)));
  string->offset = elf.stringTableTail;
  string->str = str;
  
  elf.stringTableTail += strlen(str) + 1u;

  AddToLinkedList<elf_string*>(elf.strings, string);
  return string;
}

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

/*
 * If `name == nullptr`, the symbol points towards the nulled entry of the string table.
 */
static elf_symbol* CreateSymbol(elf_file& elf, const char* name, symbol_binding binding, symbol_type type,
                                uint16_t sectionIndex, uint64_t value)
{
  elf_symbol* symbol = static_cast<elf_symbol*>(malloc(sizeof(elf_symbol)));
  symbol->name = (name ? CreateString(elf, name) : nullptr);
  symbol->info = type;
  symbol->info |= binding << 4u;
  symbol->sectionIndex = sectionIndex;
  symbol->value = value;
  symbol->size = 0u;

  // TODO: set the `info` field of the symbol table to the index of the first GLOBAL symbol
  AddToLinkedList<elf_symbol*>(elf.symbols, symbol);
  return symbol;
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
  AddToLinkedList<elf_section*>(elf.sections, section);
  return section;
}

// TODO: allow local functions to be bound as local symbols
// TODO: correct offset into section for symbol value
elf_thing* CreateThing(elf_file& elf, const char* name)
{
  const unsigned int INITIAL_DATA_SIZE = 256u;

  elf_thing* thing = static_cast<elf_thing*>(malloc(sizeof(elf_thing)));
  thing->symbol = CreateSymbol(elf, name, SYM_BIND_GLOBAL, SYM_TYPE_FUNCTION, GetSection(elf, ".text")->index, 0u);
  thing->length = 0u;
  thing->capacity = INITIAL_DATA_SIZE;
  thing->data = static_cast<uint8_t*>(malloc(sizeof(uint8_t) * INITIAL_DATA_SIZE));
  thing->fileOffset = 0u;
  thing->address = 0x0;

  return thing;
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

static void EmitStringTable(FILE* f, elf_file& elf, linked_list<elf_string*>& strings)
{
  // Lead with a null terminator to mark the null-string
  fputc('\0', f);

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

static void EmitThing(FILE* f, elf_file& elf, elf_thing* thing)
{
  GetSection(elf, ".text")->size += thing->length;

  for (unsigned int i = 0u;
       i < thing->length;
       i++)
  {
    fputc(thing->data[i], f);
  }
}

void CreateElf(elf_file& elf, codegen_target& target)
{
  elf.target = &target;
  CreateLinkedList<elf_thing*>(elf.things);
  CreateLinkedList<elf_symbol*>(elf.symbols);
  CreateLinkedList<elf_section*>(elf.sections);
  CreateLinkedList<elf_string*>(elf.strings);
  elf.stringTableTail = 1u;

  elf.header.fileType = ET_EXEC;
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

  if (!f)
  {
    fprintf(stderr, "FATAL: unable to create executable at path: %s\n", path);
    exit(1);
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

  // --- Emit the string table ---
  GetSection(elf, ".strtab")->offset = ftell(f);
  elf.header.sectionWithSectionNames = GetSection(elf, ".strtab")->index;
  EmitStringTable(f, elf, elf.strings);

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
//  elf.header.programHeaderOffset = ftell(f);

  // TODO: emit segments


  // --- Emit the ELF header ---
  fseek(f, 0x0, SEEK_SET);
  EmitHeader(f, elf.header);

  fclose(f);
}

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

  fprintf(stderr, "FATAL (PROBABLY): Couldn't find section of name '%s'!\n", name);
  return nullptr;
}

template<>
void Free<elf_file>(elf_file& elf)
{
  FreeLinkedList<elf_section*>(elf.sections);
  FreeLinkedList<elf_thing*>(elf.things);
}

template<>
void Free<elf_string*>(elf_string*& string)
{
  free(string);
}

template<>
void Free<elf_symbol*>(elf_symbol*& symbol)
{
  Free<elf_string*>(symbol->name);
  free(symbol);
}

template<>
void Free<elf_section*>(elf_section*& section)
{
  Free<elf_string*>(section->name);
  free(section);
}

template<>
void Free<elf_thing*>(elf_thing*& thing)
{
  free(thing->data);
}
