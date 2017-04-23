/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <ast.hpp>
#include <cstdio>
#include <cstdlib>
#include <cstdarg>
#include <cassert>
#include <common.hpp>
#include <error.hpp>

node* CreateNode(node_type type, ...)
{
  va_list args;
  va_start(args, type);

  node* result              = static_cast<node*>(malloc(sizeof(node)));
  result->type              = type;
  result->typeRef           = nullptr;
  result->shouldFreeTypeRef = false;
  result->next              = nullptr;

  error_state errorState = CreateErrorState(GENERAL_STUFF);

  switch (type)
  {
    case BREAK_NODE:
    {
    } break;

    case RETURN_NODE:
    {
      result->expression                        = va_arg(args, node*);
    } break;

    case BINARY_OP_NODE:
    {
      result->binaryOp.op                       = static_cast<token_type>(va_arg(args, int));
      result->binaryOp.left                     = va_arg(args, node*);
      result->binaryOp.right                    = va_arg(args, node*);
      result->binaryOp.resolvedOperator         = nullptr;
    } break;

    case PREFIX_OP_NODE:
    {
      result->prefixOp.op                       = static_cast<token_type>(va_arg(args, int));
      result->prefixOp.right                    = va_arg(args, node*);
      result->prefixOp.resolvedOperator         = nullptr;
    } break;

    case VARIABLE_NODE:
    {
      result->variable.name                     = va_arg(args, char*);
      result->variable.isResolved               = false;
    } break;

    case CONDITION_NODE:
    {
      result->condition.condition               = static_cast<token_type>(va_arg(args, int));
      result->condition.left                    = va_arg(args, node*);
      result->condition.right                   = va_arg(args, node*);
      result->condition.reverseOnJump           = false;
    } break;

    case BRANCH_NODE:
    {
      result->branch.condition                  = va_arg(args, node*);
      result->branch.thenCode                   = va_arg(args, node*);
      result->branch.elseCode                   = va_arg(args, node*);
    } break;

    case WHILE_NODE:
    {
      result->whileThing.condition              = va_arg(args, node*);
      result->whileThing.code                   = va_arg(args, node*);
    } break;

    case NUMBER_CONSTANT_NODE:
    {
      result->number.type                       = static_cast<number_part::constant_type>(va_arg(args, int));

      switch (result->number.type)
      {
        case number_part::constant_type::SIGNED_INT:
        {
          result->number.asSignedInt            = va_arg(args, int);
        } break;

        case number_part::constant_type::UNSIGNED_INT:
        {
          result->number.asUnsignedInt          = va_arg(args, unsigned int);
        } break;

        case number_part::constant_type::FLOAT:
        {
          result->number.asFloat                = static_cast<float>(va_arg(args, double));
        } break;
      }
    } break;

    case STRING_CONSTANT_NODE:
    {
      result->string                            = va_arg(args, string_constant*);
    } break;

    case CALL_NODE:
    {
      result->call.name                         = va_arg(args, char*);
      result->call.isResolved                   = false;
      InitVector<node*>(result->call.params);
    } break;

    case VARIABLE_ASSIGN_NODE:
    {
      result->variableAssignment.variable       = va_arg(args, node*);
      result->variableAssignment.newValue       = va_arg(args, node*);
      result->variableAssignment.ignoreImmutability = static_cast<bool>(va_arg(args, int));
    } break;

    case MEMBER_ACCESS_NODE:
    {
      result->memberAccess.parent               = va_arg(args, node*);
      result->memberAccess.child                = va_arg(args, node*);
      result->memberAccess.isResolved           = false;
    } break;

    case ARRAY_INIT_NODE:
    {
      result->arrayInit.items                   = va_arg(args, vector<node*>);
    } break;

    default:
    {
      RaiseError(errorState, ICE_UNHANDLED_NODE_TYPE, "CreateNode", GetNodeName(type));
    }
  }

  va_end(args);
  return result;
}

template<>
void Free<node*>(node*& n)
{
  assert(n);

  if (n->shouldFreeTypeRef)
  {
    free(n->typeRef);
  }

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

    case BRANCH_NODE:
    {
      Free<node*>(n->branch.condition);
      Free<node*>(n->branch.thenCode);
      Free<node*>(n->branch.elseCode);
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
        free(n->call.name);
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
        // NOTE(Isaac): don't free the variable
      }
      else
      {
        Free<node*>(n->memberAccess.child);
      }
    } break;

    case ARRAY_INIT_NODE:
    {
      FreeVector<node*>(n->arrayInit.items);
    } break;

    default:
    {
      error_state errorState = CreateErrorState(GENERAL_STUFF);
      RaiseError(errorState, ICE_UNHANDLED_NODE_TYPE, "FreeNode", GetNodeName(n->type));
    } break;
  }

  if (n->next)
  {
    Free<node*>(n->next);
  }

  free(n);
}

