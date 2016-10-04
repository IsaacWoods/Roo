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

void Generate(const char* outputPath, codegen_target& target, parse_result& result)
{
  FILE* file = fopen(outputPath, "wb");

  GenerateELFHeader(file);

  fclose(file);
}
