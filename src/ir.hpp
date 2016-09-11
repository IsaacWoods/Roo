/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <token_type.hpp>

enum node_type
{
  BREAK_NODE,
  RETURN_NODE,
  BINARY_OP_NODE,
  VARIABLE_NODE,
  CONDITION_NODE,
  IF_NODE,
  NUMBER_CONSTANT_NODE,
};

struct node;

struct binary_op_node_part
{
  token_type  op;
  node*       left;
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

struct node
{
  node_type type;
  node* next;

  union
  {
    node*                   expression;
    binary_op_node_part     binaryOp;
    variable_node_part      variable;
    condition_node_part     condition;
    if_node_part            ifThing;
    number_constant_part    numberConstant;
  } payload;
};

node* CreateNode(node_type type, ...);
void FreeNode(node* n);

struct parameter_def
{
  const char* name;
  const char* typeName;

  parameter_def* next;
};

struct type_ref
{
  char* typeName;
};

void FreeTypeRef(type_ref& typeRef);

struct scope_ref
{
  // TODO
};

void FreeScopeRef(scope_ref& scope);

struct variable_def
{
  char*         name;
  type_ref      type;
  node*         initValue;

  variable_def* next;
};

void FreeVariableDef(variable_def* variable);

struct function_def
{
  char*          name;
  unsigned int   arity;
  parameter_def* firstParam;
  variable_def*  firstLocal;
  type_ref*      returnType;
  node*          code;

  function_def*  next;
};

void FreeFunctionDef(function_def* function);

struct type_def
{
  char*         name;
  variable_def* firstMember;

  type_def*     next;
};

void FreeTypeDef(type_def* type);
