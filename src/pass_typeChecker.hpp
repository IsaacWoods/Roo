/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <cassert>
#include <ast.hpp>

ast_pass PASS_typeChecker = {};

__attribute__((constructor))
void InitTypeCheckerPass()
{
  PASS_typeChecker.iteratePolicy = NODE_FIRST;

  // NOTE(Isaac): This complains if we are assigning to a immutable variable
  PASS_typeChecker.f[VARIABLE_ASSIGN_NODE] =
    [](parse_result& /*parse*/, thing_of_code* /*code*/, node* n)
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
        variable = variableNode->variable.var;
      }
      else
      {
        assert(variableNode->type == MEMBER_ACCESS_NODE);
        assert(variableNode->memberAccess.isResolved);
        variable = variableNode->memberAccess.member;
      }

      if (!(variable->type.isMutable))
      {
        fprintf(stderr, "ERROR: Cannot assign to an immutable variable: %s\n", variable->name);
      }
    };

  PASS_typeChecker.f[BINARY_OP_NODE] =
    [](parse_result& /*parse*/, thing_of_code* /*code*/, node* n)
    {
      if (n->binaryOp.op == TOKEN_DOUBLE_PLUS ||
          n->binaryOp.op == TOKEN_DOUBLE_MINUS  )
      {
        node* variableNode = n->binaryOp.left;
        variable_def* variable;

        if (variableNode->type == VARIABLE_NODE)
        {
          assert(variableNode->variable.isResolved);
          variable = variableNode->variable.var;
        }
        else
        {
          assert(variableNode->type == MEMBER_ACCESS_NODE);
          assert(variableNode->memberAccess.isResolved);
          variable = variableNode->memberAccess.member;
        }
  
        if (!(variable->type.isMutable))
        {
          fprintf(stderr, "ERROR: Cannot operate on an immutable variable: %s\n", variable->name);
        }
      }
    };
}
