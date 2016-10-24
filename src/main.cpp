/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 * See LICENCE.md
 */

#include <cstdio>
#include <tinydir.hpp>
#include <common.hpp>
#include <ast.hpp>
#include <air.hpp>

// NOTE(Isaac): defined in `parsing.cpp`
void InitParseletMaps();
void Parse(parse_result* result, const char* sourcePath);

// NOTE(Isaac): defined in the relevant `codegen_xxx.cpp`
void Generate(const char* outputPath, codegen_target& target, parse_result& result);
void InitCodegenTarget(parse_result& result, codegen_target& target);

compile_result Compile(parse_result result, codegen_target target, const char* directory)
{
  // Find and parse all .roo files in the specified directory
  {
    tinydir_dir dir;
    tinydir_open(&dir, directory);
  
    while (dir.has_next)
    {
      tinydir_file file;
      tinydir_readfile(&dir, &file);
  
      if (file.is_dir || strcmp(file.extension, "roo") != 0)
      {
        tinydir_next(&dir);
        continue;
      }
  
      printf("Parsing Roo source file: %s\n", file.name);
      Parse(&result, file.name); 
      tinydir_next(&dir);
    }
  
    tinydir_close(&dir);
  }

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
    GenFunctionAIR(**functionIt);
  }

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
  if (Compile(result, target, ".") != compile_result::SUCCESS)
  {
    fprintf(stderr, "FATAL: Failed to compile a thing!\n");
    exit(1);
  }

  // Compile the dependencies
  for (auto* dependencyIt = result.dependencies.first;
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

    if (dependencyDirectory != nullptr && Compile(result, target, dependencyDirectory) != compile_result::SUCCESS)
    {
      fprintf(stderr, "FATAL: failed to compile dependency: %s\n", dependencyDirectory);
      exit(1);
    }

    free(dependencyDirectory);
  }

  // Generate the code into a final executable!
  // TODO(Isaac): find a better way to create a filename for the executable
  printf("--- Generating a %s executable ---\n", target.name);  
  Generate("test", target, result);
}
