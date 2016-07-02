/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 * See LICENCE.md
 */

#include <stdio.h>
#include <stdlib.h>

const char* ReadFile(const char* path)
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

int main()
{
  printf("Hello, World!\n");
  const char* fileContents = ReadFile("test.roo");
  printf("%s\n", fileContents);

  return 0;
}
