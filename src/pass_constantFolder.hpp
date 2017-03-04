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
    [](parse_result& /*parse*/, error_state& /*errorState*/, thing_of_code* /*code*/, node* n)
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
                error_state errorState = CreateErrorState(GENERAL_STUFF);
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
                error_state errorState = CreateErrorState(GENERAL_STUFF);
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
                error_state errorState = CreateErrorState(GENERAL_STUFF);
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

  PASS_constantFolder.f[CONDITION_NODE] =
    [](parse_result& /*parse*/, error_state& /*errorState*/, thing_of_code* /*code*/, node* n)
    {
      if (n->condition.left->type != NUMBER_CONSTANT_NODE ||
          n->condition.right->type != NUMBER_CONSTANT_NODE)
      {
        return;
      }
    };
}
