/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <cstdio>

struct code_generator
{
  FILE* output;
};

void CreateCodeGenerator(code_generator& generator, const char* outputPath);
void FreeCodeGenerator(code_generator& generator);
