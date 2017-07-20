/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <passes/passes.hpp>

struct TypeCheckingContext
{
  TypeCheckingContext(ParseResult& parse, CodeThing* code)
    :parse(parse)
    ,code(code)
  { }
  ~TypeCheckingContext() { }

  ParseResult& parse;
  CodeThing* code;
};

void TypeChecker::Apply(ParseResult& parse)
{
  for (CodeThing* code : parse.codeThings)
  {
    if (code->attribs.isPrototype)
    {
      continue;
    }

    TypeCheckingContext context(parse, code);
    Dispatch(code->ast, &context);
  }
}

void TypeChecker::VisitNode(BreakNode* node, TypeCheckingContext* context)
{
  if (node->next) Dispatch(node->next, context);
}

void TypeChecker::VisitNode(ReturnNode* node, TypeCheckingContext* context)
{
  Dispatch(node->returnValue, context);

  /*
   * Firstly, check that both types actually exist (otherwise we risk dereferencing a null pointer later)
   */
  if (!(context->code->returnType) && node->returnValue)
  {
    RaiseError(context->code->errorState, ERROR_RETURN_VALUE_NOT_EXPECTED, node->returnValue->type->AsString().c_str());
    goto Errored;
  }

  if (context->code->returnType && !(node->returnValue))
  {
    RaiseError(context->code->errorState, ERROR_RETURN_VALUE_NOT_EXPECTED, context->code->returnType->AsString().c_str());
    goto Errored;
  }

  /*
   * If both types exist, compare them
   */
  if (!AreTypeRefsCompatible(context->code->returnType, node->returnValue->type))
  {
    RaiseError(context->code->errorState, ERROR_INCOMPATIBLE_TYPE, context->code->returnType->AsString().c_str(),
                                                                   node->returnValue->type->AsString().c_str());
  }
Errored:
  if (node->next) Dispatch(node->next, context);
}

void TypeChecker::VisitNode(UnaryOpNode* node, TypeCheckingContext* context)
{
  Dispatch(node->operand, context);

  if (node->next) Dispatch(node->next, context);
}

void TypeChecker::VisitNode(BinaryOpNode* node, TypeCheckingContext* context)
{
  Dispatch(node->left, context);
  Dispatch(node->right, context);
  if (node->next) Dispatch(node->next, context);
}

void TypeChecker::VisitNode(VariableNode* node, TypeCheckingContext* context)
{
  Assert(node->isResolved, "Tried to type-check an unresolved variable");
  Assert(node->var->type.isResolved, "Tried to type-check a variable with no resolved type");
  node->type = &(node->var->type);
  node->shouldFreeTypeRef = false;

  if (node->next) Dispatch(node->next, context);
}

void TypeChecker::VisitNode(ConditionNode* node, TypeCheckingContext* context)
{
  Dispatch(node->left,  context);
  Dispatch(node->right, context);

  // TODO: check that we can compare `left` and `right`

  if (node->next) Dispatch(node->next, context);
}

void TypeChecker::VisitNode(BranchNode* node, TypeCheckingContext* context)
{
  Dispatch(node->condition, context);
  Dispatch(node->thenCode, context);
  
  if (node->elseCode)
  {
    Dispatch(node->elseCode, context);
  }

  if (node->next) Dispatch(node->next, context);
}

void TypeChecker::VisitNode(WhileNode* node, TypeCheckingContext* context)
{
  Dispatch(node->condition, context);
  Dispatch(node->loopBody, context);

  if (node->next) Dispatch(node->next, context);
}

void TypeChecker::VisitNode(ConstantNode<unsigned int>* node, TypeCheckingContext* context)
{
  node->type = new TypeRef();
  node->type->isResolved           = true;
  node->type->resolvedType         = GetTypeByName(context->parse, "uint");
  node->type->isMutable            = false;
  node->type->isReference          = false;
  node->type->isReferenceMutable   = false;
  node->type->isArray              = false;
  node->type->isArraySizeResolved  = true;
  node->type->arraySize            = 0u;
  node->shouldFreeTypeRef          = true;

  if (node->next) Dispatch(node->next, context);
}

void TypeChecker::VisitNode(ConstantNode<int>* node, TypeCheckingContext* context)
{
  node->type = new TypeRef();
  node->type->isResolved           = true;
  node->type->resolvedType         = GetTypeByName(context->parse, "int");
  node->type->isMutable            = false;
  node->type->isReference          = false;
  node->type->isReferenceMutable   = false;
  node->type->isArray              = false;
  node->type->isArraySizeResolved  = true;
  node->type->arraySize            = 0u;
  node->shouldFreeTypeRef          = true;

  if (node->next) Dispatch(node->next, context);
}

void TypeChecker::VisitNode(ConstantNode<float>* node, TypeCheckingContext* context)
{
  node->type = new TypeRef();
  node->type->isResolved           = true;
  node->type->resolvedType         = GetTypeByName(context->parse, "float");
  node->type->isMutable            = false;
  node->type->isReference          = false;
  node->type->isReferenceMutable   = false;
  node->type->isArray              = false;
  node->type->isArraySizeResolved  = true;
  node->type->arraySize            = 0u;
  node->shouldFreeTypeRef          = true;

  if (node->next) Dispatch(node->next, context);
}

