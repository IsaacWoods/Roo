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
      if (n->variableAssignment.ignoreImmutability)
      {
        return;
      }

      node* variableNode = n->variableAssignment.variable;
      variable_def* variable;

      if (variableNode->type == VARIABLE_NODE)
      {
        assert(variableNode->variable.isResolved);
        variable = variableNode->variable.var.def;
      }
      else
      {
        assert(variableNode->type == MEMBER_ACCESS_NODE);
        assert(variableNode->memberAccess.isResolved);
        variable = variableNode->memberAccess.member.def;
      }

      if (!(variable->type.isMutable))
      {
        fprintf(stderr, "ERROR: Cannot assign to an immutable variable: %s\n", variable->name);
      }
    };

  PASS_typeChecker[BINARY_OP_NODE] =
    [](parse_result& /*parse*/, function_def* /*function*/, node* n)
    {
      if (n->binaryOp.op == TOKEN_DOUBLE_PLUS ||
          n->binaryOp.op == TOKEN_DOUBLE_MINUS  )
      {
        node* variableNode = n->binaryOp.left;
        variable_def* variable;

        if (variableNode->type == VARIABLE_NODE)
        {
          assert(variableNode->variable.isResolved);
          variable = variableNode->variable.var.def;
        }
        else
        {
          assert(variableNode->type == MEMBER_ACCESS_NODE);
          assert(variableNode->memberAccess.isResolved);
          variable = variableNode->memberAccess.member.def;
        }
  
        if (!(variable->type.isMutable))
        {
          fprintf(stderr, "ERROR: Cannot operate on an immutable variable: %s\n", variable->name);
        }
      }
    };
}
