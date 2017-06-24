/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <dotEmitter.hpp>

DotState::DotState(const char* fileName)
  :nodeCounter(0u)
  ,errorState(CreateErrorState(GENERAL_STUFF))
  ,f(fopen(fileName, "w"))
{
  if (!f)
  {
    RaiseError(errorState, ERROR_FAILED_TO_OPEN_FILE, fileName);
  }
}

DotState::~DotState()
{
  fclose(f);
}

void EmitDOT(thing_of_code* code)
{
  char fileName[128u] = {};
  strcpy(fileName, code->mangledName);
  strcat(fileName, ".dot");
  DotState state(fileName);

  DotEmitterPass dotEmitter;

  fprintf(state.f, "digraph F\n{\n");
  free(dotEmitter.DispatchNode(code->ast, &state));
  fprintf(state.f, "}\n");
}

char* DotEmitterPass::VisitNode(BreakNode* node, DotState* state)
{
  printf("Visiting BreakNode\n");
  return "Potato";
}

char* DotEmitterPass::VisitNode(ReturnNode* node, DotState* state)
{
  printf("Visiting ReturnNode\n");
  return "Potato";
}

char* DotEmitterPass::VisitNode(UnaryOpNode* node, DotState* state)
{
  printf("Visiting UnaryOpNode\n");
  return "Potato";
}

char* DotEmitterPass::VisitNode(BinaryOpNode* node, DotState* state)
{
  printf("Visiting BinaryOpNode\n");
  return "Potato";
}

char* DotEmitterPass::VisitNode(VariableNode* node, DotState* state)
{
  printf("Visiting VariableNode\n");
  return "Potato";
}

char* DotEmitterPass::VisitNode(ConditionNode* node, DotState* state)
{
  printf("Visiting ConditionNode\n");
  return "Potato";
}

char* DotEmitterPass::VisitNode(BranchNode* node, DotState* state)
{
  printf("Visiting BranchNode\n");
  return "Potato";
}

char* DotEmitterPass::VisitNode(WhileNode* node, DotState* state)
{
  printf("Visiting WhileNode\n");
  return "Potato";
}

char* DotEmitterPass::VisitNode(NumberNode<unsigned int>* node, DotState* state)
{
  printf("Visiting NumberNode<unsigned int>\n");
  return "Potato";
}

char* DotEmitterPass::VisitNode(NumberNode<int>* node, DotState* state)
{
  printf("Visiting NumberNode<int>\n");
  return "Potato";
}

char* DotEmitterPass::VisitNode(NumberNode<float>* node, DotState* state)
{
  printf("Visiting NumberNode<float>\n");
  return "Potato";
}

char* DotEmitterPass::VisitNode(StringNode* node, DotState* state)
{
  printf("Visiting StringNode\n");
  return "Potato";
}

char* DotEmitterPass::VisitNode(CallNode* node, DotState* state)
{
  printf("Visiting CallNode\n");
  return "Potato";
}

char* DotEmitterPass::VisitNode(VariableAssignmentNode* node, DotState* state)
{
  printf("Visiting VariableAssignmentNode\n");
  return "Potato";
}

char* DotEmitterPass::VisitNode(MemberAccessNode* node, DotState* state)
{
  printf("Visiting MemberAccessNode\n");
  return "Potato";
}

char* DotEmitterPass::VisitNode(ArrayInitNode* node, DotState* state)
{
  printf("Visiting ArrayInitNode\n");
  return "Potato";
}
