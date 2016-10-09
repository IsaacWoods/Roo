/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <air.hpp>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstdarg>
#include <ast.hpp>

/*
 * NOTE(Isaac): because the C++11 spec is written in a stupid-ass manner, the instruction type has to be a vararg.
 * Always supply an AIR instruction type as the first vararg!
 */
#define PushInstruction(...) \
  tail->next = CreateInstruction(__VA_ARGS__); \
  tail = tail->next; \

static air_instruction* CreateInstruction(instruction_type type, ...)
{
  air_instruction* i = static_cast<air_instruction*>(malloc(sizeof(air_instruction)));
  i->type = type;
  i->label = nullptr;
  i->next = nullptr;
  air_instruction::instruction_payload& payload = i->payload;

  va_list args;
  va_start(args, type);

  switch (type)
  {
    case I_ENTER_STACK_FRAME:
    {
    } break;

    case I_LEAVE_STACK_FRAME:
    {
    } break;

    case I_RETURN:
    {
    } break;

    case I_JUMP:
    {
      payload.jump.cond                     = static_cast<jump_instruction::condition>(va_arg(args, int));
      payload.jump.label                    = va_arg(args, instruction_label*);
    } break;

    case I_MOV:
    {
      payload.mov.destination               = va_arg(args, slot*);
      payload.mov.source                    = va_arg(args, slot*);
    } break;

    case I_CMP:
    case I_ADD:
    case I_SUB:
    case I_MUL:
    case I_DIV:
    {
      payload.slotTriple.left               = va_arg(args, slot*);
      payload.slotTriple.right              = va_arg(args, slot*);
      payload.slotTriple.result             = va_arg(args, slot*);
    } break;

    case I_NEGATE:
    {
      payload.slotPair.right                = va_arg(args, slot*);
      payload.slotPair.result               = va_arg(args, slot*);
    } break;

    default:
    {
      fprintf(stderr, "Unhandled AIR instruction type in CreateInstruction!\n");
      exit(1);
    }
  }

  va_end(args);
  return i;
}

static slot* GetVariableSlot(variable_def* variable)
{
  // TODO
  return nullptr;
}

/*
 * BREAK_NODE:              `
 * RETURN_NODE:             `Nothing
 * BINARY_OP_NODE:          `slot*
 * PREFIX_OP_NODE:          `slot*
 * VARIABLE_NAME:           `slot*
 * CONDITION_NODE:          `
 * IF_NODE:                 `Nothing
 * NUMBER_NODE:             `slot*
 * STRING_CONSTANT_NODE:    `
 * FUNCTION_CALL_NODE:      `slot*      `Nothing
 * VARIABLE_ASSIGN_NODE:    `Nothing
 */

template<typename T = void>
T GenNodeAIR(air_instruction* tail, node* n);

template<>
slot* GenNodeAIR<slot*>(air_instruction* tail, node* n)
{
  assert(tail);
  assert(n);

  switch (n->type)
  {
    case BINARY_OP_NODE:
    {
      slot* left = GenNodeAIR<slot*>(tail, n->payload.binaryOp.left);
      slot* right = GenNodeAIR<slot*>(tail, n->payload.binaryOp.right);
      slot* result = static_cast<slot*>(malloc(sizeof(slot)));

      switch (n->payload.binaryOp.op)
      {
        case TOKEN_PLUS:
        {
          PushInstruction(I_ADD, left, right, result);
        } break;

        case TOKEN_MINUS:
        {
          PushInstruction(I_SUB, left, right, result);
        } break;

        case TOKEN_ASTERIX:
        {
          PushInstruction(I_MUL, left, right, result);
        } break;

        case TOKEN_SLASH:
        {
          PushInstruction(I_DIV, left, right, result);
        } break;

        default:
        {
          fprintf(stderr, "Unhandled AST binary op in GenNodeAIR!\n");
          exit(1);
        }
      }

      return result;
    } break;

    case PREFIX_OP_NODE:
    {
      slot* right = GenNodeAIR<slot*>(tail, n->payload.prefixOp.right);
      slot* result = static_cast<slot*>(malloc(sizeof(slot)));

      switch (n->payload.prefixOp.op)
      {
        case TOKEN_PLUS:
        {
          // TODO
        } break;

        case TOKEN_MINUS:
        {
          // TODO
        } break;

        case TOKEN_BANG:
        {
          // TODO
        } break;

        case TOKEN_TILDE:
        {
          PushInstruction(I_NEGATE, right, result);
        } break;

        default:
        {
          fprintf(stderr, "Unhandled AST prefix op in GenNodeAIR!\n");
          exit(1);
        }
      }

      return result;
    } break;

    case VARIABLE_NODE:
    {
      // TODO: resolve the slot of the variable we need
      return nullptr;
    } break;

    default:
    {
      fprintf(stderr, "Unhandled node type for returning a `slot*` in GenNodeAIR: %s\n", GetNodeName(n->type));
      exit(1);
    }
  }
}

