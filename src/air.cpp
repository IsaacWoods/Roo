/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <air.hpp>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <cstdarg>
#include <common.hpp>
#include <ast.hpp>

// NOTE(Isaac): defined in the relevant `codegen_xxx.cpp`
unsigned int GetNumGeneralRegisters();

slot* CreateSlot(air_function* function, slot::slot_type type, ...)
{
  va_list args;
  va_start(args, type);

  slot* s = static_cast<slot*>(malloc(sizeof(slot)));
  s->type = type;
  s->numInterferences = 0u;
  memset(s->interferences, 0, sizeof(slot*) * MAX_INTERFERENCES);
  s->color = -1;
  
  switch (type)
  {
    case slot::slot_type::PARAM:
    {
      s->payload.variable = va_arg(args, variable_def*);
      // TODO: set the tag
      s->shouldBeColored = true;
    } break;

    case slot::slot_type::LOCAL:
    {
      s->payload.variable = va_arg(args, variable_def*);
      // TODO: set the tag
      s->shouldBeColored = true;
    } break;

    case slot::slot_type::INTERMEDIATE:
    {
      s->tag = function->numIntermediates++;
      s->shouldBeColored = true;
    } break;

    case slot::slot_type::INT_CONSTANT:
    {
      s->payload.i = va_arg(args, int);
      s->tag = -1;
      s->shouldBeColored = false;
    } break;

    case slot::slot_type::FLOAT_CONSTANT:
    {
      // NOTE(Isaac): `float` is helpfully promoted to `double` all on its own...
      s->payload.f = static_cast<float>(va_arg(args, double));
      s->tag = -1;
      s->shouldBeColored = false;
    } break;
  }

  AddToLinkedList<slot*>(function->slots, s);
  va_end(args);
  return s;
}

static void AddInterference(slot* a, slot* b)
{
  a->interferences[a->numInterferences++] = b;
  b->interferences[b->numInterferences++] = a;
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
      slot* result = CreateSlot(function, slot::slot_type::INTERMEDIATE);

      AddInterference(left, right);
      AddInterference(left, result);
      AddInterference(right, result);

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
instruction_label* GenNodeAIR<instruction_label*>(air_function* /*function*/, air_instruction* tail, node* n)
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

static void ColorSlots(air_function* function)
{
  assert(GetNumGeneralRegisters() > 1u);

  // Turn the linked list of slots into a nice array
  unsigned int numSlots;
  slot** slots = LinearizeLinkedList<slot*>(function->slots, &numSlots);

  // TODO: color the params the correct colors

  // Assign the first color to the first slot (that is colorable)
  for (unsigned int i = 0;
       i < numSlots;
       i++)
  {
    if (slots[i]->shouldBeColored)
    {
      slots[i] = 0u;
      goto ColoredFirst;
    }
  }

  fprintf(stderr, "No nodes needed coloring... this stinks more than Lenin probably does by now...\n");
  exit(1);

ColoredFirst:
  // TODO: color the rest
  return;
}

void GenFunctionAIR(function_def* function)
{
  assert(function->air == nullptr);
  function->air = static_cast<air_function*>(malloc(sizeof(air_function)));
  CreateLinkedList<slot*>(function->air->slots);
  function->air->numIntermediates = 0;

  function->air->code = CreateInstruction(I_ENTER_STACK_FRAME);
  air_instruction* tail = function->air->code;

  for (node* n = function->ast;
       n;
       n = n->next)
  {
    GenNodeAIR(function->air, tail, n);
  }

  PushInstruction(I_LEAVE_STACK_FRAME);

  CreateInterferenceDOT(function->air, function->name);

#if 1
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
  FreeLinkedList<slot*>(function->slots);

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

void CreateInterferenceDOT(air_function* function, const char* functionName)
{
  // Check if the function's empty
  if (function->code == nullptr)
  {
    return;
  }

  printf("--- Outputting DOT of interference graph for: %s ---\n", functionName);

  char fileName[128u] = {0};
  strcpy(fileName, functionName);
  strcat(fileName, "_interference.dot");
  FILE* f = fopen(fileName, "w");

  if (!f)
  {
    fprintf(stderr, "Failed to open DOT file: %s!\n", fileName);
    exit(1);
  }

  fprintf(f, "digraph G\n{\n");
  unsigned int i = 0u;

  const char* snazzyColors[] =
  {
    "cyan2", "deeppink", "gainsboro", "dodgerblue4", "slategray", "goldenrod", "darkorchid1", "firebrick2",
    "green3", "lightblue2", "mediumspringgreeen", "orange1", "mistyrose3", "maroon2", "mediumpurple2",
    "steelblue2", "plum", "lightseagreen"
  };

  // Emit nodes
  for (auto* slotIt = function->slots.first;
       slotIt;
       slotIt = slotIt->next)
  {
    const char* color = "black";

    if (!(**slotIt)->shouldBeColored)
    {
      color = "gray";
    }
    else
    {
      if ((**slotIt)->color < 0)
      {
        fprintf(stderr, "WARNING: found uncolored node! Using red!\n");
        color = "red";
      }
      else
      {
        color = snazzyColors[(**slotIt)->color];
      }
    }

    switch ((**slotIt)->type)
    {
      case slot::slot_type::PARAM:
      {
        fprintf(f, "\ts%u[label=\"%s : PARAM(%d)\" color=\"%s\" fontcolor=\"%s\"];\n", i, (**slotIt)->payload.variable->name, (**slotIt)->tag, color, color);
      } break;

      case slot::slot_type::LOCAL:
      {
        fprintf(f, "\ts%u[label=\"%s : LOCAL(%d)\" color=\"%s\" fontcolor=\"%s\"];\n", i, (**slotIt)->payload.variable->name, (**slotIt)->tag, color, color);
      } break;

      case slot::slot_type::INTERMEDIATE:
      {
        fprintf(f, "\ts%u[label=\"t%d : INTERMEDIATE\" color=\"%s\" fontcolor=\"%s\"];\n", i, (**slotIt)->tag, color, color);
      } break;

      case slot::slot_type::INT_CONSTANT:
      {
        fprintf(f, "\ts%u[label=\"%d : INT\" color=\"%s\" fontcolor=\"%s\"];\n", i, (**slotIt)->payload.i, color, color);
      } break;

      case slot::slot_type::FLOAT_CONSTANT:
      {
        fprintf(f, "\ts%u[label=\"%f : FLOAT\" color=\"%s\" fontcolor=\"%s\"];\n", i, (**slotIt)->payload.f, color, color);
      } break;
    }

    (**slotIt)->dotTag = i;
    i++;
  }

  // Emit the interferences between them
  for (auto* slotIt = function->slots.first;
       slotIt;
       slotIt = slotIt->next)
  {
    for (unsigned int i = 0u;
         i < (**slotIt)->numInterferences;
         i++)
    {
      // NOTE(Isaac): this is a slightly tenuous way to avoid emitting duplicates
      if ((**slotIt)->dotTag < (**slotIt)->interferences[i]->dotTag)
      {
        fprintf(f, "\ts%u -> s%u[dir=none];\n", (**slotIt)->dotTag, (**slotIt)->interferences[i]->dotTag);
      }
    }
  }

  fprintf(f, "}\n");
  fclose(f);
}
