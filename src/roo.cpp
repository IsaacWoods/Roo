/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 * See LICENCE.md
 */

#include <cstdio>
#include <parsing.hpp>

#if 0
token NextToken(roo_parser& parser, bool ignoreLines);

int main()
{
  roo_parser parser;
  CreateParser(parser, "test.roo");
  
  while (parser.currentToken.type != TOKEN_INVALID)
  {
    if (parser.currentToken.type == TOKEN_IDENTIFIER)
      printf("Token: (%s)\n", ExtractText(parser.currentToken));
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
