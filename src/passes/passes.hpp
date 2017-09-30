/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <ast.hpp>

#define DEF_PASS(Name, R, T)\
  struct Name : ASTPass<R,T>\
  {\
    Name() : ASTPass() { }\
    \
    void Apply(ParseResult& parse, TargetMachine* target);\
    \
    R VisitNode(BreakNode* node                  , T*);\
    R VisitNode(ReturnNode* node                 , T*);\
    R VisitNode(UnaryOpNode* node                , T*);\
    R VisitNode(BinaryOpNode* node               , T*);\
    R VisitNode(VariableNode* node               , T*);\
    R VisitNode(ConditionNode* node              , T*);\
    R VisitNode(CompositeConditionNode* node     , T*);\
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
    R VisitNode(InfiniteLoopNode* node           , T*);\
    R VisitNode(ConstructNode* node              , T*);\
  };

struct TypeCheckingContext;
struct DotState;

DEF_PASS(ScopeResolverPass,        void,     CodeThing)
DEF_PASS(VariableResolverPass,     void,     CodeThing)
DEF_PASS(TypeChecker,              void,     TypeCheckingContext)
DEF_PASS(ConditionFolderPass,      bool,     CodeThing)
DEF_PASS(DotEmitterPass,           char*,    DotState)
