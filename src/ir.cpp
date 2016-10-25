/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <ir.hpp>
#include <cassert>
#include <climits>
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

/*
void Free<type_ref>(type_ref& ref)
{
  if (ref.isResolved)
  {
    Free<type_def*>(ref.type.def);
  }
  else
  {
    free(ref.type.name);
  }
}
*/

void FreeVariableDef(variable_def* variable)
{
  free(variable->name);
  //Free<type_ref>(variable.type);
  FreeNode(variable->initValue);
}

void FreeFunctionDef(function_def* function)
{
  free(function->name);
//  Free<type_ref>(function->returnType);
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

static void ResolveTypeRef(type_ref& ref, parse_result& parse)
{
  assert(!(ref.isResolved));

  for (auto* typeIt = parse.types.first;
       typeIt;
       typeIt = typeIt->next)
  {
    if ((**typeIt)->name == ref.type.name)
    {
      // TODO(Isaac): would be a good place to increment a usage counter on `typeIt`, if we ever needed one
      ref.isResolved = true;
      ref.type.def = (**typeIt);
      return;
    }
  }
}

/*
 * This calculates the size of a type, and stores it within the `type_def`.
 * NOTE(Isaac): Should not be called on inbuilt types with `overwrite = true`.
 * TODO(Isaac): should types be padded out to fit nicely on alignment boundaries?
 */
static unsigned int CalculateSizeOfType(type_def* type, bool overwrite = false)
{
  if (!overwrite && type->size != UINT_MAX)
  {
    return type->size;
  }

  type->size = 0u;

  for (auto* memberIt = type->members.first;
       memberIt;
       memberIt = memberIt->next)
  {
    assert((**memberIt)->type.isResolved);
    type->size += CalculateSizeOfType((**memberIt)->type.type.def);
  }

  return type->size;
}

void CompleteAST(parse_result& parse)
{
  // --- Resolve `variable_def`s everywhere ---
  for (auto* functionIt = parse.functions.first;
       functionIt;
       functionIt = functionIt->next)
  {
    ResolveTypeRef(*((**functionIt)->returnType), parse);
  }

  // --- Calculate the sizes of composite types ---
  for (auto* typeIt = parse.types.first;
       typeIt;
       typeIt = typeIt->next)
  {
    CalculateSizeOfType(**typeIt);
  }
}
