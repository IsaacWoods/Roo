/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <air.hpp>
#include <cstdio>
#include <cstdlib>
#include <cassert>
#include <cstdarg>
#include <ast.hpp>

slot* CreateSlot(air_function* function, slot::slot_type type, ...)
{
  va_list args;
  va_start(args, type);

  slot* s = static_cast<slot*>(malloc(sizeof(slot)));
  s->type = type;
  
  switch (type)
  {
    case slot::slot_type::PARAM:
    {
      s->payload.variableDef = va_arg(args, variable_def*);
      // TODO: set the tag
    } break;

    case slot::slot_type::LOCAL:
    {
      s->payload.variableDef = va_arg(args, variable_def*);
      // TODO: set the tag
    } break;

    case slot::slot_type::INTERMEDIATE:
    {
      s->payload.variableDef = va_arg(args, variable_def*);
      // TODO: set the tag
    } break;

    case slot::slot_type::INT_CONSTANT:
    {
      s->payload.i = va_arg(args, int);
      s->tag = -1;
    } break;

    case slot::slot_type::FLOAT_CONSTANT:
    {
      // NOTE(Isaac): `float` is helpfully promoted to `double` all on its own...
      s->payload.f = static_cast<float>(va_arg(args, double));
      s->tag = -1;
    } break;
  }

  // Add the slot to the function's definition
  slot_link* link = static_cast<slot_link*>(malloc(sizeof(slot_link)));
  link->s = s;
  link->next = nullptr;

  if (function->firstSlot)
  {
    slot_link* tail = function->firstSlot;

    while (tail->next)
    {
      tail = tail->next;
    }

    tail->next = link;
  }
  else
  {
    function->firstSlot = link;
  }

  va_end(args);
  return nullptr;
}

/*
 * NOTE(Isaac): because the C++ spec is written in a stupid-ass manner, the instruction type has to be a vararg.
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
T GenNodeAIR(air_function* function, air_instruction* tail, node* n);

template<>
slot* GenNodeAIR<slot*>(air_function* function, air_instruction* tail, node* n)
{
  assert(tail);
  assert(n);

  switch (n->type)
  {
    case BINARY_OP_NODE:
    {
      printf("AIR: BINARY_OP\n");
      slot* left = GenNodeAIR<slot*>(function, tail, n->payload.binaryOp.left);
      slot* right = GenNodeAIR<slot*>(function, tail, n->payload.binaryOp.right);
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
      printf("AIR: PREFIX_OP\n");
      slot* right = GenNodeAIR<slot*>(function, tail, n->payload.prefixOp.right);
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
      printf("AIR: VARIABLE\n");
      // TODO: resolve the slot of the variable we need
      return nullptr;
    } break;

    case NUMBER_CONSTANT_NODE:
    {
      switch (n->payload.numberConstant.type)
      {
        case number_constant_part::constant_type::CONSTANT_TYPE_INT:
        {
          return CreateSlot(function, slot::slot_type::INT_CONSTANT, n->payload.numberConstant.constant.i);
        } break;

        case number_constant_part::constant_type::CONSTANT_TYPE_FLOAT:
        {
          return CreateSlot(function, slot::slot_type::FLOAT_CONSTANT, n->payload.numberConstant.constant.f);
        } break;
      }
    };

    default:
    {
      fprintf(stderr, "Unhandled node type for returning a `slot*` in GenNodeAIR: %s\n", GetNodeName(n->type));
      exit(1);
    }
  }
}

template<>
jump_instruction::condition GenNodeAIR<jump_instruction::condition>(air_function* function, air_instruction* tail, node* n)
{
  assert(tail);
  assert(n);

  switch (n->type)
  {
    case CONDITION_NODE:
    {
      printf("AIR: CONDITION\n");
      slot* a = GenNodeAIR<slot*>(function, tail, n->payload.condition.left);
      slot* b = GenNodeAIR<slot*>(function, tail, n->payload.condition.right);
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
void GenNodeAIR<void>(air_function* function, air_instruction* tail, node* n)
{
  assert(tail);
  assert(n);

  switch (n->type)
  {
    case RETURN_NODE:
    {
      printf("AIR: RETURN\n");
      PushInstruction(I_LEAVE_STACK_FRAME);
      PushInstruction(I_RETURN);
    } break;

    case VARIABLE_ASSIGN_NODE:
    {
      printf("AIR: VARIABLE_ASSIGN\n");
      //slot* variableSlot = GetVariableSlot(n->payload.variableAssignment.variable);
      // TODO
      slot* variableSlot = nullptr;
      slot* newValueSlot = GenNodeAIR<slot*>(function, tail, n->payload.variableAssignment.newValue);
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
instruction_label* GenNodeAIR<instruction_label*>(air_function* function, air_instruction* tail, node* n)
{
  assert(tail);
  assert(n);

  switch (n->type)
  {
    case BREAK_NODE:
    {
      printf("AIR: BREAK\n");
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
  assert(function->air == nullptr);
  function->air = static_cast<air_function*>(malloc(sizeof(air_function)));
  function->air->firstSlot = nullptr;

  function->air->code = CreateInstruction(I_ENTER_STACK_FRAME);
  air_instruction* tail = function->air->code;

  for (node* n = function->ast;
       n;
       n = n->next)
  {
    GenNodeAIR(function->air, tail, n);
  }

  PushInstruction(I_LEAVE_STACK_FRAME);

#if 1
  unsigned int numSlots;

  for (slot_link* slot = function->air->firstSlot;
       slot;
       slot = slot->next)
  {
    numSlots++;
  }

  printf("Num slots in function: %u\n", numSlots);

  // Print all the instructions
  printf("--- AIR instruction listing for function: %s\n", function->name);

  for (air_instruction* i = function->air->code;
       i;
       i = i->next)
  {
    PrintInstruction(i);
  }
#endif
}

void FreeAIRFunction(air_function* function)
{
  while (function->firstSlot)
  {
    slot_link* temp = function->firstSlot;
    function->firstSlot = function->firstSlot->next;
    
    free(temp->s);
    free(temp);
  }

  function->firstSlot = nullptr;

  while (function->code)
  {
    air_instruction* temp = function->code;
    function->code = function->code->next;

    // Free the instruction
    free(temp->label);
    free(temp);
  }

  free(function);
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
