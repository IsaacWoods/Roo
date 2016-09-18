/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 * See LICENCE.md
 */

#include <cstdio>
#include <tinydir.hpp>
#include <parsing.hpp>
#include <codegen.hpp>

int main()
{
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
  
      roo_parser parser;
      CreateParser(parser, &result, file.name);
      Parse(parser);
      FreeParser(parser);
  
      tinydir_next(&dir);
    }
  
    tinydir_close(&dir);
  }

  // Generate code
  code_generator generator;
  CreateCodeGenerator(generator, "test.s");
  GenCodeSection(generator, result);
  GenDataSection(generator, result);

  // Free everything
  FreeCodeGenerator(generator);
  FreeParseResult(result);
  return 0;
}
