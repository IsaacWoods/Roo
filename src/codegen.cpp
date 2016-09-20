/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <codegen.hpp>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <common.hpp>

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

char* GenNode(code_generator&, node*);

const char* GetRegisterName(reg r)
{
  switch (r)
  {
    case RAX:
      return "rax";
    case RBX:
      return "rbx";
    case RCX:
      return "rcx";
    case RDX:
      return "rdx";
    case RSI:
      return "rsi";
    case RDI:
      return "rdi";
    case RBP:
      return "rbp";
    case RSP:
      return "rsp";
    case R8:
      return "r8";
    case R9:
      return "r9";
    case R10:
      return "r10";
    case R11:
      return "r11";
    case R12:
      return "r12";
    case R13:
      return "r13";
    case R14:
      return "r14";
    case R15:
      return "r15";
  }

  fprintf(stderr, "Unhandled register in GetRegisterName!\n");
  exit(1);
}

#define Emit(...) Emit_(generator, __VA_ARGS__)
static void Emit_(code_generator& generator, const char* format, ...)
{
  const char* tabString = "  ";

  va_list args;
  va_start(args, format);

  for (unsigned int i = 0u;
       i < generator.tabCount;
       i++)
  {
    fputs(tabString, generator.output);
  }

  while (*format)
  {
    if (*format == '%')
    {
      switch (*(++format))
      {
        case 'c':
        {
          fputc(static_cast<char>(va_arg(args, int)), generator.output);
        } break;

        case 's':
        {
          ++format;
          const char* str = va_arg(args, const char*);
          fputs(str, generator.output);
        } break;

        case 'u':
        {
          format++;
          char buffer[32];
          itoa(va_arg(args, int), buffer, 10);
          fputs(buffer, generator.output);
        } break;

        case 'x':
        {
          format++;
          char buffer[32];
          itoa(va_arg(args, int), buffer, 2);
          fputs(buffer, generator.output);
        } break;

        case 'n':
        {
          format++;
          char* nodeResult = GenNode(generator, va_arg(args, node*));

          if (!nodeResult)
          {
            fprintf(stderr, "WARNING: Tried to print node result, but it generated nothing!\n");
            fputs("[MISSING NODE RESULT]", generator.output);
            break;
          }

          fputs(nodeResult, generator.output);
          free(nodeResult);
        } break;

        case 'r':
        {
          format++;
          reg r = static_cast<reg>(va_arg(args, int));
          fputs(GetRegisterName(r), generator.output);
        } break;

        default:
        {
          fprintf(stderr, "Unsupported format specifier in Emit: '%c'!\n", *format);
          exit(1);
        }
      }
    }
    else
    {
      fputc(*format++, generator.output);
    }
  }

  va_end(args);
}

char* GenNode(code_generator& generator, node* n)
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
        Emit("mov rax, %n\n", n->payload.expression);
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
      switch (n->payload.numberConstant.type)
      {
        case number_constant_part::constant_type::CONSTANT_TYPE_INT:
        {
          size_t strLength = snprintf(nullptr, 0, "%d", n->payload.numberConstant.constant.asInt);
          // NOTE(Isaac): add one for the null terminator
          char* result = static_cast<char*>(malloc(sizeof(char) * (strLength + 1u)));
          sprintf(result, "%d", n->payload.numberConstant.constant.asInt);

          return result;
        } break;

        case number_constant_part::constant_type::CONSTANT_TYPE_FLOAT:
        {
          size_t strLength = snprintf(nullptr, 0, "%f", n->payload.numberConstant.constant.asFloat);
          // NOTE(Isaac): add one for the null terminator
          char* result = static_cast<char*>(malloc(sizeof(char) * (strLength + 1u)));
          sprintf(result, "%f", n->payload.numberConstant.constant.asFloat);

          return result;
        } break;

        default:
        {
          fprintf(stderr, "Unhandled constant type in NUMBER_CONSTANT_NODE of GenNode!\n");
          exit(1);
        } break;
      }
    } break;

    case STRING_CONSTANT_NODE:
    {
      char* result = static_cast<char*>(malloc(snprintf(nullptr, 0, "str%u", n->payload.stringConstant->handle)));
      sprintf(result, "str%u", n->payload.stringConstant->handle);
      return result;
    } break;

    case FUNCTION_CALL_NODE:
    {
      // TODO(Isaac): move parameters into correct registers and stack S&T
      Emit("call %s\n", n->payload.functionCall.name);
    } break;

    default:
    {
      fprintf(stderr, "Unhandled node type in GenNode!\n");
      exit(1);
    }
  }

  // If there is one, visit the next node in the block
  if (n->next)
  {
    GenNode(generator, n->next);
  }

  return nullptr;
}

static void GenFunction(code_generator& generator, function_def* function)
{
  printf("Generating code for function: %s\n", function->name);

  char* mangledName = MangleFunctionName(function);
  Emit("%s:\n", mangledName);
  free(mangledName);

  // Create a new stack frame
  generator.tabCount++;
  Emit("push rbp\n");
  Emit("mov rbp, rsp\n\n");

  // Recurse through the AST
  if (function->code)
  {
    GenNode(generator, function->code);
  }

  // Leave the stack frame and return
  if (function->shouldAutoReturn)
  {
    Emit("leave\n");
    Emit("ret\n");
  }

  generator.tabCount--;
}

void GenCodeSection(code_generator& generator, parse_result& parse)
{
  generator.tabCount = 0u;
  Emit("section .text\n\n");

  for (function_def* function = parse.firstFunction;
       function;
       function = function->next)
  {
    GenFunction(generator, function);
  }
}

void GenDataSection(code_generator& generator, parse_result& parse)
{
  generator.tabCount = 0u;
  Emit("\nsection .data\n");
  generator.tabCount++;

  for (string_constant* string = parse.firstString;
       string;
       string = string->next)
  {
    Emit("str%u: db \"%s\", 0\n", string->handle, string->string);
  }

  generator.tabCount--;
}
