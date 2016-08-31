/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 * See LICENCE.md
 */

#include <cstdio>
#include <parsing.hpp>

#if 1
token NextToken(roo_parser& parser, bool ignoreLines);

int main()
{
  roo_parser parser;
  CreateParser(parser, "test.roo");
  
  while (parser.currentToken.type != TOKEN_INVALID)
  {
    if (parser.currentToken.type == TOKEN_IDENTIFIER)
      printf("Token: (%s)\n", GetTextFromToken(parser.currentToken));
    else if ((parser.currentToken.type == TOKEN_NUMBER_INT) ||
             (parser.currentToken.type == TOKEN_NUMBER_FLOAT))
      printf("Token: [%s]\n", GetTextFromToken(parser.currentToken));
    else
      printf("Token: %s\n", GetTokenName(parser.currentToken.type));

    NextToken(parser, false);
  }
}
#else
int main()
{
  roo_parser parser;
  CreateParser(parser, "test.roo");
  Parse(parser);

  FreeParser(parser);

  return 0;
}
#endif
