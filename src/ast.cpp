/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <ast.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <common.hpp>

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
      payload.variable.var.name           = va_arg(args, char*);
      payload.variable.isResolved         = false;
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
          payload.numberConstant.constant.i = va_arg(args, int);
        } break;

        case number_constant_part::constant_type::CONSTANT_TYPE_FLOAT:
        {
          payload.numberConstant.constant.f = static_cast<float>(va_arg(args, double));
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
      payload.functionCall.isResolved     = false;
      payload.functionCall.function.name  = va_arg(args, char*);
      CreateLinkedList<node*>(payload.functionCall.params);
    } break;

    case VARIABLE_ASSIGN_NODE:
    {
      payload.variableAssignment.variableName = va_arg(args, char*);
      payload.variableAssignment.newValue     = va_arg(args, node*);
      payload.variableAssignment.variable     = nullptr;
    } break;

    case MEMBER_ACCESS_NODE:
    {
      payload.memberAccess.parent             = va_arg(args, node*);
      payload.memberAccess.memberName         = va_arg(args, char*);
      payload.memberAccess.variable           = nullptr;
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

template<>
void Free<node*>(node*& n)
{
  // NOTE(Isaac): allows for easier handling of child nodes
  if (!n)
  {
    fprintf(stderr, "WARNING: relying on crappy method of not freeing null AST nodes!\n");
    return;
  }

  switch (n->type)
  {
    case BREAK_NODE:
    {
    } break;

    case RETURN_NODE:
    {
      if (n->payload.expression)
      {
        Free<node*>(n->payload.expression);
      }
    } break;

    case BINARY_OP_NODE:
    {
      Free<node*>(n->payload.binaryOp.left);
      Free<node*>(n->payload.binaryOp.right);
    } break;

    case PREFIX_OP_NODE:
    {
      Free<node*>(n->payload.prefixOp.right);
    } break;

    case VARIABLE_NODE:
    {
      if (n->payload.variable.isResolved)
      {
        // NOTE(Isaac): Don't free the variable_def*; it's not ours
      }
      else
      {
        free(n->payload.variable.var.name);
      }
    } break;

    case CONDITION_NODE:
    {
      Free<node*>(n->payload.condition.left);
      Free<node*>(n->payload.condition.right);
    } break;

    case IF_NODE:
    {
      Free<node*>(n->payload.ifThing.condition);
      Free<node*>(n->payload.ifThing.thenCode);
      Free<node*>(n->payload.ifThing.elseCode);
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
      if (n->payload.functionCall.isResolved)
      {
        // NOTE(Isaac): Don't free the function_def*; it belongs to someone else
      }
      else
      {
        free(n->payload.functionCall.function.name);
      }

      FreeLinkedList<node*>(n->payload.functionCall.params);
    } break;

    case VARIABLE_ASSIGN_NODE:
    {
      free(n->payload.variableAssignment.variableName);
      Free<node*>(n->payload.variableAssignment.newValue);
    } break;

    case MEMBER_ACCESS_NODE:
    {
      Free<node*>(n->payload.memberAccess.parent);
      free(n->payload.memberAccess.memberName);
      Free<variable_def*>(n->payload.memberAccess.variable);
    } break;

    case NUM_AST_NODES:
    {
      fprintf(stderr, "Node of type NUM_AST_NODES found in actual AST!\n");
    } break;
  }

  if (n->next)
  {
    Free<node*>(n->next);
  }

  free(n);
}

static void ApplyPassToNode(node* n, function_def* function, ast_passlet pass[NUM_AST_NODES], parse_result& parse)
{
  // Apply the pass to the node
  if (pass[n->type])
  {
    (*pass[n->type])(parse, function, n);
  }

  // Apply the pass to the children, recursively
  switch (n->type)
  {
    case BREAK_NODE:
    {
    } break;

    case RETURN_NODE:
    {
      if (n->payload.expression)
      {
        ApplyPassToNode(n->payload.expression, function, pass, parse);
      }
    } break;

    case BINARY_OP_NODE:
    {
      ApplyPassToNode(n->payload.binaryOp.left, function, pass, parse);
      ApplyPassToNode(n->payload.binaryOp.right, function, pass, parse);
    } break;

    case PREFIX_OP_NODE:
    {
      ApplyPassToNode(n->payload.prefixOp.right, function, pass, parse);
    } break;

    case VARIABLE_NODE:
    {
    } break;

    case CONDITION_NODE:
    {
      ApplyPassToNode(n->payload.condition.left, function, pass, parse);
      ApplyPassToNode(n->payload.condition.right, function, pass, parse);
    } break;

    case IF_NODE:
    {
      ApplyPassToNode(n->payload.ifThing.condition, function, pass, parse);
      ApplyPassToNode(n->payload.ifThing.thenCode, function, pass, parse);

      if (n->payload.ifThing.elseCode)
      {
        ApplyPassToNode(n->payload.ifThing.elseCode, function, pass, parse);
      }
    } break;

    case NUMBER_CONSTANT_NODE:
    {
    } break;

    case STRING_CONSTANT_NODE:
    {
    } break;

    case FUNCTION_CALL_NODE:
    {
      for (auto* paramIt = n->payload.functionCall.params.first;
           paramIt;
           paramIt = paramIt->next)
      {
        ApplyPassToNode(**paramIt, function, pass, parse);
      }
    } break;

    case VARIABLE_ASSIGN_NODE:
    {
      ApplyPassToNode(n->payload.variableAssignment.newValue, function, pass, parse);
    } break;

    case MEMBER_ACCESS_NODE:
    {
    } break;

    case NUM_AST_NODES:
    {
      fprintf(stderr, "Node of type NUM_AST_NODES actually in the AST!\n");
      exit(1);
    } break;
  }

  // Apply to next node
  if (n->next)
  {
    ApplyPassToNode(n->next, function, pass, parse);
  }
}

void ApplyASTPass(parse_result& parse, ast_passlet pass[NUM_AST_NODES])
{
  for (auto* functionIt = parse.functions.first;
       functionIt;
       functionIt = functionIt->next)
  {
    if (GetAttrib(**functionIt, function_attrib::attrib_type::PROTOTYPE))
    {
      continue;
    }

    ApplyPassToNode((**functionIt)->ast, **functionIt, pass, parse);
  }
}

const char* GetNodeName(node_type type)
{
  switch (type)
  {
    case BREAK_NODE:
      return "BREAK_NODE";
    case RETURN_NODE:
      return "RETURN_NODE";
    case BINARY_OP_NODE:
      return "BINARY_OP_NODE";
    case PREFIX_OP_NODE:
      return "PREFIX_OP_NODE";
    case VARIABLE_NODE:
      return "VARIABLE_NODE";
    case CONDITION_NODE:
      return "CONDITION_NODE";
    case IF_NODE:
      return "IF_NODE";
    case NUMBER_CONSTANT_NODE:
      return "NUMBER_CONSTANT_NODE";
    case STRING_CONSTANT_NODE:
      return "STRING_CONSTANT_NODE";
    case FUNCTION_CALL_NODE:
      return "FUNCTION_CALL_NODE";
    case VARIABLE_ASSIGN_NODE:
      return "VARIABLE_ASSIGN_NODE";
    case MEMBER_ACCESS_NODE:
      return "MEMBER_ACCESS_NODE";
    case NUM_AST_NODES:
      return "!!!NUM_AST_NODES!!!";
  }

  return nullptr;
}

#include <cstring>
#include <cstdlib>
#include <cassert>
#include <functional>

/*
 * Outputs a DOT file of a function's AST.
 * NOTE(Isaac): The code of the DOT is not guaranteed to be pretty.
 */
void OutputDOTOfAST(function_def* function)
{
  // Check if the function's empty
  if (function->ast == nullptr)
  {
    return;
  }

  printf("--- Outputting DOT file for function: %s ---\n", function->name);

  unsigned int i = 0u;
  char fileName[128u] = {0};
  strcpy(fileName, function->name);
  strcat(fileName, ".dot");
  FILE* f = fopen(fileName, "w");

  if (!f)
  {
    fprintf(stderr, "Failed to open DOT file: %s!\n", fileName);
    exit(1);
  }

  fprintf(f, "digraph G\n{\n");

  // NOTE(Isaac): yes, this is a recursive lambda
  // NOTE(Isaac): yeah, this also uses disgusting C++ STL stuff, but function pointers aren't powerful enough so
  std::function<char*(node*)> EmitNode =
    [&](node* n) -> char*
    {
      assert(n);

      // Generate a new node name
      // NOTE(Isaac): it's possible this could overflow with high values of i
      char* name = static_cast<char*>(malloc(sizeof(char) * 16u));
      name[0] = 'n';
      itoa(i++, &(name[1]), 10);

      // Generate the node and its children
      switch (n->type)
      {
        case BREAK_NODE:
        {
          fprintf(f, "\t%s[label=\"Break\"];\n", name);
        } break;

        case RETURN_NODE:
        {
          fprintf(f, "\t%s[label=\"Return\"];\n", name);

          char* expressionName = EmitNode(n->payload.expression);
          fprintf(f, "\t%s -> %s;\n", name, expressionName);
          free(expressionName);
        } break;

        case BINARY_OP_NODE:
        {
          switch (n->payload.binaryOp.op)
          {
            case TOKEN_PLUS:      fprintf(f, "\t%s[label=\"+\"];\n", name); break;
            case TOKEN_MINUS:     fprintf(f, "\t%s[label=\"-\"];\n", name); break;
            case TOKEN_ASTERIX:   fprintf(f, "\t%s[label=\"*\"];\n", name); break;
            case TOKEN_SLASH:     fprintf(f, "\t%s[label=\"/\"];\n", name); break;
            default:
            {
              fprintf(stderr, "Unhandled binary op node in OutputDOTForAST\n");
              exit(1);
            }
          }

          char* leftName = EmitNode(n->payload.binaryOp.left);
          fprintf(f, "\t%s -> %s;\n", name, leftName);
          free(leftName);

          char* rightName = EmitNode(n->payload.binaryOp.right);
          fprintf(f, "\t%s -> %s;\n", name, rightName);
          free(rightName);
        } break;

        case PREFIX_OP_NODE:
        {
          switch (n->payload.prefixOp.op)
          {
            case TOKEN_PLUS:  fprintf(f, "\t%s[label=\"+\"];\n", name); break;
            case TOKEN_MINUS: fprintf(f, "\t%s[label=\"-\"];\n", name); break;
            case TOKEN_BANG:  fprintf(f, "\t%s[label=\"!\"];\n", name); break;
            case TOKEN_TILDE: fprintf(f, "\t%s[label=\"~\"];\n", name); break;
            default:
            {
              fprintf(stderr, "Unhandled prefix op node in OutputDOTForAST\n");
              exit(1);
            }
          }

          char* rightName = EmitNode(n->payload.binaryOp.left);
          fprintf(f, "\t%s -> %s;\n", name, rightName);
          free(rightName);
        } break;

        case VARIABLE_NODE:
        {
          if (n->payload.variable.isResolved)
          {
            fprintf(f, "\t%s[label=\"`%s`\"];\n", name, n->payload.variable.var.def->name);
          }
          else
          {
            fprintf(f, "\t%s[label=\"`%s`\"];\n", name, n->payload.variable.var.name);
          }
        } break;

        case CONDITION_NODE:
        {
          switch (n->payload.binaryOp.op)
          {
            case TOKEN_EQUALS_EQUALS:           fprintf(f, "\t%s[label=\"==\"];\n", name); break;
            case TOKEN_BANG_EQUALS:             fprintf(f, "\t%s[label=\"!=\"];\n", name); break;
            case TOKEN_GREATER_THAN:            fprintf(f, "\t%s[label=\">\"];\n",  name); break;
            case TOKEN_GREATER_THAN_EQUAL_TO:   fprintf(f, "\t%s[label=\">=\"];\n", name); break;
            case TOKEN_LESS_THAN:               fprintf(f, "\t%s[label=\"<\"];\n",  name); break;
            case TOKEN_LESS_THAN_EQUAL_TO:      fprintf(f, "\t%s[label=\"<=\"];\n", name); break;
            default:
            {
              fprintf(stderr, "Unhandled binary op node in OutputDOTForAST\n");
              exit(1);
            }
          }

          char* leftName = EmitNode(n->payload.binaryOp.left);
          fprintf(f, "\t%s -> %s;\n", name, leftName);
          free(leftName);

          char* rightName = EmitNode(n->payload.binaryOp.right);
          fprintf(f, "\t%s -> %s;\n", name, rightName);
          free(rightName);
        } break;

        case IF_NODE:
        {

        } break;

        case NUMBER_CONSTANT_NODE:
        {
          switch (n->payload.numberConstant.type)
          {
            case number_constant_part::constant_type::CONSTANT_TYPE_INT:
            {
              fprintf(f, "\t%s[label=\"%d\"];\n", name, n->payload.numberConstant.constant.i);
            } break;

            case number_constant_part::constant_type::CONSTANT_TYPE_FLOAT:
            {
              fprintf(f, "\t%s[label=\"%f\"];\n", name, n->payload.numberConstant.constant.f);
            } break;
          }
        } break;

        case STRING_CONSTANT_NODE:
        {
          fprintf(f, "\t%s[label=\"\"%s\"\"];\n", name, n->payload.stringConstant->string);
        } break;

        case FUNCTION_CALL_NODE:
        {

        } break;

        case VARIABLE_ASSIGN_NODE:
        {
//          assert(n->payload.variableAssignment.variable);

          fprintf(f, "\t%s[label=\"`%s`=\"];\n", name, n->payload.variableAssignment.variableName);

          char* newValueName = EmitNode(n->payload.variableAssignment.newValue);
          fprintf(f, "\t%s -> %s;\n", name, newValueName);
          free(newValueName);
        } break;

        case MEMBER_ACCESS_NODE:
        {
          // TODO
        } break;

        case NUM_AST_NODES:
        {
          fprintf(stderr, "Node of type NUM_AST_NODES found in actual AST!\n");
          exit(1);
        } break;
      }

      // Generate the next node, and draw an edge between this node and the next
      if (n->next)
      {
        char* nextName = EmitNode(n->next);
        fprintf(f, "\t%s -> %s[color=blue];\n", name, nextName);
        free(nextName);
      }

      return name;
    };

  free(EmitNode(function->ast));

  fprintf(f, "}\n");
  fclose(f);
}
