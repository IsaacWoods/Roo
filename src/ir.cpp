/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <ir.hpp>
#include <cassert>
#include <climits>
#include <cstring>
#include <cstdarg>
#include <common.hpp>
#include <ast.hpp>
#include <air.hpp>

template<>
void Free<live_range>(live_range& /*range*/)
{
}

slot_def* CreateSlot(codegen_target& target, thing_of_code* code, slot_type type, ...)
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
      type_ref& varType = slot->variable->type;
      slot->storage = (GetSizeOfTypeRef(varType) > target.generalRegisterSize ? slot_storage::STACK : slot_storage::REGISTER);
    } break;

    case slot_type::PARAMETER:
    {
      slot->variable = va_arg(args, variable_def*);
      Add<live_range>(slot->liveRanges, live_range{nullptr, nullptr});
      type_ref& varType = slot->variable->type;
      slot->storage = (GetSizeOfTypeRef(varType) > target.generalRegisterSize ? slot_storage::STACK : slot_storage::REGISTER);
    } break;

    case slot_type::MEMBER:
    {
      slot->member.parent     = va_arg(args, slot_def*);
      slot->member.memberVar  = va_arg(args, variable_def*);

      if (slot->member.parent->type == slot_type::PARAMETER)
      {
        Add<live_range>(slot->liveRanges, live_range{nullptr, nullptr});
      }

      type_def* varType = slot->member.memberVar->type.def;
      slot->storage = (varType->size > target.generalRegisterSize ? slot_storage::STACK : slot_storage::REGISTER);
    } break;

    case slot_type::TEMPORARY:
    {
      slot->tag = code->numTemporaries++;
    } break;

    case slot_type::RETURN_RESULT:
    {
      slot->tag = code->numReturnResults++;
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

  Add<slot_def*>(code->slots, slot);
  va_end(args);
  return slot;
}

