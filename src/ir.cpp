/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <ir.hpp>
#include <climits>
#include <cstring>
#include <cstdarg>
#include <common.hpp>
#include <ast.hpp>
#include <air.hpp>
#include <elf.hpp>

ParseResult::ParseResult()
  :isModule(false)
  ,name(nullptr)
  ,targetArch(nullptr)
  ,dependencies()
  ,codeThings()
  ,types()
  ,strings()
  ,filesToLink()
{
}

ParseResult::~ParseResult()
{
  delete name;
  delete targetArch;
}

DependencyDef::DependencyDef(DependencyDef::Type type, char* path)
  :type(type)
  ,path(path)
{
}

DependencyDef::~DependencyDef()
{
  delete path;
}

StringConstant::StringConstant(ParseResult& parse, char* string)
  :string(string)
  ,offset(0u)
{
  if (parse.strings.size() > 0u)
  {
    handle = parse.strings.back()->handle + 1u;
  }
  else
  {
    handle = 0u;
  }

  parse.strings.push_back(this);
}

StringConstant::~StringConstant()
{
  delete string;
}

TypeDef::TypeDef(char* name)
  :name(name)
  ,members()
  ,errorState(ErrorState::Type::TYPE_FILLING_IN, this)
  ,size(UINT_MAX)
{
}

TypeDef::~TypeDef()
{
  delete name;
}

TypeRef::TypeRef()
  :name()
  ,resolvedType(nullptr)
  ,isResolved(false)
  ,isMutable(false)
  ,isReference(false)
  ,isReferenceMutable(false)
  ,isArray(false)
  ,isArraySizeResolved(false)
  ,arraySizeExpression(nullptr)
{
}

TypeRef::~TypeRef()
{
  if (!isArraySizeResolved)
  {
    delete arraySizeExpression;
  }
}

VariableDef::VariableDef(char* name, const TypeRef& type, ASTNode* initialValue)
  :name(name)
  ,type(type)
  ,initialValue(initialValue)
  ,slot(nullptr)
  ,offset(0u)
{
}

VariableDef::~VariableDef()
{
  delete name;
  delete initialValue;
}

AttribSet::AttribSet()
  :isEntry(false)
  ,isPrototype(false)
  ,isInline(false)
  ,isNoInline(false)
{
}

ThingOfCode::ThingOfCode(ThingOfCode::Type type, ...)
  :type(type)
  ,mangledName(nullptr)
  ,shouldAutoReturn(false)
  ,attribs()
  ,returnType(nullptr)
  ,errorState(ErrorState::Type::FUNCTION_FILLING_IN, this)
  ,ast(nullptr)
  ,stackFrameSize(0u)
  ,airHead(nullptr)
  ,airTail(nullptr)
  ,numTemporaries(0u)
  ,numReturnResults(0u)
  ,symbol(nullptr)
{
  va_list args;
  va_start(args, type);

  switch (type)
  {
    case ThingOfCode::Type::FUNCTION:
    {
      name = va_arg(args, char*);
    } break;

    case ThingOfCode::Type::OPERATOR:
    {
      op = static_cast<token_type>(va_arg(args, int));
    } break;
  }

  va_end(args);
}

ThingOfCode::~ThingOfCode()
{
  switch (type)
  {
    case ThingOfCode::Type::FUNCTION:
    {
      delete name;
    } break;

    case ThingOfCode::Type::OPERATOR:
    {
    } break;
  }

  delete mangledName;
  delete returnType;
  delete ast;
  delete airHead;
  delete airTail;
  delete symbol;
}

