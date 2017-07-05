/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <ast.hpp>

#define DEF_PASS_IMPL(Name, R, T)\
  struct Name : ASTPass<R,T>\
  {\
    Name() : ASTPass(true) { }\
    void Apply(ParseResult& parse);\
    R VisitNode(BreakNode* node                  , T*);\
    R VisitNode(ReturnNode* node                 , T*);\
    R VisitNode(UnaryOpNode* node                , T*);\
    R VisitNode(BinaryOpNode* node               , T*);\
    R VisitNode(VariableNode* node               , T*);\
    R VisitNode(ConditionNode* node              , T*);\
    R VisitNode(BranchNode* node                 , T*);\
    R VisitNode(WhileNode* node                  , T*);\
    R VisitNode(ConstantNode<unsigned int>* node , T*);\
    R VisitNode(ConstantNode<int>* node          , T*);\
    R VisitNode(ConstantNode<float>* node        , T*);\
    R VisitNode(ConstantNode<bool>* node         , T*);\
    R VisitNode(StringNode* node                 , T*);\
    R VisitNode(CallNode* node                   , T*);\
    R VisitNode(VariableAssignmentNode* node     , T*);\
    R VisitNode(MemberAccessNode* node           , T*);\
    R VisitNode(ArrayInitNode* node              , T*);\
  };

struct TypeCheckingContext;
struct DotState;

DEF_PASS_IMPL(TypeChecker,            void,     TypeCheckingContext)
DEF_PASS_IMPL(DotEmitterPass,         char*,    DotState)
DEF_PASS_IMPL(VariableResolverPass,   void,     ThingOfCode)
