/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 * See LICENCE.md
 */

#include <cstdio>
#include <tinydir.hpp>
#include <parsing.hpp>
#include <codegen.hpp>

#if 0
token NextToken(roo_parser& parser, bool ignoreLines);

int main()
{
  parse_result result;
  result.firstFunction = nullptr;
  result.firstType = nullptr;

  roo_parser parser;
  CreateParser(parser, &result, "test.roo");
  
  while (parser.currentToken.type != TOKEN_INVALID)
  {
    if (parser.currentToken.type == TOKEN_IDENTIFIER)
    {
      printf("Token: (%s)\n", GetTextFromToken(parser.currentToken));
    }
    else if ((parser.currentToken.type == TOKEN_NUMBER_INT) ||
             (parser.currentToken.type == TOKEN_NUMBER_FLOAT))
    {
      printf("Token: [%s]\n", GetTextFromToken(parser.currentToken));
    }
    else
    {
      printf("Token: %s\n", GetTokenName(parser.currentToken.type));
    }

    NextToken(parser, false);
  }

  FreeParseResult(result);
}
#else
int main()
{
  parse_result result;
  result.firstFunction = nullptr;
  result.firstType = nullptr;

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
  CreateCodeGenerator(generator, "test");

  for (function_def* function = result.firstFunction;
       function;
       function = function->next)
  {
    GenFunction(generator, function);
  }

  // Free everything
  FreeCodeGenerator(generator);
  FreeParseResult(result);
  return 0;
}
#endif
