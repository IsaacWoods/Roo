/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <cstdio>
#include <ir.hpp>

struct code_generator
{
  FILE* output;
  unsigned int tabCount;
};

void CreateCodeGenerator(code_generator& generator, const char* outputPath);
void FreeCodeGenerator(code_generator& generator);

void GenFunction(code_generator& generator, function_def* function);
