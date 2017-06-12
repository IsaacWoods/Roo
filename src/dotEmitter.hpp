/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <ast.hpp>
#include <ir.hpp>
#include <error.hpp>

struct DotState
{
  DotState(const char* fileName);
  ~DotState();

  unsigned int  nodeCounter;
  error_state   errorState;
  FILE*         f;
};

struct DotEmitterPass : ASTPass<char*, DotState>
{
  DotEmitterPass() : ASTPass(NODE_FIRST) { }

  char* VisitNode(BreakNode* node                 , DotState* state);
  char* VisitNode(ReturnNode* node                , DotState* state);
  char* VisitNode(UnaryOpNode* node               , DotState* state);
  char* VisitNode(BinaryOpNode* node              , DotState* state);
  char* VisitNode(VariableNode* node              , DotState* state);
  char* VisitNode(ConditionNode* node             , DotState* state);
  char* VisitNode(BranchNode* node                , DotState* state);
  char* VisitNode(WhileNode* node                 , DotState* state);
  char* VisitNode(NumberNode<unsigned int>* node  , DotState* state);
  char* VisitNode(NumberNode<int>* node           , DotState* state);
  char* VisitNode(NumberNode<float>* node         , DotState* state);
  char* VisitNode(StringNode* node                , DotState* state);
  char* VisitNode(CallNode* node                  , DotState* state);
  char* VisitNode(VariableAssignmentNode* node    , DotState* state);
  char* VisitNode(MemberAccessNode* node          , DotState* state);
  char* VisitNode(ArrayInitNode* node             , DotState* state);
};

void EmitDOT(thing_of_code* code);
