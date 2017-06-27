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
#include <codegen.hpp>
#include <module.hpp>

#if 1
  #define TIME_EXECUTION
  #include <chrono>
#endif

#define OUTPUT_DOT
#ifdef OUTPUT_DOT
  #include <dotEmitter.hpp>
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
      bool compileSuccessful = Parse(&parse, f.name.c_str());
//      printf("%20s\n", (compileSuccessful ? "\x1B[32m[PASSED]\x1B[0m" : "\x1B[31m[FAILED]\x1B[0m"));

      failed |= !compileSuccessful;
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

  error_state errorState = CreateErrorState(GENERAL_STUFF);
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
        if (!DoesFileExist(dependency->path))
        {
          RaiseError(errorState, ERROR_MISSING_MODULE, dependency->path);
        }

        result.filesToLink.push_back(dependency->path);

        // Import the module info file
        char* modInfoPath = static_cast<char*>(malloc(sizeof(char)*(strlen(dependency->path)+strlen(ROO_MODULE_EXT)+1u)));
        strcpy(modInfoPath, dependency->path);
        strcat(modInfoPath, ROO_MODULE_EXT);

        if (ImportModule(modInfoPath, result).hasErrored)
        {
          RaiseError(errorState, ERROR_MISSING_MODULE, dependency->path);
        }

        free(modInfoPath);
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
    Crash(); 
  }

  CompleteIR(result);

  // TODO: actually apply passes
  /*
  for (auto* it = result.codeThings.head;
       it < result.codeThings.tail;
       it++)
  {
    thing_of_code* code = *it;

    if (!(code->attribs.isPrototype) && HasCode(code))
    {
      if (ApplyASTPasses(result, code, code->ast).hasErrored)
      {
        RaiseError(errorState, ERROR_COMPILE_ERRORS);
      }
    }
  }
  */

#ifdef OUTPUT_DOT
  for (ThingOfCode* code : result.codeThings)
  {
    if (code->attribs.isPrototype)
    {
      continue;
    }

    EmitDOT(code);
  }
#endif

  if (result.isModule)
  {
    char* modulePath = static_cast<char*>(malloc(sizeof(char)*(strlen(result.name)+strlen(ROO_MODULE_EXT))));
    strcpy(modulePath, result.name);
    strcat(modulePath, ROO_MODULE_EXT);
    ExportModule(modulePath, result);
    free(modulePath);
  }

  // --- Generate AIR for each code thing ---
  for (ThingOfCode* code : result.codeThings)
  {
    if (code->attribs.isPrototype)
    {
      continue;
    }

    GenerateAIR(target, code);

#ifdef OUTPUT_DOT
    //OutputInterferenceDOT(code);
#endif
  }

  if (!(result.name))
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
