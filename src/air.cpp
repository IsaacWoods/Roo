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
  s->range.definer = nullptr;
  s->range.lastUser = nullptr;
  
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
void Free<slot*>(slot*& slot)
{
  free(slot);
}

/*static void AddInterference(slot* a, slot* b)
{
  a->interferences[a->numInterferences++] = b;
  b->interferences[b->numInterferences++] = a;
}*/

instruction_label* CreateInstructionLabel()
{
  instruction_label* label = static_cast<instruction_label*>(malloc(sizeof(instruction_label)));
  label->address = 0u;
  
  return label;
}

static air_instruction* PushInstruction(air_function* function, instruction_type type, ...)
{
  va_list args;
  va_start(args, type);

  air_instruction* i = static_cast<air_instruction*>(malloc(sizeof(air_instruction)));
  i->type = type;
  i->next = nullptr;

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
      i->payload.s                     = va_arg(args, slot*);
    } break;

    case I_JUMP:
    {
      i->payload.jump.cond             = static_cast<jump_instruction::condition>(va_arg(args, int));
      i->payload.jump.label            = va_arg(args, instruction_label*);
    } break;

    case I_MOV:
    {
      i->payload.mov.dest              = va_arg(args, slot*);
      i->payload.mov.src               = va_arg(args, slot*);
    } break;

    case I_CMP:
    case I_ADD:
    case I_SUB:
    case I_MUL:
    case I_DIV:
    {
      i->payload.slotTriple.left       = va_arg(args, slot*);
      i->payload.slotTriple.right      = va_arg(args, slot*);
      i->payload.slotTriple.result     = va_arg(args, slot*);
    } break;

    case I_NEGATE:
    {
      i->payload.slotPair.left         = va_arg(args, slot*);
      i->payload.slotPair.right        = va_arg(args, slot*);
    } break;

    case I_CALL:
    {
      i->payload.function              = va_arg(args, function_def*);
    } break;

    case I_LABEL:
    {
      i->payload.label                 = va_arg(args, instruction_label*);
    } break;
  }

  if (function->code)
  {
    i->index = function->tail->index + 1u;
    function->tail->next = i;
    function->tail = i;
  }
  else
  {
    i->index = 0u;
    function->code = i;
    function->tail = i;
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
 * CONDITION_NODE:          `jump_instruction::condition
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
      air_instruction* instruction = nullptr;

      switch (n->payload.binaryOp.op)
      {
        case TOKEN_PLUS:
        {
          instruction = PushInstruction(function, I_ADD, left, right, result);
        } break;

        case TOKEN_MINUS:
        {
          instruction = PushInstruction(function, I_SUB, left, right, result);
        } break;

        case TOKEN_ASTERIX:
        {
          instruction = PushInstruction(function, I_MUL, left, right, result);
        } break;

        case TOKEN_SLASH:
        {
          instruction = PushInstruction(function, I_DIV, left, right, result);
        } break;

        default:
        {
          fprintf(stderr, "Unhandled AST binary op in GenNodeAIR!\n");
          exit(1);
        }
      }

      left->range.lastUser = instruction;
      right->range.lastUser = instruction;
      result->range.definer = instruction;

      return result;
    } break;

    case PREFIX_OP_NODE:
    {
      slot* right = GenNodeAIR<slot*>(target, function, n->payload.prefixOp.right);
      slot* result = static_cast<slot*>(malloc(sizeof(slot)));
      air_instruction* instruction = nullptr;

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
          instruction = PushInstruction(function, I_NEGATE, right, result);
        } break;

        default:
        {
          fprintf(stderr, "Unhandled AST prefix op in GenNodeAIR!\n");
          exit(1);
        }
      }

      right->range.lastUser = instruction;
      result->range.definer = instruction;

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

      air_instruction* instruction = PushInstruction(function, I_CMP, a, b);
      a->range.lastUser = instruction;
      b->range.lastUser = instruction;

      switch (n->payload.condition.condition)
      {
        case TOKEN_EQUALS_EQUALS:
        {
          return (n->payload.condition.reverseOnJump ?
                  jump_instruction::condition::IF_NOT_EQUAL :
                  jump_instruction::condition::IF_EQUAL);
        }

        case TOKEN_BANG_EQUALS:
        {
          return (n->payload.condition.reverseOnJump ?
                  jump_instruction::condition::IF_EQUAL :
                  jump_instruction::condition::IF_NOT_EQUAL);
        }

        case TOKEN_GREATER_THAN:
        {
          return (n->payload.condition.reverseOnJump ?
                  jump_instruction::condition::IF_LESSER_OR_EQUAL :
                  jump_instruction::condition::IF_GREATER);
        }

        case TOKEN_GREATER_THAN_EQUAL_TO:
        {
          return (n->payload.condition.reverseOnJump ?
                  jump_instruction::condition::IF_LESSER :
                  jump_instruction::condition::IF_GREATER_OR_EQUAL);
        }

        case TOKEN_LESS_THAN:
        {
          return (n->payload.condition.reverseOnJump ?
                  jump_instruction::condition::IF_GREATER_OR_EQUAL :
                  jump_instruction::condition::IF_LESSER);
        }

        case TOKEN_LESS_THAN_EQUAL_TO:
        {
          return (n->payload.condition.reverseOnJump ?
                  jump_instruction::condition::IF_GREATER :
                  jump_instruction::condition::IF_LESSER_OR_EQUAL);
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

      PushInstruction(function, I_LEAVE_STACK_FRAME);
      returnValue->range.lastUser = PushInstruction(function, I_RETURN, returnValue);
    } break;

    case VARIABLE_ASSIGN_NODE:
    {
      // TODO: don't assume it's a local (could be a param?)
      slot* variableSlot = CreateSlot(function, slot::slot_type::LOCAL, n->payload.variableAssignment.variable);
      slot* newValueSlot = GenNodeAIR<slot*>(target, function, n->payload.variableAssignment.newValue);

      air_instruction* instruction = PushInstruction(function, I_MOV, variableSlot, newValueSlot);
      variableSlot->range.definer = instruction;
      newValueSlot->range.lastUser = instruction;
    } break;

    case FUNCTION_CALL_NODE:
    {
      // TODO: don't assume everything will fit in a general register
      unsigned int numGeneralParams = 0u;
      for (auto* paramIt = n->payload.functionCall.params.first;
           paramIt;
           paramIt = paramIt->next)
      {
        slot* param = CreateSlot(function, slot::slot_type::IN_PARAM, target.intParamColors[numGeneralParams++]);
        slot* value = GenNodeAIR<slot*>(target, function, **paramIt);

        air_instruction* iMov = PushInstruction(function, I_MOV, param, value);
        param->range.definer = iMov;
        value->range.lastUser = iMov;
      }

      assert(n->payload.functionCall.isResolved);
      PushInstruction(function, I_CALL, n->payload.functionCall.function.def);
    } break;

    case IF_NODE:
    {
      assert(n->payload.ifThing.condition->type == CONDITION_NODE);
      assert(n->payload.ifThing.condition->payload.condition.reverseOnJump);
      jump_instruction::condition jumpCondition = GenNodeAIR<jump_instruction::condition>(target, function, n->payload.ifThing.condition);

      instruction_label* elseLabel = nullptr;
      instruction_label* endLabel = CreateInstructionLabel();
      
      if (n->payload.ifThing.elseCode)
      {
        elseLabel = CreateInstructionLabel();
      }

      PushInstruction(function, I_JUMP, jumpCondition, (elseLabel ? elseLabel : endLabel));
      GenNodeAIR(target, function, n->payload.ifThing.thenCode);

      if (n->payload.ifThing.elseCode)
      {
        air_instruction* currentTail = function->tail;
        GenNodeAIR(target, function, n->payload.ifThing.elseCode);

        // NOTE(Isaac): The first instruction after the old tail should be the first instruction of the else code
        assert(currentTail->next);
        // TODO: wat
//        currentTail->next->label = elseLabel;
      }
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
      PushInstruction(function, I_JUMP, jump_instruction::condition::UNCONDITIONAL, label);

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
static void ColorSlots(codegen_target& target, air_function* function)
{
  const unsigned int numGeneralRegisters = 14u;

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
  function->code = nullptr;
  function->tail = nullptr;
  CreateLinkedList<slot*>(function->slots);
  function->numIntermediates = 0;

  PushInstruction(function, I_ENTER_STACK_FRAME);

  for (node* n = functionDef->ast;
       n;
       n = n->next)
  {
    GenNodeAIR(target, function, n);
  }

  if (functionDef->shouldAutoReturn)
  {
    PushInstruction(function, I_LEAVE_STACK_FRAME);
    PushInstruction(function, I_RETURN, nullptr);
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
      result = static_cast<char*>(malloc(snprintf(nullptr, 0u, "%s(P)", s->payload.variable->name) + 1u));
      sprintf(result, "%s(P)", s->payload.variable->name);
    } break;

    case slot::slot_type::LOCAL:
    {
      result = static_cast<char*>(malloc(snprintf(nullptr, 0u, "%s(L)", s->payload.variable->name) + 1u));
      sprintf(result, "%s(L)", s->payload.variable->name);
    } break;

    case slot::slot_type::IN_PARAM:
    {
      result = static_cast<char*>(malloc(snprintf(nullptr, 0u, "%d(IN)", s->color) + 1u));
      sprintf(result, "%d(IN)", s->color);
    } break;

    case slot::slot_type::INTERMEDIATE:
    {
      result = static_cast<char*>(malloc(snprintf(nullptr, 0u, "i%u", s->tag) + 1u));
      sprintf(result, "i%u", s->tag);
    } break;

    case slot::slot_type::INT_CONSTANT:
    {
      result = static_cast<char*>(malloc(snprintf(nullptr, 0u, "#%d", s->payload.i) + 1u));
      sprintf(result, "#%d", s->payload.i);
    } break;

    case slot::slot_type::FLOAT_CONSTANT:
    {
      result = static_cast<char*>(malloc(snprintf(nullptr, 0u, "#%f", s->payload.f) + 1u));
      sprintf(result, "#%f", s->payload.f);
    } break;

    case slot::slot_type::STRING_CONSTANT:
    {
      result = static_cast<char*>(malloc(snprintf(nullptr, 0u, "\"%s\"", s->payload.string->string) + 1u));
      sprintf(result, "\"%s\"", s->payload.string->string);
    } break;
  }

  return result;
}

void PrintInstruction(air_instruction* instruction)
{
  switch (instruction->type)
  {
    case I_ENTER_STACK_FRAME:
    case I_LEAVE_STACK_FRAME:
    case I_NUM_INSTRUCTIONS:
    {
      printf("%u: %s\n", instruction->index, GetInstructionName(instruction->type));
    } break;

    case I_RETURN:
    {
      char* returnValue = nullptr;

      if (instruction->payload.s)
      {
        returnValue = GetSlotString(instruction->payload.s);
      }

      printf("%u: RETURN %s\n", instruction->index, returnValue);
      free(returnValue);
    } break;

    case I_JUMP:
    {

    } break;

    case I_MOV:
    {
      char* srcSlot = GetSlotString(instruction->payload.mov.src);
      char* destSlot = GetSlotString(instruction->payload.mov.dest);
      printf("%u: %s -> %s\n", instruction->index, srcSlot, destSlot);
      free(srcSlot);
      free(destSlot);
    } break;

    case I_CMP:
    {
      char* leftSlot = GetSlotString(instruction->payload.slotPair.left);
      char* rightSlot = GetSlotString(instruction->payload.slotPair.right);
      printf("%u: CMP %s, %s\n", instruction->index, leftSlot, rightSlot);
      free(leftSlot);
      free(rightSlot);
    } break;

    case I_ADD:
    {
      char* leftSlot = GetSlotString(instruction->payload.slotTriple.left);
      char* rightSlot = GetSlotString(instruction->payload.slotTriple.right);
      char* resultSlot = GetSlotString(instruction->payload.slotTriple.result);
      printf("%u: %s := %s + %s\n", instruction->index, resultSlot, leftSlot, rightSlot);
      free(leftSlot);
      free(rightSlot);
      free(resultSlot);
    } break;

    case I_SUB:
    {
      char* leftSlot = GetSlotString(instruction->payload.slotTriple.left);
      char* rightSlot = GetSlotString(instruction->payload.slotTriple.right);
      char* resultSlot = GetSlotString(instruction->payload.slotTriple.result);
      printf("%u: %s := %s - %s\n", instruction->index, resultSlot, leftSlot, rightSlot);
      free(leftSlot);
      free(rightSlot);
      free(resultSlot);
    } break;

    case I_MUL:
    {
      char* leftSlot = GetSlotString(instruction->payload.slotTriple.left);
      char* rightSlot = GetSlotString(instruction->payload.slotTriple.right);
      char* resultSlot = GetSlotString(instruction->payload.slotTriple.result);
      printf("%u: %s := %s * %s\n", instruction->index, resultSlot, leftSlot, rightSlot);
      free(leftSlot);
      free(rightSlot);
      free(resultSlot);
    } break;

    case I_DIV:
    {
      char* leftSlot = GetSlotString(instruction->payload.slotTriple.left);
      char* rightSlot = GetSlotString(instruction->payload.slotTriple.right);
      char* resultSlot = GetSlotString(instruction->payload.slotTriple.result);
      printf("%u: %s := %s / %s\n", instruction->index, resultSlot, leftSlot, rightSlot);
      free(leftSlot);
      free(rightSlot);
      free(resultSlot);
    } break;

    case I_NEGATE:
    {

    } break;

    case I_CALL:
    {
      printf("%u: CALL %s\n", instruction->index, instruction->payload.function->name);
    } break;

    case I_LABEL:
    {
      printf("LABEL\n");
      // TODO: be more helpful to the poor sod debugginh here
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
    "cyan2", "deeppink", "darkgoldenrod2", "dodgerblue4", "slategray", "goldenrod", "darkorchid1", "blue",
    "green3", "lightblue2", "mediumspringgreeen", "orange1", "mistyrose3", "maroon2", "mediumpurple2",
    "steelblue2", "plum", "lightseagreen"
  };

  for (auto* it = function->slots.first;
       it;
       it = it->next)
  {
    slot* slot = **it;
    const char* color = "black";

    if (!(slot->shouldBeColored))
    {
      color = "gray";
    }
    else
    {
      if (slot->color < 0)
      {
        fprintf(stderr, "WARNING: found uncolored node! Using red!\n");
        color = "red";
      }
      else
      {
        color = snazzyColors[slot->color];
      }
    }

    /*
     * NOTE(Isaac): this is a slightly hacky way of concatenating strings using GCC varadic macros,
     * since apparently the ## preprocessing token is next to bloody useless...
     */
    #define CAT(A, B, C) A B C
    #define PRINT_SLOT(label, ...) \
        fprintf(f, CAT("\ts%u[label=\"", label, "%s\" color=\"%s\" fontcolor=\"%s\"];\n"), \
            i, __VA_ARGS__, (liveRange ? liveRange : ""), color, color);
    // NOTE(Isaac): a format string should be supplied as the first vararg
    #define MAKE_LIVE_RANGE(...) \
        liveRange = static_cast<char*>(malloc(sizeof(char) * (snprintf(nullptr, 0u, __VA_ARGS__) + 1u))); \
        sprintf(liveRange, __VA_ARGS__);

    char* liveRange = nullptr;

    switch (slot->type)
    {
      case slot::slot_type::PARAM:
      {
        assert(slot->range.definer);

        if (slot->range.lastUser)
        {
          MAKE_LIVE_RANGE("(%u...%u)", slot->range.definer->index, slot->range.lastUser->index);
        }
        else
        {
          /*
           *  NOTE(Isaac): apparently trigraphs still exist, so we need to escape one of the question marks.
           *  IBM has a lot to answer for...
           */
          MAKE_LIVE_RANGE("(%u...?\?)", slot->range.definer->index);
        }

        PRINT_SLOT("%s : PARAM(%d)", slot->payload.variable->name, slot->tag);
      } break;

      case slot::slot_type::LOCAL:
      {
        assert(slot->range.definer);

        if (slot->range.lastUser)
        {
          MAKE_LIVE_RANGE("(%u...%u)", slot->range.definer->index, slot->range.lastUser->index);
        }
        else
        {
          MAKE_LIVE_RANGE("(%u...?\?)", slot->range.definer->index);
        }

        PRINT_SLOT("%s : LOCAL(%d)", slot->payload.variable->name, slot->tag);
      } break;

      case slot::slot_type::INTERMEDIATE:
      {
        assert(slot->range.definer);
        assert(slot->range.lastUser);
        MAKE_LIVE_RANGE("(%u...%u)", slot->range.definer->index, slot->range.lastUser->index);
        PRINT_SLOT("t%d : INTERMEDIATE", slot->tag);
      } break;

      case slot::slot_type::IN_PARAM:
      {
        assert(slot->range.definer);
        assert(slot->range.lastUser);
        MAKE_LIVE_RANGE("(%u...%u)", slot->range.definer->index, slot->range.lastUser->index);
        PRINT_SLOT("%u : IN", i);
      } break;

      case slot::slot_type::INT_CONSTANT:
      {
        PRINT_SLOT("%d : INT", slot->payload.i);
      } break;

      case slot::slot_type::FLOAT_CONSTANT:
      {
        PRINT_SLOT("%f : FLOAT", slot->payload.f);
      } break;

      case slot::slot_type::STRING_CONSTANT:
      {
        PRINT_SLOT("\"%s\" : STRING", slot->payload.string->string);
      } break;
    }

    free(liveRange);
    slot->dotTag = i;
    i++;
  }

  // Emit the interferences between them
  for (auto* it = function->slots.first;
       it;
       it = it->next)
  {
    slot* slot = **it;

    for (unsigned int i = 0u;
         i < slot->numInterferences;
         i++)
    {
      // NOTE(Isaac): this is a slightly tenuous way to avoid emitting duplicates
      if (slot->dotTag < slot->interferences[i]->dotTag)
      {
        fprintf(f, "\ts%u -> s%u[dir=none];\n", slot->dotTag, slot->interferences[i]->dotTag);
      }
    }
  }

  fprintf(f, "}\n");
  fclose(f);
}