template<>
jump_instruction::condition GenNodeAIR<jump_instruction::condition>(air_instruction* tail, node* n)
{
  assert(tail);
  assert(n);

  switch (n->type)
  {
    case CONDITION_NODE:
    {
      printf("Emitting compare instruction\n");
      slot* a = GenNodeAIR<slot*>(tail, n->payload.condition.left);
      slot* b = GenNodeAIR<slot*>(tail, n->payload.condition.right);
      PushInstruction(I_CMP, a, b);

      switch (n->payload.condition.condition)
      {
        case TOKEN_EQUALS_EQUALS:
        {
          return jump_instruction::condition::IF_EQUAL;
        }

        case TOKEN_BANG_EQUALS:
        {
          return jump_instruction::condition::IF_NOT_EQUAL;
        }

        case TOKEN_GREATER_THAN:
        {
          return jump_instruction::condition::IF_GREATER;
        }

        case TOKEN_GREATER_THAN_EQUAL_TO:
        {
          return jump_instruction::condition::IF_GREATER_OR_EQUAL;
        }

        case TOKEN_LESS_THAN:
        {
          return jump_instruction::condition::IF_LESSER;
        }

        case TOKEN_LESS_THAN_EQUAL_TO:
        {
          return jump_instruction::condition::IF_LESSER_OR_EQUAL;
        }

        default:
        {
          fprintf(stderr, "Unhandled AST conditional in GenNodeAIR!\n");
          exit(1);
        }
      }
    } break;

    default:
    {
      fprintf(stderr, "Unhandled node type for returning a `jump_instruction::condition` in GenNodeAIR: %s\n", GetNodeName(n->type));
      exit(1);
    }
  }
}

template<>
void GenNodeAIR<void>(air_instruction* tail, node* n)
{
  assert(tail);
  assert(n);

  switch (n->type)
  {
    case RETURN_NODE:
    {
      printf("Emitting return instruction\n");
      PushInstruction(I_LEAVE_STACK_FRAME);
      PushInstruction(I_RETURN);
    } break;

    case VARIABLE_ASSIGN_NODE:
    {
      slot* variableSlot = GetVariableSlot(n->payload.variableAssignment.variable);
      slot* newValueSlot = GenNodeAIR<slot*>(tail, n->payload.variableAssignment.newValue);
      PushInstruction(I_MOV, variableSlot, newValueSlot);
    } break;

    default:
    {
      fprintf(stderr, "Unhandled node type for returning nothing in GenNodeAIR: %s\n", GetNodeName(n->type));
      exit(1);
    }
  }
}

template<>
instruction_label* GenNodeAIR<instruction_label*>(air_instruction* tail, node* n)
{
  assert(tail);
  assert(n);

  switch (n->type)
  {
    case BREAK_NODE:
    {
      printf("Emitting break instruction\n");
      instruction_label* label = static_cast<instruction_label*>(malloc(sizeof(instruction_label)));
      PushInstruction(I_JUMP, jump_instruction::condition::UNCONDITIONAL, label);

      return label;
    } break;

    default:
    {
      fprintf(stderr, "Unhandled node type for returning a `instruction_label*` in GenNodeAIR: %s\n", GetNodeName(n->type));
      exit(1);
    }
  }

  return nullptr;
}

void GenFunctionAIR(function_def* function)
{
  assert(function->code == nullptr);

  function->code = CreateInstruction(I_ENTER_STACK_FRAME);
  air_instruction* tail = function->code;

  for (node* n = function->ast;
       n;
       n = n->next)
  {
    GenNodeAIR(tail, n);
  }

  PushInstruction(I_LEAVE_STACK_FRAME);
}

void PrintInstruction(air_instruction* instruction)
{
  if (instruction->label)
  {
    // TODO(Isaac): print more information about the label
    printf("[LABEL]: ");
  }

  printf("%s\n", GetInstructionName(instruction->type));
}

static void FreeInstructionLabel(instruction_label* label)
{
  free(label);
}

void FreeInstruction(air_instruction* instruction)
{
  if (instruction->label)
  {
    FreeInstructionLabel(instruction->label);
  }

  free(instruction);
}

const char* GetInstructionName(instruction_type type)
{
  switch (type)
  {
    case I_ENTER_STACK_FRAME:
      return "ENTER_STACK_FRAME";
    case I_LEAVE_STACK_FRAME:
      return "LEAVE_STACK_FRAME";
    case I_RETURN:
      return "RETURN";
    case I_JUMP:
      return "JUMP";
    case I_MOV:
      return "MOV";
    case I_CMP:
      return "CMP";
    case I_ADD:
      return "ADD";
    case I_SUB:
      return "SUB";
    case I_MUL:
      return "MUL";
    case I_DIV:
      return "DIV";
    case I_NEGATE:
      return "NEGATE";

    default:
    {
      fprintf(stderr, "Unhandled AIR instruction type in GetInstructionName!\n");
      exit(1);
    }
  }
}
