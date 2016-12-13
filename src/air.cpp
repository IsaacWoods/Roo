/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#include <air.hpp>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cassert>
#include <cstdarg>
#include <climits>
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
    case slot::slot_type::VARIABLE:
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

  Add<slot*>(function->slots, s);
  va_end(args);
  return s;
}

template<>
void Free<slot*>(slot*& slot)
{
  free(slot);
}

instruction_label* CreateInstructionLabel()
{
  instruction_label* label = static_cast<instruction_label*>(malloc(sizeof(instruction_label)));
  label->offset = 0u;
  
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
      i->payload.s                    = va_arg(args, slot*);
    } break;

    case I_JUMP:
    {
      i->payload.jump.cond            = static_cast<jump_instruction::condition>(va_arg(args, int));
      i->payload.jump.label           = va_arg(args, instruction_label*);
    } break;

    case I_MOV:
    {
      i->payload.mov.dest             = va_arg(args, slot*);
      i->payload.mov.src              = va_arg(args, slot*);
    } break;

    case I_CMP:
    {
      i->payload.slotTriple.left      = va_arg(args, slot*);
      i->payload.slotTriple.right     = va_arg(args, slot*);
      i->payload.slotTriple.result    = va_arg(args, slot*);
    } break;

    case I_BINARY_OP:
    {
      i->payload.binaryOp.operation   = static_cast<binary_op_instruction::op>(va_arg(args, int));
      i->payload.binaryOp.left        = va_arg(args, slot*);
      i->payload.binaryOp.right       = va_arg(args, slot*);
      i->payload.binaryOp.result      = va_arg(args, slot*);
    } break;

    case I_CALL:
    {
      i->payload.function             = va_arg(args, function_def*);
    } break;

    case I_LABEL:
    {
      i->payload.label                = va_arg(args, instruction_label*);
    } break;

    case I_NUM_INSTRUCTIONS:
    {
      fprintf(stderr, "Tried to push AIR instruction of type I_NUM_INSTRUCTIONS!\n");
      assert(false);
    }
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
      binary_op_instruction::op operation;

      switch (n->payload.binaryOp.op)
      {
        case TOKEN_PLUS:    operation = binary_op_instruction::op::ADD; break;
        case TOKEN_MINUS:   operation = binary_op_instruction::op::SUB; break;
        case TOKEN_ASTERIX: operation = binary_op_instruction::op::MUL; break;
        case TOKEN_SLASH:   operation = binary_op_instruction::op::DIV; break;

        default:
        {
          fprintf(stderr, "Unhandled AST binary op in GenNodeAIR!\n");
          Crash();
        }
      }

      air_instruction* instruction = PushInstruction(function, I_BINARY_OP, operation, left, right, result);
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
          // TODO
        } break;

        default:
        {
          fprintf(stderr, "Unhandled AST prefix op in GenNodeAIR!\n");
          Crash();
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
      // TODO: why the fuck does this assume it's a param??
      // TODO: how do we know which to use??

      // If no slot exists yet, create one
      if (!n->payload.variable.var.def->mostRecentSlot)
      {
        n->payload.variable.var.def->mostRecentSlot = CreateSlot(function, slot::slot_type::VARIABLE, n->payload.variable.var.def);
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
      Crash();
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
          Crash();
        }
      }
    } break;

    default:
    {
      fprintf(stderr, "Unhandled node type for returning a `jump_instruction::condition` in GenNodeAIR: %s\n", GetNodeName(n->type));
      Crash();
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
      slot* variableSlot = GenNodeAIR<slot*>(target, function, n->payload.variableAssignment.variable);
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
        PushInstruction(function, I_JUMP, jump_instruction::condition::UNCONDITIONAL, endLabel);

        PushInstruction(function, I_LABEL, elseLabel);
        GenNodeAIR(target, function, n->payload.ifThing.elseCode);
      }

      PushInstruction(function, I_LABEL, endLabel);
    } break;

    default:
    {
      fprintf(stderr, "Unhandled node type for returning nothing in GenNodeAIR: %s\n", GetNodeName(n->type));
      Crash();
    }
  }
}

