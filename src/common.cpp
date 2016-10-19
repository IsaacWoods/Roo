/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <common.hpp>
#include <algorithm>
#include <ast.hpp>
#include <air.hpp>

char* itoa(int num, char* str, int base)
{
  int i = 0;
  bool isNegative = false;

  // Explicitly handle 0
  if (num == 0)
  {
    str[i++] = '0';
    str[i] = '\0';
    return str;
  }

  // Handle negative numbers if base10
  if (num < 0 && base == 10)
  {
    isNegative = true;
    num = -num;
  }

  // Loop over each digit and output its char representation
  while (num != 0)
  {
    int rem = num % base;
    str[i++] = (rem > 9) ? ((rem - 10) + 'a') : (rem + '0');
    num /= base;
  }

  if (isNegative)
  {
    str[i++] = '-';
  }

  str[i] = '\0';

  // Reverse the string
  int start = 0;
  int end = i - 1;

  while (start < end)
  {
    std::swap(*(str + start), *(str + end));
    ++start;
    --end;
  }

  return str;
}

char* ReadFile(const char* path)
{
  FILE* file = fopen(path, "rb");

  if (!file)
  {
    fprintf(stderr, "Failed to read source file: %s\n", path);
    exit(1);
  }

  fseek(file, 0, SEEK_END);
  unsigned long length = ftell(file);
  fseek(file, 0, SEEK_SET);
  char* contents = static_cast<char*>(malloc(length + 1));

  if (!contents)
  {
    fprintf(stderr, "Failed to allocate space for source file!\n");
    exit(1);
  }

  if (fread(contents, 1, length, file) != length)
  {
    fprintf(stderr, "Failed to read source file: %s\n", path);
    exit(1);
  }

  contents[length] = '\0';
  fclose(file);

  return contents;
}


void CreateParseResult(parse_result& result)
{
  CreateLinkedList<dependency_def*>(result.dependencies);
  CreateLinkedList<function_def*>(result.functions);
  CreateLinkedList<type_def*>(result.types);
  CreateLinkedList<string_constant*>(result.strings);
}

void FreeParseResult(parse_result& result)
{
  FreeLinkedList<dependency_def*>(result.dependencies);
  FreeLinkedList<function_def*>(result.functions);
  FreeLinkedList<type_def*>(result.types);
  FreeLinkedList<string_constant*>(result.strings);
}

void FreeDependencyDef(dependency_def* dependency)
{
  free(dependency->path);
  free(dependency);
}

void PrintFunctionAttribs(uint32_t attribMask)
{
  if (attribMask == 0u)
  {
    printf("--- Emptry attribute set ---\n");
    return;
  }

  printf("--- Function Attrib Set ---\n");

  if (attribMask & function_attribs::ENTRY)
  {
    printf("| ENTRY |");
  }

  printf("---------------------------\n");
}

string_constant* CreateStringConstant(parse_result* result, char* string)
{
  string_constant* constant = static_cast<string_constant*>(malloc(sizeof(string_constant)));

  // NOTE(Isaac): get the current tail's handle before adding to the list
  constant->handle = (**result->strings.tail)->handle + 1u;
  constant->string = string;

  AddToLinkedList<string_constant*>(result->strings, constant);
  return constant;
}

void FreeStringConstant(string_constant* string)
{
  free(string->string);
  free(string);
}

type_ref* CreateTypeRef(char* typeName)
{
  type_ref* r = static_cast<type_ref*>(malloc(sizeof(type_ref)));
  r->typeName = typeName;
  return r;
}

void FreeTypeRef(type_ref* typeRef)
{
  free(typeRef->typeName);
  free(typeRef);
}

void FreeVariableDef(variable_def* variable)
{
  free(variable->name);
  FreeTypeRef(variable->type);
  FreeNode(variable->initValue);
}

void FreeFunctionDef(function_def* function)
{
  free(function->name);
  FreeTypeRef(function->returnType);
  FreeLinkedList<variable_def*>(function->params);
  FreeLinkedList<variable_def*>(function->locals);

  if (function->ast)
  {
    FreeNode(function->ast);
  }

  FreeAIRFunction(function->air);
}

void FreeTypeDef(type_def* type)
{
  free(type->name);
  FreeLinkedList<variable_def*>(type->members);
  free(type);
}

