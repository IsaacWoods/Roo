/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <ast.hpp>
#include <ir.hpp>
#include <error.hpp>

ast_pass PASS_typeChecker = {};

__attribute__((constructor))
void InitTypeCheckerPass()
{
  PASS_typeChecker.passName = "TypeChecker";
  /*
   * NOTE(Isaac): this should mean that nodes have already been marked with their types by the time
   * we try to type check their parents.
   */
  PASS_typeChecker.iteratePolicy = CHILDREN_FIRST;

  PASS_typeChecker.f[VARIABLE_NODE] =
    [](parse_result& /*parse*/, error_state& /*errorState*/, thing_of_code* /*code*/, node* n)
    {
      Assert(n->variable.isResolved, "Tried to type-check unresolved variable");
      Assert(n->variable.var->type.isResolved, "Tried to type-check variable with unresolved type");
      n->typeRef = &(n->variable.var->type);
      n->shouldFreeTypeRef = false;
    };

  PASS_typeChecker.f[MEMBER_ACCESS_NODE] =
    [](parse_result& /*parse*/, error_state& /*errorState*/, thing_of_code* /*code*/, node* n)
    {
      Assert(n->memberAccess.isResolved, "Tried to type-check unresolved member access");
      n->typeRef = &(n->memberAccess.member->type);
      n->shouldFreeTypeRef = false;
    };

  PASS_typeChecker.f[NUMBER_CONSTANT_NODE] =
    [](parse_result& parse, error_state& /*errorState*/, thing_of_code* /*code*/, node* n)
    {
      n->shouldFreeTypeRef            = true;
      n->typeRef                      = static_cast<type_ref*>(malloc(sizeof(type_ref)));
      n->typeRef->isResolved          = true;
      n->typeRef->isMutable           = false;
      n->typeRef->isReference         = false;
      n->typeRef->isReferenceMutable  = false;
      n->typeRef->isArray             = false;
      n->typeRef->isArraySizeResolved = false;
      n->typeRef->arraySizeExpression = nullptr;

      switch (n->number.type)
      {
        case number_part::constant_type::SIGNED_INT:   n->typeRef->def = GetTypeByName(parse, "int");   break;
        case number_part::constant_type::UNSIGNED_INT: n->typeRef->def = GetTypeByName(parse, "uint");  break;
        case number_part::constant_type::FLOAT:        n->typeRef->def = GetTypeByName(parse, "float"); break;
      }
    };

  PASS_typeChecker.f[STRING_CONSTANT_NODE] =
    [](parse_result& parse, error_state& /*errorState*/, thing_of_code* /*code*/, node* n)
    {
      n->shouldFreeTypeRef            = true;
      n->typeRef                      = static_cast<type_ref*>(malloc(sizeof(type_ref)));
      n->typeRef->def                 = GetTypeByName(parse, "string");
      n->typeRef->isResolved          = true;
      n->typeRef->isMutable           = false;
      n->typeRef->isReference         = false;
      n->typeRef->isReferenceMutable  = false;
      n->typeRef->isArray             = false;
      n->typeRef->isArraySizeResolved = false;
      n->typeRef->arraySizeExpression = nullptr;
    };

  PASS_typeChecker.f[ARRAY_INIT_NODE] =
    [](parse_result& /*parse*/, error_state& errorState, thing_of_code* /*code*/, node* n)
    {
      n->shouldFreeTypeRef            = true;
      n->typeRef                      = static_cast<type_ref*>(malloc(sizeof(type_ref)));
      n->typeRef->def                 = nullptr;
      n->typeRef->isResolved          = true;
      n->typeRef->isMutable           = false;
      n->typeRef->isReference         = false;
      n->typeRef->isReferenceMutable  = false;
      n->typeRef->isArray             = true;
      n->typeRef->isArraySizeResolved = true;
      n->typeRef->arraySize           = 0u;

      if (n->arrayInit.items.size > 0u)
      {
        type_ref* establishedType = nullptr;

        for (auto* it = n->arrayInit.items.head;
             it < n->arrayInit.items.tail;
             it++)
        {
          node* item = *it;

          if (establishedType)
          {
            if (!AreTypeRefsCompatible(item->typeRef, establishedType))
            {
              RaiseError(errorState, ERROR_INCOMPATIBLE_TYPE, TypeRefToString(establishedType), TypeRefToString(item->typeRef));
            }
          }
          else
          {
            establishedType = item->typeRef;
          }
        }

        n->typeRef->def = establishedType->def;
      }
    };

  PASS_typeChecker.f[RETURN_NODE] =
    [](parse_result& /*parse*/, error_state& errorState, thing_of_code* code, node* n)
    {
      /*
       * Firstly, remain null safe - if one is null but not the other, don't dereference, but still error
       * (hopefully helpfully).
       */
      if (!(code->returnType) && n->expression)
      {
        RaiseError(errorState, ERROR_RETURN_VALUE_NOT_EXPECTED, TypeRefToString(n->expression->typeRef));
        return;
      }

      if (code->returnType && !(n->expression))
      {
        RaiseError(errorState, ERROR_MUST_RETURN_SOMETHING, TypeRefToString(code->returnType));
        return;
      }

      /*
       * If both types actually exist, compare them
       */
      if (!AreTypeRefsCompatible(code->returnType, n->expression->typeRef))
      {
        RaiseError(errorState, ERROR_INCOMPATIBLE_TYPE, TypeRefToString(code->returnType), TypeRefToString(n->expression->typeRef));
      }
    };

  // XXX: This should't be done by the type checker
  PASS_typeChecker.f[CALL_NODE] =
    [](parse_result& parse, error_state& errorState, thing_of_code* /*code*/, node* n)
    {
      // --- First, we resolve the function call to the actual `thing_of_code` being called ---
      Assert(!n->call.isResolved, "Tried to type-check call that is already selected and resolved");

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
          break;
        }
      }

      if (!(n->call.isResolved))
      {
        RaiseError(errorState, ERROR_UNDEFINED_FUNCTION, n->call.name);
        return;
      }

      // --- We then work out the type of the calling expression from the return type ---
      n->shouldFreeTypeRef = false;
      n->typeRef = n->call.code->returnType;
    };

  PASS_typeChecker.f[VARIABLE_ASSIGN_NODE] =
    [](parse_result& /*parse*/, error_state& errorState, thing_of_code* /*code*/, node* n)
    {
      // This complains if we are assigning to a immutable variable
      {
        if (n->variableAssignment.ignoreImmutability)
        {
          goto ImmutabilityCheckDone;
        }
  
        node* variableNode = n->variableAssignment.variable;
        variable_def* variable;
 
        switch (variableNode->type)
        {
          case VARIABLE_NODE:
          {
            Assert(variableNode->variable.isResolved, "Tried to type-check an unresolved variable in an assign");
            variable = variableNode->variable.var;
          } break;

          case MEMBER_ACCESS_NODE:
          {
            Assert(variableNode->memberAccess.isResolved, "Tried to type-check an unresolved member access");
            variable = variableNode->memberAccess.member;
          } break;

          case BINARY_OP_NODE:
          {
            if (variableNode->binaryOp.op == TOKEN_LEFT_BLOCK)
            {
              Assert(variableNode->binaryOp.left->type == VARIABLE_NODE, "Left side of a binary op is not an l-value");
              Assert(variableNode->binaryOp.left->variable.isResolved, "Type-checking an unresolved l-value");
              variable = variableNode->binaryOp.left->variable.var;
              break;
            }
          } // NOTE(Isaac): no break

          default:
          {
            RaiseError(errorState, ERROR_UNEXPECTED_EXPRESSION, "variable-binding", GetTokenName(variableNode->binaryOp.op));
          } break;
        }
  
        if (!(variable->type.isMutable))
        {
          RaiseError(errorState, ERROR_ASSIGN_TO_IMMUTABLE, variable->name);
        }
      }

    ImmutabilityCheckDone:
      // This type-checks the variable's type against the type of the new value
      {
        type_ref* varType = n->variableAssignment.variable->typeRef;
        type_ref* newValueType = n->variableAssignment.newValue->typeRef;

        Assert(varType &&
               varType->isResolved &&
               newValueType &&
               newValueType->isResolved,  "Missing element of type-checking information");

        if (!AreTypeRefsCompatible(varType, newValueType, false))
        {
          char* varTypeString = TypeRefToString(varType);
          RaiseError(errorState, ERROR_INCOMPATIBLE_ASSIGN, TypeRefToString(newValueType), varTypeString);
          free(varTypeString);
        }
      }
    };

  PASS_typeChecker.f[BINARY_OP_NODE] =
    [](parse_result& parse, error_state& errorState, thing_of_code* /*code*/, node* n)
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
            Assert(variableNode->variable.isResolved, "Unresolved variable while type-checking binary op");
            variable = variableNode->variable.var;
          }
          else
          {
            Assert(variableNode->type == MEMBER_ACCESS_NODE, "Binary op l-value must either be variable or member access");
            Assert(variableNode->memberAccess.isResolved, "Tried to type-check an unresolved member access");
            variable = variableNode->memberAccess.member;
          }
    
          if (!(variable->type.isMutable))
          {
            RaiseError(errorState, ERROR_OPERATE_UPON_IMMUTABLE, variable->name);
          }
        }
      }

      // Type check the operands
      {
        if (n->binaryOp.op == TOKEN_DOUBLE_PLUS ||
            n->binaryOp.op == TOKEN_DOUBLE_MINUS  )
        {
          type_ref* a = n->binaryOp.left->typeRef;
          
          Assert(a && a->isResolved && a->def, "Operand of one-operand operation is invalid");

          // TODO: check that the type has the required operator
        }
        else
        {
          type_ref* a = n->binaryOp.left->typeRef;
          type_ref* b = n->binaryOp.right->typeRef;
  
          Assert(a && a->isResolved && a->def, "Left side of binary op is invald");
          Assert(b && b->isResolved && b->def, "Right side of binary op is invalid");

          // NOTE(Isaac): this isn't really actually type-checking, but here we resolve the operator_def to use
          for (auto* it = parse.codeThings.head;
               it < parse.codeThings.tail;
               it++)
          {
            thing_of_code* thing = *it;

            if (thing->type != thing_type::OPERATOR)
            {
              continue;
            }

            if ((thing->op != n->binaryOp.op) ||
                !AreTypeRefsCompatible(a, &(thing->params[0u]->type), false) ||
                !AreTypeRefsCompatible(b, &(thing->params[1u]->type), false))
            {
              continue;
            }

            n->binaryOp.resolvedOperator = thing;
            n->typeRef = thing->returnType;
            break;
          }

          if (!(n->typeRef))
          {
            char* aString = TypeRefToString(a);
            char* bString = TypeRefToString(b);
            RaiseError(errorState, ERROR_MISSING_OPERATOR, GetTokenName(n->binaryOp.op), aString, bString);
            free(aString);
            free(bString);
          }
        }
      }
    };
}