char* GetSlotString(slot_def* slot)
{
  #define SLOT_STR(slotType, format, ...) \
    case slotType: \
    { \
      char* result = static_cast<char*>(malloc(snprintf(nullptr, 0u, format, __VA_ARGS__) + 1u)); \
      sprintf(result, format, __VA_ARGS__); \
      return result; \
    }

  switch (slot->type)
  {
    SLOT_STR(slot_type::VARIABLE,               "%s(V)-%c",   slot->variable->name, (slot->storage == slot_storage::REGISTER ? 'R' : 'S'))
    SLOT_STR(slot_type::PARAMETER,              "%s(P)-%c",   slot->variable->name, (slot->storage == slot_storage::REGISTER ? 'R' : 'S'))
    SLOT_STR(slot_type::MEMBER,                 "%s(M)-%c",   slot->member.memberVar->name, (slot->storage == slot_storage::REGISTER ? 'R' : 'S'))
    SLOT_STR(slot_type::TEMPORARY,              "t%u",        slot->tag)
    SLOT_STR(slot_type::RETURN_RESULT,          "r%u",        slot->tag)
    SLOT_STR(slot_type::SIGNED_INT_CONSTANT,    "#%d",        slot->i)
    SLOT_STR(slot_type::UNSIGNED_INT_CONSTANT,  "#%u",        slot->u)
    SLOT_STR(slot_type::FLOAT_CONSTANT,         "#%f",        slot->f)
    SLOT_STR(slot_type::STRING_CONSTANT,        "\"%s\"",     slot->string->string)
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
  result.isModule = false;
  result.name     = nullptr;
  InitVector<dependency_def*>(result.dependencies);
  InitVector<thing_of_code*>(result.codeThings);
  InitVector<type_def*>(result.types);
  InitVector<string_constant*>(result.strings);
  InitVector<const char*>(result.manualLinkedFiles);
}

template<>
void Free<const char*>(const char*& str)
{
  free((char*)str);
}

template<>
void Free<parse_result>(parse_result& result)
{
  free(result.name);
  FreeVector<dependency_def*>(result.dependencies);
  FreeVector<thing_of_code*>(result.codeThings);
  FreeVector<string_constant*>(result.strings);
  FreeVector<const char*>(result.manualLinkedFiles);
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
  var->offset       = 0u;

  return var;
}

void InitAttribSet(attrib_set& set)
{
  set.isEntry     = false;
  set.isPrototype = false;
  set.isInline    = false;
  set.isNoInline  = false;
}

thing_of_code* CreateThingOfCode(thing_type type, ...)
{
  va_list args;
  va_start(args, type);

  thing_of_code* thing = static_cast<thing_of_code*>(malloc(sizeof(thing_of_code)));
  thing->type = type;

  switch (type)
  {
    case thing_type::FUNCTION:
    {
      thing->name = va_arg(args, char*);
    } break;

    case thing_type::OPERATOR:
    {
      thing->op = static_cast<token_type>(va_arg(args, int));
    } break;
  }

  thing->mangledName      = nullptr;
  thing->shouldAutoReturn = false;
  thing->returnType       = nullptr;
  thing->errorState       = CreateErrorState(FUNCTION_FILLING_IN, thing);
  thing->ast              = nullptr;
  thing->stackFrameSize   = 0u;
  thing->airHead          = nullptr;
  thing->airTail          = nullptr;
  thing->numTemporaries   = 0u;
  thing->numReturnResults = 0u;
  thing->symbol           = nullptr;

  InitVector<variable_def*>(thing->params);
  InitVector<variable_def*>(thing->locals);
  InitAttribSet(thing->attribs);
  InitVector<thing_of_code*>(thing->calledThings);
  InitVector<slot_def*>(thing->slots);

  va_end(args);
  return thing;
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
void Free<thing_of_code*>(thing_of_code*& code)
{
  switch (code->type)
  {
    case thing_type::FUNCTION:
    {
      free(code->name);
    } break;

    case thing_type::OPERATOR:
    {
    } break;
  }

  free(code->mangledName);
  FreeVector<variable_def*>(code->params);
  FreeVector<variable_def*>(code->locals);

  if (code->returnType)
  {
    Free<type_ref>(*(code->returnType));
    free(code->returnType);
  }

  if (HasCode(code))
  {
    Free<node*>(code->ast);
  }

  FreeVector<slot_def*>(code->slots);

  while (code->airHead)
  {
    air_instruction* temp = code->airHead;
    code->airHead = code->airHead->next;
    Free<air_instruction*>(temp);
  }
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

/*
 * NOTE(Isaac): the returned string must be freed by the caller
 */
char* TypeRefToString(type_ref* type)
{
  // This is probably fairly unlikely to overflow, and is easier
  char buffer[1024u] = {};
  unsigned int length = 0u;

  #define PUSH(str)\
    strcat(buffer, str);\
    length += strlen(str);

  if (type->isMutable)
  {
    PUSH("mut ");
  }

  if (type->isResolved)
  {
    if (type->isArray && !(type->def))
    {
      PUSH("EMPTY-LIST");
    }
    else
    {
      PUSH(type->def->name);
    }
  }
  else
  {
    PUSH(type->name);
  }

  if (type->isArray)
  {
    PUSH("[");

    if (type->isArraySizeResolved)
    {
      char arraySizeBuffer[8u] = {};
      itoa(type->arraySize, arraySizeBuffer, 10);
      PUSH(arraySizeBuffer);
      PUSH("]");
    }
    else
    {
      PUSH("?]");
    }
  }

  if (type->isReference)
  {
    if (type->isReferenceMutable)
    {
      PUSH(" mut&");
    }
    else
    {
      PUSH("&");
    }
  }
  #undef PUSH

  char* str = static_cast<char*>(malloc(sizeof(char) * (length + 1u)));
  strcpy(str, buffer);
  return str;
}

bool AreTypeRefsCompatible(type_ref* a, type_ref* b, bool careAboutMutability)
{
  /*
   * This covers the special case of assigning an empty-list to another list.
   * The types won't match, because we don't know the contained type of `{}`, but we assume they're compatible.
   */
  if (a->isArray && b->isArray)
  {
    if ((a->isArraySizeResolved && a->arraySize == 0u) ||
        (b->isArraySizeResolved && b->arraySize == 0u))
    {
      return true;
    }
  }

  if (a->def != b->def)
  {
    return false;
  }

  if (a->isReference != b->isReference)
  {
    return false;
  }

  if (careAboutMutability)
  {
    if (a->isMutable != b->isMutable)
    {
      return false;
    }

    if ((a->isReference && b->isReference) && (a->isReferenceMutable != b->isReferenceMutable))
    {
      return false;
    }
  }

  return true;
}

static void ResolveTypeRef(type_ref& ref, parse_result& parse, error_state& errorState)
{
  assert(!(ref.isResolved));

  for (auto* it = parse.types.head;
       it < parse.types.tail;
       it++)
  {
    type_def* type = *it;

    if (strcmp(type->name, ref.name) == 0)
    {
      // XXX(Isaac): would be a good place to increment a usage counter on `typeIt`, if we ever needed one
      ref.isResolved = true;
      ref.def = type;
      return;
    }
  }

  RaiseError(errorState, ERROR_UNDEFINED_TYPE, ref.name);
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
    member->offset = type->size;
    type->size += CalculateSizeOfType(member->type.def);
  }

  return type->size;
}

unsigned int GetSizeOfTypeRef(type_ref& type)
{
  if (type.isReference)
  {
    // TODO: this should come from the target or something
    return 8u;
  }

  assert(type.isResolved);
  unsigned int size = type.def->size;

  if (type.isArray)
  {
    assert(type.isArraySizeResolved);
    size *= type.arraySize;
  }

  return size;
}

char* MangleName(thing_of_code* thing)
{
  switch (thing->type)
  {
    case thing_type::FUNCTION:
    {
      static const char* base = "_R_";
    
      // NOTE(Isaac): add one to leave space for an added null-terminator
      char* mangled = static_cast<char*>(malloc(sizeof(char) * (strlen(base) + strlen(thing->name) + 1u)));
      strcpy(mangled, base);
      strcat(mangled, thing->name);
    
      return mangled;
    } break;

    case thing_type::OPERATOR:
    {
      static const char* base = "_RO_";
    
      const char* opName = nullptr;
      switch (thing->op)
      {
        case TOKEN_PLUS:          opName = "plus";      break;
        case TOKEN_MINUS:         opName = "minus";     break;
        case TOKEN_ASTERIX:       opName = "multiply";  break;
        case TOKEN_SLASH:         opName = "divide";    break;
        case TOKEN_DOUBLE_PLUS:   opName = "increment"; break;
        case TOKEN_DOUBLE_MINUS:  opName = "decrement"; break;
        case TOKEN_LEFT_BLOCK:    opName = "index";     break;
    
        default:
        {
          RaiseError(thing->errorState, ICE_UNHANDLED_OPERATOR, GetTokenName(thing->op), "MangleName");
        } break;
      }
    
      size_t length = strlen(base) + strlen(opName);
      for (auto* it = thing->params.head;
           it < thing->params.tail;
           it++)
      {
        variable_def* param = *it;
        assert(!(param->type.isResolved));
        length += strlen(param->type.name) + 1u;   // NOTE(Isaac): add one for the underscore
      }
    
      char* mangled = static_cast<char*>(malloc(sizeof(char) * (length + 1u)));
      strcpy(mangled, base);
      strcat(mangled, opName);
    
      for (auto* it = thing->params.head;
           it < thing->params.tail;
           it++)
      {
        variable_def* param = *it;
        strcat(mangled, "_");
        strcat(mangled, param->type.name);
      }
    
      return mangled;
    } break;
  }

  return nullptr;
}

static void CompleteVariable(variable_def* var, error_state& errorState)
{
  // If it's an array, resolve the size
  if (var->type.isArray)
  {
    assert(!(var->type.isArraySizeResolved));
    
    node* sizeExpression = var->type.arraySizeExpression;
    assert(sizeExpression);

    if (!(sizeExpression->type == NUMBER_CONSTANT_NODE &&
          sizeExpression->number.type == number_part::constant_type::UNSIGNED_INT))
    {
      RaiseError(errorState, ERROR_INVALID_ARRAY_SIZE);
    }

    var->type.isArraySizeResolved = true;
    var->type.arraySize = sizeExpression->number.asUnsignedInt;

    /*
     * NOTE(Isaac): as the `arraySizeExpression` field of the union has now been replaced and the tag changed,
     * we must manually free it here to avoid leaking the old expression.
     */
    Free<node*>(sizeExpression);
  }
}

void CompleteIR(parse_result& parse)
{
  // Mangle function and operator names
  for (auto* it = parse.codeThings.head;
       it < parse.codeThings.tail;
       it++)
  {
    thing_of_code* thing = *it;
    thing->mangledName = MangleName(thing);
  }

  // --- Resolve `variable_def`s everywhere ---
  for (auto* it = parse.codeThings.head;
       it < parse.codeThings.tail;
       it++)
  {
    thing_of_code* thing = *it;

    if (thing->returnType)
    {
      ResolveTypeRef(*(thing->returnType), parse, thing->errorState);
    }
  
    for (auto* paramIt = thing->params.head;
         paramIt < thing->params.tail;
         paramIt++)
    {
      variable_def* param = *paramIt;
      ResolveTypeRef(param->type, parse, thing->errorState);
      CompleteVariable(param, thing->errorState);
    }
  
    for (auto* localIt = thing->locals.head;
         localIt < thing->locals.tail;
         localIt++)
    {
      variable_def* local = *localIt;
      ResolveTypeRef(local->type, parse, thing->errorState);
      CompleteVariable(local, thing->errorState);
    }
  }

  for (auto* it = parse.types.head;
       it < parse.types.tail;
       it++)
  {
    type_def* type = *it;

    for (auto* memberIt = type->members.head;
         memberIt < type->members.tail;
         memberIt++)
    {
      variable_def* member = *memberIt;
      ResolveTypeRef(member->type, parse, type->errorState);
      CompleteVariable(member, type->errorState);
    }
  }

  // --- Calculate the sizes of composite types ---
  for (auto* typeIt = parse.types.head;
       typeIt < parse.types.tail;
       typeIt++)
  {
    CalculateSizeOfType(*typeIt);
  }
}
