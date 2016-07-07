/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

typedef struct
{
  const char* source;
  const char* currentChar; // NOTE(Isaac): this points into `source`
} roo_parser;

void CreateParser(roo_parser* parser, const char* sourcePath);
void FreeParser(roo_parser* parser);
char NextChar(roo_parser* parser);
