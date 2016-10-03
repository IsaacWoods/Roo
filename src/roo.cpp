/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 * See LICENCE.md
 */

#include <cstdio>
#include <tinydir.hpp>
#include <air.hpp>
#include <codegen.hpp>

// NOTE(Isaac): defined in `parsing.cpp`
void InitParseletMaps();
void Parse(parse_result* result, const char* sourcePath);

int main()
{
  InitParseletMaps();

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

  // Generate AIR instructions from the AST
  // TODO(Isaac): function generation should be independent, so parralelise this with a job server system
  for (function_def* function = result.firstFunction;
       function;
       function = function->next)
  {
    GenFunctionAIR(function);
  }

  // Generate code
/*  code_generator generator;
  CreateCodeGenerator(generator, "test.s");
  GenCodeSection(generator, result);
  GenDataSection(generator, result);*/

  // Free everything
//  FreeCodeGenerator(generator);
  FreeParseResult(result);
  return 0;
}