template<>
instruction_label* GenNodeAIR<instruction_label*>(codegen_target& /*target*/, air_function* function, node* n)
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
      Crash();
    }
  }

  return nullptr;
}

static void GenerateInterferences(air_function* function)
{
  for (auto* itA = function->slots.first;
       itA;
       itA = itA->next)
  {
    slot* a = **itA;

    if (a->type == slot::slot_type::INT_CONSTANT ||
        a->type == slot::slot_type::FLOAT_CONSTANT ||
        a->type == slot::slot_type::STRING_CONSTANT)
    {
      continue;
    }

    for (auto* itB = itA->next;
         itB;
         itB = itB->next)
    {
      slot* b = **itB;

      if (b->type == slot::slot_type::INT_CONSTANT ||
          b->type == slot::slot_type::FLOAT_CONSTANT ||
          b->type == slot::slot_type::STRING_CONSTANT)
      {
        continue;
      }

      /*
       * NOTE(Isaac): `lastUser` can be null if the variable is never used in the function
       * This shouldn't happen in optimizing builds, but that would require smart stuff to guarantee ¯\_(ツ)_/¯
       */
      unsigned int defA = a->range.definer->index;
      unsigned int useA = (a->range.lastUser ? a->range.lastUser->index : UINT_MAX);
      unsigned int defB = b->range.definer->index;
      unsigned int useB = (b->range.lastUser ? b->range.lastUser->index : UINT_MAX);

      // If their live ranges intersect, add an intersection
      if (defA <= useB && defB <= useA)
      {
        a->interferences[a->numInterferences++] = b;
        b->interferences[b->numInterferences++] = a;
      }
    }
  }
}

/*
 * Used by the AIR generator to color the interference graph to allocate slots to registers.
 */
