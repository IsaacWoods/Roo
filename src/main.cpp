/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 * See LICENCE.md
 */

#include <cstdio>
#include <common.hpp>
#include <ir.hpp>
#include <ast.hpp>
#include <air.hpp>
#include <error.hpp>

// AST Passes
#include <pass_resolveVars.hpp>
#include <pass_resolveFunctionCalls.hpp>
#include <pass_typeChecker.hpp>

// NOTE(Isaac): turning this on causes a segfault in the printf call (or the one printing "Hello, World!").
// Fuck knows what's going on there
//#define TIME_EXECUTION

#ifdef TIME_EXECUTION
  #include <chrono>
  #include <iostream>
#endif

// NOTE(Isaac): defined in `parsing.cpp`
void InitParseletMaps();
void Parse(parse_result* result, const char* sourcePath);

// NOTE(Isaac): defined in the relevant `codegen_xxx.cpp`
void Generate(const char* outputPath, codegen_target& target, parse_result& result);
void InitCodegenTarget(parse_result& result, codegen_target& target);
void FreeCodegenTarget(codegen_target& target);

static compile_result Compile(parse_result& result, const char* directoryPath)
{
  // Find and parse all .roo files in the specified directory
  directory dir;
  OpenDirectory(dir, directoryPath);

  for (auto* f = dir.files.head;
       f < dir.files.tail;
       f++)
  {
    if (f->extension && strcmp(f->extension, "roo") == 0)
    {
      Parse(&result, f->name); 
    }
  }

  Free<directory>(dir);
  return compile_result::SUCCESS;
}

int main()
{
#ifdef TIME_EXECUTION
  auto begin = std::chrono::high_resolution_clock::now();
#endif

  InitParseletMaps();

  parse_result result;
  CreateParseResult(result);

  // Create the target for the codegen
  codegen_target target;
  InitCodegenTarget(result, target);

  // Compile the current directory
  if (Compile(result, ".") != compile_result::SUCCESS)
  {
    fprintf(stderr, "FATAL: Failed to compile a thing!\n");
    Crash();
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

    if (dependencyDirectory != nullptr && Compile(result, dependencyDirectory) != compile_result::SUCCESS)
    {
      fprintf(stderr, "FATAL: failed to compile dependency: %s\n", dependencyDirectory);
      Crash();
    }

    free(dependencyDirectory);
  }

  // Complete the AST and apply all the passes
  CompleteIR(result);
  ApplyASTPass(result, PASS_resolveVars);
  ApplyASTPass(result, PASS_resolveFunctionCalls);
  ApplyASTPass(result, PASS_typeChecker);

  // Generate AIR instructions from the AST
  // TODO(Isaac): function generation should be independent, so parralelise this with a job server system
  for (auto* it = result.functions.head;
       it < result.functions.tail;
       it++)
  {
    function_def* function = *it;

    if (GetAttrib(function->code, attrib_type::PROTOTYPE))
    {
      continue;
    }

    GenerateAIR(target, function->code);
  }

  #ifdef OUTPUT_DOT
  for (auto* it = result.functions.head;
       it < result.functions.tail;
       it++)
  {
    function_def* function = *it;
    OutputDOTOfAST(function);
    OutputInterferenceDOT(function->code, function->name);

    printf("Cost of function '%s': %u\n", function->name, GetCodeCost(function->code));
  }
  #endif

  if (!(result.name))
  {
    RaiseError(FATAL_NO_PROGRAM_NAME);
  }

  // Generate the code into a final executable!
  Generate(result.name, target, result);

  Free<parse_result>(result);
  Free<codegen_target>(target);

#ifdef TIME_EXECUTION
//  auto end = std::chrono::high_resolution_clock::now();
  // NOTE(Isaac): chrono uses integer types to represent ticks, so we use microseconds then convert ourselves.
//  double elapsed = (double)(std::chrono::duration_cast<std::chrono::microseconds>(end-begin).count()) / 1000.0;
//  printf("Time taken to compile: %f ms\n", 4.3);
//  printf("Hello, World!\n");
#endif

  return 0;
}
