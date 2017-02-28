/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <parsing.hpp>
#include <ir.hpp>

enum node_type
{
  BREAK_NODE,             // Nothing
  RETURN_NODE,            // Nothing or `expression` (can be nullptr)
  BINARY_OP_NODE,         // `binary_op_part`
  PREFIX_OP_NODE,         // `prefix_op_part`
  VARIABLE_NODE,          // `variable`
  CONDITION_NODE,         // `condition_part`
  IF_NODE,                // `if_part`
  WHILE_NODE,             // `while_part`
  NUMBER_CONSTANT_NODE,   // `number_constant_part`
  STRING_CONSTANT_NODE,   // `string_constant*`
  CALL_NODE,              // `call_part`
  VARIABLE_ASSIGN_NODE,   // `variable_assign_part`
  MEMBER_ACCESS_NODE,     // `member_access_part`

  NUM_AST_NODES
};

struct node;
typedef void(*ast_passlet)(parse_result&, error_state&, thing_of_code*, node*);

enum ast_iterate_policy
{
  NODE_FIRST,
  CHILDREN_FIRST,
};

struct ast_pass
{
  ast_passlet         f[NUM_AST_NODES];
  ast_iterate_policy  iteratePolicy;
};

/*
 * Binary operations:
 *     TOKEN_PLUS
 *     TOKEN_MINUS
 *     TOKEN_ASTERIX
 *     TOKEN_SLASH
 *     TOKEN_DOUBLE_PLUS (no `right`)
 *     TOKEN_DOUBLE_MINUS (no `right`)
 *     TOKEN_LEFT_BLOCK (used to index an array)
 */
struct binary_op_part
{
  token_type      op;
  node*           left;
  node*           right;
  thing_of_code*  resolvedOperator;
};

/*
 * Prefix operations:
 *    TOKEN_PLUS
 *    TOKEN_MINUS
 *    TOKEN_BANG
 *    TOKEN_TILDE
 *    TOKEN_AND   (Reference operator)
 */
struct prefix_op_part
{
  token_type      op;
  node*           right;
  thing_of_code*  resolvedOperator;
};

struct variable_part
{
  union
  {
    char*         name;
    variable_def* var;
  };
  bool isResolved;
};

/*
 * Conditions:
 *    TOKEN_EQUALS_EQUALS
 *    TOKEN_BANG_EQUALS
 *    TOKEN_GREATER_THAN
 *    TOKEN_GREATER_THAN_EQUAL_TO
 *    TOKEN_LESS_THAN
 *    TOKEN_LESS_THAN_EQUAL_TO
 */
struct condition_part
{
  token_type  condition;
  node*       left;
  node*       right;
  bool        reverseOnJump;
};

struct if_part
{
  node* condition;
  node* thenCode;
  node* elseCode;
};

struct while_part
{
  node* condition;
  node* code;
};

struct number_part
{
  enum constant_type
  {
    SIGNED_INT,
    UNSIGNED_INT,
    FLOAT
  } type;

  union
  {
    int           asSignedInt;
    unsigned int  asUnsignedInt;
    float         asFloat;
  };
};

struct call_part
{
  union
  {
    char*           name;
    thing_of_code*  code;
  };

  bool          isResolved;
  vector<node*> params;
};

struct variable_assign_part
{
  /*
   * NOTE(Isaac): May be either a MEMBER_ACCESS_NODE or a VARIABLE_NODE
   */
  node* variable;
  node* newValue;
  bool  ignoreImmutability;
};

struct member_access_part
{
  /*
   * NOTE(Isaac): Parent may either be another MEMBER_ACCESS_NODE, or a VARIABLE_NODE
   */
  node*   parent;
  bool    isResolved;
  union
  {
    node*         child;    // Child may be a VARIABLE_NODE or another MEMBER_ACCESS_NODE
    variable_def* member;
  };
};

struct node
{
  node_type type;
  node*     next;

  type_ref* typeRef;
  bool      shouldFreeTypeRef;

  union
  {
    node*                 expression;
    binary_op_part        binaryOp;
    prefix_op_part        prefixOp;
    variable_part         variable;
    condition_part        condition;
    if_part               ifThing;
    while_part            whileThing;
    number_part           numberConstant;
    string_constant*      stringConstant;
    call_part             call;
    variable_assign_part  variableAssignment;
    member_access_part    memberAccess;
  };
};

const char* GetNodeName(node_type type);
node* CreateNode(node_type type, ...);
bool ApplyASTPass(parse_result& parse, ast_pass& pass);

#ifdef OUTPUT_DOT
void OutputDOTOfAST(thing_of_code* code);
#endif
