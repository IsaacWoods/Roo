/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 * See LICENCE.md
 */

#include <cstdio>
//#include <tinydir.hpp>
#include <common.hpp>
#include <ir.hpp>
#include <ast.hpp>
#include <air.hpp>

// AST Passes
#include <pass_resolveVars.hpp>
#include <pass_resolveFunctionCalls.hpp>
#include <pass_typeChecker.hpp>

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

  for (auto* it = dir.files.first;
       it;
       it = it->next)
  {
    file* f = **it;

    if (f->extension && strcmp(f->extension, "roo") == 0)
    {
      printf("Parsing Roo source file: %s\n", f->name);
      Parse(&result, f->name); 
    }
  }

  Free<directory>(dir);
  return compile_result::SUCCESS;
}

int main()
{
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
/*  for (auto* dependencyIt = result.dependencies.first;
       dependencyIt;
       dependencyIt = dependencyIt->next)
  {
    char* dependencyDirectory = nullptr;

    switch ((**dependencyIt)->type)
    {
      case dependency_def::dependency_type::LOCAL:
      {
        // TODO: resolve the path
        fprintf(stderr, "Failed to find local dependency!\n");
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
  }*/

  // Complete the AST and apply all the passes
  CompleteIR(result);
  ApplyASTPass(result, PASS_resolveVars);
  ApplyASTPass(result, PASS_resolveFunctionCalls);
  ApplyASTPass(result, PASS_typeChecker);

  // Emit DOT files for function ASTs
  for (auto* functionIt = result.functions.first;
       functionIt;
       functionIt = functionIt->next)
  {
    OutputDOTOfAST(**functionIt);
  }

  // Generate AIR instructions from the AST
  // TODO(Isaac): function generation should be independent, so parralelise this with a job server system
  for (auto* functionIt = result.functions.first;
       functionIt;
       functionIt = functionIt->next)
  {
    if (GetAttrib(**functionIt, function_attrib::attrib_type::PROTOTYPE))
    {
      continue;
    }

    GenFunctionAIR(target, **functionIt);
  }

  // Check the `Name` attribute has been given
  if (!GetAttrib(result, program_attrib::attrib_type::NAME))
  {
    fprintf(stderr, "FATAL: A program name must be given using the #[Name(...)] attribute!\n");
    Crash();
  }

  // Generate the code into a final executable!
  printf("--- Generating a %s executable ---\n", target.name);  
  Generate(GetAttrib(result, program_attrib::attrib_type::NAME)->payload.name, target, result);

  Free<parse_result>(result);
  Free<codegen_target>(target);
}
