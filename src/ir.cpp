/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <ir.hpp>
#include <cassert>
#include <climits>
#include <cstring>
#include <common.hpp>
#include <ast.hpp>
#include <air.hpp>

program_attrib* GetAttrib(parse_result& result, program_attrib::attrib_type type)
{
  for (auto* attribIt = result.attribs.first;
       attribIt;
       attribIt = attribIt->next)
  {
    if ((**attribIt).type == type)
    {
      return &(**attribIt);
    }
  }

  return nullptr;
}

void CreateParseResult(parse_result& result)
{
  CreateLinkedList<dependency_def*>(result.dependencies);
  CreateLinkedList<function_def*>(result.functions);
  CreateLinkedList<type_def*>(result.types);
  CreateLinkedList<string_constant*>(result.strings);
  CreateLinkedList<program_attrib>(result.attribs);
}

string_constant* CreateStringConstant(parse_result* result, char* string)
{
  string_constant* constant = static_cast<string_constant*>(malloc(sizeof(string_constant)));
  constant->string = string;

  if (result->strings.tail)
  {
    // NOTE(Isaac): get the current tail's handle before adding to the list
    constant->handle = (**result->strings.tail)->handle + 1u;
  }
  else
  {
    constant->handle = 0u;
  }

  AddToLinkedList<string_constant*>(result->strings, constant);
  return constant;
}

variable_def* CreateVariableDef(char* name, char* typeName, bool isMutable, node* initValue)
{
  variable_def* var = static_cast<variable_def*>(malloc(sizeof(variable_def)));
  var->name = name;
  var->type.type.name = typeName;
  var->type.isResolved = false;
  var->isMutable = isMutable;
  var->initValue = initValue;
  var->mostRecentSlot = nullptr;

  return var;
}

function_attrib* GetAttrib(function_def* function, function_attrib::attrib_type type)
{
  for (auto* attribIt = function->attribs.first;
       attribIt;
       attribIt = attribIt->next)
  {
    if ((**attribIt).type == type)
    {
      return &(**attribIt);
    }
  }

  return nullptr;
}

char* MangleFunctionName(function_def* function)
{
  const char* base = "_R_";

  // NOTE(Isaac): add one to leave space for an added null-terminator
  char* mangled = static_cast<char*>(malloc(sizeof(char) * (strlen(base) + strlen(function->name) + 1u)));
  strcpy(mangled, base);
  strcat(mangled, function->name);

  return mangled;
}

type_attrib* GetAttrib(type_def* typeDef, type_attrib::attrib_type type)
{
  for (auto* attribIt = typeDef->attribs.first;
       attribIt;
       attribIt = attribIt->next)
  {
    if ((**attribIt).type == type)
    {
      return &(**attribIt);
    }
  }

  return nullptr; 
}

template<>
void Free<program_attrib>(program_attrib& attrib)
{
  switch (attrib.type)
  {
    case program_attrib::attrib_type::NAME:
    {
      free(attrib.payload.name);
    } break;
  }
}

template<>
void Free<parse_result>(parse_result& result)
{
  FreeLinkedList<dependency_def*>(result.dependencies);
  FreeLinkedList<function_def*>(result.functions);
  FreeLinkedList<type_def*>(result.types);
  FreeLinkedList<string_constant*>(result.strings);
  FreeLinkedList<program_attrib>(result.attribs);
}

template<>
void Free<dependency_def*>(dependency_def*& dependency)
{
  free(dependency->path);
  free(dependency);
}

template<>
void Free<string_constant*>(string_constant*& string)
{
  free(string->string);
  free(string);
}

template<>
void Free<type_attrib>(type_attrib& /*attrib*/)
{
}

template<>
void Free<type_def*>(type_def*& type)
{
  free(type->name);
  FreeLinkedList<variable_def*>(type->members);
  FreeLinkedList<type_attrib>(type->attribs);
  free(type);
}

template<>
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

template<>
void Free<variable_def*>(variable_def*& variable)
{
  free(variable->name);
  Free<type_ref>(variable->type);

  if (variable->initValue)
  {
    Free<node*>(variable->initValue);
  }

  free(variable);
}

template<>
void Free<function_attrib>(function_attrib& /*attrib*/)
{
}

template<>
void Free<function_def*>(function_def*& function)
{
  free(function->name);
  FreeLinkedList<variable_def*>(function->params);
  FreeLinkedList<variable_def*>(function->locals);

  if (function->returnType)
  {
    Free<type_ref>(*(function->returnType));
    free(function->returnType);
  }

  FreeLinkedList<function_attrib>(function->attribs);

  if (function->ast)
  {
    Free<node*>(function->ast);
  }

  if (function->air)
  {
    Free<air_function*>(function->air);
  }

  free(function);
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
    if (GetAttrib(**functionIt, function_attrib::attrib_type::PROTOTYPE))
    {
      continue;
    }

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