TypeDef* GetTypeByName(ParseResult& parse, const char* typeName)
{
  for (TypeDef* type : parse.types)
  {
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
char* TypeRefToString(TypeRef* type)
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
    if (type->isArray && !(type->resolvedType))
    {
      PUSH("EMPTY-LIST");
    }
    else
    {
      PUSH(type->resolvedType->name);
    }
  }
  else
  {
    //PUSH(type->name);
    PUSH(type->name.c_str());
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

bool AreTypeRefsCompatible(TypeRef* a, TypeRef* b, bool careAboutMutability)
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

  if (a->resolvedType != b->resolvedType)
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

static void ResolveTypeRef(TypeRef& ref, ParseResult& parse, ErrorState& errorState)
{
  Assert(!(ref.isResolved), "Tried to resolve type reference that is already resolved");

  for (TypeDef* type : parse.types)
  {
    if (ref.name == type->name)
    {
      // XXX(Isaac): this would be a good place to increment a usage counter on the type, if we ever needed one
      ref.isResolved = true;
      ref.resolvedType = type;
      return;
    }
  }

  RaiseError(errorState, ERROR_UNDEFINED_TYPE, ref.name.c_str());
}

/*
 * This calculates the size of a type, and stores it within the `TypeDef`.
 * NOTE(Isaac): Should not be called on inbuilt types with `overwrite = true`.
 * TODO(Isaac): should types be padded out to fit nicely on alignment boundaries?
 */
static unsigned int CalculateSizeOfType(TypeDef* type, bool overwrite = false)
{
  if (!overwrite && type->size != UINT_MAX)
  {
    return type->size;
  }

  type->size = 0u;

  for (VariableDef* member : type->members)
  {
    Assert(member->type.isResolved, "Tried to calculate size of type that has unresolved members");
    member->offset = type->size;
    type->size += CalculateSizeOfType(member->type.resolvedType);
  }

  return type->size;
}

unsigned int GetSizeOfTypeRef(TypeRef& type)
{
  if (type.isReference)
  {
    // TODO: this should come from the target or something
    return 8u;  // Return the size of an address on the target architecture
  }

  Assert(type.isResolved, "Tried to get size of a type reference that hasn't been resolved");
  unsigned int size = type.resolvedType->size;

  if (type.isArray)
  {
    Assert(type.isArraySizeResolved, "Tried to find size of an array type who's size if unresolved");
    size *= type.arraySize;
  }

  return size;
}

char* MangleName(ThingOfCode* thing)
{
  switch (thing->type)
  {
    case ThingOfCode::Type::FUNCTION:
    {
      static const char* base = "_R_";
    
      // NOTE(Isaac): add one to leave space for an added null-terminator
      char* mangled = static_cast<char*>(malloc(sizeof(char) * (strlen(base) + strlen(thing->name) + 1u)));
      strcpy(mangled, base);
      strcat(mangled, thing->name);
    
      return mangled;
    } break;

    case ThingOfCode::Type::OPERATOR:
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
      for (VariableDef* param : thing->params)
      {
        Assert(!(param->type.isResolved), "Tried to mangle an operator that isn't fully resolved");
        //length += strlen(param->type.name) + 1u;   // NOTE(Isaac): add one for the underscore
        length += param->type.name.length() + 1u;   // NOTE(Isaac): add one for the underscore
      }
    
      char* mangled = static_cast<char*>(malloc(sizeof(char) * (length + 1u)));
      strcpy(mangled, base);
      strcat(mangled, opName);
    
      for (VariableDef* param : thing->params)
      {
        strcat(mangled, "_");
        //strcat(mangled, param->type.name);
        strcat(mangled, param->type.name.c_str());
      }
    
      return mangled;
    } break;
  }

  return nullptr;
}

static void CompleteVariable(VariableDef* var, ErrorState& errorState)
{
  // If it's an array, resolve the size
  if (var->type.isArray)
  {
    Assert(!(var->type.isArraySizeResolved), "Tried to resolve array size expression that already has a size");
    
    if (!IsNodeOfType<ConstantNode<unsigned int>>(var->type.arraySizeExpression))
    {
      RaiseError(errorState, ERROR_INVALID_ARRAY_SIZE);
      return;
    }

    auto* sizeExpression = reinterpret_cast<ConstantNode<unsigned int>*>(var->type.arraySizeExpression);
    var->type.isArraySizeResolved = true;
    var->type.arraySize = sizeExpression->value;

    /*
     * NOTE(Isaac): as the `arraySizeExpression` field of the union has now been replaced and the tag changed,
     * we must free it here to avoid leaking the old expression.
     */
    delete sizeExpression;
  }
}

void CompleteIR(ParseResult& parse)
{
  for (ThingOfCode* thing : parse.codeThings)
  {
    // Mangle the thing's name
    thing->mangledName = MangleName(thing);

    // Resolve its return type and the types of its parameters and locals
    if (thing->returnType)
    {
      ResolveTypeRef(*(thing->returnType), parse, thing->errorState);
    }
  
    for (VariableDef* param : thing->params)
    {
      Assert(param, "Tried to resolve nullptr parameter");
      ResolveTypeRef(param->type, parse, thing->errorState);
      CompleteVariable(param, thing->errorState);
    }
  
    for (VariableDef* local : thing->locals)
    {
      ResolveTypeRef(local->type, parse, thing->errorState);
      CompleteVariable(local, thing->errorState);
    }
  }

  for (TypeDef* type : parse.types)
  {
    for (VariableDef* member : type->members)
    {
      ResolveTypeRef(member->type, parse, type->errorState);
      CompleteVariable(member, type->errorState);
    }
  }

  /*
   * Calculate the sizes of all the types.
   * This must be done after *every* type and its members has been resolved.
   */
  for (TypeDef* type : parse.types)
  {
    CalculateSizeOfType(type);
  }

  // If there were any errors completing the IR, don't bother continuing
  for (ThingOfCode* code : parse.codeThings)
  {
    if (code->errorState.hasErrored)
    {
      RaiseError(ERROR_COMPILE_ERRORS);
    }
  }

  for (TypeDef* type : parse.types)
  {
    if (type->errorState.hasErrored)
    {
      RaiseError(ERROR_COMPILE_ERRORS);
    }
  }
}
