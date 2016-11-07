/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

enum symbol_type : uint8_t
{
  SYM_TYPE_NONE,
  SYM_TYPE_OBJECT,
  SYM_TYPE_FUNCTION,
  SYM_TYPE_SECTION,
};

struct elf_symbol
{
  uint32_t  name;
  uint8_t   info;
  uint16_t  sectionIndex;
  uint64_t  definitionOffset;
  uint64_t  size;

  unsigned int handle;
};

/*
 * Returns the offset of this string into the string table.
 */
static unsigned int CreateString(elf_file& elf, const char* string)
{
  unsigned int tail = elf.stringTableTail;
  AddToLinkedList<const char*>(elf.strings, string);
  elf.stringTableTail += strlen(string) + 1u;

  return tail;
}

/*
 * If `name == nullptr`, the symbol points towards the nulled entry of the string table.
 */
static elf_symbol* CreateSymbol(elf_file& elf, const char* name, symbol_binding binding, symbol_type type,
                                uint16_t sectionIndex, uint64_t defOffset)
{
  elf_symbol* symbol = static_cast<elf_symbol*>(malloc(sizeof(elf_symbol)));
  symbol->name = (name ? CreateString(elf, name) : 0u);
  symbol->info = type;
  symbol->info |= binding << 4u;
  symbol->sectionIndex = sectionIndex;
  symbol->definitionOffset = defOffset;
  symbol->size = 0u;

  // TODO: set the `info` field of the symbol table to the index of the first GLOBAL symbol
  AddToLinkedList<elf_symbol*>(elf.symbols, symbol);
  return symbol;
}

void CreateElf(elf_file& elf)
{
  CreateLinkedList<elf_thing*>(elf.things);
  CreateLinkedList<elf_symbol*>(elf.symbols);
  CreateLinkedList<const char*>(elf.strings);
  elf.stringTableTail = 1u;
}

static elf_section* CreateSection(elf_file& elf, const char* name, section_type type, uint64_t alignment)
{

}

static void EmitHeader(FILE* f, elf_header& header)
{
  /*
   * NOTE(Isaac): this is already a define, but `fwrite` is stupid, so it has to be a lvalue too
   */
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
/*0x20*/fputc(0x00,     f); // Offset to the program header - there isn't one
        fputc(0x00,     f);
        fputc(0x00,     f);
        fputc(0x00,     f);
        fputc(0x00,     f);
        fputc(0x00,     f);
        fputc(0x00,     f);
        fputc(0x00,     f);
/*0x28*/fwrite(&(header.sectionHeaderOffset), sizeof(uint64_t), 1, f);
/*0x30*/fputc(0x00,     f); // Specify some flags (undefined for x86_64)
        fputc(0x00,     f);
        fputc(0x00,     f);
        fputc(0x00,     f);
/*0x34*/fputc(64u,      f); // Specify the size of the header (64 bytes)
        fputc(0x00,     f);
/*0x36*/fputc(0x00,     f); // Specify the size of a program header entry as 0
        fputc(0x00,     f);
/*0x38*/fputc(0x00,     f); // Specify that there are 0 program header entries
        fputc(0x00,     f);
/*0x3A*/fwrite(&sectionHeaderEntrySize,           sizeof(uint16_t), 1, f);
/*0x3C*/fwrite(&(header.numSectionHeaderEntries), sizeof(uint16_t), 1, f);
/*0x3E*/fwrite(&(header.sectionWithSectionNames), sizeof(uint16_t), 1, f);
/*0x40*/
}

static void EmitSectionEntry(FILE* f, elf_file& elf, elf_section& section)
{
  elf.header.numSectionHeaderEntries++;

/*n + */
/*0x00*/fwrite(&(section.name),             sizeof(uint32_t), 1, f);
/*0x04*/fwrite(&(section.type),             sizeof(uint32_t), 1, f);
/*0x08*/fwrite(&(section.flags),            sizeof(uint64_t), 1, f);
/*0x10*/fwrite(&(section.address),          sizeof(uint64_t), 1, f);
/*0x18*/fwrite(&(section.offset),           sizeof(uint64_t), 1, f);
/*0x20*/fwrite(&(section.size),             sizeof(uint64_t), 1, f);
/*0x28*/fwrite(&(section.link),             sizeof(uint32_t), 1, f);
/*0x2C*/fwrite(&(section.info),             sizeof(uint32_t), 1, f);
/*0x30*/fwrite(&(section.addressAlignment), sizeof(uint64_t), 1, f);
/*0x38*/fwrite(&(section.entrySize),        sizeof(uint64_t), 1, f);
/*0x40*/
}

static void EmitStringTable(FILE* f, linked_list<const char*>& strings)
{
  for (auto* it = strings.first;
       it;
       it = it->next)
  {
    fprint("Emitting string: %s\n", **it);

    for (const char* c = **it;
         *c;
         c++)
    {
      fputc(*c, f);
    }

    fputc('\0', f);
  }
}

static void EmitThing(FILE* f, elf_thing* thing)
{
  // TODO: emit a symbol for the thing
  
  for (unsigned int i = 0u;
       i < thing->length;
       i++)
  {
    fputc(thing->data[i], f);
  }
}

void LinkElf(elf_file& elf, const char* path)
{
  FILE* f = fopen(path, "wb");

  if (!f)
  {
    fprintf(stderr, "FATAL: unable to create executable at path: %s\n", path);
    exit(1);
  }

  // Emit all the things, leave space for the header
  fseek(f, 0x40, SEEK_SET);

  for (auto* thingIt = elf.things.first;
       thingIt;
       thingIt = thingIt->next)
  {
    EmitThing(f, **thingIt);
  }

  // Emit the header
  fseek(f, 0x0, SEEK_SET);
  EmitHeader(f, elf.header);

  fclose(f);
}

void FreeElf(elf_file& elf)
{
  FreeLinkedList<elf_section*>(elf.sections);
  FreeLinkedList<elf_thing*>(elf.things);
}

template<>
void Free<elf_symbol*>(elf_symbol*& symbol)
{
  free(symbol);
}

template<>
void Free<elf_thing*>(elf_thing*& thing)
{
  free(thing->data);
}
