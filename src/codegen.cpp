/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <codegen.hpp>
#include <elf/elf.hpp>
#include <x64/codeGenerator.hpp>

TargetMachine::TargetMachine(const std::string& name, unsigned int numRegisters,
                                                      unsigned int numGeneralRegisters,
                                                      unsigned int generalRegisterSize,
                                                      unsigned int numIntParamColors,
                                                      unsigned int functionReturnColor)
  :name(name)
  ,numRegisters(numRegisters)
  ,registerSet(new RegisterDef[numRegisters])
  ,numGeneralRegisters(numGeneralRegisters)
  ,generalRegisterSize(generalRegisterSize)
  ,numIntParamColors(numIntParamColors)
  ,intParamColors(new unsigned int[numIntParamColors])
  ,functionReturnColor(functionReturnColor)
{
}

TargetMachine::~TargetMachine()
{
/*  for (unsigned int i = 0u;
       i < numRegisters;
       i++)
  {
    delete registerSet[i].pimpl;
  }*/

  delete[] registerSet;
  delete[] intParamColors;
}

void Generate(const std::string& outputPath, TargetMachine* target, ParseResult& result)
{
  /*
   * If we're compiling a module, we need to produce a relocatable.
   * If not, we want to produce a normal executable.
   */
  ElfFile elf(target, result.isModule);

  // .text
  ElfSection* textSection = new ElfSection(elf, ".text", ElfSection::Type::SHT_PROGBITS, 0x10);
  textSection->flags = SECTION_ATTRIB_A|SECTION_ATTRIB_E;

  // .rodata
  ElfSection* rodataSection = new ElfSection(elf, ".rodata", ElfSection::Type::SHT_PROGBITS, 0x04);
  rodataSection->flags = SECTION_ATTRIB_A;

  // .strtab
  ElfSection* stringTableSection = new ElfSection(elf, ".strtab", ElfSection::Type::SHT_STRTAB, 0x04);

  // .symtab
  ElfSection* symbolTableSection = new ElfSection(elf, ".symtab", ElfSection::Type::SHT_SYMTAB, 0x04);
  symbolTableSection->link = stringTableSection->index;
  symbolTableSection->entrySize = 0x18;

  // Create an ElfThing to put the contents of .rodata into
  ElfSymbol* rodataSymbol = new ElfSymbol(elf, nullptr, ElfSymbol::Binding::SYM_BIND_GLOBAL, ElfSymbol::Type::SYM_TYPE_SECTION, rodataSection->index, 0x00);
  ElfThing* rodataThing = new ElfThing(rodataSection, rodataSymbol);

  if (!(result.isModule))
  {
    ElfSegment* loadSegment = new ElfSegment(elf, ElfSegment::Type::PT_LOAD, SEGMENT_ATTRIB_X|SEGMENT_ATTRIB_R, 0x400000, 0x200000);
    loadSegment->offset = 0x00;
    loadSegment->size.inFile = 0x40;  // NOTE(Isaac): set the tail to the end of the ELF header

    MapSection(elf, loadSegment, textSection);
    MapSection(elf, loadSegment, rodataSection);
  }

  // Link with any files we've been told to
  for (const std::string& file : result.filesToLink)
  {
    // TODO: eww use std::string throughout
    LinkObject(elf, file.c_str());
  }

  // Emit string constants into the .rodata thing
  unsigned int tail = 0u;
  for (StringConstant* constant : result.strings)
  {
    constant->offset = tail;

    for (const char* c = constant->str.c_str();
         *c;
         c++)
    {
      Emit<uint8_t>(rodataThing, *c);
      tail++;
    }

    // Add a null terminator
    Emit<uint8_t>(rodataThing, '\0');
    tail++;
  }

  // --- Generate error states and symbols for things of code ---
  for (CodeThing* thing : result.codeThings)
  {
    thing->errorState = ErrorState(ErrorState::Type::CODE_GENERATION, thing);

    /*
     * If it's a prototype, we want to reference the symbol of an already loaded (hopefully) function.
     */
    if (thing->attribs.isPrototype)
    {
      for (ElfThing* elfThing : textSection->things)
      {
        if (thing->mangledName == elfThing->symbol->name->str)
        {
          thing->symbol = elfThing->symbol;
        }
      }

      if (!(thing->symbol))
      {
        RaiseError(thing->errorState, ERROR_UNIMPLEMENTED_PROTOTYPE, thing->mangledName.c_str());
      }
    }
    else
    {
      thing->symbol = new ElfSymbol(elf, thing->mangledName.c_str(), ElfSymbol::Binding::SYM_BIND_GLOBAL, ElfSymbol::Type::SYM_TYPE_FUNCTION, textSection->index, 0x00);
    }
  }

  CodeGenerator* codeGenerator = target->CreateCodeGenerator(elf);

  // --- Create a thing for the bootstrap, if this isn't a module ---
  if (!(result.isModule))
  {
    ElfSymbol* bootstrapSymbol = new ElfSymbol(elf, "_start", ElfSymbol::Binding::SYM_BIND_GLOBAL, ElfSymbol::Type::SYM_TYPE_FUNCTION, textSection->index, 0x00);
    ElfThing* bootstrapThing = new ElfThing(textSection, bootstrapSymbol);
    codeGenerator->GenerateBootstrap(bootstrapThing, result);
  }

  // --- Generate `ElfThing`s for each thing of code ---
  for (CodeThing* thing : result.codeThings)
  {
    if (thing->attribs.isPrototype)
    {
      continue;
    }

    codeGenerator->Generate(thing, rodataThing);
  }

  delete codeGenerator;
  WriteElf(elf, outputPath.c_str());
}

