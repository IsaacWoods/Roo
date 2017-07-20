/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <string>
#include <vector>
#include <type_traits>
#include <typeinfo>
#include <cstring>
#include <ir.hpp>
#include <error.hpp>

template<typename R, typename T>
struct ASTPass;

struct ASTNode
{
  ASTNode();
  virtual ~ASTNode();

  virtual std::string AsString() = 0;

  /*
   * XXX: These should not be set manually, as it is very easy to end up with a malformed AST.
   * Instead, use functions like `AppendNode` or `ReplaceNode`
   */
  ASTNode*  next;
  ASTNode*  prev;

  TypeRef*  type;
  bool      shouldFreeTypeRef;    // TODO: eww

  /*
   * This is the scope this node operates in. The node can only access variables within this scope and its parent
   * scopes.
   */
  ScopeDef* containingScope;
};

/*
 * Functions for organising and mutating the AST.
 * NOTE(Isaac): These never change the memory allocated for the nodes (i.e. they must still be manually freed)
 */
void AppendNode(ASTNode* parent, ASTNode* child);
void AppendNodeOntoTail(ASTNode* parent, ASTNode* child);
void ReplaceNode(CodeThing* code, ASTNode* oldNode, ASTNode* newNode);
void RemoveNode(CodeThing* code, ASTNode* node);

struct BreakNode : ASTNode
{
  BreakNode() : ASTNode() {}
  ~BreakNode() {}

  std::string AsString();
};

struct ReturnNode : ASTNode
{
  ReturnNode(ASTNode* returnValue);
  ~ReturnNode();

  std::string AsString();
  
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

  std::string AsString();

  Operator    op;
  ASTNode*    operand;
  CodeThing*  resolvedOperator;
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

  std::string AsString();

  Operator    op;
  ASTNode*    left;
  ASTNode*    right;
  CodeThing*  resolvedOperator;
};

struct VariableNode : ASTNode
{
  VariableNode(char* name);
  VariableNode(VariableDef* variable);
  ~VariableNode();

  std::string AsString();

  union
  {
    char*         name;
    VariableDef*  var;
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

  std::string AsString();

  ConditionNode(Condition condition, ASTNode* left, ASTNode* right);
  ~ConditionNode();

  Condition condition;
  ASTNode*  left;
  ASTNode*  right;
};

struct BranchNode : ASTNode
{
  BranchNode(ASTNode* condition, ASTNode* thenCode, ASTNode* elseCode);
  ~BranchNode();

  std::string AsString();

  ASTNode* condition;
  ASTNode* thenCode;
  ASTNode* elseCode;
};

struct WhileNode : ASTNode
{
  WhileNode(ASTNode* condition, ASTNode* loopBody);
  ~WhileNode();

  std::string AsString();

  ASTNode* condition;
  ASTNode* loopBody;
};

/*
 * T should only be:
 *    `unsigned int`
 *    `int`
 *    `float`
 *    `bool`
 */
template<typename T>
struct ConstantNode : ASTNode
{
  ConstantNode(T value)
    :value(value)
  {
    static_assert(std::is_same<T, unsigned int>::value  ||
                  std::is_same<T, int>::value           ||
                  std::is_same<T, float>::value         ||
                  std::is_same<T, bool>::value, "NumberNode only supports unsigned int, int, float and bool");
  }
  ~ConstantNode() {}

  std::string AsString();

  T value;
};

struct StringNode : ASTNode
{
  StringNode(StringConstant* string);
  ~StringNode();

  std::string AsString();

  StringConstant* string;
};

struct CallNode : ASTNode
{
  CallNode(char* name, const std::vector<ASTNode*>& params);
  ~CallNode();

  std::string AsString();

  union
  {
    char*               name;
    CodeThing*          resolvedFunction;
  };
  bool                  isResolved;
  std::vector<ASTNode*> params;
};

struct VariableAssignmentNode : ASTNode
{
  VariableAssignmentNode(ASTNode* variable, ASTNode* newValue, bool ignoreImmutability);
  ~VariableAssignmentNode();

  std::string AsString();

  ASTNode*  variable;   // Should either be a VariableNode or a MemberAccessNode
  ASTNode*  newValue;
  bool      ignoreImmutability;
};

struct MemberAccessNode : ASTNode
{
  MemberAccessNode(ASTNode* parent, ASTNode* child);
  ~MemberAccessNode();

  std::string AsString();

