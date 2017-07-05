/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <passes/passes.hpp>

void VariableResolverPass::Apply(ParseResult& parse)
{
  for (ThingOfCode* code : parse.codeThings)
  {
    if (!(code->attribs.isPrototype) && code->ast)
    {
      Dispatch(code->ast, code);
    }
  }
}

void VariableResolverPass::VisitNode(VariableNode* node, ThingOfCode* code)
{
  if (node->isResolved)
  {
    return;
  }

  for (VariableDef* local : code->locals)
  {
    if (strcmp(local->name, node->name) == 0)
    {
      delete node->name;
      node->var = local;
      node->isResolved = true;
      return;
    }
  }

  for (VariableDef* param : code->params)
  {
    if (strcmp(param->name, node->name) == 0)
    {
      delete node->name;
      node->var = param;
      node->isResolved = true;
      return;
    }
  }

   if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(MemberAccessNode* node, ThingOfCode* code)
{
  // TODO

   if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(BreakNode* node, ThingOfCode* code)
{
   if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(ReturnNode* node, ThingOfCode* code)
{
  if (node->returnValue)
  {
    Dispatch(node->returnValue, code);
  }

   if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(UnaryOpNode* node, ThingOfCode* code)
{
  Dispatch(node->operand, code);

   if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(BinaryOpNode* node, ThingOfCode* code)
{
  Dispatch(node->left, code);
  Dispatch(node->right, code);

   if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(ConditionNode* node, ThingOfCode* code)
{
  Dispatch(node->left, code);
  Dispatch(node->right, code);

   if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(BranchNode* node, ThingOfCode* code)
{
  Dispatch(node->condition, code);
  Dispatch(node->thenCode, code);
  Dispatch(node->elseCode, code);

   if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(WhileNode* node, ThingOfCode* code)
{
  Dispatch(node->condition, code);
  Dispatch(node->loopBody, code);

   if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(ConstantNode<unsigned int>* node, ThingOfCode* code)
{
   if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(ConstantNode<int>* node, ThingOfCode* code)
{
   if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(ConstantNode<float>* node, ThingOfCode* code)
{
   if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(ConstantNode<bool>* node, ThingOfCode* code)
{
   if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(StringNode* node, ThingOfCode* code)
{
   if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(CallNode* node, ThingOfCode* code)
{
  for (ASTNode* paramNode : node->params)
  {
    Dispatch(paramNode, code);
  }

   if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(VariableAssignmentNode* node, ThingOfCode* code)
{
  Dispatch(node->variable, code);
  Dispatch(node->newValue, code);

   if (node->next) Dispatch(node->next, code);
}

void VariableResolverPass::VisitNode(ArrayInitNode* node, ThingOfCode* code)
{
  for (ASTNode* itemNode : node->items)
  {
    Dispatch(itemNode, code);
  }

   if (node->next) Dispatch(node->next, code);
}
