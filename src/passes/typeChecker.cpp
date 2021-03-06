/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <passes/passes.hpp>
#include <target.hpp>

struct TypeCheckingContext
{
  TypeCheckingContext(ParseResult& parse, TargetMachine* target, CodeThing* code)
    :parse(parse)
    ,target(target)
    ,code(code)
  { }
  ~TypeCheckingContext() { }

  ParseResult&    parse;
  TargetMachine*  target;
  CodeThing*      code;
};

void TypeChecker::Apply(ParseResult& parse, TargetMachine* target)
{
  for (CodeThing* code : parse.codeThings)
  {
    if (code->attribs.isPrototype || !(code->ast))
    {
      continue;
    }

    TypeCheckingContext context(parse, target, code);
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

  /*
   * Firstly, we handle intrinsic operations
   */
  if (AreTypeRefsCompatible(node->left->type, context->target->intrinsicTypes[UNSIGNED_INT_INTRINSIC]) &&
      AreTypeRefsCompatible(node->right->type, context->target->intrinsicTypes[UNSIGNED_INT_INTRINSIC]))
  {
    node->type = context->target->intrinsicTypes[UNSIGNED_INT_INTRINSIC];
    node->intrinsicType = UNSIGNED_INT_INTRINSIC;
    node->shouldFreeTypeRef = false;
  }
  else if (AreTypeRefsCompatible(node->left->type, context->target->intrinsicTypes[SIGNED_INT_INTRINSIC]) &&
           AreTypeRefsCompatible(node->right->type, context->target->intrinsicTypes[SIGNED_INT_INTRINSIC]))
  {
    node->type = context->target->intrinsicTypes[SIGNED_INT_INTRINSIC];
    node->intrinsicType = SIGNED_INT_INTRINSIC;
    node->shouldFreeTypeRef = false;
  }
  else if (AreTypeRefsCompatible(node->left->type, context->target->intrinsicTypes[FLOAT_INTRINSIC]) &&
           AreTypeRefsCompatible(node->right->type, context->target->intrinsicTypes[FLOAT_INTRINSIC]))
  {
    node->type = context->target->intrinsicTypes[FLOAT_INTRINSIC];
    node->intrinsicType = FLOAT_INTRINSIC;
    node->shouldFreeTypeRef = false;
  }
  else if (AreTypeRefsCompatible(node->left->type, context->target->intrinsicTypes[BOOL_INTRINSIC]) &&
           AreTypeRefsCompatible(node->right->type, context->target->intrinsicTypes[BOOL_INTRINSIC]))
  {
    node->type = context->target->intrinsicTypes[BOOL_INTRINSIC];
    node->intrinsicType = BOOL_INTRINSIC;
    node->shouldFreeTypeRef = false;
  }
  else if (AreTypeRefsCompatible(node->left->type, context->target->intrinsicTypes[STRING_INTRINSIC]) &&
           AreTypeRefsCompatible(node->right->type, context->target->intrinsicTypes[STRING_INTRINSIC]))
  {
    /*
     * TODO: Strings are a bit special, so check the operation to see if we actually handle the intrinsic case
     */
    node->type = context->target->intrinsicTypes[STRING_INTRINSIC];
    node->intrinsicType = STRING_INTRINSIC;
    node->shouldFreeTypeRef = false;
  }
  else
  {
    /*
     * We couldn't find a suitable intrinsic operator that we know should exist, so we try to find an overloaded
     * operator that fits the types instead (this also resolves the CodeThing to call for overloaded operations)
     */
    for (CodeThing* thing : context->parse.codeThings)
    {
      if (thing->type != CodeThing::Type::OPERATOR || thing->params.size() != 2u)
      {
        continue;
      }

      TokenType token = static_cast<OperatorThing*>(thing)->token;
      if ((token == TOKEN_PLUS    && node->op == BinaryOpNode::Operator::ADD)      ||
          (token == TOKEN_MINUS   && node->op == BinaryOpNode::Operator::SUBTRACT) ||
          (token == TOKEN_ASTERIX && node->op == BinaryOpNode::Operator::MULTIPLY) ||
          (token == TOKEN_SLASH   && node->op == BinaryOpNode::Operator::DIVIDE))
      {
        if (AreTypeRefsCompatible(node->left->type, &(thing->params[0u]->type)) &&
            AreTypeRefsCompatible(node->right->type, &(thing->params[1u]->type)))
        {
          node->overloadedOperator = thing;
          node->type = thing->returnType;
          node->shouldFreeTypeRef = false;
          break;
        }
      }
    }

    if (!(node->overloadedOperator))
    {
      RaiseError(context->code->errorState, ERROR_MISSING_OPERATOR, node->left->type->AsString().c_str(),
                                                                     node->right->type->AsString().c_str());
    }
  }

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

void TypeChecker::VisitNode(CompositeConditionNode* node, TypeCheckingContext* context)
{
  Dispatch(node->left, context);
  Dispatch(node->right, context);

  /*
   * The only requirement is that `left` and `right` are both conditions, which is
   * ensured on the type level, so we don't need to type-check anything.
   */
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
  node->type = context->target->intrinsicTypes[UNSIGNED_INT_INTRINSIC];
  node->shouldFreeTypeRef = false;
  if (node->next) Dispatch(node->next, context);
}

void TypeChecker::VisitNode(ConstantNode<int>* node, TypeCheckingContext* context)
{
  node->type = context->target->intrinsicTypes[SIGNED_INT_INTRINSIC];
  node->shouldFreeTypeRef = false;
  if (node->next) Dispatch(node->next, context);
}

void TypeChecker::VisitNode(ConstantNode<float>* node, TypeCheckingContext* context)
{
  node->type = context->target->intrinsicTypes[FLOAT_INTRINSIC];
  node->shouldFreeTypeRef = false;
  if (node->next) Dispatch(node->next, context);
}

void TypeChecker::VisitNode(ConstantNode<bool>* node, TypeCheckingContext* context)
{
  node->type = context->target->intrinsicTypes[BOOL_INTRINSIC];
  node->shouldFreeTypeRef = false;
  if (node->next) Dispatch(node->next, context);
}

void TypeChecker::VisitNode(StringNode* node, TypeCheckingContext* context)
{
  node->type = context->target->intrinsicTypes[STRING_INTRINSIC];
  node->shouldFreeTypeRef = false;
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
    delete node->name;
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

  /*
   * This handles whether we can assign to the variable at all (regarding mutability), so we can now
   * disregard whether the mutabilities match after this (you can assign a immutable to a mutable, as
   * long as it doesn't copy).
   */
  if (!(node->ignoreImmutability) && !(node->variable->type->isMutable))
  {
    RaiseError(context->code->errorState, ERROR_ASSIGN_TO_IMMUTABLE,
                                          node->variable->AsString().c_str());
  }

  if (!(node->variable->type && node->variable->type->isResolved))
  {
    RaiseError(context->code->errorState, ERROR_MISSING_TYPE_INFORMATION,
                                          "Couldn't deduce left-side of assignment");
    goto TypeError;
  }

  if (!(node->newValue->type && node->newValue->type->isResolved))
  {
    RaiseError(context->code->errorState, ERROR_MISSING_TYPE_INFORMATION,
                                          "Couldn't deduce right-side of assignment");
    goto TypeError;
  }
  
  if (!AreTypeRefsCompatible(node->variable->type, node->newValue->type, false))
  {
    RaiseError(context->code->errorState, ERROR_INCOMPATIBLE_ASSIGN,
                                          node->newValue->AsString().c_str(),
                                          node->variable->type->name.c_str());
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
  Dispatch(node->variable, context);
  for (ASTNode* item : node->items)
  {
    Dispatch(item, context);
  }

  // Create the type for the ConstructNode
  node->type = new TypeRef();
  node->type->isResolved           = true;
  node->type->resolvedType         = GetTypeByName(context->parse, node->typeName);
  node->type->isMutable            = false;
  node->type->isReference          = false;
  node->type->isReferenceMutable   = false;
  node->type->isArray              = false;
  node->type->isArraySizeResolved  = true;
  node->type->arraySize            = 0u;
  node->shouldFreeTypeRef          = true;

  // Check that we're constructing the correct type for the variable
  if (!AreTypeRefsCompatible(node->type, node->variable->type))
  {
    RaiseError(context->code->errorState, ERROR_INCOMPATIBLE_TYPE, node->variable->type->AsString().c_str(),
                                                                   node->type->AsString().c_str());
  }

  // Check that we're supplying the correct number of items
  if (node->items.size() != node->type->resolvedType->members.size())
  {
    RaiseError(context->code->errorState, ERROR_TYPE_CONSTRUCT_TOO_FEW_EXPRESSIONS, node->typeName.c_str());
    return;
  }
  else
  {
    // We only do this check if the lists have the same length, or we could overrun one of the iterators
    auto itemIt = node->items.begin();
    auto memberIt = node->type->resolvedType->members.begin();
    for (;
         itemIt < node->items.end() && memberIt != node->type->resolvedType->members.end();
         itemIt++, memberIt++)
    {
      ASTNode* item = *itemIt;
      MemberDef* member = *memberIt;

      if (!AreTypeRefsCompatible(item->type, &(member->type)))
      {
        RaiseError(context->code->errorState, ERROR_INCOMPATIBLE_TYPE, member->type.AsString().c_str(),
                                                                       item->type->AsString().c_str());
      }
    }
  }

  if (node->next) Dispatch(node->next, context);
}
