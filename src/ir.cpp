/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <ir.hpp>
#include <ast.hpp>
#include <air.hpp>

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
