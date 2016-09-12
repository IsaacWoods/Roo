/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <codegen.hpp>
#include <cstring>
#include <cstdio>
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
  const char* tabString = "  ";

  va_list args;
  va_start(args, format);

  char c;

  while ((c = *format++))
  {
    if (c == '%')
    {
      // TODO
    }

    fputc(c, generator.output);
  }

  va_end(args);
}

void GenNode(code_generator& generator, node* n)
{
  switch (n->type)
  {
    case BREAK_NODE:
    {

    } break;

    case RETURN_NODE:
    {
      if (n->payload.expression)
      {
        Emit("mov rax, %n", n->payload.expression);
      }

      Emit("leave\n");
      Emit("ret\n");
    } break;

    case BINARY_OP_NODE:
    {

    } break;

    case VARIABLE_NODE:
    {
      
    } break;

    case CONDITION_NODE:
    {

    } break;

    case IF_NODE:
    {

    } break;

    case NUMBER_CONSTANT_NODE:
    {

    } break;

    default:
    {
      fprintf(stderr, "Unhandled node type in GenNode!\n");
      exit(1);
    }
  }
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
