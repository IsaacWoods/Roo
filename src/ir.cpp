/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <ir.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>

node* CreateNode(node_type type, ...)
{
  va_list args;
  va_start(args, type);

  node* result = static_cast<node*>(malloc(sizeof(node)));
  result->type = type;
  result->next = nullptr;

  switch (type)
  {
    case BREAK_NODE:      break;
    case RETURN_NODE:     break;

    default:
      fprintf(stderr, "Unhandled node type in CreateNode!\n");
      exit(1);
  }

  va_end(args);
  return result;
}

void FreeNode(node* n)
{
  // NOTE(Isaac): allows for easier handling of child nodes
  if (!n)
  {
    return;
  }

  switch (n->type)
  {
    case BREAK_NODE:      break;

    case RETURN_NODE:
    {
      FreeNode(n->payload.expression);
      free(n->payload.expression);
    } break;

    default:
      fprintf(stderr, "Unhandled node type in FreeNode!\n");
  }
}

void FreeFunctionDef(function_def* function)
{
  free(function->name);
  free(function->returnType);
  
  FreeNode(function->code);
  free(function->code);

  parameter_def* tempParam;

  while (function->params)
  {
    tempParam = function->params;
    function->params = function->params->next;
    free(tempParam);
  }
}
