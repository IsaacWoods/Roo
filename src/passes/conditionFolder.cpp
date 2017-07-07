/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <passes/passes.hpp>

void ConditionFolderPass::Apply(ParseResult& parse)
{
  for (ThingOfCode* code : parse.codeThings)
  {
   if (!(code->attribs.isPrototype) && code->ast)
   {
    (void)Dispatch(code->ast, code);
   }
  }
}

/*
 * This returns the result of evaluating the condition, if it can be.
 * This should only be passed ones that we know we can evaluate at compile-time.
 */
bool ConditionFolderPass::VisitNode(ConditionNode* node, ThingOfCode* code)
{
  // TODO
  if (node->next) (void)Dispatch(node->next, code);
  return false;
}

bool ConditionFolderPass::VisitNode(BranchNode* node, ThingOfCode* code)
{
  Dispatch(node->thenCode, code);
  
  if (node->elseCode)
  {
    Dispatch(node->elseCode, code);
  }

  ASTNode* newNode = node;
  if(IsNodeOfType<ConstantNode<bool>>(node->condition))
  {
    bool value = dynamic_cast<ConstantNode<bool>*>(node->condition)->value;

    if (value)
    {
      // Replace the branch with the `then`
      ReplaceNode(node, node->thenCode);
      newNode = node->thenCode;
      node->thenCode = nullptr;
    }
    else
    {
      // Replace the branch with the `else`
      ReplaceNode(node, node->elseCode);
      newNode = node->elseCode;
      node->elseCode = nullptr;
    }

    delete node;
  }
  else if (IsNodeOfType<ConditionNode>(node->condition))
  {
    // TODO: check if both sides are constant and evaluate the condition. Replace with correct side of branch
  }
  else
  {
    RaiseError(ICE_UNHANDLED_NODE_TYPE, "ConditionFolderPass::BranchNode", node->condition->AsString());
  }

  if (newNode->next) (void)Dispatch(newNode->next, code);
  return false;
}

bool ConditionFolderPass::VisitNode(WhileNode* node, ThingOfCode* code)
{
  ASTNode* nextNode = node->next;
  if (IsNodeOfType<ConstantNode<bool>>(node->condition))
  {
    bool value = dynamic_cast<ConstantNode<bool>*>(node->condition)->value;

    if (value)
    {
      // Turn the WhileNode into an InfiniteLoopNode
      (void)Dispatch(node->loopBody, code);
      nextNode = node->next;
      ReplaceNode(node, new InfiniteLoopNode(node->loopBody));
      node->loopBody = nullptr;
    }
    else
    {
      // If we never even do one iteration, there's no point generating code for the loop
      nextNode = node->next;
      RemoveNode(code, node);
    }

    delete node;
  }
  else if (IsNodeOfType<ConditionNode>(node->condition))
  {
    // TODO
  }
  else
  {
    RaiseError(ICE_UNHANDLED_NODE_TYPE, "ConditionFolderPass::WhileNode", node->condition->AsString());
  }

  if (nextNode) (void)Dispatch(nextNode, code);
  return false;
}

bool ConditionFolderPass::VisitNode(BreakNode* node, ThingOfCode* code)
{
  if (node->next) (void)Dispatch(node->next, code);
  return false;
}

bool ConditionFolderPass::VisitNode(ReturnNode* node, ThingOfCode* code)
{
  if (node->next) (void)Dispatch(node->next, code);
  return false;
}

bool ConditionFolderPass::VisitNode(UnaryOpNode* node, ThingOfCode* code)
{
  if (node->next) (void)Dispatch(node->next, code);
  return false;
}

bool ConditionFolderPass::VisitNode(BinaryOpNode* node, ThingOfCode* code)
{
  if (node->next) (void)Dispatch(node->next, code);
  return false;
}

bool ConditionFolderPass::VisitNode(VariableNode* node, ThingOfCode* code)
{
  if (node->next) (void)Dispatch(node->next, code);
  return false;
}

bool ConditionFolderPass::VisitNode(ConstantNode<unsigned int>* node, ThingOfCode* code)
{
  if (node->next) (void)Dispatch(node->next, code);
  return false;
}

bool ConditionFolderPass::VisitNode(ConstantNode<int>* node, ThingOfCode* code)
{
  if (node->next) (void)Dispatch(node->next, code);
  return false;
}

bool ConditionFolderPass::VisitNode(ConstantNode<float>* node, ThingOfCode* code)
{
  if (node->next) (void)Dispatch(node->next, code);
  return false;
}

bool ConditionFolderPass::VisitNode(ConstantNode<bool>* node, ThingOfCode* code)
{
  if (node->next) (void)Dispatch(node->next, code);
  return false;
}

bool ConditionFolderPass::VisitNode(StringNode* node, ThingOfCode* code)
{
  if (node->next) (void)Dispatch(node->next, code);
  return false;
}

bool ConditionFolderPass::VisitNode(CallNode* node, ThingOfCode* code)
{
  if (node->next) (void)Dispatch(node->next, code);
  return false;
}

bool ConditionFolderPass::VisitNode(VariableAssignmentNode* node, ThingOfCode* code)
{
  if (node->next) (void)Dispatch(node->next, code);
  return false;
}

bool ConditionFolderPass::VisitNode(MemberAccessNode* node, ThingOfCode* code)
{
  if (node->next) (void)Dispatch(node->next, code);
  return false;
}

bool ConditionFolderPass::VisitNode(ArrayInitNode* node, ThingOfCode* code)
{
  if (node->next) (void)Dispatch(node->next, code);
  return false;
}

bool ConditionFolderPass::VisitNode(InfiniteLoopNode* node, ThingOfCode* code)
{
  (void)Dispatch(node->loopBody, code);
  if (node->next) (void)Dispatch(node->next, code);
  return false;
}
