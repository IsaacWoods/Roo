/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <cassert>
#include <ast.hpp>
#include <ir.hpp>
#include <error.hpp>

ast_pass PASS_typeChecker = {};

__attribute__((constructor))
void InitTypeCheckerPass()
{
  /*
   * NOTE(Isaac): this should mean that nodes have already been marked with their types by the time
   * we try to type check their parents.
   */
  PASS_typeChecker.iteratePolicy = CHILDREN_FIRST;

  PASS_typeChecker.f[VARIABLE_NODE] =
    [](parse_result& /*parse*/, thing_of_code* /*code*/, node* n)
    {
      assert(n->variable.isResolved);
      n->typeRef = &(n->variable.var->type);
      n->shouldFreeTypeRef = false;
    };

  PASS_typeChecker.f[NUMBER_CONSTANT_NODE] =
    [](parse_result& parse, thing_of_code* /*code*/, node* n)
    {
      n->shouldFreeTypeRef = true;

      switch (n->numberConstant.type)
      {
        case number_constant_part::constant_type::INT:
        {
          n->typeRef = static_cast<type_ref*>(malloc(sizeof(type_ref)));
          n->typeRef->def = GetTypeByName(parse, "int");
          n->typeRef->isResolved = true;
          n->typeRef->isMutable = false;
        } break;

        case number_constant_part::constant_type::FLOAT:
        {
          n->typeRef = static_cast<type_ref*>(malloc(sizeof(type_ref)));
          n->typeRef->def = GetTypeByName(parse, "float");
          n->typeRef->isResolved = true;
          n->typeRef->isMutable = false;
        } break;
      }
    };

  PASS_typeChecker.f[STRING_CONSTANT_NODE] =
    [](parse_result& parse, thing_of_code* /*code*/, node* n)
    {
      n->shouldFreeTypeRef = true;
      n->typeRef = static_cast<type_ref*>(malloc(sizeof(type_ref)));
      n->typeRef->def = GetTypeByName(parse, "string");
      n->typeRef->isResolved = true;
      n->typeRef->isMutable = false;
    };

  PASS_typeChecker.f[CALL_NODE] =
    [](parse_result& /*parse*/, thing_of_code* /*code*/, node* n)
    {
      n->shouldFreeTypeRef = false;
      n->typeRef = n->call.code->returnType;
    };

  PASS_typeChecker.f[VARIABLE_ASSIGN_NODE] =
    [](parse_result& /*parse*/, thing_of_code* /*code*/, node* n)
    {
      // This complains if we are assigning to a immutable variable
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
      // For operators that change the variable, check that they're mutable
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
      }

      // Type check the operands
      {
        type_ref* a = n->binaryOp.left->typeRef;
        type_ref* b = n->binaryOp.right->typeRef;

        // NOTE(Isaac): this seems slightly defensive, design flaws showing?
        assert(a);
        assert(a->isResolved);
        assert(b);
        assert(b->isResolved);
        assert(a->def);
        assert(b->def);

        /*
         * NOTE(Isaac): We don't care about their mutability
         */
        if (a->def != b->def)
        {
          RaiseError(ERROR_MISSING_OPERATOR, a->def->name, b->def->name);
        }
      }
    };
}
