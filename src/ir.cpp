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
  switch (n->type)
  {
    default:
      fprintf(stderr, "Unhandled node type in FreeNode!\n");
  }
}
