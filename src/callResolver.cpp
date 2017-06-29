/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <callResolver.hpp>

void CallResolverPass::Apply(ParseResult& parse)
{
  for (ThingOfCode* thing : parse.codeThings)
  {
    if (!(thing->attribs.isPrototype) && thing->ast)
    {
      Dispatch(thing->ast, &parse);
    }
  }
}

void CallResolverPass::VisitNode(CallNode* node, ParseResult* parse)
{
  Assert(!node->isResolved, "Tried to resolve call that has already been resolved");

  for (ThingOfCode* thing : parse->codeThings)
  {
    if (thing->type != ThingOfCode::Type::FUNCTION)
    {
      continue;
    }

    if (strcmp(node->name, thing->name) != 0)
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
    break;
NotCorrectThing:
    continue;
  }

  for (ASTNode* paramNode : node->params)
  {
    Dispatch(paramNode, parse);
  }

   if (node->next) Dispatch(node->next, parse);
}

void CallResolverPass::VisitNode(BreakNode* node, ParseResult* parse)
{
   if (node->next) Dispatch(node->next, parse);
}

void CallResolverPass::VisitNode(ReturnNode* node, ParseResult* parse)
{
  if (node->returnValue)
  {
    Dispatch(node->returnValue, parse);
  }

   if (node->next) Dispatch(node->next, parse);
}

void CallResolverPass::VisitNode(UnaryOpNode* node, ParseResult* parse)
{
  Dispatch(node->operand, parse);

   if (node->next) Dispatch(node->next, parse);
}

void CallResolverPass::VisitNode(BinaryOpNode* node, ParseResult* parse)
{
  Dispatch(node->left, parse);
  Dispatch(node->right, parse);

   if (node->next) Dispatch(node->next, parse);
}

void CallResolverPass::VisitNode(VariableNode* node, ParseResult* parse)
{
   if (node->next) Dispatch(node->next, parse);
}

void CallResolverPass::VisitNode(ConditionNode* node, ParseResult* parse)
{
  Dispatch(node->left, parse);
  Dispatch(node->right, parse);

   if (node->next) Dispatch(node->next, parse);
}

void CallResolverPass::VisitNode(BranchNode* node, ParseResult* parse)
{
  Dispatch(node->condition, parse);
  Dispatch(node->thenCode, parse);
  Dispatch(node->elseCode, parse);

   if (node->next) Dispatch(node->next, parse);
}

void CallResolverPass::VisitNode(WhileNode* node, ParseResult* parse)
{
  Dispatch(node->condition, parse);
  Dispatch(node->loopBody, parse);

   if (node->next) Dispatch(node->next, parse);
}

void CallResolverPass::VisitNode(NumberNode<unsigned int>* node, ParseResult* parse)
{
   if (node->next) Dispatch(node->next, parse);
}

void CallResolverPass::VisitNode(NumberNode<int>* node, ParseResult* parse)
{
   if (node->next) Dispatch(node->next, parse);
}

void CallResolverPass::VisitNode(NumberNode<float>* node, ParseResult* parse)
{
   if (node->next) Dispatch(node->next, parse);
}

void CallResolverPass::VisitNode(StringNode* node, ParseResult* parse)
{
   if (node->next) Dispatch(node->next, parse);
}

void CallResolverPass::VisitNode(VariableAssignmentNode* node, ParseResult* parse)
{
  Dispatch(node->variable, parse);
  Dispatch(node->newValue, parse);

   if (node->next) Dispatch(node->next, parse);
}

void CallResolverPass::VisitNode(MemberAccessNode* node, ParseResult* parse)
{
   if (node->next) Dispatch(node->next, parse);
}

void CallResolverPass::VisitNode(ArrayInitNode* node, ParseResult* parse)
{
  for (ASTNode* itemNode : node->items)
  {
    Dispatch(itemNode, parse);
  }

   if (node->next) Dispatch(node->next, parse);
}
