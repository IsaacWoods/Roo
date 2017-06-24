/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <type_traits>
#include <typeinfo>
#include <vector>
#include <cstring>
#include <ir.hpp>
#include <error.hpp>

template<typename R, typename T>
struct ASTPass;

struct ASTNode
{
  ASTNode();
  virtual ~ASTNode();

  ASTNode*  next;
  type_ref* type;
  bool      shouldFreeTypeRef;    // TODO: eww
};

struct BreakNode : ASTNode
{
  BreakNode() : ASTNode() {}
  ~BreakNode() {}
};

struct ReturnNode : ASTNode
{
  ReturnNode(ASTNode* returnValue);
  ~ReturnNode();
  
  ASTNode* returnValue;
};

struct UnaryOpNode : ASTNode
{
  enum Operator
  {
    POSITIVE,         // +x
    NEGATIVE,         // -x
    NEGATE,           // ~x
    LOGICAL_NOT,      // !x
    TAKE_REFERENCE,   // &x
    PRE_INCREMENT,    // ++i
    POST_INCREMENT,   // i++
    PRE_DECREMENT,    // --i
    POST_DECREMENT    // i--
  };

  UnaryOpNode(Operator op, ASTNode* operand);
  ~UnaryOpNode();

  Operator        op;
  ASTNode*        operand;
  thing_of_code*  resolvedOperator;
};

struct BinaryOpNode : ASTNode
{
  enum Operator
  {
    ADD,
    SUBTRACT,
    MULTIPLY,
    DIVIDE,
    INDEX_ARRAY
  };

  BinaryOpNode(Operator op, ASTNode* left, ASTNode* right);
  ~BinaryOpNode();

  Operator        op;
  ASTNode*        left;
  ASTNode*        right;
  thing_of_code*  resolvedOperator;
};

struct VariableNode : ASTNode
{
  VariableNode(char* name);
  VariableNode(variable_def* variable);
  ~VariableNode();

  union
  {
    char*         name;
    variable_def* var;
  };
  bool isResolved;
};

struct ConditionNode : ASTNode
{
  enum Condition
  {
    EQUAL,
    NOT_EQUAL,
    LESS_THAN,
    LESS_THAN_OR_EQUAL,
    GREATER_THAN,
    GREATER_THAN_OR_EQUAL
  };

  ConditionNode(Condition condition, ASTNode* left, ASTNode* right, bool reverseOnJump = false);
  ~ConditionNode();

  Condition condition;
  ASTNode*  left;
  ASTNode*  right;
  bool      reverseOnJump;
};

struct BranchNode : ASTNode
{
  BranchNode(ConditionNode* condition, ASTNode* thenCode, ASTNode* elseCode);
  ~BranchNode();

  ConditionNode*  condition;
  ASTNode*        thenCode;
  ASTNode*        elseCode;
};

struct WhileNode : ASTNode
{
  WhileNode(ConditionNode* condition, ASTNode* loopBody);
  ~WhileNode();

  ConditionNode*  condition;
  ASTNode*        loopBody;
};

/*
 * T should only be:
 *    `unsigned int`
 *    `int`
 *    `float`
 */
template<typename T>
struct NumberNode : ASTNode
{
  NumberNode(T value)
    :value(value)
  {
    static_assert(std::is_same<T, unsigned int>::value ||
                  std::is_same<T, int>::value ||
                  std::is_same<T, float>::value, "NumberNode only supports unsigned int, int and float");
  }
  ~NumberNode() {}

  T value;
};

struct StringNode : ASTNode
{
  StringNode(string_constant* string);
  ~StringNode();

  string_constant* string;
};

struct CallNode : ASTNode
{
  CallNode(char* name, const std::vector<ASTNode*>& params);
  ~CallNode();

  union
  {
    char*               name;
    thing_of_code*      resolvedFunction;
  };
  bool                  isResolved;
  std::vector<ASTNode*> params;
};

struct VariableAssignmentNode : ASTNode
{
  VariableAssignmentNode(ASTNode* variable, ASTNode* newValue, bool ignoreImmutability);
  ~VariableAssignmentNode();

  ASTNode*  variable;   // Should either be a VariableNode or a MemberAccessNode
  ASTNode*  newValue;
  bool      ignoreImmutability;
};

