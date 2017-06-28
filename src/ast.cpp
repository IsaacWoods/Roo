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
    Assert(type, "Nullptr type-ref marked as should be freed");
    delete type;
  }
}

ReturnNode::ReturnNode(ASTNode* returnValue)
  :ASTNode()
  ,returnValue(returnValue)
{
}

ReturnNode::~ReturnNode()
{
  delete returnValue;
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
  delete operand;
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
  delete left;
  delete right;
}

VariableNode::VariableNode(char* name)
  :ASTNode()
  ,name(name)
  ,isResolved(false)
{
}

VariableNode::VariableNode(VariableDef* variable)
  :ASTNode()
  ,var(variable)
  ,isResolved(true)
{
}

VariableNode::~VariableNode()
{
  if (!isResolved)
  {
    free(name);
  }
}

ConditionNode::ConditionNode(Condition condition, ASTNode* left, ASTNode* right/*, bool reverseOnJump*/)
  :ASTNode()
  ,condition(condition)
  ,left(left)
  ,right(right)
//  ,reverseOnJump(reverseOnJump)
{
}

ConditionNode::~ConditionNode()
{
  delete left;
  delete right;
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
  delete condition;
  delete thenCode;
  delete elseCode;
}

WhileNode::WhileNode(ConditionNode* condition, ASTNode* loopBody)
  :ASTNode()
  ,condition(condition)
  ,loopBody(loopBody)
{
}

WhileNode::~WhileNode()
{
  delete condition;
  delete loopBody;
}

StringNode::StringNode(StringConstant* string)
  :ASTNode()
  ,string(string)
{
}

StringNode::~StringNode()
{
}

CallNode::CallNode(char* name, const std::vector<ASTNode*>& params)
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
  delete variable;
  delete newValue;
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
  delete parent;
  
  if (!isResolved)
  {
    delete child;
  }
}

ArrayInitNode::ArrayInitNode(const std::vector<ASTNode*>& items)
  :ASTNode()
  ,items(items)
{
}
