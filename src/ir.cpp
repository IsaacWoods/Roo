/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <ir.hpp>
#include <cstdio>

void FreeNode(node* n)
{
  switch (n->type)
  {
    default:
      fprintf(stderr, "Unhandled node type in FreeNode!\n");
  }
}