const char* GetTokenName(token_type type)
{
  switch (type)
  {
    case TOKEN_TYPE:
      return "TOKEN_TYPE";
    case TOKEN_FN:
      return "TOKEN_FN";
    case TOKEN_TRUE:
      return "TOKEN_TRUE";
    case TOKEN_FALSE:
      return "TOKEN_FALSE";
    case TOKEN_IMPORT:
      return "TOKEN_IMPORT";
    case TOKEN_BREAK:
      return "TOKEN_BREAK";
    case TOKEN_RETURN:
      return "TOKEN_RETURN";
    case TOKEN_IF:
      return "TOKEN_IF";
    case TOKEN_ELSE:
      return "TOKEN_ELSE";

    case TOKEN_DOT:
      return "TOKEN_DOT";
    case TOKEN_COMMA:
      return "TOKEN_COMMA";
    case TOKEN_COLON:
      return "TOKEN_COLON";
    case TOKEN_LEFT_PAREN:
      return "TOKEN_LEFT_PAREN";
    case TOKEN_RIGHT_PAREN:
      return "TOKEN_RIGHT_PAREN";
    case TOKEN_LEFT_BRACE:
      return "TOKEN_LEFT_BRACE";
    case TOKEN_RIGHT_BRACE:
      return "TOKEN_RIGHT_BRACE";
    case TOKEN_LEFT_BLOCK:
      return "TOKEN_LEFT_BLOCK";
    case TOKEN_RIGHT_BLOCK:
      return "TOKEN_RIGHT_BLOCK";
    case TOKEN_ASTERIX:
      return "TOKEN_ASTERIX";
    case TOKEN_PLUS:
      return "TOKEN_PLUS";
    case TOKEN_MINUS:
      return "TOKEN_MINUS";
    case TOKEN_SLASH:
      return "TOKEN_SLASH";
    case TOKEN_EQUALS:
      return "TOKEN_EQUALS";
    case TOKEN_BANG:
      return "TOKEN_BANG";
    case TOKEN_TILDE:
      return "TOKEN_TILDE";
    case TOKEN_PERCENT:
      return "TOKEN_PERCENT";
    case TOKEN_QUESTION_MARK:
      return "TOKEN_QUESTION_MARK";
    case TOKEN_POUND:
      return "TOKEN_POUND";

    case TOKEN_YIELDS:
      return "TOKEN_YIELDS";
    case TOKEN_START_ATTRIBUTE:
      return "TOKEN_START_ATTRIBUTE";
    case TOKEN_EQUALS_EQUALS:
      return "TOKEN_EQUALS_EQUALS";
    case TOKEN_BANG_EQUALS:
      return "TOKEN_BANG_EQUALS";
    case TOKEN_GREATER_THAN:
      return "TOKEN_GREATER_THAN";
    case TOKEN_GREATER_THAN_EQUAL_TO:
      return "TOKEN_GREATER_THAN_EQUAL_TO";
    case TOKEN_LESS_THAN:
      return "TOKEN_LESS_THAN";
    case TOKEN_LESS_THAN_EQUAL_TO:
      return "TOKEN_LESS_THAN_EQUAL_TO";
    case TOKEN_DOUBLE_PLUS:
      return "TOKEN_DOUBLE_PLUS";
    case TOKEN_DOUBLE_MINUS:
      return "TOKEN_DOUBLE_MINUS";
    case TOKEN_LEFT_SHIFT:
      return "TOKEN_LEFT_SHIFT";
    case TOKEN_RIGHT_SHIFT:
      return "TOKEN_RIGHT_SHIFT";
    case TOKEN_LOGICAL_AND:
      return "TOKEN_LOGICAL_AND";
    case TOKEN_LOGICAL_OR:
      return "TOKEN_LOGICAL_OR";
    case TOKEN_BITWISE_AND:
      return "TOKEN_BITWISE_AND";
    case TOKEN_BITWISE_OR:
      return "TOKEN_BITWISE_OR";
    case TOKEN_BITWISE_XOR:
      return "TOKEN_BITWISE_XOR";

    case TOKEN_IDENTIFIER:
      return "TOKEN_IDENTIFIER";
    case TOKEN_STRING:
      return "TOKEN_STRING";
    case TOKEN_NUMBER_INT:
      return "TOKEN_NUMBER_INT";
    case TOKEN_NUMBER_FLOAT:
      return "TOKEN_NUMBER_FLOAT";
    case TOKEN_CHAR_CONSTANT:
      return "TOKEN_CHAR_CONSTANT";
    case TOKEN_LINE:
      return "TOKEN_LINE";
    case TOKEN_INVALID:
      return "TOKEN_INVALID";

    default:
      fprintf(stderr, "Unhandled token type in GetTokenName!\n");
      exit(1);
  }
}
