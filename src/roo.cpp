/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 * See LICENCE.md
 */

#include <cstdio>
#include <parsing.hpp>
#include <tinydir.hpp>

#if 0
token NextToken(roo_parser& parser, bool ignoreLines);

int main()
{
  roo_parser parser;
  CreateParser(parser, "test.roo");
  
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
}
#else
int main()
{
  parse_result result;

  // Parse .roo files in the current directory
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

  // TODO: do something with the parse result

  FreeParseResult(result);
  return 0;
}
#endif
