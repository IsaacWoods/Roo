/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <ast.hpp>

ASTNode::ASTNode()
  :next(nullptr)
  ,prev(nullptr)
  ,type(nullptr)
  ,shouldFreeTypeRef(false)
  ,containingScope(nullptr)
{
}

ASTNode::~ASTNode()
{
  if (shouldFreeTypeRef)
  {
    Assert(type, "Nullptr type-ref marked as should be freed");
    delete type;
  }

  delete next;
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
    return FormatString("Var(R): %s", var->name.c_str());
  }
  else
  {
    return FormatString("Var(UR): %s", name);
  }
}

ConditionNode::ConditionNode(Condition condition, ASTNode* left, ASTNode* right)
  :ASTNode()
  ,condition(condition)
  ,left(left)
  ,right(right)
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

BranchNode::BranchNode(ASTNode* condition, ASTNode* thenCode, ASTNode* elseCode)
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

WhileNode::WhileNode(ASTNode* condition, ASTNode* loopBody)
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
std::string ConstantNode<unsigned int>::AsString()
{
  return FormatString("#%uu", value);
}

template<>
std::string ConstantNode<int>::AsString()
{
  return FormatString("#%d", value);
}

template<>
std::string ConstantNode<float>::AsString()
{
  return FormatString("#%ff", value);
}

template<>
std::string ConstantNode<bool>::AsString()
{
  return FormatString("#%s", (value ? "TRUE" : "FALSE"));
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
  return FormatString("\"%s\"", string->str.c_str());
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
    if (resolvedFunction->type == CodeThing::Type::FUNCTION)
    {
      return FormatString("Call(R) (%s) {%s}", dynamic_cast<FunctionThing*>(resolvedFunction)->name.c_str(), paramString.c_str());
    }
    else
    {
      return FormatString("Call(R) (%s) {%s}", GetTokenName(dynamic_cast<OperatorThing*>(resolvedFunction)->token), paramString.c_str());
    }
  }
  else
  {
    return FormatString("Call(UR) (%s) {%s}", name, paramString.c_str());
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
    return FormatString("(%s).(%s)(R)", parent->AsString().c_str(), member->name.c_str());
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

InfiniteLoopNode::InfiniteLoopNode(ASTNode* loopBody)
  :ASTNode()
  ,loopBody(loopBody)
{
}

InfiniteLoopNode::~InfiniteLoopNode()
{
  delete loopBody;
}

std::string InfiniteLoopNode::AsString()
{
  return FormatString("Loop(%s)", loopBody->AsString().c_str());
}

void AppendNode(ASTNode* parent, ASTNode* child)
{
  Assert(parent != child, "Can't append a node to itself");
  Assert(!(parent->next), "Parent already has next node");
  parent->next = child;
  child->prev = parent;
}

void AppendNodeOntoTail(ASTNode* parent, ASTNode* child)
{
  Assert(parent, "Tried to append onto nullptr node");
  ASTNode* tail = parent;

  while (tail->next)
  {
    tail = tail->next;
  }

  AppendNode(tail, child);
}

void ReplaceNode(CodeThing* code, ASTNode* oldNode, ASTNode* newNode)
{
  Assert(oldNode != newNode, "Can't replace old node with new node");

  if (oldNode->prev)
  {
    oldNode->prev->next = newNode;
    newNode->prev       = oldNode->prev;
  }
  else
  {
    Assert(code->ast == oldNode, "Floating node doesn't have prev pointer");
    code->ast     = newNode;
    newNode->prev = nullptr;
  }

  if (oldNode->next)
  {
    oldNode->next->prev = newNode;
    newNode->next       = oldNode->next;
  }

  oldNode->prev = nullptr;
  oldNode->next = nullptr;
}

void RemoveNode(CodeThing* code, ASTNode* node)
{
  if (node->prev)
  {
    node->prev->next = node->next;
    node->next->prev = node->prev;
  }
  else
  {
    Assert(code->ast == node, "Floating node doesn't have prev pointer");
    code->ast = node->next;
  }

  node->prev = nullptr;
  node->next = nullptr;
}
