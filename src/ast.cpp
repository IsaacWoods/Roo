/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <ast.hpp>

ASTNode::ASTNode()
  :next(nullptr)
  ,type(nullptr)
  ,shouldFreeTypeRef(false)
{
}

ASTNode::~ASTNode()
{
  if (shouldFreeTypeRef)
  {
    Free<type_ref*>(type);
  }
}

ReturnNode::ReturnNode(ASTNode* returnValue)
  :ASTNode()
  ,returnValue(returnValue)
{
}

ReturnNode::~ReturnNode()
{
  free(returnValue);
}

UnaryOpNode::UnaryOpNode(Operator op, ASTNode* operand)
  :ASTNode()
  ,op(op)
  ,operand(operand)
  ,resolvedOperator(nullptr)
{
}

UnaryOpNode::~UnaryOpNode()
{
  free(operand);
}

BinaryOpNode::BinaryOpNode(Operator op, ASTNode* left, ASTNode* right)
  :ASTNode()
  ,op(op)
  ,left(left)
  ,right(right)
  ,resolvedOperator(nullptr)
{
}

BinaryOpNode::~BinaryOpNode()
{
  free(left);
  free(right);
}

VariableNode::VariableNode(char* name)
  :ASTNode()
  ,name(name)
  ,isResolved(false)
{
}

VariableNode::~VariableNode()
{
  if (!isResolved)
  {
    free(name);
  }
}

ConditionNode::ConditionNode(Condition condition, ASTNode* left, ASTNode* right, bool reverseOnJump)
  :ASTNode()
  ,condition(condition)
  ,left(left)
  ,right(right)
  ,reverseOnJump(reverseOnJump)
{
}

ConditionNode::~ConditionNode()
{
  free(left);
  free(right);
}

BranchNode::BranchNode(ConditionNode* condition, ASTNode* thenCode, ASTNode* elseCode)
  :ASTNode()
  ,condition(condition)
  ,thenCode(thenCode)
  ,elseCode(elseCode)
{
}

BranchNode::~BranchNode()
{
  free(condition);
  free(thenCode);
  free(elseCode);
}

WhileNode::WhileNode(ConditionNode* condition, ASTNode* loopBody)
  :ASTNode()
  ,condition(condition)
  ,loopBody(loopBody)
{
}

WhileNode::~WhileNode()
{
  free(condition);
  free(loopBody);
}

StringNode::StringNode(char* string)
  :ASTNode()
  ,string(string)
{
}

StringNode::~StringNode()
{
}

CallNode::CallNode(char* name, vector<ASTNode*>& params)
  :ASTNode()
  ,name(name)
  ,isResolved(false)
  ,params(params)
{
}

CallNode::~CallNode()
{
  if (!isResolved)
  {
    free(name);
  }

  FreeVector<ASTNode*>(params);
}

VariableAssignmentNode::VariableAssignmentNode(ASTNode* variable, ASTNode* newValue, bool ignoreImmutability)
  :ASTNode()
  ,variable(variable)
  ,newValue(newValue)
  ,ignoreImmutability(ignoreImmutability)
{
}

VariableAssignmentNode::~VariableAssignmentNode()
{
  free(variable);
  free(newValue);
}

MemberAccessNode::MemberAccessNode(ASTNode* parent, ASTNode* child)
  :ASTNode()
  ,parent(parent)
  ,child(child)
  ,isResolved(false)
{
}

MemberAccessNode::~MemberAccessNode()
{
  free(parent);
  
  if (!isResolved)
  {
    free(child);
  }
}

ArrayInitNode::ArrayInitNode(vector<ASTNode*>& items)
  :ASTNode()
  ,items(items)
{
}

ArrayInitNode::~ArrayInitNode()
{
  FreeVector<ASTNode*>(items);
}
