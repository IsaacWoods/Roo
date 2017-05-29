/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <cstdio>
#include <cassert>
#include <common.hpp>
#include <ir.hpp>
#include <ast.hpp>
#include <air.hpp>
#include <error.hpp>
#include <scheduler.hpp>
#include <codegen.hpp>
#include <module.hpp>

#if 1
  #define TIME_EXECUTION
  #include <chrono>
#endif

/*
 * Find and compile all .roo files in the specified directory.
 * NOTE(Isaac): Returns `true` if the compilation was successful, `false` if an error occured.
 */
static bool Compile(parse_result& parse, const char* directoryPath)
{
  directory dir;
  OpenDirectory(dir, directoryPath);
  bool failed = false;

  for (auto* f = dir.files.head;
       f < dir.files.tail;
       f++)
  {
    if (f->extension && strcmp(f->extension, "roo") == 0)
    {
      printf("Compiling file \x1B[1;37m%s\x1B[0m\n", f->name);
      bool compileSuccessful = Parse(&parse, f->name);
//      printf("%20s\n", (compileSuccessful ? "\x1B[32m[PASSED]\x1B[0m" : "\x1B[31m[FAILED]\x1B[0m"));

      failed |= !compileSuccessful;
    }
  }

  Free<directory>(dir);
  return !failed;
}

#define ROO_MODULE_EXT ".roomod"

int main()
{
#ifdef TIME_EXECUTION
  auto begin = std::chrono::high_resolution_clock::now();
#endif

  error_state errorState = CreateErrorState(GENERAL_STUFF);
  parse_result result;
  CreateParseResult(result);

  // Create the target for the codegen
  codegen_target target;
  InitCodegenTarget(result, target);

  // Compile the current directory
  if (!Compile(result, "."))
  {
    RaiseError(errorState, ERROR_COMPILE_ERRORS);
  }

  // Compile the dependencies
  for (auto* it = result.dependencies.head;
       it < result.dependencies.tail;
       it++)
  {
    dependency_def* dependency = *it;

    switch (dependency->type)
    {
      /*
       * E.g: For a module `Prelude`, there will be two files:
       *  - An ELF relocatable called `Prelude` that should be linked against
       *  - A binary file called `Prelude.roomod` that should be imported using `ImportModule`
       */
      case dependency_def::dependency_type::LOCAL:
      {
        // Check the relocatable exists and say to link against it
        if (!DoesFileExist(dependency->path))
        {
          RaiseError(errorState, ERROR_MISSING_MODULE, dependency->path);
        }

        Add<char*>(result.filesToLink, dependency->path);

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

      case dependency_def::dependency_type::REMOTE:
      {
      } break;
    }
  }

  CompleteIR(result);

#ifdef OUTPUT_DOT
  for (auto* it = result.codeThings.head;
       it < result.codeThings.tail;
       it++)
  {
    thing_of_code* code = *it;

    if (code->attribs.isPrototype)
    {
      continue;
    }

    OutputDOTOfAST(code);
  }
#endif

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

#ifdef OUTPUT_DOT
  for (auto* it = result.codeThings.head;
       it < result.codeThings.tail;
       it++)
  {
    thing_of_code* code = *it;

    if (code->attribs.isPrototype)
    {
      continue;
    }

    OutputDOTOfAST(code);
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

  // --- Create tasks for generating AIR for functions and operators ---
  scheduler s;
  InitScheduler(s);

  for (auto* it = result.codeThings.head;
       it < result.codeThings.tail;
       it++)
  {
    thing_of_code* code = *it;

    if (code->attribs.isPrototype)
    {
      continue;
    }

    AddTask(s, task_type::GENERATE_AIR, code);
  }

  // --- Run the tasks ---
  // TODO: In the future, these are in theory parallelizable, so run them on multiple threads?
  while (s.tasks.size > 0u)
  {
    task_info* task = GetTask(s);

    switch (task->type)
    {
      case task_type::GENERATE_AIR:
      {
        GenerateAIR(target, task->code);

#ifdef OUTPUT_DOT
        OutputInterferenceDOT(task->code);
#endif
      } break;
    }
  }

  Free<scheduler>(s);

  if (!(result.name))
  {
    RaiseError(errorState, ERROR_NO_PROGRAM_NAME);
  }

  Generate(result.name, target, result);

  Free<parse_result>(result);
  Free<codegen_target>(target);

#ifdef TIME_EXECUTION
  auto end = std::chrono::high_resolution_clock::now();
  // NOTE(Isaac): chrono uses integer types to represent ticks, so we use microseconds then convert ourselves.
  double elapsed = (double)(std::chrono::duration_cast<std::chrono::microseconds>(end-begin).count()) / 1000.0;
  printf("Time taken to compile: %f ms\n", elapsed);
#endif

  return 0;
}
