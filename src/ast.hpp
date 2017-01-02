/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <common.hpp>
#include <ir.hpp>

struct node;
typedef void(*ast_passlet)(parse_result&, function_def*, node*);

enum node_type
{
  BREAK_NODE,             // Nothing
  RETURN_NODE,            // Nothing or `expression` (can be nullptr)
  BINARY_OP_NODE,         // `binary_op_node_part`
  PREFIX_OP_NODE,         // `prefix_op_node_part`
  VARIABLE_NODE,          // `variable`
  CONDITION_NODE,         // `condition_node_part`
  IF_NODE,                // `if_node_part`
  WHILE_NODE,             // `while_node_part`
  NUMBER_CONSTANT_NODE,   // `number_constant_part`
  STRING_CONSTANT_NODE,   // `string_constant*`
  FUNCTION_CALL_NODE,     // `function_call_part
  VARIABLE_ASSIGN_NODE,   // `variable_assign_part`
  MEMBER_ACCESS_NODE,     // `member_access_part`

  NUM_AST_NODES
};

/*
 * Binary operations:
 *     TOKEN_PLUS
 *     TOKEN_MINUS
 *     TOKEN_ASTERIX
 *     TOKEN_SLASH
 *     TOKEN_DOUBLE_PLUS (no `right`)
 *     TOKEN_DOUBLE_MINUS (no `right`)
 */
struct binary_op_node_part
{
  token_type  op;
  node*       left;
  node*       right;
};

/*
 * Prefix operations:
 *    TOKEN_PLUS
 *    TOKEN_MINUS
 *    TOKEN_BANG
 *    TOKEN_TILDE
 */
struct prefix_op_node_part
{
  token_type  op;
  node*       right;
};

struct variable_node_part
{
  union
  {
    char*         name;
    variable_def* var;
  };
  bool isResolved;
};

/*
 * Conditions:
 *    TOKEN_EQUALS_EQUALS
 *    TOKEN_BANG_EQUALS
 *    TOKEN_GREATER_THAN
 *    TOKEN_GREATER_THAN_EQUAL_TO
 *    TOKEN_LESS_THAN
 *    TOKEN_LESS_THAN_EQUAL_TO
 */
struct condition_node_part
{
  token_type  condition;
  node*       left;
  node*       right;
  bool        reverseOnJump;
};

struct if_node_part
{
  node* condition;
  node* thenCode;
  node* elseCode;
};

struct while_node_part
{
  node* condition;
  node* code;
};

struct number_constant_part
{
  enum constant_type
  {
    CONSTANT_TYPE_INT,
    CONSTANT_TYPE_FLOAT
  } type;

  union
  {
    int   asInt;
    float asFloat;
  };
};

struct function_call_part
{
  union
  {
    char*         name;
    function_def* function;
  };

  bool          isResolved;
  vector<node*> params;
};

struct variable_assign_part
{
  /*
   * NOTE(Isaac): May be either a MEMBER_ACCESS_NODE or a VARIABLE_NODE
   */
  node* variable;
  node* newValue;
  bool  ignoreImmutability;
};

struct member_access_part
{
  /*
   * NOTE(Isaac): Parent may either be another MEMBER_ACCESS_NODE, or a VARIABLE_NODE
   */
  node*   parent;
  bool    isResolved;

  union
  {
    char*         name;
    variable_def* member;
  };
};

struct node
{
  node_type type;
  node*     next;

  union
  {
    node*                   expression;
    binary_op_node_part     binaryOp;
    prefix_op_node_part     prefixOp;
    variable_node_part      variable;
    condition_node_part     condition;
    if_node_part            ifThing;
    while_node_part         whileThing;
    number_constant_part    numberConstant;
    string_constant*        stringConstant;
    function_call_part      functionCall;
    variable_assign_part    variableAssignment;
    member_access_part      memberAccess;
  };
};

const char* GetNodeName(node_type type);
node* CreateNode(node_type type, ...);
void ApplyASTPass(parse_result& parse, ast_passlet pass[NUM_AST_NODES]);
void OutputDOTOfAST(function_def* function);
