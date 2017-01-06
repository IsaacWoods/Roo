/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <cstdio>
#include <cstring>
#include <cassert>
#include <ast.hpp>
#include <error.hpp>

ast_pass PASS_resolveVars = {};

__attribute__((constructor))
void InitResolveVarsPass()
{
  PASS_resolveVars.iteratePolicy = NODE_FIRST;

  PASS_resolveVars.f[VARIABLE_NODE] =
    [](parse_result& /*parse*/, thing_of_code* code, node* n)
    {
      if (n->variable.isResolved)
      {
        return;
      }

      for (auto* it = code->locals.head;
           it < code->locals.tail;
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

      for (auto* it = code->params.head;
           it < code->params.tail;
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

  PASS_resolveVars.f[MEMBER_ACCESS_NODE] =
    [](parse_result& /*parse*/, thing_of_code* /*code*/, node* /*n*/)
    {
      // TODO: actually implement this maybes
      fprintf(stderr, "WE HAVEN'T IMPLEMENTED VARIABLE RESOLUTION FOR `MEMBER_ACCESS_NODE`S YET\n");
      exit(1);
    };
}
