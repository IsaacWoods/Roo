/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
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

// AST Passes
#include <pass_resolveVars.hpp>
#include <pass_typeChecker.hpp>
#include <pass_constantFolder.hpp>

#if 1
  #define TIME_EXECUTION
  #include <chrono>
#endif

// NOTE(Isaac): defined in the relevant `codegen_xxx.cpp`
void Generate(const char* outputPath, codegen_target& target, parse_result& result);
void InitCodegenTarget(parse_result& result, codegen_target& target);
void FreeCodegenTarget(codegen_target& target);

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
    char* dependencyDirectory = nullptr;

    switch (dependency->type)
    {
      case dependency_def::dependency_type::LOCAL:
      {
        // TODO: resolve the path
        fprintf(stderr, "Failed to find local dependency: %s\n", dependency->path);
        continue;
      } break;

      case dependency_def::dependency_type::REMOTE:
      {
        // TODO: clone the repo and pass it's path
      } break;
    }

    assert(dependencyDirectory);

    if (!Compile(result, dependencyDirectory))
    {
      RaiseError(errorState, ERROR_COMPILE_ERRORS);
    }

    free(dependencyDirectory);
  }

  CompleteIR(result);

  // --- Apply AST Passes ---
  bool failedToApplyASTPasses = false;
  #define APPLY_PASS(passName) failedToApplyASTPasses |= !(ApplyASTPass(result, passName));
  APPLY_PASS(PASS_resolveVars);
  APPLY_PASS(PASS_typeChecker);
  APPLY_PASS(PASS_constantFolder);
  #undef APPLY_PASS

  if (failedToApplyASTPasses)
  {
    RaiseError(errorState, ERROR_COMPILE_ERRORS);
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
