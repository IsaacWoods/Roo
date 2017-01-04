/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <ast.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cassert>
#include <common.hpp>

node* CreateNode(node_type type, ...)
{
  va_list args;
  va_start(args, type);

  node* result = static_cast<node*>(malloc(sizeof(node)));
  result->type = type;
  result->next = nullptr;

  switch (type)
  {
    case BREAK_NODE:
    {
    } break;

    case RETURN_NODE:
    {
      result->expression                      = va_arg(args, node*);
    } break;

    case BINARY_OP_NODE:
    {
      // NOTE(Isaac): enum types are promoted to `int`s on the stack
      result->binaryOp.op                     = static_cast<token_type>(va_arg(args, int));
      result->binaryOp.left                   = va_arg(args, node*);
      result->binaryOp.right                  = va_arg(args, node*);
    } break;

    case PREFIX_OP_NODE:
    {
      result->prefixOp.op                     = static_cast<token_type>(va_arg(args, int));
      result->prefixOp.right                  = va_arg(args, node*);
    } break;

    case VARIABLE_NODE:
    {
      result->variable.name                   = va_arg(args, char*);
      result->variable.isResolved             = false;
    } break;

    case CONDITION_NODE:
    {
      result->condition.condition             = static_cast<token_type>(va_arg(args, int));
      result->condition.left                  = va_arg(args, node*);
      result->condition.right                 = va_arg(args, node*);
      result->condition.reverseOnJump         = static_cast<bool>(va_arg(args, int));
    } break;

    case IF_NODE:
    {
      result->ifThing.condition               = va_arg(args, node*);
      result->ifThing.thenCode                = va_arg(args, node*);
      result->ifThing.elseCode                = va_arg(args, node*);
    } break;

    case WHILE_NODE:
    {
      result->whileThing.condition            = va_arg(args, node*);
      result->whileThing.code                 = va_arg(args, node*);
    } break;

    case NUMBER_CONSTANT_NODE:
    {
      result->numberConstant.type = static_cast<number_constant_part::constant_type>(va_arg(args, int));

      switch (result->numberConstant.type)
      {
        case number_constant_part::constant_type::CONSTANT_TYPE_INT:
        {
          result->numberConstant.asInt   = va_arg(args, int);
        } break;

        case number_constant_part::constant_type::CONSTANT_TYPE_FLOAT:
        {
          result->numberConstant.asFloat   = static_cast<float>(va_arg(args, double));
        } break;

        default:
        {
          fprintf(stderr, "Unhandled number constant type in CreateNode!\n");
          Crash();
        }
      }
    } break;

    case STRING_CONSTANT_NODE:
    {
      result->stringConstant                  = va_arg(args, string_constant*);
    } break;

    case CALL_NODE:
    {
      result->call.isResolved                 = false;
      result->call.type                       = static_cast<call_part::call_type>(va_arg(args, int));
      
      switch (result->call.type)
      {
        case call_part::call_type::FUNCTION:
        {
          result->call.name                   = va_arg(args, char*);
        } break;

        case call_part::call_type::OPERATOR:
        {
          result->call.op                     = static_cast<token_type>(va_arg(args, int));
        } break;
      }

      InitVector<node*>(result->call.params);
    } break;

    case VARIABLE_ASSIGN_NODE:
    {
      result->variableAssignment.variable     = va_arg(args, node*);
      result->variableAssignment.newValue     = va_arg(args, node*);
      result->variableAssignment.ignoreImmutability = static_cast<bool>(va_arg(args, int));
    } break;

    case MEMBER_ACCESS_NODE:
    {
      result->memberAccess.parent             = va_arg(args, node*);
      result->memberAccess.name               = va_arg(args, char*);
      result->memberAccess.isResolved         = false;
    } break;

    default:
    {
      fprintf(stderr, "Unhandled node type in CreateNode!\n");
      Crash();
    }
  }

  va_end(args);
  return result;
}

