/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <ir.hpp>
#include <cassert>
#include <climits>
#include <cstring>
#include <cstdarg>
#include <common.hpp>
#include <ast.hpp>
#include <air.hpp>

attribute* GetAttrib(linked_list<attribute>& attribs, attrib_type type)
{
  for (auto* it = attribs.first;
       it;
       it = it->next)
  {
    attribute& attrib = **it;

    if (attrib.type == type)
    {
      return &attrib;
    }
  }

  return nullptr;
}

template<>
void Free<live_range>(live_range& /*range*/)
{
}

slot_def* CreateSlot(function_def* function, slot_type type, ...)
{
  va_list args;
  va_start(args, type);

  slot_def* slot = static_cast<slot_def*>(malloc(sizeof(slot_def)));
  slot->type = type;
  slot->color = -1;
  slot->numInterferences = 0u;
  memset(slot->interferences, 0, sizeof(slot_def*) * MAX_SLOT_INTERFERENCES);
  CreateLinkedList<live_range>(slot->liveRanges);

  switch (type)
  {
    case slot_type::VARIABLE:
    {
      slot->variable = va_arg(args, variable_def*);
    } break;

    case slot_type::TEMPORARY:
    {
      slot->tag = function->numTemporaries++;
    } break;

    case slot_type::INT_CONSTANT:
    {
      slot->i = va_arg(args, int);
    } break;

    case slot_type::FLOAT_CONSTANT:
    {
      slot->f = static_cast<float>(va_arg(args, double));
    } break;

    case slot_type::STRING_CONSTANT:
    {
      slot->string = va_arg(args, string_constant*);
    } break;
  }

  Add<slot_def*>(function->slots, slot);
  va_end(args);
  return slot;
}

char* SlotAsString(slot_def* slot)
{
  #define SLOT_STR(slotType, format, arg) \
    case slotType: \
    { \
      char* result = static_cast<char*>(malloc(snprintf(nullptr, 0u, format, arg) + 1u)); \
      sprintf(result, format, arg); \
      return result; \
    }

  switch (slot->type)
  {
    SLOT_STR(slot_type::VARIABLE,         "%s(V)",  slot->variable->name)
    SLOT_STR(slot_type::TEMPORARY,        "t%u",    slot->tag)
    SLOT_STR(slot_type::INT_CONSTANT,     "#%d",    slot->i)
    SLOT_STR(slot_type::FLOAT_CONSTANT,   "#%f",    slot->f)
    SLOT_STR(slot_type::STRING_CONSTANT,  "\"%s\"", slot->string->string)
  }

  return nullptr;
}

template<>
void Free<slot_def*>(slot_def*& slot)
{
  FreeLinkedList<live_range>(slot->liveRanges);
  free(slot);
}

template<>
void Free<attribute>(attribute& attrib)
{
  free(attrib.payload);
}

void CreateParseResult(parse_result& result)
{
  result.name = nullptr;
  CreateLinkedList<dependency_def*>(result.dependencies);
  CreateLinkedList<function_def*>(result.functions);
  CreateLinkedList<operator_def*>(result.operators);
  CreateLinkedList<type_def*>(result.types);
  CreateLinkedList<string_constant*>(result.strings);
}

template<>
void Free<parse_result>(parse_result& result)
{
  free(result.name);
  FreeLinkedList<dependency_def*>(result.dependencies);
  FreeLinkedList<function_def*>(result.functions);
  FreeLinkedList<type_def*>(result.types);
  FreeLinkedList<string_constant*>(result.strings);
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

  Add<string_constant*>(result->strings, constant);
  return constant;
}

variable_def* CreateVariableDef(char* name, type_ref& typeRef, node* initValue)
{
  variable_def* var     = static_cast<variable_def*>(malloc(sizeof(variable_def)));
  var->name             = name;
  var->type             = typeRef;
  var->initValue        = initValue;
  var->slot             = nullptr;

  return var;
}

function_def* CreateFunctionDef(char* name)
{
  function_def* function = static_cast<function_def*>(malloc(sizeof(function_def)));
  function->name = name;
  CreateLinkedList<variable_def*>(function->scope.params);
  CreateLinkedList<variable_def*>(function->scope.locals);
  function->scope.shouldAutoReturn = false;
  function->returnType = nullptr;
  CreateLinkedList<attribute>(function->attribs);

  CreateLinkedList<slot_def*>(function->slots);
  function->airHead = nullptr;
  function->airTail = nullptr;
  function->numTemporaries = 0u;
  function->symbol = nullptr;

  return function;
}

