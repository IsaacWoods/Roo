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

std::string BreakNode::AsString()
{
  return "BreakNode";
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

std::string ReturnNode::AsString()
{
  if (returnValue)
  {
    return FormatString("Return(%s)", returnValue->AsString().c_str());
  }
  else
  {
    return "Return";
  }
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

std::string UnaryOpNode::AsString()
{
  switch (op)
  {
    case UnaryOpNode::Operator::POSITIVE:       return FormatString("+(%s)",  operand->AsString().c_str());
    case UnaryOpNode::Operator::NEGATIVE:       return FormatString("-(%s)",  operand->AsString().c_str());
    case UnaryOpNode::Operator::NEGATE:         return FormatString("~(%s)",  operand->AsString().c_str());
    case UnaryOpNode::Operator::LOGICAL_NOT:    return FormatString("!(%s)",  operand->AsString().c_str());
    case UnaryOpNode::Operator::TAKE_REFERENCE: return FormatString("&(%s)",  operand->AsString().c_str());
    case UnaryOpNode::Operator::PRE_INCREMENT:  return FormatString("++(%s)", operand->AsString().c_str());
    case UnaryOpNode::Operator::POST_INCREMENT: return FormatString("(%s)++", operand->AsString().c_str());
    case UnaryOpNode::Operator::PRE_DECREMENT:  return FormatString("--(%s)", operand->AsString().c_str());
    case UnaryOpNode::Operator::POST_DECREMENT: return FormatString("(%s)--", operand->AsString().c_str());
  }

  __builtin_unreachable();
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

std::string BinaryOpNode::AsString()
{
  switch (op)
  {
    case BinaryOpNode::Operator::ADD:         return FormatString("(%s)+(%s)" , left ->AsString().c_str(),
                                                                                right->AsString().c_str());
    case BinaryOpNode::Operator::SUBTRACT:    return FormatString("(%s)-(%s)" , left ->AsString().c_str(),
                                                                                right->AsString().c_str());
    case BinaryOpNode::Operator::MULTIPLY:    return FormatString("(%s)*(%s)" , left ->AsString().c_str(),
                                                                                right->AsString().c_str());
    case BinaryOpNode::Operator::DIVIDE:      return FormatString("(%s)/(%s)" , left ->AsString().c_str(),
                                                                                right->AsString().c_str());
    case BinaryOpNode::Operator::INDEX_ARRAY: return FormatString("(%s)[(%s)]", left ->AsString().c_str(),
                                                                                right->AsString().c_str());
  }

  __builtin_unreachable();
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

std::string VariableNode::AsString()
{
  if (isResolved)
  {
    return FormatString("Var(R): %s", var->name);
  }
  else
  {
    return FormatString("Var(UR): %s", name);
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

std::string ConditionNode::AsString()
{
  switch (condition)
  {
    case ConditionNode::Condition::EQUAL:                   return FormatString("(%s)==(%s)", left ->AsString().c_str(),
                                                                                              right->AsString().c_str());
    case ConditionNode::Condition::NOT_EQUAL:               return FormatString("(%s)!=(%s)", left ->AsString().c_str(),
                                                                                              right->AsString().c_str());
    case ConditionNode::Condition::LESS_THAN:               return FormatString("(%s)<(%s)" , left ->AsString().c_str(),
                                                                                              right->AsString().c_str());
    case ConditionNode::Condition::LESS_THAN_OR_EQUAL:      return FormatString("(%s)<=(%s)", left ->AsString().c_str(),
                                                                                              right->AsString().c_str());
    case ConditionNode::Condition::GREATER_THAN:            return FormatString("(%s)>(%s)" , left ->AsString().c_str(),
                                                                                              right->AsString().c_str());
    case ConditionNode::Condition::GREATER_THAN_OR_EQUAL:   return FormatString("(%s)>=(%s)", left ->AsString().c_str(),
                                                                                              right->AsString().c_str());
  }

  __builtin_unreachable();
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

std::string BranchNode::AsString()
{
  return FormatString("(%s) => (%s) | (%s)", condition->AsString().c_str(), thenCode->AsString().c_str(),
                                                                            elseCode->AsString().c_str());
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

std::string WhileNode::AsString()
{
  return FormatString("While (%s) => (%s)", condition->AsString().c_str(), loopBody->AsString().c_str());
}

template<>
std::string NumberNode<unsigned int>::AsString()
{
  return FormatString("#%uu", value);
}

template<>
std::string NumberNode<int>::AsString()
{
  return FormatString("#%d", value);
}

template<>
std::string NumberNode<float>::AsString()
{
  return FormatString("#%ff", value);
}

StringNode::StringNode(StringConstant* string)
  :ASTNode()
  ,string(string)
{
}

StringNode::~StringNode()
{
}

std::string StringNode::AsString()
{
  return FormatString("\"%s\"", string->string);
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

std::string CallNode::AsString()
{
  std::string paramString;
  for (auto it = params.begin();
       it < std::prev(params.end());
       it++)
  {
    ASTNode* param = *it;
    paramString += FormatString("(%s), ", param->AsString().c_str());
  }
  paramString += FormatString("(%s)", params.back()->AsString().c_str());

  if (isResolved)
  {
    return FormatString("Call(R) (%s) {%s}", resolvedFunction->name, paramString);
  }
  else
  {
    return FormatString("Call(UR) (%s) {%s}", name, paramString);
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

std::string VariableAssignmentNode::AsString()
{
  return FormatString("(%s) = (%s)", variable->AsString().c_str(), newValue->AsString().c_str());
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

std::string MemberAccessNode::AsString()
{
  if (isResolved)
  {
    return FormatString("(%s).(%s)(R)", parent->AsString().c_str(), member->name);
  }
  else
  {
    return FormatString("(%s).(%s)(UR)", parent->AsString().c_str(), child->AsString().c_str());
  }
}

ArrayInitNode::ArrayInitNode(const std::vector<ASTNode*>& items)
  :ASTNode()
  ,items(items)
{
}

std::string ArrayInitNode::AsString()
{
  std::string itemString;
  for (auto it = items.begin();
       it < std::prev(items.end());
       it++)
  {
    ASTNode* item = *it;
    itemString += FormatString("(%s), ", item->AsString().c_str());
  }
  itemString += FormatString("(%s)", items.back()->AsString().c_str());
  return itemString;
}
