/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 * See LICENCE.md
 */

#include <cstdio>
#include <parsing.hpp>

int main()
{
  roo_parser parser;
  CreateParser(parser, "test.roo");

  while (parser.currentToken.type != TOKEN_INVALID)
  {
    printf("Token: %s\n", GetTokenName(parser.currentToken.type));
    NextToken(parser);
  }

  return 0;
}
