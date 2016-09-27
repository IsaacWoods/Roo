/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <codegen.hpp>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <common.hpp>

static void InitRegisterStateSet(register_state_set& set, const char* tag = nullptr)
{
  set.tag = tag;

  for (unsigned int i = 0u;
       i < NUM_REGISTERS;
       i++)
  {
    register_state& r = set[static_cast<reg>(i)];
    r.usage = register_state::register_usage::FREE;
    r.variable = nullptr;
  }

  set[RBP].usage           = register_state::register_usage::UNUSABLE;
  set[RSP].usage           = register_state::register_usage::UNUSABLE;
}

static void PrintRegisterStateSet(register_state_set& set)
{
  printf("/ %20s \\\n", (set.tag ? set.tag : "UNTAGGED"));
  printf("|----------------------|\n");
  
  for (unsigned int i = 0u;
       i < NUM_REGISTERS;
       i++)
  {
    register_state& r = set[static_cast<reg>(i)];
    printf("| %3s     - %10s |\n", GetRegisterName(static_cast<reg>(i)),
        ((r.usage == register_state::register_usage::FREE) ? "FREE" :
        ((r.usage == register_state::register_usage::IN_USE) ? "IN USE" : "UNUSABLE")));
  }

  printf("\\----------------------/\n");
}

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
    default:
      return "[INVALID REGISTER]";
  }

  fprintf(stderr, "Unhandled register in GetRegisterName!\n");
  exit(1);
}