void ApplyPassToNode(node* n, thing_of_code* code, ast_pass& pass, parse_result& parse, error_state& errorState)
{
  assert(n->type);

  if (pass.iteratePolicy == NODE_FIRST)
  {
    if (pass.forAllNodes)
    {
      (*pass.forAllNodes)(parse, errorState, code, n);
    }

    if (pass.f[n->type])
    {
      (*pass.f[n->type])(parse, errorState, code, n);
    }
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
        ApplyPassToNode(n->expression, code, pass, parse, errorState);
      }
    } break;

    case BINARY_OP_NODE:
    {
      ApplyPassToNode(n->binaryOp.left, code, pass, parse, errorState);
      
      if (n->binaryOp.op != TOKEN_DOUBLE_PLUS &&
          n->binaryOp.op != TOKEN_DOUBLE_MINUS)
      {
        ApplyPassToNode(n->binaryOp.right, code, pass, parse, errorState);
      }
    } break;

    case PREFIX_OP_NODE:
    {
      ApplyPassToNode(n->prefixOp.right, code, pass, parse, errorState);
    } break;

    case VARIABLE_NODE:
    {
    } break;

    case CONDITION_NODE:
    {
      ApplyPassToNode(n->condition.left, code, pass, parse, errorState);
      ApplyPassToNode(n->condition.right, code, pass, parse, errorState);
    } break;

    case BRANCH_NODE:
    {
      ApplyPassToNode(n->branch.condition, code, pass, parse, errorState);
      ApplyPassToNode(n->branch.thenCode, code, pass, parse, errorState);

      if (n->branch.elseCode)
      {
        ApplyPassToNode(n->branch.elseCode, code, pass, parse, errorState);
      }
    } break;

    case WHILE_NODE:
    {
      ApplyPassToNode(n->whileThing.condition, code, pass, parse, errorState);
      ApplyPassToNode(n->whileThing.code, code, pass, parse, errorState);
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
        ApplyPassToNode(*it, code, pass, parse, errorState);
      }
    } break;

    case VARIABLE_ASSIGN_NODE:
    {
      ApplyPassToNode(n->variableAssignment.variable, code, pass, parse, errorState);
      ApplyPassToNode(n->variableAssignment.newValue, code, pass, parse, errorState);
    } break;

    case MEMBER_ACCESS_NODE:
    {
      ApplyPassToNode(n->memberAccess.parent, code, pass, parse, errorState);

      // NOTE(Isaac): we don't want to resolve inner variable defs (because they can't be resolved)
      if (n->memberAccess.child->type == MEMBER_ACCESS_NODE)
      {
        ApplyPassToNode(n->memberAccess.child, code, pass, parse, errorState);
      }
    } break;

    case ARRAY_INIT_NODE:
    {
      for (auto* it = n->arrayInit.items.head;
           it < n->arrayInit.items.tail;
           it++)
      {
        ApplyPassToNode(*it, code, pass, parse, errorState);
      }
    } break;

    default:
    {
      RaiseError(errorState, ICE_UNHANDLED_NODE_TYPE, "ApplyPassToNode", GetNodeName(n->type));
    } break;
  }

  if (pass.iteratePolicy == CHILDREN_FIRST)
  {
    if (pass.f[n->type])
    {
      (*pass.f[n->type])(parse, errorState, code, n);
    }
  }

  // Apply to next node
  if (n->next)
  {
    if (pass.forAllNodes)
    {
      (*pass.forAllNodes)(parse, errorState, code, n);
    }

    ApplyPassToNode(n->next, code, pass, parse, errorState);
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
    case BRANCH_NODE:
      return "BRANCH_NODE";
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
    case ARRAY_INIT_NODE:
      return "ARRAY_INIT_NODE";
    case NUM_AST_NODES:
      return "!!!NUM_AST_NODES!!!";
  }

  return nullptr;
}

