/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <cassert>
#include <ast.hpp>

ast_passlet PASS_typeChecker[NUM_AST_NODES] = {};

__attribute__((constructor))
void InitTypeCheckerPass()
{
  // NOTE(Isaac): This complains if we are assigning to a immutable variable
  PASS_typeChecker[VARIABLE_ASSIGN_NODE] =
    [](parse_result& /*parse*/, function_def* /*function*/, node* n)
    {
      if (n->payload.variableAssignment.ignoreImmutability)
      {
        return;
      }

      node* variableNode = n->payload.variableAssignment.variable;
      variable_def* variable;

      if (variableNode->type == VARIABLE_NODE)
      {
        assert(variableNode->payload.variable.isResolved);
        variable = variableNode->payload.variable.var.def;
      }
      else
      {
        assert(variableNode->type == MEMBER_ACCESS_NODE);
        assert(variableNode->payload.memberAccess.isResolved);
        variable = variableNode->payload.memberAccess.member.def;
      }

      if (!(variable->type.isMutable))
      {
        fprintf(stderr, "ERROR: Cannot assign to an immutable variable: %s\n", variable->name);
      }
    };
}
