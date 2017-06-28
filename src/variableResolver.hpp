/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <ast.hpp>

struct VariableResolverPass : ASTPass<void,ThingOfCode>
{
  VariableResolverPass() : ASTPass(true) { }
  
  void Apply(ParseResult& parse);

  void VisitNode(BreakNode* node                 , ThingOfCode* code);
  void VisitNode(ReturnNode* node                , ThingOfCode* code);
  void VisitNode(UnaryOpNode* node               , ThingOfCode* code);
  void VisitNode(BinaryOpNode* node              , ThingOfCode* code);
  void VisitNode(VariableNode* node              , ThingOfCode* code);
  void VisitNode(ConditionNode* node             , ThingOfCode* code);
  void VisitNode(BranchNode* node                , ThingOfCode* code);
  void VisitNode(WhileNode* node                 , ThingOfCode* code);
  void VisitNode(NumberNode<unsigned int>* node  , ThingOfCode* code);
  void VisitNode(NumberNode<int>* node           , ThingOfCode* code);
  void VisitNode(NumberNode<float>* node         , ThingOfCode* code);
  void VisitNode(StringNode* node                , ThingOfCode* code);
  void VisitNode(CallNode* node                  , ThingOfCode* code);
  void VisitNode(VariableAssignmentNode* node    , ThingOfCode* code);
  void VisitNode(MemberAccessNode* node          , ThingOfCode* code);
  void VisitNode(ArrayInitNode* node             , ThingOfCode* code);
};
