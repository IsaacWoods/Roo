/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <ast.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>

void CreateParseResult(parse_result& result)
{
  result.firstDependency  = nullptr;
  result.firstFunction    = nullptr;
  result.firstType        = nullptr;
  result.firstString      = nullptr;
}

void FreeParseResult(parse_result& result)
{
  while (result.firstDependency)
  {
    dependency_def* temp = result.firstDependency;
    FreeDependencyDef(temp);
    result.firstDependency = result.firstDependency->next;
    free(temp);
  }

  while (result.firstFunction)
  {
    function_def* temp = result.firstFunction;
    FreeFunctionDef(temp);
    result.firstFunction = result.firstFunction->next;
    free(temp);
  }

  while (result.firstType)
  {
    type_def* temp = result.firstType;
    FreeTypeDef(temp);
    result.firstType = result.firstType->next;
    free(temp);
  }

  while (result.firstString)
  {
    string_constant* temp = result.firstString;
    FreeStringConstant(temp);
    result.firstString = result.firstString->next;
    free(temp);
  }

  result.firstDependency  = nullptr;
  result.firstFunction    = nullptr;
  result.firstType        = nullptr;
  result.firstString      = nullptr;
}

void FreeDependencyDef(dependency_def* dependency)
{
  free(dependency->path);
}

string_constant* CreateStringConstant(parse_result* result, char* string)
{
  string_constant* constant = static_cast<string_constant*>(malloc(sizeof(string_constant)));
  string_constant* tail = result->firstString;

  while (tail->next)
  {
    tail = tail->next;
  }

  constant->handle = tail->handle + 1u;
  constant->string = string;
  constant->next = nullptr;

  tail->next = constant;
  return constant;
}

void FreeStringConstant(string_constant* string)
{
  free(string->string);
}

node* CreateNode(node_type type, ...)
{
  va_list args;
  va_start(args, type);

  node* result = static_cast<node*>(malloc(sizeof(node)));
  result->type = type;
  result->next = nullptr;

  node::node_payload& payload = result->payload;

  switch (type)
  {
    case BREAK_NODE:
    {
    } break;

    case RETURN_NODE:
    {
      payload.expression                  = va_arg(args, node*);
    } break;

    case BINARY_OP_NODE:
    {
      // NOTE(Isaac): enum types are promoted to `int`s on the stack
      payload.binaryOp.op                 = static_cast<token_type>(va_arg(args, int));
      payload.binaryOp.left               = va_arg(args, node*);
      payload.binaryOp.right              = va_arg(args, node*);
    } break;

    case PREFIX_OP_NODE:
    {
      payload.prefixOp.op                 = static_cast<token_type>(va_arg(args, int));
      payload.prefixOp.right              = va_arg(args, node*);
    } break;

    case VARIABLE_NODE:
    {
      payload.variable.name               = va_arg(args, char*);
    } break;

    case CONDITION_NODE:
    {
      payload.condition.condition         = static_cast<token_type>(va_arg(args, int));
      payload.condition.left              = va_arg(args, node*);
      payload.condition.right             = va_arg(args, node*);
    } break;

    case IF_NODE:
    {
      payload.ifThing.condition           = va_arg(args, node*);
      payload.ifThing.thenCode            = va_arg(args, node*);
      payload.ifThing.elseCode            = va_arg(args, node*);
    } break;

    case NUMBER_CONSTANT_NODE:
    {
      payload.numberConstant.type         = static_cast<number_constant_part::constant_type>(va_arg(args, int));

      switch (payload.numberConstant.type)
      {
        case number_constant_part::constant_type::CONSTANT_TYPE_INT:
        {
          payload.numberConstant.constant.asInt = va_arg(args, int);
        } break;

        case number_constant_part::constant_type::CONSTANT_TYPE_FLOAT:
        {
          payload.numberConstant.constant.asFloat = static_cast<float>(va_arg(args, double));
        } break;

        default:
        {
          fprintf(stderr, "Unhandled number constant type in CreateNode!\n");
          exit(1);
        }
      }
    } break;

    case STRING_CONSTANT_NODE:
    {
      payload.stringConstant              = va_arg(args, string_constant*);
    } break;

    case FUNCTION_CALL_NODE:
    {
      payload.functionCall.name           = va_arg(args, char*);
      payload.functionCall.firstParam     = va_arg(args, function_call_part::param_def*);
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
    case BREAK_NODE:
    {
    } break;

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

    case PREFIX_OP_NODE:
    {
      FreeNode(n->payload.prefixOp.right);
      free(n->payload.prefixOp.right);
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

    case NUMBER_CONSTANT_NODE:
    {
    } break;

    case STRING_CONSTANT_NODE:
    {
      // NOTE(Isaac): Don't free the string constant here; it might be shared!
    } break;

    case FUNCTION_CALL_NODE:
    {
      free(n->payload.functionCall.name);

      while (n->payload.functionCall.firstParam)
      {
        function_call_part::param_def* temp = n->payload.functionCall.firstParam;
        n->payload.functionCall.firstParam = n->payload.functionCall.firstParam->next;
        free(temp->expression);
      }

      n->payload.functionCall.firstParam = nullptr;
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

  while (function->firstParam)
  {
    variable_def* temp = function->firstParam;
    function->firstParam = function->firstParam->next;
    FreeVariableDef(temp);
    free(temp);
  }

  while (function->firstLocal)
  {
    variable_def* temp = function->firstLocal;
    function->firstLocal = function->firstLocal->next;
    FreeVariableDef(temp);
    free(temp);
  }
}

void FreeTypeDef(type_def* type)
{
  free(type->name);
  
  while (type->firstMember)
  {
    variable_def* temp = type->firstMember;
    type->firstMember = type->firstMember->next;
    FreeVariableDef(temp);
    free(temp);
  }

  type->firstMember = nullptr;
}
