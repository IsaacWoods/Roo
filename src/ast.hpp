/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <common.hpp>

enum node_type
{
  BREAK_NODE,
  RETURN_NODE,
  BINARY_OP_NODE,
  PREFIX_OP_NODE,
  VARIABLE_NODE,
  CONDITION_NODE,
  IF_NODE,
  NUMBER_CONSTANT_NODE,
  STRING_CONSTANT_NODE,
  FUNCTION_CALL_NODE,
  VARIABLE_ASSIGN_NODE,
  MEMBER_ACCESS_NODE,
};

const char* GetNodeName(node_type type);

struct node;

/*
 * Binary operations:
 *     TOKEN_PLUS
 *     TOKEN_MINUS
 *     TOKEN_ASTERIX
 *     TOKEN_SLASH
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
  char* name;
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
};

struct if_node_part
{
  node* condition;
  node* thenCode;
  node* elseCode;
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
    int   i;
    float f;
  } constant;
};

struct function_call_part
{
  char* name;
  linked_list<node*> params;
};

struct variable_assign_part
{
  char* variableName;
  node* newValue;

  /*
   * NOTE(Isaac): this starts as `nullptr`, and is filled in after we finish parsing... hopefully
   */
  variable_def* variable;
};

struct member_access_part
{
  /*
   * NOTE(Isaac): Parent may either be another MEMBER_ACCESS_NODE, or a VARIABLE_NODE
   */
  node* parent;
  char* memberName;

  /*
   * NOTE(Isaac): this starts as `nullptr`, and should be filled in after we finish parsing and know everything
   */
  variable_def* variable;
};

struct node
{
  node_type type;
  node*     next;

  union node_payload
  {
    node*                   expression;
    binary_op_node_part     binaryOp;
    prefix_op_node_part     prefixOp;
    variable_node_part      variable;
    condition_node_part     condition;
    if_node_part            ifThing;
    number_constant_part    numberConstant;
    string_constant*        stringConstant;
    function_call_part      functionCall;
    variable_assign_part    variableAssignment;
    member_access_part      memberAccess;
  } payload;
};

node* CreateNode(node_type type, ...);
void FreeNode(node* n);
void OutputDOTOfAST(function_def* function);
