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

// AST Passes
#include <pass_resolveVars.hpp>
#include <pass_resolveCalls.hpp>
#include <pass_typeChecker.hpp>

template<>
void Free<live_range>(live_range& /*range*/)
{
}

slot_def* CreateSlot(thing_of_code& code, slot_type type, ...)
{
  va_list args;
  va_start(args, type);

  slot_def* slot = static_cast<slot_def*>(malloc(sizeof(slot_def)));
  slot->type = type;
  slot->color = -1;
  slot->numInterferences = 0u;
  memset(slot->interferences, 0, sizeof(slot_def*) * MAX_SLOT_INTERFERENCES);
  InitVector<live_range>(slot->liveRanges);

  switch (type)
  {
    case slot_type::VARIABLE:
    {
      slot->variable = va_arg(args, variable_def*);
    } break;

    case slot_type::TEMPORARY:
    {
      slot->tag = code.numTemporaries++;
    } break;

    case slot_type::RETURN_RESULT:
    {
      slot->tag = code.numReturnResults++;
    } break;

    case slot_type::SIGNED_INT_CONSTANT:
    {
      slot->i = va_arg(args, int);
    } break;

    case slot_type::UNSIGNED_INT_CONSTANT:
    {
      slot->u = va_arg(args, unsigned int);
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

  Add<slot_def*>(code.slots, slot);
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
    SLOT_STR(slot_type::VARIABLE,               "%s(V)",  slot->variable->name)
    SLOT_STR(slot_type::TEMPORARY,              "t%u",    slot->tag)
    SLOT_STR(slot_type::RETURN_RESULT,          "r%u",    slot->tag)
    SLOT_STR(slot_type::SIGNED_INT_CONSTANT,    "#%d",    slot->i)
    SLOT_STR(slot_type::UNSIGNED_INT_CONSTANT,  "#%u",    slot->u)
    SLOT_STR(slot_type::FLOAT_CONSTANT,         "#%f",    slot->f)
    SLOT_STR(slot_type::STRING_CONSTANT,        "\"%s\"", slot->string->string)
  }

  return nullptr;
}

template<>
void Free<slot_def*>(slot_def*& slot)
{
  FreeVector<live_range>(slot->liveRanges);
  free(slot);
}

void CreateParseResult(parse_result& result)
{
  result.name = nullptr;
  InitVector<dependency_def*>(result.dependencies);
  InitVector<function_def*>(result.functions);
  InitVector<operator_def*>(result.operators);
  InitVector<type_def*>(result.types);
  InitVector<string_constant*>(result.strings);
}

template<>
void Free<parse_result>(parse_result& result)
{
  free(result.name);
  FreeVector<dependency_def*>(result.dependencies);
  FreeVector<function_def*>(result.functions);
  FreeVector<type_def*>(result.types);
  FreeVector<string_constant*>(result.strings);
}

string_constant* CreateStringConstant(parse_result* result, char* string)
{
  string_constant* constant = static_cast<string_constant*>(malloc(sizeof(string_constant)));
  constant->string = string;

  if (result->strings.size > 0u)
  {
    // NOTE(Isaac): get the current tail's handle before adding to the list
    constant->handle = result->strings[result->strings.size - 1u]->handle + 1u;
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
  variable_def* var = static_cast<variable_def*>(malloc(sizeof(variable_def)));
  var->name         = name;
  var->type         = typeRef;
  var->initValue    = initValue;
  var->slot         = nullptr;

  return var;
}

void InitAttribSet(attrib_set& set)
{
  set.isEntry     = false;
  set.isPrototype = false;
  set.isInline    = false;
  set.isNoInline  = false;
}

static void InitThingOfCode(thing_of_code& code)
{
  code.mangledName      = nullptr;
  InitVector<variable_def*>(code.params);
  InitVector<variable_def*>(code.locals);
  code.shouldAutoReturn = false;
  InitAttribSet(code.attribs);
  code.returnType       = nullptr;

  code.ast              = nullptr;
  InitVector<slot_def*>(code.slots);
  code.airHead          = nullptr;
  code.airTail          = nullptr;
  code.numTemporaries   = 0u;
  code.numReturnResults = 0u;
  code.symbol           = nullptr;
}

function_def* CreateFunctionDef(char* name)
{
  function_def* function = static_cast<function_def*>(malloc(sizeof(function_def)));
  function->name = name;
  InitThingOfCode(function->code);

  return function;
}

operator_def* CreateOperatorDef(token_type op)
{
  operator_def* operatorDef = static_cast<operator_def*>(malloc(sizeof(operator_def)));
  operatorDef->op = op;
  InitThingOfCode(operatorDef->code);

  return operatorDef;
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
  FreeVector<variable_def*>(type->members);
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
void Free<thing_of_code>(thing_of_code& code)
{
  free(code.mangledName);
  FreeVector<variable_def*>(code.params);
  FreeVector<variable_def*>(code.locals);

  if (code.returnType)
  {
    Free<type_ref>(*(code.returnType));
    free(code.returnType);
  }

  if (code.ast)
  {
    Free<node*>(code.ast);
  }

  FreeVector<slot_def*>(code.slots);

  while (code.airHead)
  {
    air_instruction* temp = code.airHead;
    code.airHead = code.airHead->next;
    Free<air_instruction*>(temp);
  }
}

template<>
void Free<function_def*>(function_def*& function)
{
  free(function->name);
  Free<thing_of_code>(function->code);
  free(function);
}

template<>
void Free<operator_def*>(operator_def*& operatorDef)
{
  Free<thing_of_code>(operatorDef->code);
  free(operatorDef);
}

type_def* GetTypeByName(parse_result& parse, const char* typeName)
{
  for (auto* it = parse.types.head;
       it < parse.types.tail;
       it++)
  {
    type_def* type = *it;

    if (strcmp(type->name, typeName) == 0)
    {
      return type;
    }
  }

  return nullptr;
}

static void ResolveTypeRef(type_ref& ref, parse_result& parse)
{
  assert(!(ref.isResolved));

  for (auto* it = parse.types.head;
       it < parse.types.tail;
       it++)
  {
    type_def* type = *it;

    if (type->name == ref.name)
    {
      // TODO(Isaac): would be a good place to increment a usage counter on `typeIt`, if we ever needed one
      ref.isResolved = true;
      ref.def = type;
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

  for (auto* it = type->members.head;
       it < type->members.tail;
       it++)
  {
    variable_def* member = *it;
    assert(member->type.isResolved);
    type->size += CalculateSizeOfType(member->type.def);
  }

  return type->size;
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

void CompleteIR(codegen_target& target, parse_result& parse)
{
  // Mangle function and operator names
  for (auto* it = parse.functions.head;
       it < parse.functions.tail;
       it++)
  {
    function_def* function = *it;
    function->code.mangledName = MangleFunctionName(function);
  }

  // TODO: also mangle operator names

  // --- Resolve `variable_def`s everywhere ---
  for (auto* it = parse.functions.head;
       it < parse.functions.tail;
       it++)
  {
    function_def* function = *it;

    if (function->code.attribs.isPrototype)
    {
      continue;
    }

    if (function->code.returnType)
    {
      ResolveTypeRef(*(function->code.returnType), parse);
    }

    for (auto* paramIt = function->code.params.head;
         paramIt < function->code.params.tail;
         paramIt++)
    {
      variable_def* param = *paramIt;
      ResolveTypeRef(param->type, parse);
    }

    for (auto* localIt = function->code.locals.head;
         localIt < function->code.locals.tail;
         localIt++)
    {
      variable_def* local = *localIt;
      ResolveTypeRef(local->type, parse);
    }
  }

  // --- Calculate the sizes of composite types ---
  for (auto* typeIt = parse.types.head;
       typeIt < parse.types.tail;
       typeIt++)
  {
    CalculateSizeOfType(*typeIt);
  }

  // --- Apply AST Passes ---
  ApplyASTPass(parse, PASS_resolveVars);
  ApplyASTPass(parse, PASS_resolveCalls);
  ApplyASTPass(parse, PASS_typeChecker);

  // --- Generate AIR for functions and operators ---
  // TODO: parralelise this with a job server
  for (auto* it = parse.functions.head;
       it < parse.functions.tail;
       it++)
  {
    function_def* function = *it;

    if (function->code.attribs.isPrototype)
    {
      continue;
    }

    GenerateAIR(target, function->code);
  }

  // TODO: generate AIR for operators
}
