/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <passes/passes.hpp>
#include <target.hpp>

void ScopeResolverPass::Apply(ParseResult& parse, TargetMachine* /*target*/)
{
  for (CodeThing* code : parse.codeThings)
  {
    if (!(code->attribs.isPrototype) && code->ast)
    {
      Dispatch(code->ast, code);
    }
  }
}

void ScopeResolverPass::VisitNode(BreakNode* node, CodeThing* code)
{
  if (!(node->containingScope))
  {
    node->containingScope = node->prev->containingScope;
  }

  if (node->next) Dispatch(node->next, code);
}

void ScopeResolverPass::VisitNode(ReturnNode* node, CodeThing* code)
{
  if (!(node->containingScope))
  {
    node->containingScope = node->prev->containingScope;
  }

  if (!(node->returnValue->containingScope))
  {
    node->returnValue->containingScope = node->containingScope;
  }

  Dispatch(node->returnValue, code);
  if (node->next) Dispatch(node->next, code);
}

void ScopeResolverPass::VisitNode(UnaryOpNode* node, CodeThing* code)
{
  if (!(node->containingScope))
  {
    node->containingScope = node->prev->containingScope;
  }

  if (!(node->operand->containingScope))
  {
    node->operand->containingScope = node->containingScope;
  }

  Dispatch(node->operand, code);
  if (node->next) Dispatch(node->next, code);
}

void ScopeResolverPass::VisitNode(BinaryOpNode* node, CodeThing* code)
{
  if (!(node->containingScope))
  {
    node->containingScope = node->prev->containingScope;
  }

  if (!(node->left->containingScope))
  {
    node->left->containingScope = node->containingScope;
  }

  if (!(node->right->containingScope))
  {
    node->right->containingScope = node->containingScope;
  }

  Dispatch(node->left, code);
  Dispatch(node->right, code);
  if (node->next) Dispatch(node->next, code);
}

void ScopeResolverPass::VisitNode(VariableNode* node, CodeThing* code)
{
  if (!(node->containingScope))
  {
    node->containingScope = node->prev->containingScope;
  }

  if (node->next) Dispatch(node->next, code);
}

void ScopeResolverPass::VisitNode(ConditionNode* node, CodeThing* code)
{
  if (!(node->containingScope))
  {
    node->containingScope = node->prev->containingScope;
  }

  if (!(node->left->containingScope))
  {
    node->left->containingScope = node->containingScope;
  }

  if (!(node->right->containingScope))
  {
    node->right->containingScope = node->containingScope;
  }

  Dispatch(node->left, code);
  Dispatch(node->right, code);
  if (node->next) Dispatch(node->next, code);
}

void ScopeResolverPass::VisitNode(BranchNode* node, CodeThing* code)
{
  if (!(node->containingScope))
  {
    node->containingScope = node->prev->containingScope;
  }

  if (!(node->condition->containingScope))
  {
    node->condition->containingScope = node->containingScope;
  }

  if (!(node->thenCode->containingScope))
  {
    node->thenCode->containingScope = node->containingScope;
  }

  if (node->elseCode && !(node->elseCode->containingScope))
  {
    node->elseCode->containingScope = node->containingScope;
  }

  Dispatch(node->condition, code);
  Dispatch(node->thenCode, code);
  if (node->elseCode) Dispatch(node->elseCode, code);
  if (node->next)     Dispatch(node->next, code);
}

void ScopeResolverPass::VisitNode(WhileNode* node, CodeThing* code)
{
  if (!(node->containingScope))
  {
    node->containingScope = node->prev->containingScope;
  }

  if (!(node->condition->containingScope))
  {
    node->condition->containingScope = node->containingScope;
  }

  if (!(node->loopBody->containingScope))
  {
    node->loopBody->containingScope = node->containingScope;
  }

  Dispatch(node->condition, code);
  Dispatch(node->loopBody, code);
  if (node->next) Dispatch(node->next, code);
}

