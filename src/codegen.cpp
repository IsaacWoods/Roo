/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <codegen.hpp>

void CreateCodeGenerator(code_generator& generator, const char* outputPath)
{
  generator.output = fopen(outputPath, "w");
  fprintf(generator.output, "Hello, World!\n");
}

void FreeCodeGenerator(code_generator& generator)
{
  fclose(generator.output);
}
