/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <ast.hpp>

struct CallResolverPass : ASTPass<void,ParseResult>
{
  CallResolverPass() : ASTPass(true) { }
  
  void Apply(ParseResult& parse);

  void VisitNode(BreakNode* node                 , ParseResult* parse);
  void VisitNode(ReturnNode* node                , ParseResult* parse);
  void VisitNode(UnaryOpNode* node               , ParseResult* parse);
  void VisitNode(BinaryOpNode* node              , ParseResult* parse);
  void VisitNode(VariableNode* node              , ParseResult* parse);
  void VisitNode(ConditionNode* node             , ParseResult* parse);
  void VisitNode(BranchNode* node                , ParseResult* parse);
  void VisitNode(WhileNode* node                 , ParseResult* parse);
  void VisitNode(NumberNode<unsigned int>* node  , ParseResult* parse);
  void VisitNode(NumberNode<int>* node           , ParseResult* parse);
  void VisitNode(NumberNode<float>* node         , ParseResult* parse);
  void VisitNode(StringNode* node                , ParseResult* parse);
  void VisitNode(CallNode* node                  , ParseResult* parse);
  void VisitNode(VariableAssignmentNode* node    , ParseResult* parse);
  void VisitNode(MemberAccessNode* node          , ParseResult* parse);
  void VisitNode(ArrayInitNode* node             , ParseResult* parse);
};
