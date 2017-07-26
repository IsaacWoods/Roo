/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <passes/passes.hpp>

void VariableResolverPass::Apply(ParseResult& parse)
{
  for (CodeThing* code : parse.codeThings)
  {
   if (!(code->attribs.isPrototype) && code->ast)
    {
      Dispatch(code->ast, code);
    }
  }
}

void VariableResolverPass::VisitNode(VariableNode* node, CodeThing* code)
{
  if (node->isResolved)
  {
    goto Done;
  }

  Assert(node->containingScope, "Must resolve scopes before trying to resolve variables");
  for (VariableDef* local : node->containingScope->GetReachableVariables())
  {
   if (local->name == node->name)
    {
      delete node->name;
      node->var = local;
      node->isResolved = true;
      goto Done;
    }
  }

  for (VariableDef* param : code->params)
  {
   if (param->name == node->name)
    {
      delete node->name;
      node->var = param;
      node->isResolved = true;
      goto Done;
    }
  }

  RaiseError(code->errorState, ERROR_VARIABLE_NOT_IN_SCOPE, node->name);

Done:
  if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(MemberAccessNode* node, CodeThing* code)
{
  VariableDef* parent;
  Dispatch(node->parent, code);

  // TODO: Get the type of nodes properly with an enum or something
  if (IsNodeOfType<VariableNode>(node->parent))
  {
    parent = dynamic_cast<VariableNode*>(node->parent)->var;
  }
  else if (IsNodeOfType<MemberAccessNode>(node->parent))
  {
    MemberAccessNode* parentMemberAccess = dynamic_cast<MemberAccessNode*>(node->parent);
    Assert(parentMemberAccess->isResolved, "Parent member access has not been resolved");
    parent = parentMemberAccess->member;
  }
  else
  {
    Crash();
  }

  if (!IsNodeOfType<VariableNode>(node->child))
  {
    RaiseError(ICE_GENERIC, "MemberAccessNode's children must be VariableNodes");
  }

  VariableNode* child = dynamic_cast<VariableNode*>(node->child);

  for (VariableDef* member : parent->members)
  {
    if (child->name == member->name)
    {
      node->isResolved = true;
      node->member = member;
      break;
    }
  }

  if (!(node->isResolved))
  {
    RaiseError(code->errorState, ERROR_MEMBER_NOT_FOUND, child->name, parent->type.name.c_str());
  }

  if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(BreakNode* node, CodeThing* code)
{
  if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(ReturnNode* node, CodeThing* code)
{
  if (node->returnValue)
  {
    Dispatch(node->returnValue, code);
  }

  if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(UnaryOpNode* node, CodeThing* code)
{
  Dispatch(node->operand, code);

  if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(BinaryOpNode* node, CodeThing* code)
{
  Dispatch(node->left, code);
  Dispatch(node->right, code);

  if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(ConditionNode* node, CodeThing* code)
{
  Dispatch(node->left, code);
  Dispatch(node->right, code);

  if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(BranchNode* node, CodeThing* code)
{
  Dispatch(node->condition, code);
  Dispatch(node->thenCode, code);

  if (node->elseCode)
  {
    Dispatch(node->elseCode, code);
  }

  if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(WhileNode* node, CodeThing* code)
{
  Dispatch(node->condition, code);
  Dispatch(node->loopBody, code);

  if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(ConstantNode<unsigned int>* node, CodeThing* code)
{
  if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(ConstantNode<int>* node, CodeThing* code)
{
  if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(ConstantNode<float>* node, CodeThing* code)
{
  if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(ConstantNode<bool>* node, CodeThing* code)
{
  if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(StringNode* node, CodeThing* code)
{
  if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(CallNode* node, CodeThing* code)
{
  for (ASTNode* paramNode : node->params)
  {
    Dispatch(paramNode, code);
  }

  if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(VariableAssignmentNode* node, CodeThing* code)
{
  Dispatch(node->variable, code);
  Dispatch(node->newValue, code);

  if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(ArrayInitNode* node, CodeThing* code)
{
  for (ASTNode* item : node->items)
  {
    Dispatch(item, code);
  }

  if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(InfiniteLoopNode* node, CodeThing* code)
{
  Dispatch(node->loopBody, code);
  if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(ConstructNode* node, CodeThing* code)
{
  for (ASTNode* item : node->items)
  {
    Dispatch(item, code);
  }

  if (node->next) Dispatch(node->next, code);
}
