/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <ast.hpp>

struct TypeCheckingContext
{
  TypeCheckingContext(ParseResult& parse, ThingOfCode* code)
    :parse(parse)
    ,code(code)
  { }
  ~TypeCheckingContext() { }

  ParseResult& parse;
  ThingOfCode* code;
};

struct TypeChecker : ASTPass<void,TypeCheckingContext>
{
  TypeChecker() : ASTPass(true) { }
  
  void Apply(ParseResult& parse);

  void VisitNode(BreakNode* node                 , TypeCheckingContext* context);
  void VisitNode(ReturnNode* node                , TypeCheckingContext* context);
  void VisitNode(UnaryOpNode* node               , TypeCheckingContext* context);
  void VisitNode(BinaryOpNode* node              , TypeCheckingContext* context);
  void VisitNode(VariableNode* node              , TypeCheckingContext* context);
  void VisitNode(ConditionNode* node             , TypeCheckingContext* context);
  void VisitNode(BranchNode* node                , TypeCheckingContext* context);
  void VisitNode(WhileNode* node                 , TypeCheckingContext* context);
  void VisitNode(NumberNode<unsigned int>* node  , TypeCheckingContext* context);
  void VisitNode(NumberNode<int>* node           , TypeCheckingContext* context);
  void VisitNode(NumberNode<float>* node         , TypeCheckingContext* context);
  void VisitNode(StringNode* node                , TypeCheckingContext* context);
  void VisitNode(CallNode* node                  , TypeCheckingContext* context);
  void VisitNode(VariableAssignmentNode* node    , TypeCheckingContext* context);
  void VisitNode(MemberAccessNode* node          , TypeCheckingContext* context);
  void VisitNode(ArrayInitNode* node             , TypeCheckingContext* context);
};
