/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <codegen.hpp>
#include <cstring>
#include <cstdlib>
#include <cstdarg>

void CreateCodeGenerator(code_generator& generator, const char* outputPath)
{
  generator.output = fopen(outputPath, "w");
  generator.tabCount = 0u;
}

void FreeCodeGenerator(code_generator& generator)
{
  fclose(generator.output);
}

static char* MangleFunctionName(function_def* function)
{
  const char* base = "_R_";
  char* mangled = static_cast<char*>(malloc(sizeof(char) * (strlen(base) + strlen(function->name))));
  strcpy(mangled, base);
  strcat(mangled, function->name);

  return mangled;
}

#define Emit(...) Emit_(generator, __VA_ARGS__)
static void Emit_(code_generator& generator, const char* format, ...)
{
  va_list args;
  va_start(args, format);

  if (generator.tabCount == 0)
  {
    vfprintf(generator.output, format, args);
  }
  else
  {
    char* newFormat = static_cast<char*>(malloc(sizeof(char) * (strlen(format) +
              strlen("\t") * generator.tabCount)));
    strcpy(newFormat, format);

    for (unsigned int i = 0u;
         i < generator.tabCount;
         i++)
    {
      strcat(newFormat, "\t");
    }

    vfprintf(generator.output, newFormat, args);
    free(newFormat);
  }

  va_end(args);
}

void GenFunction(code_generator& generator, function_def* function)
{
  printf("Generating code for function: %s\n", function->name);

  char* mangledName = MangleFunctionName(function);
  Emit("%s:\n", mangledName);
  free(mangledName);

  generator.tabCount++;
  Emit("push rbp\n");
  Emit("mov rbp, rsp\n\n");

  Emit("leave\n");
  Emit("ret\n");
  generator.tabCount--;
}
