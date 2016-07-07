/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <parsing.h>
#include <stdio.h>
#include <stdlib.h>

/*
 * Reads a file as a string. The string is allocated and it is the responsibility of the caller to free it.
 */
static const char* ReadFile(const char* path)
{
  FILE* file = fopen(path, "rb");

  if (!file)
  {
    fprintf(stderr, "Failed to read source file: %s\n", path);
    exit(1);
  }

  fseek(file, 0, SEEK_END);
  unsigned long length = ftell(file);
  fseek(file, 0, SEEK_SET);
  char* contents = malloc(length + 1);

  if (!contents)
  {
    fprintf(stderr, "Failed to allocate space for source file!\n");
    exit(1);
  }

  if (fread(contents, 1, length, file) != length)
  {
    fprintf(stderr, "Failed to read source file: %s\n", path);
    exit(1);
  }

  contents[length] = '\0';
  fclose(file);

  return contents;
}

void CreateParser(roo_parser* parser, const char* sourcePath)
{
  parser->source = ReadFile(sourcePath);
  parser->currentChar = parser->source;
}

void FreeParser(roo_parser* parser)
{
  free(parser->source);
  parser->source = NULL;
  parser->currentChar = NULL;
}

char NextChar(roo_parser* parser)
{
  // Don't dereference memory past the end of the string
  if (parser->currentChar == '\0')
    return '\0';

  parser->currentChar++;
  return parser->currentChar;
}
