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

ast_passlet PASS_resolveCalls[NUM_AST_NODES] = {};

__attribute__((constructor))
void InitFunctionCallsPass()
{
  PASS_resolveCalls[CALL_NODE] =
    [](parse_result& parse, function_def* /*function*/, node* n)
    {
      assert(!n->call.isResolved);

      switch (n->call.type)
      {
        case call_part::call_type::FUNCTION:
        {
          for (auto* it = parse.functions.head;
               it < parse.functions.tail;
               it++)
          {
            function_def* function = *it;
   
            // TODO: be cleverer here - compare mangled names or something (functions can have the same basic name)
            if (strcmp(function->name, n->call.name) == 0)
            {
              free(n->call.name);
              n->call.isResolved = true;
              n->call.code = &(function->code);
              return;
            }
          }

          RaiseError(ERROR_UNDEFINED_FUNCTION, n->call.name);
        } break;

        case call_part::call_type::OPERATOR:
        {
          // TODO: implement
        } break;
      }
    };
}
