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
  Parse(parser);

  FreeParser(parser);

  return 0;
}
