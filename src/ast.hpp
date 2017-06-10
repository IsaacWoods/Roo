/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <type_traits>
#include <vector.hpp>
#include <ir.hpp>

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
    POSITIVE,
    NEGATIVE,
    NEGATE,
    TAKE_REFERENCE,
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

  ConditionNode(Condition condition, ASTNode* left, ASTNode* right, bool reverseOnJump);
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
  StringNode(char* string);
  ~StringNode();

  char* string;
};

struct CallNode : ASTNode
{
  CallNode(char* name, vector<ASTNode*>& params);
  ~CallNode();

  union
  {
    char*           name;
    thing_of_code*  resolvedFunction;
  };
  bool              isResolved;
  vector<ASTNode*>  params;
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
  ArrayInitNode(vector<ASTNode*>& items);
  ~ArrayInitNode();

  vector<ASTNode*> items;
};