#define Emit(...) Emit_(generator, __VA_ARGS__)
static void Emit_(code_generator& generator, const char* format, ...)
{
  static const char* TAB_STRING = "  ";
  static const char* MISSING_NODE_RESULT = "[MISSING NODE RESULT]";

  va_list args;
  va_start(args, format);

  /*
   * NOTE(Isaac): this was previously written with `realloc` to dynamically
   * grow the string, but glibc's implementation of `realloc` was returning
   * bogus memory, so let's just hope this doesn't overflow instead...
   */
  char string[1024u] = { 0 };

  // Add the correct amount of indent
  for (unsigned int i = 0u;
       i < generator.tabCount;
       i++)
  {
    strcat(string, TAB_STRING);
  }

  while (*format)
  {
    if (*format == '%')
    {
      switch (*(++format))
      {
        case 'c':
        {
          string[strlen(string)] = static_cast<char>(va_arg(args, int));
        } break;

        case 's':
        {
          ++format;
          const char* str = va_arg(args, const char*);

          strcat(string, str);
        } break;

        case 'u':
        {
          format++;
          char buffer[32];
          itoa(va_arg(args, int), buffer, 10);

          strcat(string, buffer);
        } break;

        case 'x':
        {
          format++;
          char buffer[32];
          itoa(va_arg(args, int), buffer, 2);

          strcat(string, buffer);
        } break;

        case 'n':
        {
          format++;
          char* nodeResult = GenNode(generator, va_arg(args, node*));

          if (!nodeResult)
          {
            fprintf(stderr, "WARNING: Tried to print node result, but it generated nothing!\n");
            strcat(string, MISSING_NODE_RESULT);
            break;
          }

          strcat(string, nodeResult);
          free(nodeResult);
        } break;

        case 'r':
        {
          format++;
          reg r = static_cast<reg>(va_arg(args, int));
          const char* regName = GetRegisterName(r);

          strcat(string, regName);
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
      string[strlen(string)] = *format++;
    }
  }

  fputs(string, generator.output);
  va_end(args);
}

static reg Registerize(code_generator& generator, node* n)
{
  // TODO: find the correct register
  reg r = RCX;
  Emit("mov %r, %n\n", r, n);
  return r;
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
      reg leftReg = Registerize(generator, n->payload.binaryOp.left);

      switch (n->payload.binaryOp.op)
      {
        case TOKEN_PLUS:
        {
          Emit("add %r, %n\n", leftReg, n->payload.binaryOp.right);
        } break;

        case TOKEN_MINUS:
        {
          Emit("sub %r, %n\n", leftReg, n->payload.binaryOp.right);
        } break;

        case TOKEN_ASTERIX:
        {
          Emit("mul %r, %n\n", leftReg, n->payload.binaryOp.right);
        } break;

        case TOKEN_SLASH:
        {
          Emit("div %r, %n\n", leftReg, n->payload.binaryOp.right);
        } break;

        default:
        {
          fprintf(stderr, "Unhandled binary operation in GenNode!\n");
          exit(1);
        }
      }

      // NOTE(Isaac): yes, this is a dirty dirty hack, but we need to be able to free the memory later...
      // TODO(Isaac): dear God please find a better way to manage this
      const char* leftRegName = GetRegisterName(leftReg);
      char* leftRegNameCopy = static_cast<char*>(malloc(sizeof(char) * strlen(leftRegName)));
      strcpy(leftRegNameCopy, leftRegName);
      return leftRegNameCopy;
    } break;

    case PREFIX_OP_NODE:
    {
      reg rightReg = Registerize(generator, n->payload.prefixOp.right);

      switch (n->payload.prefixOp.op)
      {
        case TOKEN_PLUS:
        {
          // TODO
          Emit("[FIND ABSOLUTE OF %r]\n", rightReg);
        } break;

        case TOKEN_MINUS:
        {
          Emit("neg %r\n", rightReg);
        } break;

        case TOKEN_BANG:
        {
          // TODO: what do we actually want to do here?
        } break;

        case TOKEN_TILDE:
        {
          Emit("not %r\n", rightReg);
        } break;

        default:
        {
          fprintf(stderr, "Unhandled prefix operation in GenNode!\n");
          exit(1);
        }
      }

      // TODO(Isaac): eww
      const char* rightRegName = GetRegisterName(rightReg);
      char* rightRegNameCopy = static_cast<char*>(malloc(sizeof(char) * strlen(rightRegName)));
      strcpy(rightRegNameCopy, rightRegName);
      return rightRegNameCopy;
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
      unsigned int i = 0u;
      const unsigned int NUM_INT_REGS = 5u;
      reg paramRegs[] = { RDI, RSI, RDX, RCX, R8 };

      for (function_call_part::param_def* param = n->payload.functionCall.firstParam;
           param;
           param = param->next)
      {
        if (i == NUM_INT_REGS)
        {
          // TODO: push them onto the stack instead
          fprintf(stderr, "Failed to fit parameters in registers!\n");
          break;
        }

        printf("Putting parameter in register: %s\n", GetRegisterName(paramRegs[i]));
        Emit("mov %r, %n\n", paramRegs[i++], param->expression);
      }

      Emit("call %s\n", n->payload.functionCall.name);

      // TODO(Isaac): sort this disgrace out
      char* eww = static_cast<char*>(malloc(sizeof(char) * strlen("rax")));
      strcpy(eww, "rax");
      return eww;
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

  register_state_set initialState;
  InitRegisterStateSet(initialState, "Initial");

  // Mark registers holding parameters as in-use
  unsigned int i = 0u;
  const unsigned int NUM_INT_REGS = 5u;
  reg paramRegs[] = { RDI, RSI, RDX, RCX, R8 };

  for (variable_def* param = function->firstParam;
       param;
       param = param->next)
  {
    if (i == NUM_INT_REGS)
    {
      break;
    }

    // TODO(Isaac): continue if it can't be put in a register for some reason

    initialState[paramRegs[i]].usage = register_state::register_usage::IN_USE;
    initialState[paramRegs[i]].variable = param;
    i++;
  }

  PrintRegisterStateSet(initialState);

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
  Emit("\n");
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
  Emit("section .data\n");
  generator.tabCount++;

  for (string_constant* string = parse.firstString;
       string;
       string = string->next)
  {
    Emit("str%u: db \"%s\", 0\n", string->handle, string->string);
  }

  generator.tabCount--;
}