operator_def* CreateOperatorDef(token_type op)
{
  operator_def* operatorDef = static_cast<operator_def*>(malloc(sizeof(operator_def)));
  operatorDef->op = op;
  CreateLinkedList<variable_def*>(operatorDef->scope.params);
  CreateLinkedList<variable_def*>(operatorDef->scope.locals);
  operatorDef->scope.shouldAutoReturn = false;
  CreateLinkedList<attribute>(operatorDef->attribs);

  CreateLinkedList<slot_def*>(operatorDef->slots);
  operatorDef->airHead = nullptr;
  operatorDef->airTail = nullptr;
  operatorDef->numTemporaries = 0u;
  operatorDef->symbol = nullptr;

  return operatorDef;
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
void Free<type_def*>(type_def*& type)
{
  free(type->name);
  FreeLinkedList<variable_def*>(type->members);
  FreeLinkedList<attribute>(type->attribs);
  free(type);
}

template<>
void Free<type_ref>(type_ref& ref)
{
  if (ref.isResolved)
  {
    // TODO: don't free the type_def
  }
  else
  {
    free(ref.name);
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
void Free<block_def>(block_def& scope)
{
  FreeLinkedList<variable_def*>(scope.params);
  FreeLinkedList<variable_def*>(scope.locals);
}

template<>
void Free<function_def*>(function_def*& function)
{
  free(function->name);
  Free<block_def>(function->scope);

  if (function->returnType)
  {
    Free<type_ref>(*(function->returnType));
    free(function->returnType);
  }

  FreeLinkedList<attribute>(function->attribs);

  if (function->ast)
  {
    Free<node*>(function->ast);
  }

  FreeLinkedList<slot_def*>(function->slots);

  while (function->airHead)
  {
    air_instruction* temp = function->airHead;
    function->airHead = function->airHead->next;
    Free<air_instruction*>(temp);
  }

  free(function);
}

template<>
void Free<operator_def*>(operator_def*& operatorDef)
{
  Free<block_def>(operatorDef->scope);

  if (operatorDef->ast)
  {
    Free<node*>(operatorDef->ast);
  }

  FreeLinkedList<attribute>(operatorDef->attribs);
  FreeLinkedList<slot_def*>(operatorDef->slots);
  Free<air_instruction*>(operatorDef->airHead);
  Free<air_instruction*>(operatorDef->airTail);

  free(operatorDef);
}

static void ResolveTypeRef(type_ref& ref, parse_result& parse)
{
  assert(!(ref.isResolved));

  for (auto* typeIt = parse.types.first;
       typeIt;
       typeIt = typeIt->next)
  {
    if ((**typeIt)->name == ref.name)
    {
      // TODO(Isaac): would be a good place to increment a usage counter on `typeIt`, if we ever needed one
      ref.isResolved = true;
      ref.def = (**typeIt);
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
    type->size += CalculateSizeOfType((**memberIt)->type.def);
  }

  return type->size;
}

void CompleteIR(parse_result& parse)
{
  // --- Resolve `variable_def`s everywhere ---
  for (auto* functionIt = parse.functions.first;
       functionIt;
       functionIt = functionIt->next)
  {
    function_def* function = **functionIt;

    if (GetAttrib(function->attribs, attrib_type::PROTOTYPE))
    {
      continue;
    }

    if (function->returnType)
    {
      ResolveTypeRef(*(function->returnType), parse);
    }

    for (auto* paramIt = function->scope.params.first;
         paramIt;
         paramIt = paramIt->next)
    {
      variable_def* param = **paramIt;
      ResolveTypeRef(param->type, parse);
    }

    for (auto* localIt = function->scope.locals.first;
         localIt;
         localIt = localIt->next)
    {
      variable_def* local = **localIt;
      ResolveTypeRef(local->type, parse);
    }
  }

  // --- Calculate the sizes of composite types ---
  for (auto* typeIt = parse.types.first;
       typeIt;
       typeIt = typeIt->next)
  {
    CalculateSizeOfType(**typeIt);
  }
}
