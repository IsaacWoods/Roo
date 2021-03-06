/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <cstdio>
#include <common.hpp>
#include <ir.hpp>
#include <parsing.hpp>
#include <ast.hpp>
#include <air.hpp>
#include <error.hpp>
#include <module.hpp>
#include <passes/passes.hpp>
#include <codegen.hpp>
#include <x64/x64.hpp>
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
      RooParser parser(parse, f.name);
      failed |= parser.errorState->hasErrored;
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

  ErrorState* errorState = new ErrorState();
  ParseResult result;

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
        ErrorState* moduleState = ImportModule(dependency->path + ROO_MODULE_EXT, result);
        if (moduleState->hasErrored)
        {
          RaiseError(errorState, ERROR_MISSING_MODULE, dependency->path.c_str());
        }
        delete moduleState;
      } break;

      case DependencyDef::Type::REMOTE:
      {
        // TODO
      } break;
    }
  }

  // If we've already encoutered (and hopefully correctly reported) error(s), there's no point carrying on
  if (errorState->hasErrored)
  {
    RaiseError(errorState, ERROR_COMPILE_ERRORS);
  }

  if (result.name == "")  // TODO: Better way to detect no program name given
  {
    RaiseError(errorState, ERROR_NO_PROGRAM_NAME);
  }

  TargetMachine* target = new TargetMachine_x64(result);
  CompleteIR(result, target);

  #define APPLY_PASS(PassType)\
  {\
    PassType pass;\
    pass.Apply(result, target);\
  }

  /*
   * We emit the DOT of the AST both before and after the passes run, so if one fails, we still have an
   * AST to look at.
   */
#ifdef OUTPUT_DOT
  APPLY_PASS(DotEmitterPass);
#endif

  APPLY_PASS(ScopeResolverPass);
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

    if (thing->errorState->hasErrored)
    {
      RaiseError(errorState, ERROR_COMPILE_ERRORS);
    }
  }

  if (result.isModule)
  {
    ErrorState* moduleState = ExportModule(result.name + ROO_MODULE_EXT, result);
    if (moduleState->hasErrored)
    {
      RaiseError(errorState, ERROR_FAILED_TO_EXPORT_MODULE, (result.name + ROO_MODULE_EXT).c_str(), "Export failed");
    }
    delete moduleState;
  }

  // --- Generate AIR for each code thing ---
  AirGenerator airGenerator;
  airGenerator.Apply(result, target);

  Generate(result.name, target, result);

#ifdef TIME_EXECUTION
  auto end = std::chrono::high_resolution_clock::now();
  // NOTE(Isaac): chrono uses integer types to represent ticks, so we use microseconds then convert ourselves.
  double elapsed = (double)(std::chrono::duration_cast<std::chrono::microseconds>(end-begin).count()) / 1000.0;
  printf("Time taken to compile: %f ms\n", elapsed);
#endif

  return 0;
}
