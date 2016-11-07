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
    case slot::slot_type::LOCAL:
    {
      s->payload.variable = va_arg(args, variable_def*);
      s->shouldBeColored = true;
      s->tag = 0;

      // Set this slot to be more recent that the most recent (so it's the newest)
      if (s->payload.variable->mostRecentSlot)
      {
        s->tag = s->payload.variable->mostRecentSlot->tag + 1;
      }

      s->payload.variable->mostRecentSlot = s;
    } break;

    case slot::slot_type::INTERMEDIATE:
    {
      s->tag = function->numIntermediates++;
      s->shouldBeColored = true;
    } break;

    case slot::slot_type::IN_PARAM:
    {
      s->color = va_arg(args, signed int);
      s->shouldBeColored = true; // TODO: uhhh it's precolored, so yes or no?
      s->tag = -1; // TODO: do we need a nicer tag here?
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

    case slot::slot_type::STRING_CONSTANT:
    {
      s->payload.string = va_arg(args, string_constant*);
      s->tag = -1;
      s->shouldBeColored = false;
    } break;
  }

  AddToLinkedList<slot*>(function->slots, s);
  va_end(args);
  return s;
}

template<>
void Free<slot*>(slot*& /*slot*/)
{
}

static void AddInterference(slot* a, slot* b)
{
  a->interferences[a->numInterferences++] = b;
  b->interferences[b->numInterferences++] = a;
}

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
      payload.s                     = va_arg(args, slot*);
    } break;

    case I_JUMP:
    {
      payload.jump.cond             = static_cast<jump_instruction::condition>(va_arg(args, int));
      payload.jump.label            = va_arg(args, instruction_label*);
    } break;

    case I_MOV:
    {
      payload.mov.dest              = va_arg(args, slot*);
      payload.mov.src               =va_arg(args, slot*);
    } break;

    case I_CMP:
    case I_ADD:
    case I_SUB:
    case I_MUL:
    case I_DIV:
    {
      payload.slotTriple.left       = va_arg(args, slot*);
      payload.slotTriple.right      = va_arg(args, slot*);
      payload.slotTriple.result     = va_arg(args, slot*);
    } break;

    case I_NEGATE:
    {
      payload.slotPair.left         = va_arg(args, slot*);
      payload.slotPair.right        = va_arg(args, slot*);
    } break;

    case I_CALL:
    {
      payload.function              = va_arg(args, function_def*);
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
 * NOTE(Isaac): because the C++ spec is written in a stupid-ass manner, the instruction type has to be a vararg.
 * Always supply an AIR instruction type as the first vararg!
 */
#define PushInstruction(...) \
  function->tail->next = CreateInstruction(__VA_ARGS__); \
  function->tail = function->tail->next; \

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
T GenNodeAIR(codegen_target& target, air_function* function, node* n);

template<>
slot* GenNodeAIR<slot*>(codegen_target& target, air_function* function, node* n)
{
  assert(function->tail);
  assert(n);

  switch (n->type)
  {
    case BINARY_OP_NODE:
    {
      slot* left = GenNodeAIR<slot*>(target, function, n->payload.binaryOp.left);
      slot* right = GenNodeAIR<slot*>(target, function, n->payload.binaryOp.right);
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
      slot* right = GenNodeAIR<slot*>(target, function, n->payload.prefixOp.right);
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

    /*
     * NOTE(Isaac): at the moment, this always returns the most recent slot
     * At some point, we have to create new slots for the variable
     */
    case VARIABLE_NODE:
    {
      // If no slot exists yet, create one
      if (!n->payload.variable.var.def->mostRecentSlot)
      {
        n->payload.variable.var.def->mostRecentSlot = CreateSlot(function, slot::slot_type::PARAM, n->payload.variable.var.def);
      }

      assert(n->payload.variable.isResolved);
      return n->payload.variable.var.def->mostRecentSlot;
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

    case STRING_CONSTANT_NODE:
    {
      return CreateSlot(function, slot::slot_type::STRING_CONSTANT, n->payload.stringConstant);
    } break;

    default:
    {
      fprintf(stderr, "Unhandled node type for returning a `slot*` in GenNodeAIR: %s\n", GetNodeName(n->type));
      exit(1);
    }
  }
}

template<>
jump_instruction::condition GenNodeAIR<jump_instruction::condition>(codegen_target& target, air_function* function,
                                                                    node* n)
{
  assert(function->tail);
  assert(n);

  switch (n->type)
  {
    case CONDITION_NODE:
    {
      slot* a = GenNodeAIR<slot*>(target, function, n->payload.condition.left);
      slot* b = GenNodeAIR<slot*>(target, function, n->payload.condition.right);
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
void GenNodeAIR<void>(codegen_target& target, air_function* function, node* n)
{
  assert(function->tail);
  assert(n);

  switch (n->type)
  {
    case RETURN_NODE:
    {
      slot* returnValue = nullptr;

      if (n->payload.expression)
      {
        returnValue = GenNodeAIR<slot*>(target, function, n->payload.expression);
      }

      PushInstruction(I_LEAVE_STACK_FRAME);
      PushInstruction(I_RETURN, returnValue);
    } break;

    case VARIABLE_ASSIGN_NODE:
    {
      // TODO: don't assume it's a local (could be a param?)
      slot* variableSlot = CreateSlot(function, slot::slot_type::LOCAL, n->payload.variableAssignment.variable);
      slot* newValueSlot = GenNodeAIR<slot*>(target, function, n->payload.variableAssignment.newValue);
      AddInterference(variableSlot, newValueSlot);

      PushInstruction(I_MOV, variableSlot, newValueSlot);
    } break;

    case FUNCTION_CALL_NODE:
    {
      // TODO: don't assume everything will fit in a general register
      unsigned int numGeneralParams = 0u;
      for (auto* paramIt = n->payload.functionCall.params.first;
           paramIt;
           paramIt = paramIt->next)
      {
        slot* paramSlot = CreateSlot(function, slot::slot_type::IN_PARAM, target.intParamColors[numGeneralParams++]);
        slot* valueSlot = GenNodeAIR<slot*>(target, function, **paramIt);
        AddInterference(paramSlot, valueSlot);
        PushInstruction(I_MOV, paramSlot, valueSlot);
      }

      assert(n->payload.functionCall.isResolved);
      PushInstruction(I_CALL, n->payload.functionCall.function.def);
    } break;

    default:
    {
      fprintf(stderr, "Unhandled node type for returning nothing in GenNodeAIR: %s\n", GetNodeName(n->type));
      exit(1);
    }
  }
}

template<>
instruction_label* GenNodeAIR<instruction_label*>(codegen_target& target, air_function* function, node* n)
{
  assert(function->tail);
  assert(n);

  switch (n->type)
  {
    case BREAK_NODE:
    {
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

/*
 * Used by the AIR generator to color the interference graph to allocate slots to registers.
 */
void ColorSlots(codegen_target& target, air_function* function)
{
  const unsigned int numGeneralRegisters = 14u;
//  unsigned int numSlots;
//  slot** slots = LinearizeLinkedList<slot*>(function->slots, &numSlots);

  // Color params
  unsigned int intParamCounter = 0u;

  for (auto* slotIt = function->slots.first;
       slotIt;
       slotIt = slotIt->next)
  {
    if ((**slotIt)->type == slot::slot_type::PARAM)
    {
      printf("Coloring param slot: %s\n", (**slotIt)->payload.variable->name);
      (**slotIt)->color = target.intParamColors[intParamCounter++];
    }
  }

  // Color all colorable slots
  bool isColorUsed[numGeneralRegisters] = {0};

  for (auto* slotIt = function->slots.first;
       slotIt;
       slotIt = slotIt->next)
  {
    // Skip if uncolorable or already colored
    if ((!(**slotIt)->shouldBeColored) || (**slotIt)->color != -1)
    {
      continue;
    }

    // Find colors already used by interferring nodes
    bool usedColors[numGeneralRegisters] = {0};
    for (unsigned int i = 0u;
         i < (**slotIt)->numInterferences;
         i++)
    {
      signed int interference = (**slotIt)->interferences[i]->color;

      if (interference != -1)
      {
        usedColors[interference] = true;
      }
    }

    // Choose a free color
    for (unsigned int i = 0u;
         i < numGeneralRegisters;
         i++)
    {
      if (!usedColors[i])
      {
        (**slotIt)->color = static_cast<signed int>(i);
        break;
      }
    }

    if ((**slotIt)->color == -1)
    {
      // TODO: spill something instead of crashing
      fprintf(stderr, "FATAL: failed to find valid k-coloring of interference graph!\n");
      exit(1);
    }
  }
}

void GenFunctionAIR(codegen_target& target, function_def* functionDef)
{
  assert(functionDef->air == nullptr);

  air_function* function = functionDef->air = static_cast<air_function*>(malloc(sizeof(air_function)));
  CreateLinkedList<slot*>(function->slots);
  function->numIntermediates = 0;

  function->code = CreateInstruction(I_ENTER_STACK_FRAME);
  function->tail = function->code;

  for (node* n = functionDef->ast;
       n;
       n = n->next)
  {
    GenNodeAIR(target, function, n);
  }

  if (functionDef->shouldAutoReturn)
  {
    PushInstruction(I_LEAVE_STACK_FRAME);
    PushInstruction(I_RETURN, nullptr);
  }

  // Color the interference graph
  ColorSlots(target, functionDef->air);

#if 1
  CreateInterferenceDOT(function, functionDef->name);

  // Print all the instructions
  printf("--- AIR instruction listing for function: %s\n", functionDef->name);

  for (air_instruction* i = function->code;
       i;
       i = i->next)
  {
    PrintInstruction(i);
  }
#endif
}

template<>
void Free<air_function*>(air_function*& function)
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

static char* GetSlotString(slot* s)
{
  char* result;

  switch (s->type)
  {
    case slot::slot_type::PARAM:
    {
      result = static_cast<char*>(malloc(snprintf(nullptr, 0u, "%s(P)", s->payload.variable->name)));
      sprintf(result, "%s(P)", s->payload.variable->name);
    } break;

    case slot::slot_type::LOCAL:
    {
      result = static_cast<char*>(malloc(snprintf(nullptr, 0u, "%s(L)", s->payload.variable->name)));
      sprintf(result, "%s(L)", s->payload.variable->name);
    } break;

    case slot::slot_type::IN_PARAM:
    {
      result = static_cast<char*>(malloc(snprintf(nullptr, 0u, "%d(IN)", s->color)));
      sprintf(result, "%d(IN)", s->color);
    } break;

    case slot::slot_type::INTERMEDIATE:
    {
      result = static_cast<char*>(malloc(snprintf(nullptr, 0u, "i%u", s->tag)));
      sprintf(result, "i%u", s->tag);
    } break;

    case slot::slot_type::INT_CONSTANT:
    {
      result = static_cast<char*>(malloc(snprintf(nullptr, 0u, "#%d", s->payload.i)));
      sprintf(result, "#%d", s->payload.i);
    } break;

    case slot::slot_type::FLOAT_CONSTANT:
    {
      result = static_cast<char*>(malloc(snprintf(nullptr, 0u, "#%f", s->payload.f)));
      sprintf(result, "#%f", s->payload.f);
    } break;

    case slot::slot_type::STRING_CONSTANT:
    {
      result = static_cast<char*>(malloc(snprintf(nullptr, 0u, "\"%s\"", s->payload.string->string)));
      sprintf(result, "\"%s\"", s->payload.string->string);
    } break;
  }

  return result;
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
    case I_LEAVE_STACK_FRAME:
    case I_NUM_INSTRUCTIONS:
    {
      printf("%s\n", GetInstructionName(instruction->type));
    } break;

    case I_RETURN:
    {
      char* returnValue = nullptr;

      if (instruction->payload.s)
      {
        returnValue = GetSlotString(instruction->payload.s);
      }

      printf("RETURN %s\n", returnValue);
      free(returnValue);
    } break;

    case I_JUMP:
    {

    } break;

    case I_MOV:
    {
      char* srcSlot = GetSlotString(instruction->payload.mov.src);
      char* destSlot = GetSlotString(instruction->payload.mov.dest);
      printf("%s -> %s\n", srcSlot, destSlot);
      free(srcSlot);
      free(destSlot);
    } break;

    case I_CMP:
    {
      char* leftSlot = GetSlotString(instruction->payload.slotPair.left);
      char* rightSlot = GetSlotString(instruction->payload.slotPair.right);
      printf("CMP %s, %s\n", leftSlot, rightSlot);
      free(leftSlot);
      free(rightSlot);
    } break;

    case I_ADD:
    {
      char* leftSlot = GetSlotString(instruction->payload.slotTriple.left);
      char* rightSlot = GetSlotString(instruction->payload.slotTriple.right);
      char* resultSlot = GetSlotString(instruction->payload.slotTriple.result);
      printf("%s := %s + %s\n", resultSlot, leftSlot, rightSlot);
      free(leftSlot);
      free(rightSlot);
      free(resultSlot);
    } break;

    case I_SUB:
    {
      char* leftSlot = GetSlotString(instruction->payload.slotTriple.left);
      char* rightSlot = GetSlotString(instruction->payload.slotTriple.right);
      char* resultSlot = GetSlotString(instruction->payload.slotTriple.result);
      printf("%s := %s - %s\n", resultSlot, leftSlot, rightSlot);
      free(leftSlot);
      free(rightSlot);
      free(resultSlot);
    } break;

    case I_MUL:
    {
      char* leftSlot = GetSlotString(instruction->payload.slotTriple.left);
      char* rightSlot = GetSlotString(instruction->payload.slotTriple.right);
      char* resultSlot = GetSlotString(instruction->payload.slotTriple.result);
      printf("%s := %s * %s\n", resultSlot, leftSlot, rightSlot);
      free(leftSlot);
      free(rightSlot);
      free(resultSlot);
    } break;

    case I_DIV:
    {
      char* leftSlot = GetSlotString(instruction->payload.slotTriple.left);
      char* rightSlot = GetSlotString(instruction->payload.slotTriple.right);
      char* resultSlot = GetSlotString(instruction->payload.slotTriple.result);
      printf("%s := %s / %s\n", resultSlot, leftSlot, rightSlot);
      free(leftSlot);
      free(rightSlot);
      free(resultSlot);
    } break;

    case I_NEGATE:
    {

    } break;

    case I_CALL:
    {
      printf("CALL %s\n", instruction->payload.function->name);
    } break;
  }
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
    "cyan2", "deeppink", "gainsboro", "dodgerblue4", "slategray", "goldenrod", "darkorchid1", "blue",
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

      case slot::slot_type::IN_PARAM:
      {
        fprintf(f, "\ts%u[label=\"IN\" color=\"%s\" fontcolor=\"%s\"];\n", i, color, color);
      } break;

      case slot::slot_type::INT_CONSTANT:
      {
        fprintf(f, "\ts%u[label=\"%d : INT\" color=\"%s\" fontcolor=\"%s\"];\n", i, (**slotIt)->payload.i, color, color);
      } break;

      case slot::slot_type::FLOAT_CONSTANT:
      {
        fprintf(f, "\ts%u[label=\"%f : FLOAT\" color=\"%s\" fontcolor=\"%s\"];\n", i, (**slotIt)->payload.f, color, color);
      } break;

      case slot::slot_type::STRING_CONSTANT:
      {
        fprintf(f, "\ts%u[label=\"\\\"%s\\\" : STRING\" color=\"%s\" fontcolor=\"%s\"];\n", i, (**slotIt)->payload.string->string, color, color);
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
