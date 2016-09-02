/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

enum node_type
{
  BREAK_NODE,
  RETURN_NODE
};

struct node
{
  node_type type;
  node* next;

  union
  {
    
  } types;
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
  const char* typeName;
};

struct function_def
{
  char*          name;
  unsigned int   arity;
  parameter_def* params;
  type_ref*      returnType;
  node*          code;

  function_def*  next;
};
