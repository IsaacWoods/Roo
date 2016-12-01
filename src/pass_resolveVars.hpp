/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <cstdio>
#include <cstring>
#include <cassert>
#include <ast.hpp>

ast_passlet PASS_resolveVars[NUM_AST_NODES] = {};

__attribute__((constructor))
void InitResolveVarsPass()
{
  __builtin_puts("Init pass: Resolve Variables");

  PASS_resolveVars[VARIABLE_NODE] =
    [](parse_result& /*parse*/, function_def* function, node* n)
    {
      assert(!n->payload.variable.isResolved);
      printf("Resolving variable: %s\n", n->payload.variable.var.name);

      for (auto* localIt = function->locals.first;
           localIt;
           localIt = localIt->next)
      {
        variable_def* local = **localIt;

        if (strcmp(local->name, n->payload.variable.var.name) == 0)
        {
          free(n->payload.variable.var.name);
          n->payload.variable.isResolved = true;
          n->payload.variable.var.def = local;
          return;
        }
      }

      for (auto* paramIt = function->params.first;
           paramIt;
           paramIt = paramIt->next)
      {
        variable_def* param = **paramIt;

        if (strcmp(param->name, n->payload.variable.var.name) == 0)
        {
          free(n->payload.variable.var.name);
          n->payload.variable.isResolved = true;
          n->payload.variable.var.def = param;
          return;
        }
      }

      // TODO: use fancy-ass error system (when it's built)
      fprintf(stderr, "Failed to resolve variable: '%s'!\n", n->payload.variable.var.name);
    };
}