void TypeChecker::VisitNode(ConstantNode<bool>* node, TypeCheckingContext* context)
{
  node->type = new TypeRef();
  node->type->isResolved           = true;
  node->type->resolvedType         = GetTypeByName(context->parse, "bool");
  node->type->isMutable            = false;
  node->type->isReference          = false;
  node->type->isReferenceMutable   = false;
  node->type->isArray              = false;
  node->type->isArraySizeResolved  = true;
  node->type->arraySize            = 0u;
  node->shouldFreeTypeRef          = true;

  if (node->next) Dispatch(node->next, context);
}

void TypeChecker::VisitNode(StringNode* node, TypeCheckingContext* context)
{
  node->type = new TypeRef();
  node->type->isResolved           = true;
  node->type->resolvedType         = GetTypeByName(context->parse, "string");
  node->type->isMutable            = false;
  node->type->isReference          = false;
  node->type->isReferenceMutable   = false;
  node->type->isArray              = false;
  node->type->isArraySizeResolved  = true;
  node->type->arraySize            = 0u;
  node->shouldFreeTypeRef          = true;

  if (node->next) Dispatch(node->next, context);
}

void TypeChecker::VisitNode(CallNode* node, TypeCheckingContext* context)
{
  for (ASTNode* param : node->params)
  {
    Dispatch(param, context);
  }

  /*
   * This isn't really typechecking, but between typing the parameters and inferring the return type, we need to
   * work out what function / operator we're actually calling.
   */
  for (CodeThing* thing : context->parse.codeThings)
  {
    if (thing->type != CodeThing::Type::FUNCTION)
    {
      continue;
    }

    if (node->name != dynamic_cast<FunctionThing*>(thing)->name)
    {
      continue;
    }

    if (node->params.size() != thing->params.size())
    {
      continue;
    }

    for (unsigned int i = 0u;
         i < node->params.size();
         i++)
    {
      if (!AreTypeRefsCompatible(node->params[i]->type, &(thing->params[i]->type), false))
      {
        goto NotCorrectThing;
      }
    }

    // This is the correct thing!
    free(node->name);
    node->resolvedFunction = thing;
    node->isResolved = true;
    context->code->calledThings.push_back(thing);

    /*
     * We can then retrieve the return type from the definition of the function/operator.
     */
    node->shouldFreeTypeRef = false;
    node->type = node->resolvedFunction->returnType;
    break;

NotCorrectThing:
    continue;
  }

  if (!(node->isResolved))
  {
    RaiseError(context->code->errorState, ERROR_UNDEFINED_FUNCTION, node->name);
  }

  if (node->next) Dispatch(node->next, context);
}

void TypeChecker::VisitNode(VariableAssignmentNode* node, TypeCheckingContext* context)
{
  Dispatch(node->variable, context);
  Dispatch(node->newValue, context);

  if (!(node->ignoreImmutability) && !(node->variable->type->isMutable))
  {
    RaiseError(context->code->errorState, ERROR_ASSIGN_TO_IMMUTABLE, node->variable->AsString().c_str());
  }

  if (!(node->variable->type && node->variable->type->isResolved))
  {
    RaiseError(context->code->errorState, ERROR_MISSING_TYPE_INFORMATION, "Couldn't deduce left-side of assignment");
    goto TypeError;
  }

  if (!(node->newValue->type && node->newValue->type->isResolved))
  {
    RaiseError(context->code->errorState, ERROR_MISSING_TYPE_INFORMATION, "Couldn't deduce right-side of assignment");
    goto TypeError;
  }
  
  if (!AreTypeRefsCompatible(node->variable->type, node->newValue->type, false))
  {
    RaiseError(context->code->errorState, ERROR_INCOMPATIBLE_ASSIGN, node->newValue->AsString().c_str(), node->variable->type->name.c_str());
  }

TypeError:
  if (node->next) Dispatch(node->next, context);
}

void TypeChecker::VisitNode(MemberAccessNode* node, TypeCheckingContext* context)
{
  if (node->next) Dispatch(node->next, context);
}

void TypeChecker::VisitNode(ArrayInitNode* node, TypeCheckingContext* context)
{
  for (ASTNode* item : node->items)
  {
    Dispatch(item, context);
  }

  if (node->next) Dispatch(node->next, context);
}

void TypeChecker::VisitNode(InfiniteLoopNode* node, TypeCheckingContext* context)
{
  Dispatch(node->loopBody, context);
  if (node->next) Dispatch(node->next, context);
}

void TypeChecker::VisitNode(ConstructNode* node, TypeCheckingContext* context)
{
  for (ASTNode* item : node->items)
  {
    Dispatch(item, context);
  }

  // TODO: resolve the type of the constructor called - mark the type of this node as the type of the value it creates
  // TODO: check each item's type against the type in the TypeDef

  if (node->next) Dispatch(node->next, context);
}