static void ColorSlots(codegen_target& /*target*/, air_function* function)
{
  const unsigned int numGeneralRegisters = 14u;

  // --- Color params ---
  // TODO: color incoming parameters to this function
/*  unsigned int intParamCounter = 0u;


  for (auto* slotIt = function->slots.first;
       slotIt;
       slotIt = slotIt->next)
  {
    if ((**slotIt)->type == slot::slot_type::PARAM)
    {
      printf("Coloring param slot: %s\n", (**slotIt)->payload.variable->name);
      (**slotIt)->color = target.intParamColors[intParamCounter++];
    }
  }*/

  // --- Color other slots ---
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
      Crash();
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
  GenerateInterferences(functionDef->air);
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
    case slot::slot_type::VARIABLE:
    {
      result = static_cast<char*>(malloc(snprintf(nullptr, 0u, "%s(V)", s->payload.variable->name) + 1u));
      sprintf(result, "%s(V)", s->payload.variable->name);
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
    {
      printf("%u: %s\n", instruction->index, GetInstructionName(instruction));
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
      switch (instruction->payload.jump.cond)
      {
        case jump_instruction::condition::UNCONDITIONAL:
        {
          printf("%u: JMP I(0x%lx)\n", instruction->index, instruction->payload.jump.label->offset);
        } break;

        case jump_instruction::condition::IF_EQUAL:
        {
          printf("%u: JE I(0x%lx)\n", instruction->index, instruction->payload.jump.label->offset);
        } break;

        case jump_instruction::condition::IF_NOT_EQUAL:
        {
          printf("%u: JNE I(0x%lx)\n", instruction->index, instruction->payload.jump.label->offset);
        } break;

        case jump_instruction::condition::IF_OVERFLOW:
        {
          printf("%u: JO I(0x%lx)\n", instruction->index, instruction->payload.jump.label->offset);
        } break;

        case jump_instruction::condition::IF_NOT_OVERFLOW:
        {
          printf("%u: JNO I(0x%lx)\n", instruction->index, instruction->payload.jump.label->offset);
        } break;

        case jump_instruction::condition::IF_SIGN:
        {
          printf("%u: JS I(0x%lx)\n", instruction->index, instruction->payload.jump.label->offset);
        } break;

        case jump_instruction::condition::IF_NOT_SIGN:
        {
          printf("%u: JNS I(0x%lx)\n", instruction->index, instruction->payload.jump.label->offset);
        } break;

        case jump_instruction::condition::IF_GREATER:
        {
          printf("%u: JG I(0x%lx)\n", instruction->index, instruction->payload.jump.label->offset);
        } break;

        case jump_instruction::condition::IF_GREATER_OR_EQUAL:
        {
          printf("%u: JGE I(0x%lx)\n", instruction->index, instruction->payload.jump.label->offset);
        } break;

        case jump_instruction::condition::IF_LESSER:
        {
          printf("%u: JL I(0x%lx)\n", instruction->index, instruction->payload.jump.label->offset);
        } break;

        case jump_instruction::condition::IF_LESSER_OR_EQUAL:
        {
          printf("%u: JLE I(0x%lx)\n", instruction->index, instruction->payload.jump.label->offset);
        } break;

        case jump_instruction::condition::IF_PARITY_EVEN:
        {
          printf("%u: JPE I(0x%lx)\n", instruction->index, instruction->payload.jump.label->offset);
        } break;

        case jump_instruction::condition::IF_PARITY_ODD:
        {
          printf("%u: JPO I(0x%lx)\n", instruction->index, instruction->payload.jump.label->offset);
        } break;
      }
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

    case I_BINARY_OP:
    {
      const char* opString;

      switch (instruction->payload.binaryOp.operation)
      {
        case binary_op_instruction::op::ADD: opString = "+"; break;
        case binary_op_instruction::op::SUB: opString = "-"; break;
        case binary_op_instruction::op::MUL: opString = "*"; break;
        case binary_op_instruction::op::DIV: opString = "/"; break;
      }

      char* leftSlot = GetSlotString(instruction->payload.binaryOp.left);
      char* rightSlot = GetSlotString(instruction->payload.binaryOp.right);
      char* resultSlot = GetSlotString(instruction->payload.binaryOp.result);
      printf("%u: %s := %s %s %s\n", instruction->index, resultSlot, leftSlot, opString, rightSlot);
      free(leftSlot);
      free(rightSlot);
      free(resultSlot);
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

    case I_NUM_INSTRUCTIONS:
    {
      printf("!!!I_NUM_INSTRUCTIONS!!!\n");
    } break;
  }
}

const char* GetInstructionName(air_instruction* instruction)
{
  switch (instruction->type)
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
    case I_BINARY_OP:
    {
      switch (instruction->payload.binaryOp.operation)
      {
        case binary_op_instruction::op::ADD: return "ADD";
        case binary_op_instruction::op::SUB: return "SUB";
        case binary_op_instruction::op::MUL: return "MUL";
        case binary_op_instruction::op::DIV: return "DIV";
      }
    }
    case I_CALL:
      return "CALL";
    case I_LABEL:
      return "LABEL";
    case I_NUM_INSTRUCTIONS:
      return "!!!I_NUM_INSTRUCTIONS!!!";
  }

  return nullptr;
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
    Crash();
  }

  fprintf(f, "digraph G\n{\n");
  unsigned int i = 0u;

  const char* snazzyColors[] =
  {
    "cyan2", "deeppink", "darkgoldenrod2", "mediumpurple2", "slategray", "goldenrod", "darkorchid1", "plum",
    "green3", "lightblue2", "mediumspringgreeen", "orange1", "mistyrose3", "maroon2", "dodgerblue4",
    "steelblue2", "blue", "lightseagreen"
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
      case slot::slot_type::VARIABLE:
      {
        assert(slot->range.definer);

        if (slot->range.lastUser)
        {
          MAKE_LIVE_RANGE("(%u...%u)", slot->range.definer->index, slot->range.lastUser->index);
        }
        else
        {
          /*
           *  NOTE(Isaac): apparently trigraphs still exist, so we need to escape one of the question marks
           */
          MAKE_LIVE_RANGE("(%u...?\?)", slot->range.definer->index);
        }

        PRINT_SLOT("%s : VAR(%d)", slot->payload.variable->name, slot->tag);
      } break;

      case slot::slot_type::INTERMEDIATE:
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

        PRINT_SLOT("t%d : INTERMEDIATE", slot->tag);
      } break;

      case slot::slot_type::IN_PARAM:
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
        PRINT_SLOT("\\\"%s\\\" : STRING", slot->payload.string->string);
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