#ifdef OUTPUT_DOT
#include <functional>
void OutputDOTOfAST(thing_of_code* code)
{
  // NOTE(Isaac): don't bother for empty functions/operators
  if (!(code->ast))
  {
    return;
  }

  unsigned int i = 0u;
  char fileName[128u] = {0};
  strcpy(fileName, code->mangledName);
  strcat(fileName, ".dot");
  FILE* f = fopen(fileName, "w");

  error_state errorState = CreateErrorState(GENERAL_STUFF);

  if (!f)
  {
    RaiseError(errorState, ERROR_FAILED_TO_OPEN_FILE, fileName);
  }

  fprintf(f, "digraph G\n{\n");

  /*
   * NOTE(Isaac): Trust me, I've tried pretty damn hard to eliminate this capture and use a normal
   * C function pointer, but I've had to resort to STD stuff because of the capture ¯\_(ツ)_/¯
   */
  std::function<char*(FILE*, node*)> emitNode = [&](FILE* f, node* n) -> char*
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

          char* expressionName = emitNode(f, n->expression);
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
            case TOKEN_LEFT_BLOCK:    fprintf(f, "\t%s[label=\"[]\"];\n", name); break;

            default:
            {
              RaiseError(errorState, ICE_UNHANDLED_OPERATOR, GetTokenName(n->binaryOp.op), "OuputDOTForAST:BINARY");
            }
          }

          char* leftName = emitNode(f, n->binaryOp.left);
          fprintf(f, "\t%s -> %s;\n", name, leftName);
          free(leftName);

          if (n->binaryOp.right)
          {
            char* rightName = emitNode(f, n->binaryOp.right);
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
            case TOKEN_AND:   fprintf(f, "\t%s[label=\"&\"];\n", name); break;

            default:
            {
              RaiseError(errorState, ICE_UNHANDLED_OPERATOR, GetTokenName(n->prefixOp.op), "OuputDOTForAST:PREFIX");
            }
          }

          char* rightName = emitNode(f, n->binaryOp.left);
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
              RaiseError(errorState, ICE_UNHANDLED_OPERATOR, "OutputDOTForAST:CONDITION");
            }
          }

          char* leftName = emitNode(f, n->binaryOp.left);
          fprintf(f, "\t%s -> %s;\n", name, leftName);
          free(leftName);

          char* rightName = emitNode(f, n->binaryOp.right);
          fprintf(f, "\t%s -> %s;\n", name, rightName);
          free(rightName);
        } break;

        case BRANCH_NODE:
        {
          fprintf(f, "\t%s[label=\"If\"];\n", name);

          char* conditionName = emitNode(f, n->branch.condition);
          fprintf(f, "\t%s -> %s;\n", name, conditionName);
          free(conditionName);

          char* thenName = emitNode(f, n->branch.thenCode);
          fprintf(f, "\t%s -> %s;\n", name, thenName);
          free(thenName);

          if (n->branch.elseCode)
          {
            char* elseName = emitNode(f, n->branch.elseCode);
            fprintf(f, "\t%s -> %s;\n", name, elseName);
            free(elseName);
          }
        } break;

        case WHILE_NODE:
        {
          fprintf(f, "\t%s[label=\"While\"];\n", name);

          char* conditionName = emitNode(f, n->whileThing.condition);
          fprintf(f, "\t%s -> %s;\n", name, conditionName);
          free(conditionName);

          char* codeName = emitNode(f, n->whileThing.code);
          fprintf(f, "\t%s -> %s;\n", name, codeName);
          free(codeName);
        } break;

        case NUMBER_CONSTANT_NODE:
        {
          switch (n->number.type)
          {
            case number_part::constant_type::SIGNED_INT:
            {
              fprintf(f, "\t%s[label=\"%d\"];\n", name, n->number.asSignedInt);
            } break;

            case number_part::constant_type::UNSIGNED_INT:
            {
              fprintf(f, "\t%s[label=\"%u\"];\n", name, n->number.asUnsignedInt);
            } break;

            case number_part::constant_type::FLOAT:
            {
              fprintf(f, "\t%s[label=\"%f\"];\n", name, n->number.asFloat);
            } break;
          }
        } break;

        case STRING_CONSTANT_NODE:
        {
          fprintf(f, "\t%s[label=\"\\\"%s\\\"\"];\n", name, n->string->string);
        } break;

        case CALL_NODE:
        {
          if (n->call.isResolved)
          {
            fprintf(f, "\t%s[label=\"Call(%s)\"];\n", name, n->call.code->mangledName);
          }
          else
          {
            fprintf(f, "\t%s[label=\"Call(%s)\"];\n", name, n->call.name);
          }
        } break;

        case VARIABLE_ASSIGN_NODE:
        {
          fprintf(f, "\t%s[label=\"=\"];\n", name);

          assert(n->variableAssignment.variable);
          char* variableName = emitNode(f, n->variableAssignment.variable);
          fprintf(f, "\t%s -> %s;\n", name, variableName);
          free(variableName);

          char* newValueName = emitNode(f, n->variableAssignment.newValue);
          fprintf(f, "\t%s -> %s;\n", name, newValueName);
          free(newValueName);
        } break;

        case MEMBER_ACCESS_NODE:
        {
          char* parentName = emitNode(f, n->memberAccess.parent);
          fprintf(f, "\t%s -> %s;\n", name, parentName);
          free(parentName);

          if (n->memberAccess.isResolved)
          {
            fprintf(f, "\t%s[label=\"%s.\"];\n", name, n->memberAccess.member->name);
          }
          else
          {
            char* childName = emitNode(f, n->memberAccess.child);
            fprintf(f, "\t%s[label=\".\"];\n", name);
            fprintf(f, "\t%s -> %s;\n", name, childName);
            free(childName);
          }
        } break;

        case ARRAY_INIT_NODE:
        {
          // TODO
        } break;

        case NUM_AST_NODES:
        {
          RaiseError(errorState, ICE_GENERIC, "Node of type NUM_AST_NODES found in actual AST!");
        } break;
      }

      // Generate the next node, and draw an edge between this node and the next
      if (n->next)
      {
        char* nextName = emitNode(f, n->next);
        fprintf(f, "\t%s -> %s[color=blue];\n", name, nextName);
        free(nextName);
      }

      return name;
    };

  free(emitNode(f, code->ast));

  fprintf(f, "}\n");
  fclose(f);
}
#endif
