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
void InitCodegenTarget(codegen_target& target);

int main()
{
  InitParseletMaps();

  // Create the target for the codegen
  codegen_target target;
  InitCodegenTarget(target);

  parse_result result;
  CreateParseResult(result);

  // Parse .roo files in the current directory
  {
    tinydir_dir dir;
    tinydir_open(&dir, ".");
  
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

  // Generate code
  // TODO(Isaac): find a better way to create a filename for the executable
  printf("--- Generating a %s executable ---\n", target.name);  
  Generate("test", target, result);

  // Free everything
  FreeParseResult(result);
  return 0;
}
