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

void EmitDOT(ThingOfCode* code)
{
  char fileName[128u] = {};
  strcpy(fileName, code->mangledName);
  strcat(fileName, ".dot");

  DotState state(fileName);
  DotEmitterPass dotEmitter;

  fprintf(state.f, "digraph G\n{\n");
  free(dotEmitter.DispatchNode(code->ast, &state));
  fprintf(state.f, "}\n");
}

static char* GetNextNode(DotState* state)
{
  unsigned int size = snprintf(nullptr, 0u, "n%d", state->nodeCounter);
  char* name = static_cast<char*>(malloc(size+1u));
  snprintf(name, size+1u, "n%d", state->nodeCounter);

  state->nodeCounter++;
  return name;
}

#define CONCAT(a,b,c) a b c
#define EMIT_WITH_LABEL(label) fprintf(state->f, CONCAT("\t%s[label=\"", label, "\"];\n"), nodeName);

#define LINK_CHILD(child)\
{\
  char* childName = DispatchNode(child, state);\
  fprintf(state->f, "\t%s -> %s;\n", nodeName, childName);\
  free(childName);\
}

#define VISIT_NEXT()\
{\
  if (node->next)\
  {\
    char* nextName = DispatchNode(node->next, state);\
    fprintf(state->f, "\t%s -> %s[color=blue];\n", nodeName, nextName);\
    free(nextName);\
  }\
}

char* DotEmitterPass::VisitNode(BreakNode* node, DotState* state)
{
  char* nodeName = GetNextNode(state);
  EMIT_WITH_LABEL("Break");
  VISIT_NEXT();
  return nodeName;
}

char* DotEmitterPass::VisitNode(ReturnNode* node, DotState* state)
{
  char* nodeName = GetNextNode(state);
  EMIT_WITH_LABEL("Return");

  if (node->returnValue)
  {
    LINK_CHILD(node->returnValue);
  }

  VISIT_NEXT();
  return nodeName;
}

char* DotEmitterPass::VisitNode(UnaryOpNode* node, DotState* state)
{
  char* nodeName = GetNextNode(state);

  switch (node->op)
  {
    case UnaryOpNode::Operator::POSITIVE:         EMIT_WITH_LABEL("+");   break;
    case UnaryOpNode::Operator::NEGATIVE:         EMIT_WITH_LABEL("-");   break;
    case UnaryOpNode::Operator::NEGATE:           EMIT_WITH_LABEL("~");   break;
    case UnaryOpNode::Operator::LOGICAL_NOT:      EMIT_WITH_LABEL("!");   break;
    case UnaryOpNode::Operator::TAKE_REFERENCE:   EMIT_WITH_LABEL("&");   break;
    case UnaryOpNode::Operator::PRE_INCREMENT:    EMIT_WITH_LABEL("++x"); break;
    case UnaryOpNode::Operator::POST_INCREMENT:   EMIT_WITH_LABEL("x++"); break;
    case UnaryOpNode::Operator::PRE_DECREMENT:    EMIT_WITH_LABEL("--x"); break;
    case UnaryOpNode::Operator::POST_DECREMENT:   EMIT_WITH_LABEL("x--"); break;
  }

  LINK_CHILD(node->operand);

  VISIT_NEXT();
  return nodeName;
}

char* DotEmitterPass::VisitNode(BinaryOpNode* node, DotState* state)
{
  char* nodeName = GetNextNode(state);

  switch (node->op)
  {
    case BinaryOpNode::Operator::ADD:             EMIT_WITH_LABEL("+");   break;
    case BinaryOpNode::Operator::SUBTRACT:        EMIT_WITH_LABEL("-");   break;
    case BinaryOpNode::Operator::MULTIPLY:        EMIT_WITH_LABEL("*");   break;
    case BinaryOpNode::Operator::DIVIDE:          EMIT_WITH_LABEL("/");   break;
    case BinaryOpNode::Operator::INDEX_ARRAY:     EMIT_WITH_LABEL("[]");  break;
  }

  LINK_CHILD(node->left);
  LINK_CHILD(node->right);

  VISIT_NEXT();
  return nodeName;
}

char* DotEmitterPass::VisitNode(VariableNode* node, DotState* state)
{
  char* nodeName = GetNextNode(state);

  if (node->isResolved)
  {
    fprintf(state->f, "\t%s[label=\"`%s`\"];\n", nodeName, node->var->name);
  }
  else
  {
    fprintf(state->f, "\t%s[label=\"`%s`\"];\n", nodeName, node->name);
  }

  VISIT_NEXT();
  return nodeName;
}

