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

      for (auto* it = function->code.locals.head;
           it < function->code.locals.tail;
           it++)
      {
        variable_def* local = *it;

        if (strcmp(local->name, n->variable.name) == 0)
        {
          free(n->variable.name);
          n->variable.isResolved = true;
          n->variable.var = local;
          return;
        }
      }

      for (auto* it = function->code.params.head;
           it < function->code.params.tail;
           it++)
      {
        variable_def* param = *it;

        if (strcmp(param->name, n->variable.name) == 0)
        {
          free(n->variable.name);
          n->variable.isResolved = true;
          n->variable.var = param;
          return;
        }
      }

      RaiseError(ERROR_UNDEFINED_VARIABLE, n->variable.name);
    };

  PASS_resolveVars[MEMBER_ACCESS_NODE] =
    [](parse_result& /*parse*/, function_def* /*function*/, node* /*n*/)
    {
      // TODO: actually implement this maybes
      fprintf(stderr, "WE HAVEN'T IMPLEMENTED VARIABLE RESOLUTION FOR `MEMBER_ACCESS_NODE`S YET\n");
      exit(1);
    };
}
