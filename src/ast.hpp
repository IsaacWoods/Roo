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
};

struct node;

struct binary_op_node_part
{
  token_type  op;
  node*       left;
  node*       right;
};

struct prefix_op_node_part
{
  token_type  op;
  node*       right;
};

struct variable_node_part
{
  char* name;
};

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
    int   asInt;
    float asFloat;
  } constant;
};

struct function_call_part
{
  char* name;
  
  struct param_def
  {
    node*       expression;
    param_def*  next;
  } *firstParam;
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
  } payload;
};

node* CreateNode(node_type type, ...);
void FreeNode(node* n);