char* DotEmitterPass::VisitNode(ConditionNode* node, DotState* state)
{
  char* nodeName = GetNextNode(state);

  switch (node->condition)
  {
    case ConditionNode::Condition::EQUAL:                   EMIT_WITH_LABEL("==");  break;
    case ConditionNode::Condition::NOT_EQUAL:               EMIT_WITH_LABEL("!=");  break;
    case ConditionNode::Condition::LESS_THAN:               EMIT_WITH_LABEL("<");   break;
    case ConditionNode::Condition::LESS_THAN_OR_EQUAL:      EMIT_WITH_LABEL("<=");  break;
    case ConditionNode::Condition::GREATER_THAN:            EMIT_WITH_LABEL(">");   break;
    case ConditionNode::Condition::GREATER_THAN_OR_EQUAL:   EMIT_WITH_LABEL(">=");  break;
  }

  LINK_CHILD(node->left);
  LINK_CHILD(node->right);

  VISIT_NEXT();
  return nodeName;
}

char* DotEmitterPass::VisitNode(BranchNode* node, DotState* state)
{
  char* nodeName = GetNextNode(state);

  EMIT_WITH_LABEL("Branch");
  LINK_CHILD(node->condition);
  LINK_CHILD(node->thenCode);

  if (node->elseCode)
  {
    LINK_CHILD(node->elseCode);
  }

  VISIT_NEXT();
  return nodeName;
}

char* DotEmitterPass::VisitNode(WhileNode* node, DotState* state)
{
  char* nodeName = GetNextNode(state);

  EMIT_WITH_LABEL("While");
  LINK_CHILD(node->condition);
  LINK_CHILD(node->loopBody);

  VISIT_NEXT();
  return nodeName;
}

char* DotEmitterPass::VisitNode(NumberNode<unsigned int>* node, DotState* state)
{
  char* nodeName = GetNextNode(state);
  fprintf(state->f, "\t%s[label=\"%uu\"];\n", nodeName, node->value);
  VISIT_NEXT();
  return nodeName;
}

char* DotEmitterPass::VisitNode(NumberNode<int>* node, DotState* state)
{
  char* nodeName = GetNextNode(state);
  fprintf(state->f, "\t%s[label=\"%d\"];\n", nodeName, node->value);
  VISIT_NEXT();
  return nodeName;
}

char* DotEmitterPass::VisitNode(NumberNode<float>* node, DotState* state)
{
  char* nodeName = GetNextNode(state);
  fprintf(state->f, "\t%s[label=\"%f\"];\n", nodeName, node->value);
  VISIT_NEXT();
  return nodeName;
}

char* DotEmitterPass::VisitNode(StringNode* node, DotState* state)
{
  char* nodeName = GetNextNode(state);
  fprintf(state->f, "\t%s[label=\"\\\"%s\\\"\"];\n", nodeName, node->string->string);
  VISIT_NEXT();
  return nodeName;
}

char* DotEmitterPass::VisitNode(CallNode* node, DotState* state)
{
  char* nodeName = GetNextNode(state);

  fprintf(state->f, "\t%s[label=\"Call(%s)\"];\n", nodeName, (node->isResolved ? node->resolvedFunction->name :
                                                                                 node->name));

  for (ASTNode* param : node->params)
  {
    LINK_CHILD(param);
  }

  VISIT_NEXT();
  return nodeName;
}

char* DotEmitterPass::VisitNode(VariableAssignmentNode* node, DotState* state)
{
  char* nodeName = GetNextNode(state);

  EMIT_WITH_LABEL("=");
  LINK_CHILD(node->variable);
  LINK_CHILD(node->newValue);

  VISIT_NEXT();
  return nodeName;
}

char* DotEmitterPass::VisitNode(MemberAccessNode* node, DotState* state)
{
  char* nodeName = GetNextNode(state);

  LINK_CHILD(node->parent);

  if (node->isResolved)
  {
    fprintf(state->f, "\t%s[label=\"%s.\"];\n", nodeName, node->member->name);
  }
  else
  {
    EMIT_WITH_LABEL(".");
    LINK_CHILD(node->child);
  }

  VISIT_NEXT();
  return nodeName;
}

char* DotEmitterPass::VisitNode(ArrayInitNode* node, DotState* state)
{
  char* nodeName = GetNextNode(state);

  EMIT_WITH_LABEL("{...}");

  for (ASTNode* item : node->items)
  {
    LINK_CHILD(item);
  }

  VISIT_NEXT();
  return nodeName;
}