void ScopeResolverPass::VisitNode(ConstantNode<unsigned int>* node, CodeThing* code)
{
  if (!(node->containingScope))
  {
    node->containingScope = node->prev->containingScope;
  }

  if (node->next) Dispatch(node->next, code);
}

void ScopeResolverPass::VisitNode(ConstantNode<int>* node, CodeThing* code)
{
  if (!(node->containingScope))
  {
    node->containingScope = node->prev->containingScope;
  }

  if (node->next) Dispatch(node->next, code);
}

void ScopeResolverPass::VisitNode(ConstantNode<float>* node, CodeThing* code)
{
  if (!(node->containingScope))
  {
    node->containingScope = node->prev->containingScope;
  }

  if (node->next) Dispatch(node->next, code);
}

void ScopeResolverPass::VisitNode(ConstantNode<bool>* node, CodeThing* code)
{
  if (!(node->containingScope))
  {
    node->containingScope = node->prev->containingScope;
  }

  if (node->next) Dispatch(node->next, code);
}

void ScopeResolverPass::VisitNode(StringNode* node, CodeThing* code)
{
  if (!(node->containingScope))
  {
    node->containingScope = node->prev->containingScope;
  }

  if (node->next) Dispatch(node->next, code);
}

void ScopeResolverPass::VisitNode(CallNode* node, CodeThing* code)
{
  if (!(node->containingScope))
  {
    node->containingScope = node->prev->containingScope;
  }

  for (ASTNode* param : node->params)
  {
    if (!(param->containingScope))
    {
      param->containingScope = node->containingScope;
    }
  }

  if (node->next) Dispatch(node->next, code);
}

void ScopeResolverPass::VisitNode(VariableAssignmentNode* node, CodeThing* code)
{
  if (!(node->containingScope))
  {
    node->containingScope = node->prev->containingScope;
  }

  if (!(node->variable->containingScope))
  {
    node->variable->containingScope = node->containingScope;
  }

  if (!(node->newValue->containingScope))
  {
    node->newValue->containingScope = node->containingScope;
  }

  Dispatch(node->variable, code);
  Dispatch(node->newValue, code);
  if (node->next) Dispatch(node->next, code);
}

void ScopeResolverPass::VisitNode(MemberAccessNode* node, CodeThing* code)
{
  if (!(node->containingScope))
  {
    node->containingScope = node->prev->containingScope;
  }

  if (!(node->parent->containingScope))
  {
    node->parent->containingScope = node->containingScope;
  }

  if (!(node->isResolved))
  {
    if (!(node->child->containingScope))
    {
      node->child->containingScope = node->containingScope;
    }

    Dispatch(node->child, code);
  }

  Dispatch(node->parent, code);
  if (node->next) Dispatch(node->next, code);
}

void ScopeResolverPass::VisitNode(ArrayInitNode* node, CodeThing* code)
{
  if (!(node->containingScope))
  {
    node->containingScope = node->prev->containingScope;
  }

  for (ASTNode* item : node->items)
  {
    if (!(item->containingScope))
    {
      item->containingScope = node->containingScope;
    }

    Dispatch(item, code);
  }

  if (node->next) Dispatch(node->next, code);
}

void ScopeResolverPass::VisitNode(InfiniteLoopNode* node, CodeThing* code)
{
  if (!(node->containingScope))
  {
    node->containingScope = node->prev->containingScope;
  }

  if (!(node->loopBody->containingScope))
  {
    node->loopBody->containingScope = node->containingScope;
  }

  Dispatch(node->loopBody, code);
  if (node->next) Dispatch(node->next, code);
}

void ScopeResolverPass::VisitNode(ConstructNode* node, CodeThing* code)
{
  if (!(node->containingScope))
  {
    node->containingScope = node->prev->containingScope;
  }

  for (auto* item : node->items)
  {
    if (!(item->containingScope))
    {
      item->containingScope = node->containingScope;
    }

    Dispatch(item, code);
  }

  if (node->next) Dispatch(node->next, code);
}
