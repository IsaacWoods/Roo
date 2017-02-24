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

ast_pass PASS_resolveCalls = {};

__attribute__((constructor))
void InitFunctionCallsPass()
{
  PASS_resolveCalls.iteratePolicy = NODE_FIRST;

  PASS_resolveCalls.f[CALL_NODE] =
    [](parse_result& parse, thing_of_code* code, node* n)
    {
      assert(!n->call.isResolved);
      error_state state = CreateErrorState(TRAVERSING_AST, code, n);

      switch (n->call.type)
      {
        case call_part::call_type::FUNCTION:
        {
          for (auto* it = parse.codeThings.head;
               it < parse.codeThings.tail;
               it++)
          {
            thing_of_code* thing = *it;
   
            if (thing->type != thing_type::FUNCTION)
            {
              continue;
            }

            // TODO: do this betterer - take into account params and stuff
            if (strcmp(thing->name, n->call.name) == 0)
            {
              free(n->call.name);
              n->call.isResolved = true;
              n->call.code = thing;
              return;
            }
          }

          RaiseError(state, ERROR_UNDEFINED_FUNCTION, n->call.name);
        } break;

        case call_part::call_type::OPERATOR:
        {
          // TODO: implement
        } break;
      }
    };
}
