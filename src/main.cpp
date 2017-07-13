/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <cstdio>
#include <common.hpp>
#include <ir.hpp>
#include <ast.hpp>
#include <air.hpp>
#include <error.hpp>
#include <module.hpp>
#include <passes/passes.hpp>
#include <codegen.hpp>
#include <x64/codeGenerator.hpp>

#if 1
  #define TIME_EXECUTION
  #include <chrono>
#endif

/*
 * Find and compile all .roo files in the specified directory.
 * Returns `true` if the compilation was successful, `false` if an error occured.
 */
static bool Compile(ParseResult& parse, const char* directoryPath)
{
  Directory directory(directoryPath);
  bool failed = false;

  for (File& f : directory.files)
  {
    if (f.extension == "roo")
    {
      printf("Compiling file \x1B[1;37m%s\x1B[0m\n", f.name.c_str());
      Parser parser(parse, f.name.c_str());
      failed |= parser.errorState.hasErrored;
    }
  }

  return !failed;
}

#define ROO_MODULE_EXT ".roomod"

int main()
{
#ifdef TIME_EXECUTION
  auto begin = std::chrono::high_resolution_clock::now();
#endif

  ErrorState errorState(ErrorState::Type::GENERAL_STUFF);
  ParseResult result;
  CodegenTarget target;

  // Compile the current directory
  if (!Compile(result, "."))
  {
    RaiseError(errorState, ERROR_COMPILE_ERRORS);
  }

  // Compile the dependencies
  for (DependencyDef* dependency : result.dependencies)
  {
    switch (dependency->type)
    {
      /*
       * E.g: For a module `Prelude`, there will be two files:
       *  - An ELF relocatable called `Prelude` that should be linked against
       *  - A binary file called `Prelude.roomod` that should be imported using `ImportModule`
       */
      case DependencyDef::Type::LOCAL:
      {
        // Check the relocatable exists and say to link against it
        if (!DoesFileExist(dependency->path.c_str()))
        {
          RaiseError(errorState, ERROR_MISSING_MODULE, dependency->path.c_str());
        }

        result.filesToLink.push_back(dependency->path);

        // Import the module info file
        if (ImportModule(dependency->path + ROO_MODULE_EXT, result).hasErrored)
        {
          RaiseError(errorState, ERROR_MISSING_MODULE, dependency->path.c_str());
        }
      } break;

      case DependencyDef::Type::REMOTE:
      {
        // TODO
      } break;
    }
  }

  // If we've already encoutered (and hopefully correctly reported) error(s), there's no point carrying on
  if (errorState.hasErrored)
  {
    RaiseError(errorState, ERROR_COMPILE_ERRORS);
  }

  CompleteIR(result);

  #define APPLY_PASS(PassType)\
  {\
    PassType pass;\
    pass.Apply(result);\
  }

  APPLY_PASS(VariableResolverPass);
  APPLY_PASS(TypeChecker);
  //APPLY_PASS(ConstantFolderPass);
  APPLY_PASS(ConditionFolderPass);

#ifdef OUTPUT_DOT
  APPLY_PASS(DotEmitterPass);
#endif

  for (CodeThing* thing : result.codeThings)
  {
    if (thing->attribs.isPrototype)
    {
      continue;
    }

    if (thing->errorState.hasErrored)
    {
      RaiseError(errorState, ERROR_COMPILE_ERRORS);
    }
  }

  if (result.isModule)
  {
    ExportModule(result.name + ROO_MODULE_EXT, result);
  }

  // --- Generate AIR for each code thing ---
  AirGenerator airGenerator(target);
  airGenerator.Apply(result);

  if (result.name == "")  // TODO: Better way to detect no program name given
  {
    RaiseError(errorState, ERROR_NO_PROGRAM_NAME);
  }

  Generate(result.name, target, result);

#ifdef TIME_EXECUTION
  auto end = std::chrono::high_resolution_clock::now();
  // NOTE(Isaac): chrono uses integer types to represent ticks, so we use microseconds then convert ourselves.
  double elapsed = (double)(std::chrono::duration_cast<std::chrono::microseconds>(end-begin).count()) / 1000.0;
  printf("Time taken to compile: %f ms\n", elapsed);
#endif

  return 0;
}
