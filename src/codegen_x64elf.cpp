/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <cstdio>
#include <cstdlib>
#include <common.hpp>

static void GenerateELFHeader(FILE* f)
{
  fputc('E', f);
  fputc('L', f);
  fputc('F', f);
}

void Generate(const char* outputPath, parse_result* result)
{
  printf("--- Generating a x64 ELF executable ---\n");
  FILE* file = fopen(outputPath, "wb");

  GenerateELFHeader(file);

  fclose(file);
}
