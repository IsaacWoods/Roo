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
      assert(n->variable.var->type.isResolved);
      n->typeRef = &(n->variable.var->type);
      n->shouldFreeTypeRef = false;
    };

  PASS_typeChecker.f[MEMBER_ACCESS_NODE] =
    [](parse_result& /*parse*/, thing_of_code* /*code*/, node* n)
    {
      assert(n->memberAccess.isResolved);
      n->typeRef = &(n->memberAccess.member->type);
      n->shouldFreeTypeRef = false;
    };

  PASS_typeChecker.f[NUMBER_CONSTANT_NODE] =
    [](parse_result& parse, thing_of_code* /*code*/, node* n)
    {
      n->shouldFreeTypeRef = true;
      n->typeRef = static_cast<type_ref*>(malloc(sizeof(type_ref)));
      n->typeRef->isResolved = true;
      n->typeRef->isMutable = false;
      n->typeRef->isReference = false;
      n->typeRef->isReferenceMutable = false;

      switch (n->numberConstant.type)
      {
        case number_part::constant_type::SIGNED_INT:   n->typeRef->def = GetTypeByName(parse, "int");   break;
        case number_part::constant_type::UNSIGNED_INT: n->typeRef->def = GetTypeByName(parse, "uint");  break;
        case number_part::constant_type::FLOAT:        n->typeRef->def = GetTypeByName(parse, "float"); break;
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
      n->typeRef->isReference = false;
      n->typeRef->isReferenceMutable = false;
    };

  PASS_typeChecker.f[CALL_NODE] =
    [](parse_result& /*parse*/, thing_of_code* /*code*/, node* n)
    {
      n->shouldFreeTypeRef = false;
      n->typeRef = n->call.code->returnType;
    };

  PASS_typeChecker.f[VARIABLE_ASSIGN_NODE] =
    [](parse_result& /*parse*/, thing_of_code* code, node* n)
    {
      error_state state = CreateErrorState(TRAVERSING_AST, code, n);
      // This complains if we are assigning to a immutable variable
      {
        if (n->variableAssignment.ignoreImmutability)
        {
          goto ImmutabilityCheckDone;
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
          RaiseError(state, ERROR_ASSIGN_TO_IMMUTABLE, variable->name);
        }
      }

    ImmutabilityCheckDone:
      // This type-checks the variable's type against the type of the new value
      {
        type_ref* varType = n->variableAssignment.variable->typeRef;
        type_ref* newValueType = n->variableAssignment.newValue->typeRef;

        assert(varType);
        assert(varType->isResolved);
        assert(newValueType);
        assert(newValueType->isResolved);

        // NOTE(Isaac): We don't care about their mutability
        if (varType->def != newValueType->def)
        {
          char* varTypeString = TypeRefToString(varType);
          RaiseError(state, ERROR_INCOMPATIBLE_ASSIGN, newValueType->def->name, varTypeString);
          free(varTypeString);
        }
      }
    };

  PASS_typeChecker.f[BINARY_OP_NODE] =
    [](parse_result& parse, thing_of_code* code, node* n)
    {
      error_state state = CreateErrorState(TRAVERSING_AST, code, n);
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
            RaiseError(state, ERROR_OPERATE_UPON_IMMUTABLE, variable->name);
          }
        }
      }

      // Type check the operands
      {
        if (n->binaryOp.op == TOKEN_DOUBLE_PLUS ||
            n->binaryOp.op == TOKEN_DOUBLE_MINUS  )
        {
          type_ref* a = n->binaryOp.left->typeRef;
          
          assert(a);
          assert(a->isResolved);
          assert(a->def);

          // TODO: check that the type has the required operator
        }
        else
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

          // NOTE(Isaac): this isn't really actually type-checking, but here we resolve the operator_def to use
          for (auto* it = parse.operators.head;
               it < parse.operators.tail;
               it++)
          {
            operator_def* op = *it;

            if ((op->op != n->binaryOp.op) ||
                !AreTypeRefsCompatible(a, &(op->code.params[0u]->type)) ||
                !AreTypeRefsCompatible(b, &(op->code.params[1u]->type)))
            {
              continue;
            }

            n->binaryOp.resolvedOperator = op;
            n->typeRef = op->code.returnType;
            break;
          }

          if (!(n->typeRef))
          {
            char* aString = TypeRefToString(a);
            char* bString = TypeRefToString(b);
            RaiseError(state, ERROR_MISSING_OPERATOR, GetTokenName(n->binaryOp.op), aString, bString);
            free(aString);
            free(bString);
          }
        }
      }
    };
}
