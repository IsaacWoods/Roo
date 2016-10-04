/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <air.hpp>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstdarg>
#include <ast.hpp>

static air_instruction* CreateInstruction(instruction_type type, ...)
{
  air_instruction* i = static_cast<air_instruction*>(malloc(sizeof(air_instruction)));
  i->type = type;
  i->next = nullptr;

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
      i->payload.jump.cond                    = static_cast<jump_instruction::condition>(va_arg(args, int));
      i->payload.jump.label                   = va_arg(args, instruction_label*);
    } break;

    // NOTE(Isaac): a lot of instructions operate on two slots apparently...
    case I_MOV:
    case I_CMP:
    case I_ADD:
    case I_SUB:
    case I_MUL:
    case I_DIV:
    {
      i->payload.slotPair.a                   = va_arg(args, slot*);
      i->payload.slotPair.b                   = va_arg(args, slot*);
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

#define PUSH(instruction) \
  tail->next = instruction; \
  tail = tail->next;

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

      switch (n->payload.binaryOp.op)
      {
        case TOKEN_PLUS:
        {
          PUSH(CreateInstruction(I_ADD, left, right));
        } break;

        case TOKEN_MINUS:
        {
          PUSH(CreateInstruction(I_SUB, left, right));
        } break;

        case TOKEN_ASTERIX:
        {
          PUSH(CreateInstruction(I_MUL, left, right));
        } break;

        case TOKEN_SLASH:
        {
          PUSH(CreateInstruction(I_DIV, left, right));
        } break;

        default:
        {
          fprintf(stderr, "Unhandled AST binary op in GenNodeAIR!\n");
          exit(1);
        }
      }

      // TODO(Isaac): return the slot that the result is in
    } break;

    default:
    {
      fprintf(stderr, "Unhandled node type for returning a `slot*` in GenNodeAIR!\n");
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
      slot* a = GenNodeAIR<slot*>(tail, n->payload.condition.left);
      slot* b = GenNodeAIR<slot*>(tail, n->payload.condition.right);
      PUSH(CreateInstruction(I_CMP, a, b));

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
      fprintf(stderr, "Unhandled node type for returning a `jump_instruction::condition` in GenNodeAIR!\n");
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
      PUSH(CreateInstruction(I_LEAVE_STACK_FRAME));
      PUSH(CreateInstruction(I_RETURN));
    } break;

    default:
    {
      fprintf(stderr, "Unhandled node type for returning nothing in GenNodeAIR!\n");
      exit(1);
    }
  }

  fprintf(stderr, "Unhandled stuff and things in returnless GenNodeAIR!");
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

  PUSH(CreateInstruction(I_LEAVE_STACK_FRAME));
}

void PrintInstruction(air_instruction* instruction)
{
  if (instruction->label)
  {
    // TODO(Isaac): print more information about the label
    printf("[LABEL]: ");
  }

  switch (instruction->type)
  {
    case I_ENTER_STACK_FRAME:
    {
      printf("ENTER_STACK_FRAME\n");
    } break;

    case I_LEAVE_STACK_FRAME:
    {
      printf("LEAVE_STACK_FRAME\n");
    } break;

    default:
    {
      fprintf(stderr, "Unhandled AIR instruction type in PrintInstruction!\n");
      exit(1);
    }
  }
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
