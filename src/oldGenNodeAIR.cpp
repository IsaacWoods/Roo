template<typename T = void>
static void GenNodeAIR(air_instruction* tail, node* n, T* result = nullptr)
{
  assert(n != nullptr);

  switch (n->type)
  {
    case BREAK_NODE:
    {
      instruction_label* label = static_cast<instruction_label*>(malloc(sizeof(instruction_label)));
      // TODO: find the end of the loop and attach the label

      PUSH(CreateInstruction(I_JUMP, jump_instruction_part::condition::UNCONDITIONAL, label));
    } break;

    case RETURN_NODE:
    {
      PUSH(CreateInstruction(I_LEAVE_STACK_FRAME));
      PUSH(CreateInstruction(I_RETURN));
    } break;

    case BINARY_OP_NODE:
    {
      slot left;
      GenNodeAIR<slot>(tail, n->payload.binaryOp.left, &left);
      slot right;
      GenNodeAIR<slot>(tail, n->payload.binaryOp.right, &right);
      instruction_type type;

      switch (n->payload.binaryOp.op)
      {
        case TOKEN_PLUS:
        {
          type = I_ADD;
        } break;

        case TOKEN_MINUS:
        {
          type = I_SUB;
        } break;

        case TOKEN_ASTERIX:
        {
          type = I_MUL;
        } break;

        case TOKEN_SLASH:
        {
          type = I_DIV;
        } break;

        default:
        {
          fprintf(stderr, "Unhandled AST binary op in GenNodeAIR!\n");
          exit(1);
        }
      }

      PUSH(CreateInstruction(type, left, right));
    } break;

    case PREFIX_OP_NODE:
    {

    } break;

    case VARIABLE_NODE:
    {

    };

    case IF_NODE:
    {

    } break;

    case NUMBER_CONSTANT_NODE:
    {

    } break;

    case STRING_CONSTANT_NODE:
    {

    } break;

    case FUNCTION_CALL_NODE:
    {

    } break;

    case VARIABLE_ASSIGN_NODE:
    {
      assert(n->payload.variableAssignment.variable);
      // TODO: find the slot of a `variable_def`
//      slot* variable = GenNodeAIR<slot*>(tail, n->payload.variableAssignment.variable);
      slot variable;
      slot newValue;
      GenNodeAIR<slot>(tail, n->payload.variableAssignment.newValue, &newValue);
      PUSH(CreateInstruction(I_MOV, variable, newValue));
    } break;
  }

  return nullptr;
}
