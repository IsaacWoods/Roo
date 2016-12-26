/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <cstdio>
#include <cstring>
#include <cassert>
#include <ast.hpp>
#include <error.hpp>

ast_passlet PASS_resolveVars[NUM_AST_NODES] = {};

__attribute__((constructor))
void InitResolveVarsPass()
{
  PASS_resolveVars[VARIABLE_NODE] =
    [](parse_result& /*parse*/, function_def* function, node* n)
    {
      if (n->variable.isResolved)
      {
        return;
      }

      for (auto* localIt = function->scope.locals.first;
           localIt;
           localIt = localIt->next)
      {
        variable_def* local = **localIt;

        if (strcmp(local->name, n->variable.var.name) == 0)
        {
          free(n->variable.var.name);
          n->variable.isResolved = true;
          n->variable.var.def = local;
          return;
        }
      }

      for (auto* paramIt = function->scope.params.first;
           paramIt;
           paramIt = paramIt->next)
      {
        variable_def* param = **paramIt;

        if (strcmp(param->name, n->variable.var.name) == 0)
        {
          free(n->variable.var.name);
          n->variable.isResolved = true;
          n->variable.var.def = param;
          return;
        }
      }

      RaiseError(ERROR_UNDEFINED_VARIABLE, n->variable.var.name);
    };

  PASS_resolveVars[MEMBER_ACCESS_NODE] =
    [](parse_result& /*parse*/, function_def* /*function*/, node* /*n*/)
    {
      // TODO: actually implement this maybes
      fprintf(stderr, "WE HAVEN'T IMPLEMENTED VARIABLE RESOLUTION FOR `MEMBER_ACCESS_NODE`S YET\n");
      exit(1);
    };
}
