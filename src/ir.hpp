/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

enum node_type
{

};

struct node
{
  node_type type;

  union
  {
    
  } types;
};

struct function_def
{
  const char* name;
  unsigned int arity;

  function_def* next;
};