template<>
void Free<node*>(node*& n)
{
  assert(n);

  switch (n->type)
  {
    case BREAK_NODE:
    {
    } break;

    case RETURN_NODE:
    {
      if (n->expression)
      {
        Free<node*>(n->expression);
      }
    } break;

    case BINARY_OP_NODE:
    {
      Free<node*>(n->binaryOp.left);

      if (n->binaryOp.op != TOKEN_DOUBLE_PLUS &&
          n->binaryOp.op != TOKEN_DOUBLE_MINUS)
      {
        Free<node*>(n->binaryOp.right);
      }
    } break;

    case PREFIX_OP_NODE:
    {
      Free<node*>(n->prefixOp.right);
    } break;

    case VARIABLE_NODE:
    {
      if (n->variable.isResolved)
      {
        // NOTE(Isaac): Don't free the variable_def*; it's not ours
      }
      else
      {
        free(n->variable.name);
      }
    } break;

    case CONDITION_NODE:
    {
      Free<node*>(n->condition.left);
      Free<node*>(n->condition.right);
    } break;

    case IF_NODE:
    {
      Free<node*>(n->ifThing.condition);
      Free<node*>(n->ifThing.thenCode);
      Free<node*>(n->ifThing.elseCode);
    } break;

    case WHILE_NODE:
    {
      Free<node*>(n->whileThing.condition);
      Free<node*>(n->whileThing.code);
    } break;

    case NUMBER_CONSTANT_NODE:
    {
    } break;

    case STRING_CONSTANT_NODE:
    {
      // NOTE(Isaac): Don't free the string constant here; it might be shared!
    } break;

    case CALL_NODE:
    {
      if (!(n->call.isResolved))
      {
        switch (n->call.type)
        {
          case call_part::call_type::FUNCTION:
          {
            free(n->call.name);
          } break;

          case call_part::call_type::OPERATOR:
          {
          } break;
        }
      }

      FreeVector<node*>(n->call.params);
    } break;

    case VARIABLE_ASSIGN_NODE:
    {
      Free<node*>(n->variableAssignment.variable);
      Free<node*>(n->variableAssignment.newValue);
    } break;

    case MEMBER_ACCESS_NODE:
    {
      Free<node*>(n->memberAccess.parent);

      if (n->memberAccess.isResolved)
      {
        // TODO: don't free the variable
      }
      else
      {
        free(n->memberAccess.name);
      }
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

static void ApplyPassToNode(node* n, thing_of_code* code, ast_passlet pass[NUM_AST_NODES], parse_result& parse)
{
  assert(n->type);

  // Apply the pass to the node
  if (pass[n->type])
  {
    (*pass[n->type])(parse, code, n);
  }

  // Apply the pass to the children, recursively
  switch (n->type)
  {
    case BREAK_NODE:
    {
    } break;

    case RETURN_NODE:
    {
      if (n->expression)
      {
        ApplyPassToNode(n->expression, code, pass, parse);
      }
    } break;

    case BINARY_OP_NODE:
    {
      ApplyPassToNode(n->binaryOp.left, code, pass, parse);
      
      if (n->binaryOp.op != TOKEN_DOUBLE_PLUS &&
          n->binaryOp.op != TOKEN_DOUBLE_MINUS)
      {
        ApplyPassToNode(n->binaryOp.right, code, pass, parse);
      }
    } break;

    case PREFIX_OP_NODE:
    {
      ApplyPassToNode(n->prefixOp.right, code, pass, parse);
    } break;

    case VARIABLE_NODE:
    {
    } break;

    case CONDITION_NODE:
    {
      ApplyPassToNode(n->condition.left, code, pass, parse);
      ApplyPassToNode(n->condition.right, code, pass, parse);
    } break;

    case IF_NODE:
    {
      ApplyPassToNode(n->ifThing.condition, code, pass, parse);
      ApplyPassToNode(n->ifThing.thenCode, code, pass, parse);

      if (n->ifThing.elseCode)
      {
        ApplyPassToNode(n->ifThing.elseCode, code, pass, parse);
      }
    } break;

    case WHILE_NODE:
    {
      ApplyPassToNode(n->whileThing.condition, code, pass, parse);
      ApplyPassToNode(n->whileThing.code, code, pass, parse);
    } break;

    case NUMBER_CONSTANT_NODE:
    {
    } break;

    case STRING_CONSTANT_NODE:
    {
    } break;

    case CALL_NODE:
    {
      for (auto* it = n->call.params.head;
           it < n->call.params.tail;
           it++)
      {
        ApplyPassToNode(*it, code, pass, parse);
      }
    } break;

    case VARIABLE_ASSIGN_NODE:
    {
      ApplyPassToNode(n->variableAssignment.variable, code, pass, parse);
      ApplyPassToNode(n->variableAssignment.newValue, code, pass, parse);
    } break;

    case MEMBER_ACCESS_NODE:
    {
    } break;

    case NUM_AST_NODES:
    {
      fprintf(stderr, "ICE: Node of type NUM_AST_NODES actually in the AST!\n");
      Crash();
    } break;
  }

  // Apply to next node
  if (n->next)
  {
    ApplyPassToNode(n->next, code, pass, parse);
  }
}

void ApplyASTPass(parse_result& parse, ast_passlet pass[NUM_AST_NODES])
{
  for (auto* it = parse.functions.head;
       it < parse.functions.tail;
       it++)
  {
    function_def* function = *it;

    if (GetAttrib(function->code, attrib_type::PROTOTYPE))
    {
      continue;
    }

    if (function->code.ast)
    {
      ApplyPassToNode(function->code.ast, &(function->code), pass, parse);
    }
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
    case WHILE_NODE:
      return "WHILE_NODE";
    case NUMBER_CONSTANT_NODE:
      return "NUMBER_CONSTANT_NODE";
    case STRING_CONSTANT_NODE:
      return "STRING_CONSTANT_NODE";
    case CALL_NODE:
      return "CALL_NODE";
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
  if (function->code.ast == nullptr)
  {
    return;
  }

  unsigned int i = 0u;
  char fileName[128u] = {0};
  strcpy(fileName, function->name);
  strcat(fileName, ".dot");
  FILE* f = fopen(fileName, "w");

  if (!f)
  {
    fprintf(stderr, "Failed to open DOT file: %s!\n", fileName);
    Crash();
  }

  fprintf(f, "digraph G\n{\n");

  // NOTE(Isaac): yes, this is a recursive lambda
  // NOTE(Isaac): yeah, this uses disgusting C++ STL stuff, but function pointers aren't powerful enough so
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

          char* expressionName = EmitNode(n->expression);
          fprintf(f, "\t%s -> %s;\n", name, expressionName);
          free(expressionName);
        } break;

        case BINARY_OP_NODE:
        {
          switch (n->binaryOp.op)
          {
            case TOKEN_PLUS:          fprintf(f, "\t%s[label=\"+\"];\n",  name); break;
            case TOKEN_MINUS:         fprintf(f, "\t%s[label=\"-\"];\n",  name); break;
            case TOKEN_ASTERIX:       fprintf(f, "\t%s[label=\"*\"];\n",  name); break;
            case TOKEN_SLASH:         fprintf(f, "\t%s[label=\"/\"];\n",  name); break;
            case TOKEN_DOUBLE_PLUS:   fprintf(f, "\t%s[label=\"++\"];\n", name); break;
            case TOKEN_DOUBLE_MINUS:  fprintf(f, "\t%s[label=\"--\"];\n", name); break;

            default:
            {
              fprintf(stderr, "Unhandled binary op node in OutputDOTForAST\n");
              Crash();
            }
          }

          char* leftName = EmitNode(n->binaryOp.left);
          fprintf(f, "\t%s -> %s;\n", name, leftName);
          free(leftName);

          if (n->binaryOp.right)
          {
            char* rightName = EmitNode(n->binaryOp.right);
            fprintf(f, "\t%s -> %s;\n", name, rightName);
            free(rightName);
          }
        } break;

        case PREFIX_OP_NODE:
        {
          switch (n->prefixOp.op)
          {
            case TOKEN_PLUS:  fprintf(f, "\t%s[label=\"+\"];\n", name); break;
            case TOKEN_MINUS: fprintf(f, "\t%s[label=\"-\"];\n", name); break;
            case TOKEN_BANG:  fprintf(f, "\t%s[label=\"!\"];\n", name); break;
            case TOKEN_TILDE: fprintf(f, "\t%s[label=\"~\"];\n", name); break;
            default:
            {
              fprintf(stderr, "Unhandled prefix op node in OutputDOTForAST\n");
              Crash();
            }
          }

          char* rightName = EmitNode(n->binaryOp.left);
          fprintf(f, "\t%s -> %s;\n", name, rightName);
          free(rightName);
        } break;

        case VARIABLE_NODE:
        {
          if (n->variable.isResolved)
          {
            fprintf(f, "\t%s[label=\"`%s`\"];\n", name, n->variable.var->name);
          }
          else
          {
            fprintf(f, "\t%s[label=\"`%s`\"];\n", name, n->variable.name);
          }
        } break;

        case CONDITION_NODE:
        {
          switch (n->binaryOp.op)
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
              Crash();
            }
          }

          char* leftName = EmitNode(n->binaryOp.left);
          fprintf(f, "\t%s -> %s;\n", name, leftName);
          free(leftName);

          char* rightName = EmitNode(n->binaryOp.right);
          fprintf(f, "\t%s -> %s;\n", name, rightName);
          free(rightName);
        } break;

        case IF_NODE:
        {
          // TODO
        } break;

        case WHILE_NODE:
        {
          fprintf(f, "\t%s[label=\"While\"];\n", name);

          char* conditionName = EmitNode(n->whileThing.condition);
          fprintf(f, "\t%s -> %s;\n", name, conditionName);
          free(conditionName);

          char* codeName = EmitNode(n->whileThing.code);
          fprintf(f, "\t%s -> %s;\n", name, codeName);
          free(codeName);
        } break;

        case NUMBER_CONSTANT_NODE:
        {
          switch (n->numberConstant.type)
          {
            case number_constant_part::constant_type::CONSTANT_TYPE_INT:
            {
              fprintf(f, "\t%s[label=\"%d\"];\n", name, n->numberConstant.asInt);
            } break;

            case number_constant_part::constant_type::CONSTANT_TYPE_FLOAT:
            {
              fprintf(f, "\t%s[label=\"%f\"];\n", name, n->numberConstant.asFloat);
            } break;
          }
        } break;

        case STRING_CONSTANT_NODE:
        {
          fprintf(f, "\t%s[label=\"\"%s\"\"];\n", name, n->stringConstant->string);
        } break;

        case CALL_NODE:
        {
          switch (n->call.type)
          {
            case call_part::call_type::FUNCTION:
            {
              // TODO
//              fprintf(f, "\t%s[label=\"Call(%s)\"];\n", name, n->call.function->name);
            } break;

            case call_part::call_type::OPERATOR:
            {
              fprintf(f, "\t%s[label=\"Call(%s)\"];\n", name, GetTokenName(n->call.op));
            } break;
          }
        } break;

        case VARIABLE_ASSIGN_NODE:
        {
          fprintf(f, "\t%s[label=\"=\"];\n", name);

          assert(n->variableAssignment.variable);
          char* variableName = EmitNode(n->variableAssignment.variable);
          fprintf(f, "\t%s -> %s;\n", name, variableName);
          free(variableName);

          char* newValueName = EmitNode(n->variableAssignment.newValue);
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
          Crash();
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

  free(EmitNode(function->code.ast));

  fprintf(f, "}\n");
  fclose(f);
}
