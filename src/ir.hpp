/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <token_type.hpp>

struct dependency_def;
struct function_def;
struct type_def;
struct string_constant;

struct parse_result
{
  dependency_def*   firstDependency;
  function_def*     firstFunction;
  type_def*         firstType;
  string_constant*  firstString;
};

void CreateParseResult(parse_result& result);
void FreeParseResult(parse_result& result);

struct dependency_def
{
  enum dependency_type
  {
    LOCAL,
    REMOTE
  } type;

  char* path;

  dependency_def* next;
};

void FreeDependencyDef(dependency_def* dependency);

struct string_constant
{
  unsigned int      handle;
  char*             string;

  string_constant*  next;
};

string_constant* CreateStringConstant(parse_result* result, char* string);
void FreeStringConstant(string_constant* string);

enum node_type
{
  BREAK_NODE,
  RETURN_NODE,
  BINARY_OP_NODE,
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
  char*           name;
  unsigned int    arity;
  variable_def*   firstParam;
  variable_def*   firstLocal;
  type_ref*       returnType;
  node*           code;
  bool            shouldAutoReturn;

  function_def*   next;
};

void FreeFunctionDef(function_def* function);

struct type_def
{
  char*         name;
  variable_def* firstMember;

  type_def*     next;
};

void FreeTypeDef(type_def* type);
