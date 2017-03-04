/*
 * Copyright (C) 2016-2017, Isaac Woods. All rights reserved.
 */

#pragma once

/*
 * The constant folding pass collapses operations on constants within the AST. This is mainly for optimisation,
 * but some platforms also rely on the simplified instructions (e.g. x86 can't compare two immediates).
 *
 * For example:
 *    +
 *   / \                + 
 *  3   *       ->     / \      ->  11
 *     / \            3   8
 *    2   4
 */
ast_pass PASS_constantFolder = {};

__attribute__((constructor))
void InitConstantFolderPass()
{
  PASS_constantFolder.iteratePolicy = CHILDREN_FIRST;

  PASS_constantFolder.f[BINARY_OP_NODE] =
    [](parse_result& /*parse*/, error_state& errorState, thing_of_code* /*code*/, node* n)
    {
      if ((n->binaryOp.left && n->binaryOp.right)         &&
          n->binaryOp.left->type == NUMBER_CONSTANT_NODE  &&
          n->binaryOp.right->type == NUMBER_CONSTANT_NODE)
      {
        // NOTE(Isaac): we make a copy of the node, so we can change stuff in `n`
        node oldNode = *n;
        n->type = NUMBER_CONSTANT_NODE;

        assert(oldNode.binaryOp.left->number.type == oldNode.binaryOp.right->number.type);
        n->number.type = oldNode.binaryOp.left->number.type;

        switch (oldNode.binaryOp.left->number.type)
        {
          case number_part::constant_type::SIGNED_INT:
          {
            switch (oldNode.binaryOp.op)
            {
              #define DO_OP(op) n->number.asSignedInt = oldNode.binaryOp.left->number.asSignedInt op oldNode.binaryOp.right->number.asSignedInt;
              case TOKEN_PLUS:      DO_OP(+); break;
              case TOKEN_MINUS:     DO_OP(-); break;
              case TOKEN_ASTERIX:   DO_OP(*); break;
              case TOKEN_SLASH:     DO_OP(/); break;

              default:
              {
                RaiseError(errorState, ICE_UNHANDLED_NODE_TYPE, GetNodeName(oldNode.type));
              } break;
              #undef DO_OP
            }
          } break;

          case number_part::constant_type::UNSIGNED_INT:
          {
            switch (oldNode.binaryOp.op)
            {
              #define DO_OP(op) n->number.asUnsignedInt = oldNode.binaryOp.left->number.asUnsignedInt op oldNode.binaryOp.right->number.asUnsignedInt;
              case TOKEN_PLUS:      DO_OP(+); break;
              case TOKEN_MINUS:     DO_OP(-); break;
              case TOKEN_ASTERIX:   DO_OP(*); break;
              case TOKEN_SLASH:     DO_OP(/); break;

              default:
              {
                RaiseError(errorState, ICE_UNHANDLED_NODE_TYPE, GetNodeName(oldNode.type));
              } break;
              #undef DO_OP
            }
          } break;

          case number_part::constant_type::FLOAT:
          {
            switch (oldNode.binaryOp.op)
            {
              #define DO_OP(op) n->number.asFloat = oldNode.binaryOp.left->number.asFloat op oldNode.binaryOp.right->number.asFloat;
              case TOKEN_PLUS:      DO_OP(+); break;
              case TOKEN_MINUS:     DO_OP(-); break;
              case TOKEN_ASTERIX:   DO_OP(*); break;
              case TOKEN_SLASH:     DO_OP(/); break;

              default:
              {
                RaiseError(errorState, ICE_UNHANDLED_NODE_TYPE, GetNodeName(oldNode.type));
              } break;
              #undef DO_OP
            }
          } break;
        }

        Free<node*>(oldNode.binaryOp.left);
        Free<node*>(oldNode.binaryOp.right);
      }
    };

  PASS_constantFolder.f[BRANCH_NODE] =
    [](parse_result& /*parse*/, error_state& errorState, thing_of_code* /*code*/, node* n)
    {
      condition_part& condition = n->branch.condition->condition;

      if (condition.left->type != NUMBER_CONSTANT_NODE ||
          condition.right->type != NUMBER_CONSTANT_NODE)
      {
        return;
      }

      number_part& left = condition.left->number;
      number_part& right = condition.right->number;
      assert(left.type == right.type);

      bool comparisonResult;

      switch (left.type)
      {
        case number_part::constant_type::SIGNED_INT:
        {
          #define DO_COMPARE(op) comparisonResult = (left.asSignedInt op right.asSignedInt);
          switch (condition.condition)
          {
            case TOKEN_EQUALS_EQUALS:         DO_COMPARE(==); break;
            case TOKEN_BANG_EQUALS:           DO_COMPARE(!=); break;
            case TOKEN_GREATER_THAN:          DO_COMPARE(>);  break;
            case TOKEN_GREATER_THAN_EQUAL_TO: DO_COMPARE(>=); break;
            case TOKEN_LESS_THAN:             DO_COMPARE(<);  break;
            case TOKEN_LESS_THAN_EQUAL_TO:    DO_COMPARE(<=); break;

            default:
            {
              RaiseError(errorState, ICE_UNHANDLED_OPERATOR, GetTokenName(condition.condition), "PASS_constantFolder::CONDITION_NODE");
            } break;
          }
          #undef DO_COMPARE
        } break;

        case number_part::constant_type::UNSIGNED_INT:
        {
          #define DO_COMPARE(op) comparisonResult = (left.asUnsignedInt op right.asUnsignedInt);
          switch (condition.condition)
          {
            case TOKEN_EQUALS_EQUALS:         DO_COMPARE(==); break;
            case TOKEN_BANG_EQUALS:           DO_COMPARE(!=); break;
            case TOKEN_GREATER_THAN:          DO_COMPARE(>);  break;
            case TOKEN_GREATER_THAN_EQUAL_TO: DO_COMPARE(>=); break;
            case TOKEN_LESS_THAN:             DO_COMPARE(<);  break;
            case TOKEN_LESS_THAN_EQUAL_TO:    DO_COMPARE(<=); break;

            default:
            {
              RaiseError(errorState, ICE_UNHANDLED_OPERATOR, GetTokenName(condition.condition), "PASS_constantFolder::CONDITION_NODE");
            } break;
          }
          #undef DO_COMPARE
        } break;

        case number_part::constant_type::FLOAT:
        {
          #define DO_COMPARE(op) comparisonResult = (left.asFloat op right.asFloat);
          switch (condition.condition)
          {
            case TOKEN_EQUALS_EQUALS:         DO_COMPARE(==); break;
            case TOKEN_BANG_EQUALS:           DO_COMPARE(!=); break;
            case TOKEN_GREATER_THAN:          DO_COMPARE(>);  break;
            case TOKEN_GREATER_THAN_EQUAL_TO: DO_COMPARE(>=); break;
            case TOKEN_LESS_THAN:             DO_COMPARE(<);  break;
            case TOKEN_LESS_THAN_EQUAL_TO:    DO_COMPARE(<=); break;

            default:
            {
              RaiseError(errorState, ICE_UNHANDLED_OPERATOR, GetTokenName(condition.condition), "PASS_constantFolder::CONDITION_NODE");
            } break;
          }
          #undef DO_COMPARE
        } break;
      }

      node oldN = *n;
      Free<node*>(oldN.branch.condition);

      if (comparisonResult)
      {
        *n = *(oldN.branch.thenCode);
        n->next = oldN.next;
        Free<node*>(oldN.branch.elseCode);
      }
      else
      {
        *n = *(oldN.branch.elseCode);
        n->next = oldN.next;
        Free<node*>(oldN.branch.thenCode);
      }
    };
}