  ASTNode*        parent;
  union
  {
    ASTNode*      child;
    VariableDef*  member;
  };
  bool            isResolved;
};

struct ArrayInitNode : ASTNode
{
  ArrayInitNode(const std::vector<ASTNode*>& items);
  ~ArrayInitNode();

  std::string AsString();

  std::vector<ASTNode*> items;
};

struct InfiniteLoopNode : ASTNode
{
  InfiniteLoopNode(ASTNode* loopBody);
  ~InfiniteLoopNode();

  std::string AsString();

  ASTNode* loopBody;
};

/*
 * A ConstructNode describes the construction of a value of a user-defined type, such as in:
 * `color : Color(255, 0, 255)`
 */
struct ConstructNode : ASTNode
{
  ConstructNode(const std::string& typeName, const std::vector<ASTNode*>& items);
  ~ConstructNode();

  std::string AsString();

  std::string           typeName;
  TypeDef*              type;
  bool                  isTypeResolved;
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
  ASTPass() = default;

  virtual R VisitNode(BreakNode*                    , T* = nullptr) = 0;
  virtual R VisitNode(ReturnNode*                   , T* = nullptr) = 0;
  virtual R VisitNode(UnaryOpNode*                  , T* = nullptr) = 0;
  virtual R VisitNode(BinaryOpNode*                 , T* = nullptr) = 0;
  virtual R VisitNode(VariableNode*                 , T* = nullptr) = 0;
  virtual R VisitNode(ConditionNode*                , T* = nullptr) = 0;
  virtual R VisitNode(BranchNode*                   , T* = nullptr) = 0;
  virtual R VisitNode(WhileNode*                    , T* = nullptr) = 0;
  virtual R VisitNode(ConstantNode<unsigned int>*   , T* = nullptr) = 0;
  virtual R VisitNode(ConstantNode<int>*            , T* = nullptr) = 0;
  virtual R VisitNode(ConstantNode<float>*          , T* = nullptr) = 0;
  virtual R VisitNode(ConstantNode<bool>*           , T* = nullptr) = 0;
  virtual R VisitNode(StringNode*                   , T* = nullptr) = 0;
  virtual R VisitNode(CallNode*                     , T* = nullptr) = 0;
  virtual R VisitNode(VariableAssignmentNode*       , T* = nullptr) = 0;
  virtual R VisitNode(MemberAccessNode*             , T* = nullptr) = 0;
  virtual R VisitNode(ArrayInitNode*                , T* = nullptr) = 0;
  virtual R VisitNode(InfiniteLoopNode*             , T* = nullptr) = 0;
  virtual R VisitNode(ConstructNode*                , T* = nullptr) = 0;

  virtual void Apply(ParseResult& parse) = 0;

  /*
   * This is required since we can't use a normal visitor pattern (because we can't template a virtual function,
   * so can't return whatever type we want). So this should dynamically dispatch based upon the subclass.
   */
  R Dispatch(ASTNode* node, T* state = nullptr)
  {
    Assert(node, "Tried to dispatch on a nullptr node");

    /*
     * Poor man's dynamic dispatch - we compare the type-identifier of the node with each node type, then
     * cast and call the correct virtual function.
     *
     * XXX: If this is too slow, I guess we could provide a virtual function to get a id number for each node
     *      which could then by dispatched from (but that's got the same problems as before)
     */
    #define DISPATCH(nodeType)\
      if (strcmp(typeid(*node).name(), typeid(nodeType).name()) == 0)\
      {\
        return VisitNode(reinterpret_cast<nodeType*>(node), state);\
      }

         DISPATCH(BreakNode)
    else DISPATCH(ReturnNode)
    else DISPATCH(UnaryOpNode)
    else DISPATCH(BinaryOpNode)
    else DISPATCH(VariableNode)
    else DISPATCH(ConditionNode)
    else DISPATCH(BranchNode)
    else DISPATCH(WhileNode)
    else DISPATCH(ConstantNode<unsigned int>)
    else DISPATCH(ConstantNode<int>)
    else DISPATCH(ConstantNode<float>)
    else DISPATCH(ConstantNode<bool>)
    else DISPATCH(StringNode)
    else DISPATCH(CallNode)
    else DISPATCH(VariableAssignmentNode)
    else DISPATCH(MemberAccessNode)
    else DISPATCH(ArrayInitNode)
    else DISPATCH(InfiniteLoopNode)
    else DISPATCH(ConstructNode)
    else
    {
      RaiseError(ICE_UNHANDLED_NODE_TYPE, "DispatchNode", typeid(*node).name());
    }
    #undef DISPATCH

    __builtin_unreachable();
  }
};