struct MemberAccessNode : ASTNode
{
  MemberAccessNode(ASTNode* parent, ASTNode* child);
  ~MemberAccessNode();

  ASTNode*        parent;
  union
  {
    ASTNode*      child;
    variable_def* member;
  };
  bool            isResolved;
};

struct ArrayInitNode : ASTNode
{
  ArrayInitNode(const std::vector<ASTNode*>& items);

  std::vector<ASTNode*> items;
};

template<typename T>
bool IsNodeOfType(ASTNode* node)
{
  static const char* TYPE_NAME = typeid(T).name();

  Assert(node, "Tried to find type of nullptr node");
  return (strcmp(typeid(*node).name(), TYPE_NAME) == 0);
}

template<typename R, typename T>
struct ASTPass
{
  enum IteratePolicy
  {
    NODE_FIRST,
    CHILDREN_FIRST
  } iteratePolicy;

  ASTPass(IteratePolicy iteratePolicy)
    :iteratePolicy(iteratePolicy)
  {
  }

  // TODO: specify whether this pass should error on a pass not being supplied, and provide this as a base case
  virtual R VisitNode(BreakNode* node               , T* state = nullptr) = 0;
  virtual R VisitNode(ReturnNode* node              , T* state = nullptr) = 0;
  virtual R VisitNode(UnaryOpNode* node             , T* state = nullptr) = 0;
  virtual R VisitNode(BinaryOpNode* node            , T* state = nullptr) = 0;
  virtual R VisitNode(VariableNode* node            , T* state = nullptr) = 0;
  virtual R VisitNode(ConditionNode* node           , T* state = nullptr) = 0;
  virtual R VisitNode(BranchNode* node              , T* state = nullptr) = 0;
  virtual R VisitNode(WhileNode* node               , T* state = nullptr) = 0;
  virtual R VisitNode(NumberNode<unsigned int>* node, T* state = nullptr) = 0;
  virtual R VisitNode(NumberNode<int>* node         , T* state = nullptr) = 0;
  virtual R VisitNode(NumberNode<float>* node       , T* state = nullptr) = 0;
  virtual R VisitNode(StringNode* node              , T* state = nullptr) = 0;
  virtual R VisitNode(CallNode* node                , T* state = nullptr) = 0;
  virtual R VisitNode(VariableAssignmentNode* node  , T* state = nullptr) = 0;
  virtual R VisitNode(MemberAccessNode* node        , T* state = nullptr) = 0;
  virtual R VisitNode(ArrayInitNode* node           , T* state = nullptr) = 0;

  /*
   * This is required since we can't use a normal visitor pattern (because we can't template a virtual function,
   * so can't return whatever type we want). So this should dynamically dispatch based upon the subclass.
   */
  R DispatchNode(ASTNode* node, T* state = nullptr)
  {
    Assert(node, "Tried to dispatch on a nullptr node");

    /*
     * Poor man's dynamic dispatch - we compare the type-identifier of the node with each node type, then
     * cast and call the correct virtual function.
     */
    #define DISPATCH_NODE(nodeType)\
      if (strcmp(typeid(*node).name(), typeid(nodeType).name()) == 0)\
      {\
        return VisitNode(reinterpret_cast<nodeType*>(node), state);\
      }

         DISPATCH_NODE(BreakNode)
    else DISPATCH_NODE(ReturnNode)
    else DISPATCH_NODE(UnaryOpNode)
    else DISPATCH_NODE(BinaryOpNode)
    else DISPATCH_NODE(VariableNode)
    else DISPATCH_NODE(ConditionNode)
    else DISPATCH_NODE(BranchNode)
    else DISPATCH_NODE(WhileNode)
    else DISPATCH_NODE(NumberNode<unsigned int>)
    else DISPATCH_NODE(NumberNode<int>)
    else DISPATCH_NODE(NumberNode<float>)
    else DISPATCH_NODE(StringNode)
    else DISPATCH_NODE(CallNode)
    else DISPATCH_NODE(VariableAssignmentNode)
    else DISPATCH_NODE(MemberAccessNode)
    else DISPATCH_NODE(ArrayInitNode)
    else
    {
      RaiseError(ICE_UNHANDLED_NODE_TYPE, "DispatchNode", typeid(*node).name());
    }

    __builtin_unreachable();
  }
};
