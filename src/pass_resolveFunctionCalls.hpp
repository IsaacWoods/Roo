/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <cstdio>
#include <cstring>
#include <cassert>
#include <common.hpp>
#include <ast.hpp>
#include <error.hpp>

ast_passlet PASS_resolveFunctionCalls[NUM_AST_NODES] = {};

__attribute__((constructor))
void InitFunctionCallsPass()
{
  PASS_resolveFunctionCalls[FUNCTION_CALL_NODE] =
    [](parse_result& parse, function_def* /*function*/, node* n)
    {
      assert(!n->functionCall.isResolved);

      for (auto* functionIt = parse.functions.first;
           functionIt;
           functionIt = functionIt->next)
      {
        // TODO: be cleverer here - compare mangled names or something (functions can have the same basic name)
        if (strcmp((**functionIt)->name, n->functionCall.function.name) == 0)
        {
          free(n->functionCall.function.name);
          n->functionCall.isResolved = true;
          n->functionCall.function.def = **functionIt;
          return;
        }
      }

      RaiseError(ERROR_UNDEFINED_FUNCTION, n->functionCall.function.name);
    };
}
