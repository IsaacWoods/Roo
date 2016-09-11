/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <ir.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>

node* CreateNode(node_type type, ...)
{
  va_list args;
  va_start(args, type);

  node* result = static_cast<node*>(malloc(sizeof(node)));
  result->type = type;
  result->next = nullptr;

  switch (type)
  {
    case BREAK_NODE:      break;
    case RETURN_NODE:     break;

    case BINARY_OP_NODE:
    {
      // NOTE(Isaac): enum types are promoted to `int`s on the stack
      result->payload.binaryOp.op           = static_cast<token_type>(va_arg(args, int));
      result->payload.binaryOp.left         = va_arg(args, node*);
      result->payload.binaryOp.right        = va_arg(args, node*);
    } break;

    case VARIABLE_NODE:
    {
      result->payload.variable.name         = va_arg(args, char*);
    } break;

    case CONDITION_NODE:
    {
      result->payload.condition.condition   = static_cast<token_type>(va_arg(args, int));
      result->payload.condition.left        = va_arg(args, node*);
      result->payload.condition.right       = va_arg(args, node*);
    } break;

    case IF_NODE:
    {
      result->payload.ifThing.condition     = va_arg(args, node*);
      result->payload.ifThing.thenCode      = va_arg(args, node*);
      result->payload.ifThing.elseCode      = va_arg(args, node*);
    } break;

    case NUMBER_CONSTANT_NODE:
    {
      result->payload.numberConstant.type   = static_cast<number_constant_part::constant_type>(va_arg(args, int));

      switch (result->payload.numberConstant.type)
      {
        case number_constant_part::constant_type::CONSTANT_TYPE_INT:
        {
          result->payload.numberConstant.constant.asInt = va_arg(args, int);
        } break;

        case number_constant_part::constant_type::CONSTANT_TYPE_FLOAT:
        {
          result->payload.numberConstant.constant.asFloat = static_cast<float>(va_arg(args, double));
        } break;

        default:
        {
          fprintf(stderr, "Unhandled number constant types in CreateNode!\n");
          exit(1);
        }
      }
    } break;

    default:
    {
      fprintf(stderr, "Unhandled node type in CreateNode!\n");
      exit(1);
    }
  }

  va_end(args);
  return result;
}

void FreeNode(node* n)
{
  // NOTE(Isaac): allows for easier handling of child nodes
  if (!n)
  {
    return;
  }

  switch (n->type)
  {
    case BREAK_NODE:      break;

    case RETURN_NODE:
    {
      FreeNode(n->payload.expression);
      free(n->payload.expression);
    } break;

    case BINARY_OP_NODE:
    {
      FreeNode(n->payload.binaryOp.left);
      free(n->payload.binaryOp.left);

      FreeNode(n->payload.binaryOp.right);
      free(n->payload.binaryOp.right);
    } break;

    case VARIABLE_NODE:
    {
      free(n->payload.variable.name);
    } break;

    case CONDITION_NODE:
    {
      FreeNode(n->payload.condition.left);
      free(n->payload.condition.left);

      FreeNode(n->payload.condition.right);
      free(n->payload.condition.right);
    } break;

    case IF_NODE:
    {
      FreeNode(n->payload.ifThing.condition);
      free(n->payload.ifThing.condition);

      FreeNode(n->payload.ifThing.thenCode);
      free(n->payload.ifThing.thenCode);

      FreeNode(n->payload.ifThing.elseCode);
      free(n->payload.ifThing.elseCode);
    } break;

    default:
      fprintf(stderr, "Unhandled node type in FreeNode!\n");
  }
}

void FreeTypeRef(type_ref& typeRef)
{
  free(typeRef.typeName);
}

void FreeScopeRef(scope_ref& scope)
{
  // TODO
}

void FreeVariableDef(variable_def* variable)
{
  free(variable->name);

  FreeNode(variable->initValue);
  free(variable->initValue);
}

void FreeFunctionDef(function_def* function)
{
  free(function->name);
  free(function->returnType);
  
  FreeNode(function->code);
  free(function->code);

  parameter_def* tempParam;

  while (function->firstParam)
  {
    tempParam = function->firstParam;
    function->firstParam = function->firstParam->next;
    free(tempParam);
  }

  variable_def* tempLocal;

  while (function->firstLocal)
  {
    tempLocal = function->firstLocal;
    function->firstLocal = function->firstLocal->next;
    free(tempLocal);
  }
}
