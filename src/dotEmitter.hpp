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
  ErrorState    errorState;
  FILE*         f;
};

struct DotEmitterPass : ASTPass<char*, DotState>
{
  DotEmitterPass() : ASTPass(true) { }

  void Apply(ParseResult& parse);

  char* VisitNode(BreakNode* node                   , DotState* state);
  char* VisitNode(ReturnNode* node                  , DotState* state);
  char* VisitNode(UnaryOpNode* node                 , DotState* state);
  char* VisitNode(BinaryOpNode* node                , DotState* state);
  char* VisitNode(VariableNode* node                , DotState* state);
  char* VisitNode(ConditionNode* node               , DotState* state);
  char* VisitNode(BranchNode* node                  , DotState* state);
  char* VisitNode(WhileNode* node                   , DotState* state);
  char* VisitNode(ConstantNode<unsigned int>* node  , DotState* state);
  char* VisitNode(ConstantNode<int>* node           , DotState* state);
  char* VisitNode(ConstantNode<float>* node         , DotState* state);
  char* VisitNode(ConstantNode<bool>* node          , DotState* state);
  char* VisitNode(StringNode* node                  , DotState* state);
  char* VisitNode(CallNode* node                    , DotState* state);
  char* VisitNode(VariableAssignmentNode* node      , DotState* state);
  char* VisitNode(MemberAccessNode* node            , DotState* state);
  char* VisitNode(ArrayInitNode* node               , DotState* state);
};
