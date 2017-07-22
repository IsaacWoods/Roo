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
#include <codegen.hpp>

ParseResult::ParseResult()
  :isModule(false)
  ,name()
  ,targetArch()
  ,dependencies()
  ,codeThings()
  ,types()
  ,strings()
  ,filesToLink()
{
}

DependencyDef::DependencyDef(DependencyDef::Type type, const std::string& path)
  :type(type)
  ,path(path)
{
}

StringConstant::StringConstant(ParseResult& parse, const std::string& str)
  :str(str)
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

TypeDef::TypeDef(const std::string& name)
  :name(name)
  ,members()
  ,errorState(ErrorState::Type::TYPE_FILLING_IN, this)
  ,size(UINT_MAX)
{
}

TypeDef::~TypeDef()
{
  for (VariableDef* member : members)
  {
    delete member;
  }
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

std::string TypeRef::AsString()
{
  std::string result;

  if (isMutable)
  {
    result += "mut ";
  }

  if (isResolved)
  {
    if (isArray && !resolvedType)
    {
      result += "EMPTY-LIST";
    }
    else
    {
      result += resolvedType->name;
    }
  }
  else
  {
    result += name;
  }

  if (isArray)
  {
    if (isArraySizeResolved)
    {
      result += FormatString("[%u]", arraySize);
    }
    else
    {
      result += "[??]";
    }
  }

  return result;
}

unsigned int TypeRef::GetSize()
{
  if (isReference)
  {
    // TODO: actually get the size of an address on the target arch
    return 8u;
  }

  Assert(isResolved, "Tried to calc size of an unresolved TypeRef");
  unsigned int size = resolvedType->size;

  if (isArray)
  {
    Assert(isArraySizeResolved, "Tried to calc size of an array who's size expression is unresolved");
    size *= arraySize;
  }

  return size;
}

VariableDef::VariableDef(const std::string& name, const TypeRef& type, ASTNode* initExpression)
  :name(name)
  ,type(type)
  ,initExpression(initExpression)
  ,storage(VariableDef::Storage::UNDECIDED)
  ,slot(nullptr)
  ,offset(0u)
{
}

VariableDef::~VariableDef()
{
  delete initExpression;
}

char VariableDef::GetStorageChar()
{
  switch (storage)
  {
    case VariableDef::Storage::UNDECIDED:   return '?';
    case VariableDef::Storage::REGISTER:    return 'R';
    case VariableDef::Storage::STACK:       return 'S';
  }

  __builtin_unreachable();
}

AttribSet::AttribSet()
  :isEntry(false)
  ,isPrototype(false)
  ,isInline(false)
  ,isNoInline(false)
{
}

ScopeDef::ScopeDef(CodeThing* thing, ScopeDef* parent)
  :parent(parent)
  ,locals()
{
  Assert(thing, "Tried to create scope in nullptr CodeThing");
  thing->scopes.push_back(this);
}

std::vector<VariableDef*> ScopeDef::GetReachableVariables()
{
  std::vector<VariableDef*> reachable;
  reachable.insert(reachable.end(), locals.begin(), locals.end());

  ScopeDef* parentScope = parent;
  while (parentScope)
  {
    reachable.insert(reachable.end(), parentScope->locals.begin(), parentScope->locals.end());
    parentScope = parentScope->parent;
  }

  return reachable;
}

CodeThing::CodeThing(CodeThing::Type type)
  :type(type)
  ,mangledName()
  ,params()
  ,scopes()
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
  ,neededStackSpace(0u)
  ,symbol(nullptr)
{
}

CodeThing::~CodeThing()
{
  delete returnType;
  delete ast;
  
  for (Slot* slot : slots)
  {
    delete slot;
  }

  delete airHead;
  delete airTail;
  delete symbol;
}

FunctionThing::FunctionThing(const std::string& name)
  :CodeThing(CodeThing::Type::FUNCTION)
  ,name(name)
{
}

OperatorThing::OperatorThing(TokenType token)
  :CodeThing(CodeThing::Type::OPERATOR)
  ,token(token)
{
}

TypeDef* GetTypeByName(ParseResult& parse, const std::string& name)
{
  for (TypeDef* type : parse.types)
  {
    if (type->name == name)
    {
      return type;
    }
  }

  return nullptr;
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

static std::string MangleName(CodeThing* thing)
{
  switch (thing->type)
  {
    case CodeThing::Type::FUNCTION:
    {
      static const std::string BASE = "_R_";
      return BASE + dynamic_cast<FunctionThing*>(thing)->name;
    } break;

    case CodeThing::Type::OPERATOR:
    {
      static const std::string BASE = "_RO_";
      OperatorThing* opThing = dynamic_cast<OperatorThing*>(thing);
      
      std::string mangling = BASE;
      switch (opThing->token)
      {
        case TOKEN_PLUS:          mangling += "plus";      break;
        case TOKEN_MINUS:         mangling += "minus";     break;
        case TOKEN_ASTERIX:       mangling += "multiply";  break;
        case TOKEN_SLASH:         mangling += "divide";    break;
        case TOKEN_DOUBLE_PLUS:   mangling += "increment"; break;
        case TOKEN_DOUBLE_MINUS:  mangling += "decrement"; break;
        case TOKEN_LEFT_BLOCK:    mangling += "index";     break;
    
        default:
        {
          RaiseError(thing->errorState, ICE_UNHANDLED_OPERATOR, GetTokenName(opThing->token), "MangleName::OPERATOR");
        } break;
      }
    
      for (VariableDef* param : thing->params)
      {
        mangling += std::string("_") + param->type.name;
      }
    
      return mangling;
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

void CompleteIR(ParseResult& parse, TargetMachine& target)
{
  for (CodeThing* thing : parse.codeThings)
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
  
    for (ScopeDef* scope : thing->scopes)
    {
      for (VariableDef* local : scope->locals)
      {
        ResolveTypeRef(local->type, parse, thing->errorState);
        CompleteVariable(local, thing->errorState);
      }
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

  for (CodeThing* thing : parse.codeThings)
  {
    for (ScopeDef* scope : thing->scopes)
    {
      for (VariableDef* local : scope->locals)
      {
        Assert(local->type.isResolved, "Tried to allocate stack frame before types have been resolved");

        // Work out where to store this local
        if (local->type.resolvedType->size > target.generalRegisterSize)
        {
          local->storage = VariableDef::Storage::STACK;
        }
        else
        {
          local->storage = VariableDef::Storage::REGISTER;
        }

        // Calculate the total size of the stack frame
        thing->neededStackSpace += local->type.resolvedType->size;
      }
    }

    // Calculate the offset from the base pointer of each local on the stack
    unsigned int runningOffset = 0u;
    for (ScopeDef* scope : thing->scopes)
    {
      for (VariableDef* local : scope->locals)
      {
        local->offset = runningOffset;
        runningOffset += local->type.resolvedType->size;
      }
    }
    Assert(runningOffset == thing->neededStackSpace, "We didn't use the entire stack-frame; why?!");
  }

  // If there were any errors completing the IR, don't bother continuing
  for (CodeThing* code : parse.codeThings)
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
