/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <cstdio>
#include <ast.hpp>

ast_passlet PASS_resolveVars[NUM_AST_NODES] = {};

__attribute__((constructor))
void InitResolveVarsPass()
{
  __builtin_puts("Init pass: Resolve Variables");

  PASS_resolveVars[VARIABLE_NODE] =
    [](parse_result& parse, node* n)
    {
      printf("Resolving variable: %s\n", n->payload.variable.var.name);
    };
}
