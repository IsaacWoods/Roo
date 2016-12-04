/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <cstdio>
#include <cstring>
#include <cassert>
#include <ast.hpp>

ast_passlet PASS_resolveFunctionCalls[NUM_AST_NODES] = {};

__attribute__((constructor))
void InitFunctionCallsPass()
{
  __builtin_puts("Init pass: Resolve Function Calls");

  PASS_resolveFunctionCalls[FUNCTION_CALL_NODE] =
    [](parse_result& parse, function_def* /*function*/, node* n)
    {
      assert(!n->payload.functionCall.isResolved);
      printf("Resolving function call: %s\n", n->payload.functionCall.function.name);

      for (auto* functionIt = parse.functions.first;
           functionIt;
           functionIt = functionIt->next)
      {
        // TODO: be cleverer here - compare mangled names or something (functions can have the same basic name)
        if (strcmp((**functionIt)->name, n->payload.functionCall.function.name) == 0)
        {
          printf("Resolved function call to: %s!\n", n->payload.functionCall.function.name);
          free(n->payload.functionCall.function.name);
          n->payload.functionCall.isResolved = true;
          n->payload.functionCall.function.def = **functionIt;
          return;
        }
      }

      // TODO: use fancy-ass error system (when it's built)
      fprintf(stderr, "FATAL: Failed to resolve function call to '%s'!\n", n->payload.functionCall.function.name);
      exit(1);
    };
}
