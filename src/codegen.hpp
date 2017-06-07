/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <cstdio>
#include <ast.hpp>

struct code_generator
{
  FILE* output;
};

void Generate(const char* outputPath, codegen_target& target, parse_result& result);
void InitCodegenTarget(codegen_target& target);
void FreeCodegenTarget(codegen_target& target);

void CreateCodeGenerator(code_generator& generator, const char* outputPath);
void FreeCodeGenerator(code_generator& generator);
void GenCodeSection(code_generator& generator, parse_result& parse);
void GenDataSection(code_generator& generator, parse_result& parse);
