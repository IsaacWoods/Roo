/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <parsing.hpp>
#include <cstdio>
#include <cstring>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <cstdarg>
#include <climits>
#include <common.hpp>
#include <ir.hpp>
#include <ast.hpp>
#include <error.hpp>

template<>
const char* GetKeywordName(RooKeyword keyword)
{
  #define KEYWORD(keyword) case keyword: return #keyword;

  switch (keyword)
  {
    KEYWORD(KEYWORD_TYPE);
    KEYWORD(KEYWORD_FN);
    KEYWORD(KEYWORD_TRUE);
    KEYWORD(KEYWORD_FALSE);
    KEYWORD(KEYWORD_IMPORT);
    KEYWORD(KEYWORD_BREAK);
    KEYWORD(KEYWORD_RETURN);
    KEYWORD(KEYWORD_IF);
    KEYWORD(KEYWORD_ELSE);
    KEYWORD(KEYWORD_WHILE);
    KEYWORD(KEYWORD_MUT);
    KEYWORD(KEYWORD_OPERATOR);
  }

  #undef KEYWORD
}

/*
 * When this flag is set, the parser emits detailed logging throughout the parse.
 * It should probably be left off, unless debugging the lexer or parser.
 */
#if 1
  // NOTE(Isaac): format must be specified as the first vararg
  #define Log(parser, ...) Log_(parser, __VA_ARGS__);
//  #define Log(...) Log_(__VA_ARGS__); TODO
  static void Log_(RooParser& /*parser*/, const char* fmt, ...)
  {
    va_list args;
    va_start(args, fmt);
    vprintf(fmt, args);
    va_end(args);
  }
#else
  #define Log(parser, ...)
#endif

void RooParser::PeekNPrint(bool ignoreLines)
{
  if (PeekToken(ignoreLines).type == TOKEN_IDENTIFIER)
  {
    printf("PEEK: (%s)\n", PeekToken(ignoreLines).GetText().c_str());
  }
  else if ((PeekToken(ignoreLines).type == TOKEN_SIGNED_INT)   ||
           (PeekToken(ignoreLines).type == TOKEN_UNSIGNED_INT) ||
           (PeekToken(ignoreLines).type == TOKEN_FLOAT))
  {
    printf("PEEK: [%s]\n", PeekToken(ignoreLines).GetText().c_str());
  }
  else
  {
    printf("PEEK: %s\n", PeekToken(ignoreLines).AsString().c_str());
  }
}

void RooParser::PeekNPrintNext(bool ignoreLines)
{
  if (PeekNextToken(ignoreLines).type == TOKEN_IDENTIFIER)
  {
    printf("PEEK_NEXT: (%s)\n", PeekNextToken(ignoreLines).GetText().c_str());
  }
  else if ((PeekToken(ignoreLines).type == TOKEN_SIGNED_INT)   ||
           (PeekToken(ignoreLines).type == TOKEN_UNSIGNED_INT) ||
           (PeekToken(ignoreLines).type == TOKEN_FLOAT))
  {
    printf("PEEK_NEXT: [%s]\n", PeekNextToken(ignoreLines).GetText().c_str());
  }
  else
  {
    printf("PEEK_NEXT: %s\n", PeekNextToken(ignoreLines).AsString().c_str());
  }
}

typedef ASTNode* (*PrefixParselet)(RooParser&);
typedef ASTNode* (*InfixParselet)(RooParser&, ASTNode*);

PrefixParselet g_prefixMap      [NUM_TOKENS];
InfixParselet  g_infixMap       [NUM_TOKENS];
unsigned int   g_precedenceTable[NUM_TOKENS];

/*
 * If the previous operator is right-associative, the new precedence should be one less than that of the operator
 */
ASTNode* RooParser::ParseExpression(unsigned int precedence)
{
  Log(*this, "--> ParseExpression(%u)\n", precedence);
  PrefixParselet prefixParselet = g_prefixMap[PeekToken().type];

  if (!prefixParselet)
  {
    RaiseError(errorState, ERROR_UNEXPECTED, "prefix-expression", GetTokenName(PeekToken().type));
    RaiseError(errorState, ERROR_COMPILE_ERRORS);
  }

  ASTNode* expression = prefixParselet(*this);

  while (precedence < g_precedenceTable[PeekToken(false).type])
  {
    InfixParselet infixParselet = g_infixMap[PeekToken(false).type];

    // NOTE(Isaac): there is no infix expression part - just return the prefix expression
    if (!infixParselet)
    {
      Log(*this, "<-- ParseExpression(NO INFIX)\n");
      return expression;
    }

    expression = infixParselet(*this, expression);
  }

  Log(*this, "<-- ParseExpression\n");
  return expression;
}

TypeRef RooParser::ParseTypeRef()
{
  TypeRef ref;
  
  if (Match(KEYWORD_MUT))
  {
    ref.isMutable = true;
    NextToken();
  }

  ref.name = PeekToken().GetText();
  NextToken();

  if (Match(TOKEN_LEFT_BLOCK))
  {
    Consume(TOKEN_LEFT_BLOCK);
    ref.isArray = true;
    ref.arraySizeExpression = ParseExpression();
    Consume(TOKEN_RIGHT_BLOCK);
  }

  if (Match(KEYWORD_MUT))
  {
    Consume(KEYWORD_MUT);
    Consume(TOKEN_AND);
    ref.isReference = true;
    ref.isReferenceMutable = true;
  }
  else if (Match(TOKEN_AND))
  {
    Consume(TOKEN_AND);
    ref.isReference = true;
  }

  return ref;
}

void RooParser::ParseParameterList(std::vector<VariableDef*>& params)
{
  Consume(TOKEN_LEFT_PAREN);

  // Check for an empty parameter list
  if (Match(TOKEN_RIGHT_PAREN))
  {
    Consume(TOKEN_RIGHT_PAREN);
    return;
  }

  while (true)
  {
    std::string varName = PeekToken().GetText();
    ConsumeNext(TOKEN_COLON);

    TypeRef typeRef = ParseTypeRef();
    VariableDef* param = new VariableDef(varName, typeRef, nullptr);

    Log(*this, "Param: %s of type %s\n", param->name.c_str(), param->type.name.c_str());
    params.push_back(param);

    if (Match(TOKEN_COMMA))
    {
      Consume(TOKEN_COMMA);
    }
    else
    {
      Consume(TOKEN_RIGHT_PAREN);
      break;
    }
  }
}

VariableDef* RooParser::ParseVariableDef()
{
  std::string name = PeekToken().GetText();
  ConsumeNext(TOKEN_COLON);

  TypeRef typeRef = ParseTypeRef();
  VariableDef* variable = new VariableDef(name, typeRef, nullptr);

  Log(*this, "Defined variable: '%s' which is %s%s'%s', which is %s\n",
              variable->name.c_str(),
              (variable->type.isArray ? "an array of " : "a "),
              (variable->type.isReference ? (variable->type.isReferenceMutable ? "mutable reference to a " : "reference to a ") : ""),
              variable->type.name.c_str(),
              (variable->type.isMutable ? "mutable" : "immutable"));

  if (Match(TOKEN_EQUALS))
  {
    Consume(TOKEN_EQUALS);
    variable->initExpression = new VariableAssignmentNode(new VariableNode(variable), ParseExpression(), true);
  }
  else if (Match(TOKEN_LEFT_PAREN))
  {
    // Parse a type construction of the form `x : X(a, b, c)`
    Log(*this, "--> Type construction\n");
    Consume(TOKEN_LEFT_PAREN);
    std::vector<ASTNode*> items;

    if (Match(TOKEN_RIGHT_PAREN))
    {
      Consume(TOKEN_RIGHT_PAREN, false);
    }
    else
    {
      while (true)
      {
        items.push_back(ParseExpression());

        if (Match(TOKEN_COMMA))
        {
          Consume(TOKEN_COMMA);
        }
        else
        {
          Consume(TOKEN_RIGHT_PAREN, false);
          break;
        }
      }
    }

    variable->initExpression = new ConstructNode(new VariableNode(variable), typeRef.name, items);
    Log(*this, "<-- Type construction\n");
  }

  return variable;
}

ASTNode* RooParser::ParseBlock()
{
  Log(*this, "--> Block\n");
  Consume(TOKEN_LEFT_BRACE);
  ASTNode* code = nullptr;

  // Each block should introduce a new scope
  ScopeDef* scope = new ScopeDef(currentThing, (scopeStack.size() > 0u ? scopeStack.top() : nullptr));
  scopeStack.push(scope);

  while (!Match(TOKEN_RIGHT_BRACE))
  {
    ASTNode* statement = ParseStatement();

    Assert(statement, "Failed to parse statement");
    statement->containingScope = scope;

    if (code)
    {
      AppendNodeOntoTail(code, statement);
    }
    else
    {
      code = statement;
    }
  }

  Consume(TOKEN_RIGHT_BRACE);
  scopeStack.pop();  // Remove the block's scope from the stack
  Log(*this, "<-- Block\n");
  return code;
}

ASTNode* RooParser::ParseIf()
{
  Log(*this, "--> If\n");

  Consume(KEYWORD_IF);
  Consume(TOKEN_LEFT_PAREN);
  ASTNode* condition = ParseExpression();
  Consume(TOKEN_RIGHT_PAREN);

  ASTNode* thenCode = ParseBlock();
  ASTNode* elseCode = nullptr;

  if (Match(KEYWORD_ELSE))
  {
    NextToken();
    elseCode = ParseBlock();
  }

  Log(*this, "<-- If\n");
  return new BranchNode(condition, thenCode, elseCode);
}

ASTNode* RooParser::ParseWhile()
{
  Log(*this, "--> While\n");
  isInLoop = true;

  Consume(KEYWORD_WHILE);
  Consume(TOKEN_LEFT_PAREN);
  ASTNode* condition = ParseExpression();
  Consume(TOKEN_RIGHT_PAREN);
  ASTNode* code = ParseBlock();

  isInLoop = false;
  Log(*this, "<-- While\n");
  return new WhileNode(condition, code);
}

ASTNode* RooParser::ParseStatement()
{
  Log(*this, "--> Statement(%s)", (isInLoop ? "IN LOOP" : "NOT IN LOOP"));
  ASTNode* result = nullptr;
  bool needTerminatingLine = true;

  switch (PeekToken().type)
  {
    case KEYWORD_BREAK:
    {
      if (!isInLoop)
      {
        RaiseError(errorState, ERROR_UNEXPECTED, "not-in-a-loop", GetKeywordName(KEYWORD_BREAK));
        return nullptr;
      }

      result = new BreakNode();
      Log(*this, "(BREAK)\n");
      NextToken();
    } break;

    case KEYWORD_RETURN:
    {
      Log(*this, "(RETURN)\n");
      currentThing->shouldAutoReturn = false;
      NextToken(false);

      if (Match(TOKEN_LINE, false))
      {
        result = new ReturnNode(nullptr);
      }
      else
      {
        result = new ReturnNode(ParseExpression());
      }
    } break;

    case KEYWORD_IF:
    {
      Log(*this, "(IF)\n");
      result = ParseIf();
      needTerminatingLine = false;
    } break;

    case KEYWORD_WHILE:
    {
      Log(*this, "(WHILE)\n");
      result = ParseWhile();
      needTerminatingLine = false;
    } break;

    case TOKEN_IDENTIFIER:
    {
      // It's a variable definition (probably)
      if (MatchNext(TOKEN_COLON))
      {
        Log(*this, "(VARIABLE DEFINITION)\n");
        VariableDef* variable = ParseVariableDef();
        result = variable->initExpression;

        /*
         * Put the variable into the inner-most scope.
         */
        Assert(scopeStack.size() >= 1u, "Parsed statement without surrounding scope");
        scopeStack.top()->locals.push_back(variable);
        break;
      }
    } // XXX: no break

    default:
    {
      Log(*this, "(EXPRESSION STATEMENT)\n");
      result = ParseExpression();
    }
  }

  if (needTerminatingLine)
  {
    Consume(TOKEN_LINE, false);
  }

  Log(*this, "<-- Statement\n");
  return result;
}

void RooParser::ParseTypeDef()
{
  Log(*this, "--> TypeDef(");
  Consume(KEYWORD_TYPE);
  TypeDef* type = new TypeDef(PeekToken().GetText());
  Log(*this, "%s)\n", type->name.c_str());
  
  ConsumeNext(TOKEN_LEFT_BRACE);

  while (PeekToken().type != TOKEN_RIGHT_BRACE)
  {
    VariableDef* member = ParseVariableDef();
    // TODO: clone the initExpression
    type->members.push_back(new MemberDef(member->name, member->type, /*member->initExpression->Clone()*/nullptr, member->offset));
    free(member);
  }

  Consume(TOKEN_RIGHT_BRACE);
  result.types.push_back(type);
  Log(*this, "<-- TypeDef\n");
}

void RooParser::ParseImport()
{
  Log(*this, "--> Import\n");
  Consume(KEYWORD_IMPORT);

  DependencyDef* dependency;
  switch (PeekToken().type)
  {
    case TOKEN_IDENTIFIER:    // Local dependency
    {
      // TODO(Isaac): handle dotted identifiers
      Log(*this, "Importing: %s\n", PeekToken().GetText().c_str());
      dependency = new DependencyDef(DependencyDef::Type::LOCAL, PeekToken().GetText());
    } break;

    case TOKEN_STRING:        // Remote repository
    {
      Log(*this, "Importing remote: %s\n", PeekToken().GetText().c_str());
      dependency = new DependencyDef(DependencyDef::Type::REMOTE, PeekToken().GetText());
    } break;

    default:
    {
      RaiseError(errorState, ERROR_EXPECTED_BUT_GOT, "string-literal or dotted-identifier", GetTokenName(PeekToken().type));
    }
  }

  result.dependencies.push_back(dependency);
  NextToken();
  Log(*this, "<-- Import\n");
}

void RooParser::ParseFunction(AttribSet& attribs)
{
  Log(*this, "--> Function(");
  Assert(scopeStack.size() == 0, "Scope stack is not empty at function entry");

  FunctionThing* function = new FunctionThing(NextToken().GetText());
  function->attribs = attribs;
  Log(*this, "%s)\n", function->name.c_str());
  result.codeThings.push_back(function);
  currentThing = function;

  NextToken();
  ParseParameterList(function->params);

  // Optionally parse a return type
  if (Match(TOKEN_YIELDS))
  {
    Consume(TOKEN_YIELDS);
    function->returnType = new TypeRef(ParseTypeRef());
    Log(*this, "Function returns a: %s\n", function->returnType->name.c_str());
  }
  else
  {
    function->returnType = nullptr;
  }

  if (function->attribs.isPrototype)
  {
    function->ast = nullptr;
  }
  else
  {
    function->ast = ParseBlock();
  }

  Log(*this, "<-- Function\n");
}

void RooParser::ParseOperator(AttribSet& attribs)
{
  Log(*this, "--> Operator(");

  OperatorThing* operatorDef = new OperatorThing(NextToken().type);
  operatorDef->attribs = attribs;
  Log(*this, "%s)\n", GetTokenName(operatorDef->token));
  result.codeThings.push_back(operatorDef);
  currentThing = operatorDef;

  switch (operatorDef->token)
  {
    case TOKEN_PLUS:
    case TOKEN_MINUS:
    case TOKEN_ASTERIX:
    case TOKEN_SLASH:
    case TOKEN_DOUBLE_PLUS:
    case TOKEN_DOUBLE_MINUS:
    {
      NextToken();
    } break;

    case TOKEN_LEFT_BLOCK:
    {
      ConsumeNext(TOKEN_RIGHT_BLOCK);
    } break;

    default:
    {
      RaiseError(errorState, ERROR_INVALID_OPERATOR, GetTokenName(operatorDef->token));
    } break;
  }

  ParseParameterList(operatorDef->params);

  Consume(TOKEN_YIELDS);
  operatorDef->returnType = new TypeRef();
  *(operatorDef->returnType) = ParseTypeRef();
  Log(*this, "Return type: %s\n", operatorDef->returnType->name.c_str());

  if (operatorDef->attribs.isPrototype)
  {
    operatorDef->ast = nullptr;
  }
  else
  {
    operatorDef->ast = ParseBlock();
    Assert(!(operatorDef->shouldAutoReturn), "Parsed an operator that should apparently auto-return");
  }

  Log(*this, "<-- Operator\n");
}

void RooParser::ParseAttribute(AttribSet& attribs)
{
  std::string attribName = NextToken().GetText();
  Log(*this, "--> Attribute(%s)\n", attribName.c_str());

  if (attribName == "Entry")
  {
    attribs.isEntry = true;
    NextToken();
  }
  else if (attribName == "Name")
  {
    ConsumeNext(TOKEN_LEFT_PAREN);

    if (!Match(TOKEN_IDENTIFIER))
    {
      RaiseError(errorState, ERROR_EXPECTED, "identifier as program / library name");
      return;
    }

    result.name = PeekToken().GetText();
    Log(*this, "Setting program name: %s\n", result.name.c_str());
    ConsumeNext(TOKEN_RIGHT_PAREN);
  }
  else if (attribName == "TargetArch")
  {
    ConsumeNext(TOKEN_LEFT_PAREN);

    if (!Match(TOKEN_STRING))
    {
      RaiseError(errorState, ERROR_EXPECTED, "string as target identifier");
      return;
    }

    result.targetArch = PeekToken().GetText();
    ConsumeNext(TOKEN_RIGHT_PAREN);
  }
  else if (attribName == "Module")
  {
    ConsumeNext(TOKEN_LEFT_PAREN);

    if (!Match(TOKEN_IDENTIFIER))
    {
      RaiseError(errorState, ERROR_EXPECTED, "identifier as module name");
      return;
    }

    result.isModule = true;
    result.name = PeekToken().GetText();
    ConsumeNext(TOKEN_RIGHT_PAREN);
  }
  else if (attribName == "LinkFile")
  {
    ConsumeNext(TOKEN_LEFT_PAREN);

    if (!Match(TOKEN_STRING))
    {
      RaiseError(errorState, ERROR_EXPECTED, "string constant to specify path of file to manually link");
      return;
    }

    result.filesToLink.push_back(PeekToken().GetText());
    ConsumeNext(TOKEN_RIGHT_PAREN);
  }
  else if (attribName == "DefinePrimitive")
  {
    ConsumeNext(TOKEN_LEFT_PAREN);

    if (!Match(TOKEN_STRING))
    {
      RaiseError(errorState, ERROR_EXPECTED, "string containing name of primitive");
      return;
    }

    TypeDef* type = new TypeDef(PeekToken().GetText());

    ConsumeNext(TOKEN_COMMA);
    if (!Match(TOKEN_UNSIGNED_INT))
    {
      RaiseError(errorState, ERROR_EXPECTED, "size of primitive as unsigned int (in bytes)");
      return;
    }
    type->size = PeekToken().asUnsignedInt;

    result.types.push_back(type);
    ConsumeNext(TOKEN_RIGHT_PAREN);
  }
  else if (attribName == "Prototype")
  {
    attribs.isPrototype = true;
    NextToken();
  }
  else if (attribName == "Inline")
  {
    attribs.isInline = true;
    NextToken();
  }
  else if (attribName == "NoInline")
  {
    attribs.isNoInline = true;
    NextToken();
  }
  else
  {
    RaiseError(errorState, ERROR_ILLEGAL_ATTRIBUTE, attribName.c_str());
  }

  Consume(TOKEN_RIGHT_BLOCK);
  Log(*this, "<-- Attribute\n");
}

RooParser::RooParser(ParseResult& result, const std::string& path)
  :Parser(path)
  ,result(result)
  ,isInLoop(false)
  ,scopeStack()
{
  keywordMap["type"]      = KEYWORD_TYPE;
  keywordMap["fn"]        = KEYWORD_FN;
  keywordMap["operator"]  = KEYWORD_OPERATOR;
  keywordMap["true"]      = KEYWORD_TRUE;
  keywordMap["false"]     = KEYWORD_FALSE;
  keywordMap["import"]    = KEYWORD_IMPORT;
  keywordMap["break"]     = KEYWORD_BREAK;
  keywordMap["return"]    = KEYWORD_RETURN;
  keywordMap["if"]        = KEYWORD_IF;
  keywordMap["else"]      = KEYWORD_ELSE;
  keywordMap["while"]     = KEYWORD_WHILE;
  keywordMap["mut"]       = KEYWORD_MUT;

  AttribSet attribs;

  Log(*this, "--> Parse\n");
  while (!Match(TOKEN_INVALID))
  {
    if (Match(KEYWORD_IMPORT))
    {
      ParseImport();
    }
    else if (Match(KEYWORD_FN))
    {
      ParseFunction(attribs);
      attribs = AttribSet();
    }
    else if (Match(KEYWORD_OPERATOR))
    {
      ParseOperator(attribs);
      attribs = AttribSet();
    }
    else if (Match(KEYWORD_TYPE))
    {
      ParseTypeDef();
    }
    else if (Match(TOKEN_START_ATTRIBUTE))
    {
      ParseAttribute(attribs);
    }
    else
    {
      RaiseError(errorState, ERROR_UNEXPECTED, "top-level", GetTokenName(PeekToken().type));
    }
  }
  Log(*this, "<-- Parse\n");
}

__attribute__((constructor))
static void InitParseletMaps()
{
  // --- Precedence table ---
  memset(g_precedenceTable, 0, sizeof(unsigned int) * NUM_TOKENS);

  /*
   * The higher the precedence, the tighter binding an operator is (higher-precendence operations are done first)
   * NOTE(Isaac): Precedence starts at 1, as a precedence of 0 has special meaning
   */
  enum Precedence
  {
    P_ASSIGNMENT = 1u,          // =
    P_TERNARY,                  // a?b:c
    P_LOGICAL_OR,               // ||
    P_LOGICAL_AND,              // &&
    P_BITWISE_OR,               // |
    P_BITWISE_XOR,              // ^
    P_BITWISE_AND,              // &
    P_EQUALS_RELATIONAL,        // == and !=
    P_COMPARATIVE_RELATIONAL,   // <, <=, > and >=
    P_BITWISE_SHIFTING,         // >> and <<
    P_ADDITIVE,                 // + and -
    P_MULTIPLICATIVE,           // *, / and %
    P_PREFIX,                   // !, ~, +x, -x, ++x, --x, &, *
    P_PRIMARY,                  // x(...), x[...]
    P_SUFFIX,                   // x++, x--
    P_MEMBER_ACCESS,            // x.y
  };
  
  g_precedenceTable[TOKEN_EQUALS]                 = P_ASSIGNMENT;
  g_precedenceTable[TOKEN_QUESTION_MARK]          = P_TERNARY;
  g_precedenceTable[TOKEN_PLUS]                   = P_ADDITIVE;
  g_precedenceTable[TOKEN_MINUS]                  = P_ADDITIVE;
  g_precedenceTable[TOKEN_ASTERIX]                = P_MULTIPLICATIVE;
  g_precedenceTable[TOKEN_SLASH]                  = P_MULTIPLICATIVE;
  g_precedenceTable[TOKEN_LEFT_PAREN]             = P_PRIMARY;
  g_precedenceTable[TOKEN_LEFT_BLOCK]             = P_PRIMARY;
  g_precedenceTable[TOKEN_DOT]                    = P_MEMBER_ACCESS;
  g_precedenceTable[TOKEN_DOUBLE_PLUS]            = P_PREFIX;
  g_precedenceTable[TOKEN_DOUBLE_MINUS]           = P_PREFIX;
  g_precedenceTable[TOKEN_EQUALS_EQUALS]          = P_EQUALS_RELATIONAL;
  g_precedenceTable[TOKEN_BANG_EQUALS]            = P_EQUALS_RELATIONAL;
  g_precedenceTable[TOKEN_GREATER_THAN]           = P_COMPARATIVE_RELATIONAL;
  g_precedenceTable[TOKEN_GREATER_THAN_EQUAL_TO]  = P_COMPARATIVE_RELATIONAL;
  g_precedenceTable[TOKEN_LESS_THAN]              = P_COMPARATIVE_RELATIONAL;
  g_precedenceTable[TOKEN_LESS_THAN_EQUAL_TO]     = P_COMPARATIVE_RELATIONAL;

  // --- Parselets ---
  memset(g_prefixMap, 0, sizeof(PrefixParselet) * NUM_TOKENS);
  memset(g_infixMap,  0, sizeof(InfixParselet)  * NUM_TOKENS);

  // --- Prefix Parselets
  g_prefixMap[TOKEN_IDENTIFIER] =
    [](RooParser& parser) -> ASTNode*
    {
      Log(parser, "--> [PARSELET] Identifier\n");
      std::string name = parser.PeekToken().GetText();

      parser.NextToken(false);
      Log(parser, "<-- [PARSELET] Identifier\n");
      return new VariableNode(strdup(name.c_str()));
    };

  g_prefixMap[TOKEN_SIGNED_INT] =
    [](RooParser& parser) -> ASTNode*
    {
      Log(parser, "--> [PARSELET] Number constant (signed integer)\n");
      int value = parser.PeekToken().asSignedInt;
      parser.NextToken(false);
      Log(parser, "<-- [PARSELET] Number constant (signed integer)\n");
      return new ConstantNode<int>(value);
    };

  g_prefixMap[TOKEN_UNSIGNED_INT] =
    [](RooParser& parser) -> ASTNode*
    {
      Log(parser, "--> [PARSELET] Number constant (unsigned integer)\n");
      unsigned int value = parser.PeekToken().asUnsignedInt;
      parser.NextToken(false);
      Log(parser, "<-- [PARSELET] Number constant (unsigned integer)\n");
      return new ConstantNode<unsigned int>(value);
    };

  g_prefixMap[TOKEN_FLOAT] =
    [](RooParser& parser) -> ASTNode*
    {
      Log(parser, "--> [PARSELET] Number constant (floating point)\n");
      float value = parser.PeekToken().asFloat;
      parser.NextToken(false);
      Log(parser, "<-- [PARSELET] Number constant (floating point)\n");
      return new ConstantNode<float>(value);
    };

  g_prefixMap[TOKEN_KEYWORD] =
    [](RooParser& parser) -> ASTNode*
    {
      switch (parser.PeekToken().asKeyword)
      {
        case KEYWORD_TRUE:
        case KEYWORD_FALSE:
        {
          Log(parser, "--> [PARSELET] Bool\n");
          bool value = (parser.PeekToken().asKeyword == KEYWORD_TRUE);
          parser.NextToken(false);

          Log(parser, "<-- [PARSELET] Bool\n");
          return new ConstantNode<bool>(value);
        } break;

        default:
        {
          RaiseError(parser.errorState, ERROR_UNEXPECTED, "prefix-expression", GetKeywordName(parser.PeekToken().asKeyword));
          __builtin_unreachable();
        } break;
      }
    };

  g_prefixMap[TOKEN_STRING] =
    [](RooParser& parser) -> ASTNode*
    {
      Log(parser, "--> [PARSELET] String\n");
      std::string tokenText = parser.PeekToken().GetText();
      parser.NextToken(false);

      Log(parser, "<-- [PARSELET] String\n");
      return new StringNode(new StringConstant(parser.result, tokenText));
    };

  g_prefixMap[TOKEN_PLUS]         =
  g_prefixMap[TOKEN_MINUS]        =
  g_prefixMap[TOKEN_BANG]         =
  g_prefixMap[TOKEN_TILDE]        =
  g_prefixMap[TOKEN_AND]          =
  g_prefixMap[TOKEN_DOUBLE_PLUS]  =   // ++i
  g_prefixMap[TOKEN_DOUBLE_MINUS] =   // --i
    [](RooParser& parser) -> ASTNode*
    {
      Log(parser, "--> [PARSELET] Prefix operator (%s)\n", GetTokenName(parser.PeekToken().type));
      TokenType operation = parser.PeekToken().type;
      UnaryOpNode::Operator unaryOp;

      switch (operation)
      {
        case TOKEN_PLUS:          unaryOp = UnaryOpNode::Operator::POSITIVE;          break;
        case TOKEN_MINUS:         unaryOp = UnaryOpNode::Operator::NEGATIVE;          break;
        case TOKEN_BANG:          unaryOp = UnaryOpNode::Operator::LOGICAL_NOT;       break;
        case TOKEN_TILDE:         unaryOp = UnaryOpNode::Operator::NEGATE;            break;
        case TOKEN_AND:           unaryOp = UnaryOpNode::Operator::TAKE_REFERENCE;    break;
        case TOKEN_DOUBLE_PLUS:   unaryOp = UnaryOpNode::Operator::PRE_INCREMENT;     break;
        case TOKEN_DOUBLE_MINUS:  unaryOp = UnaryOpNode::Operator::PRE_DECREMENT;     break;

        default:
        {
          RaiseError(parser.errorState, ICE_UNHANDLED_TOKEN_TYPE, "PrefixParselet", GetTokenName(parser.PeekToken().type));
          __builtin_unreachable();
        } break;
      }

      parser.NextToken();
      ASTNode* operand = parser.ParseExpression(P_PREFIX);
      Log(parser, "<-- [PARSELET] Prefix operation\n");
      return new UnaryOpNode(unaryOp, operand);
    };

  g_prefixMap[TOKEN_LEFT_PAREN] =
    [](RooParser& parser) -> ASTNode*
    {
      Log(parser, "--> [PARSELET] Parentheses\n");
      
      parser.NextToken();
      ASTNode* expression = parser.ParseExpression();
      parser.Consume(TOKEN_RIGHT_PAREN);

      Log(parser, "<-- [PARSELET] Parentheses\n");
      return expression;
    };

  // Parses an array literal
  g_prefixMap[TOKEN_LEFT_BRACE] =
    [](RooParser& parser) -> ASTNode*
    {
      Log(parser, "--> [PARSELET] Array literal\n");
      
      parser.NextToken();
      std::vector<ASTNode*> items;

      // Check for an empty initialiser-list
      if (parser.Match(TOKEN_RIGHT_BRACE))
      {
        parser.Consume(TOKEN_RIGHT_BRACE);
      }
      else
      {
        while (true)
        {
          ASTNode* item = parser.ParseExpression();
          items.push_back(item);

          if (parser.Match(TOKEN_COMMA))
          {
            parser.Consume(TOKEN_COMMA);
          }
          else
          {
            parser.Consume(TOKEN_RIGHT_BRACE);
            break;
          }
        }
      }

      Log(parser, "<-- [PARSELET] Array literal\n");
      return new ArrayInitNode(items);
    };

  // --- Infix Parselets ---
  // Parses binary operators
  g_infixMap[TOKEN_PLUS]          =
  g_infixMap[TOKEN_MINUS]         =
  g_infixMap[TOKEN_ASTERIX]       =
  g_infixMap[TOKEN_SLASH]         =
  g_infixMap[TOKEN_DOUBLE_PLUS]   =   // i++
  g_infixMap[TOKEN_DOUBLE_MINUS]  =   // i--
    [](RooParser& parser, ASTNode* left) -> ASTNode*
    {
      Log(parser, "--> [PARSELET] Binary operator (%s)\n", GetTokenName(parser.PeekToken().type));
      TokenType operation = parser.PeekToken().type;

      // Special ones - these parse like infix operations but are actually unary ops
      if (operation == TOKEN_DOUBLE_PLUS ||
          operation == TOKEN_DOUBLE_MINUS)
      {
        parser.NextToken(false);
        Log(parser, "<-- [PARSELET] Binary operator\n");
        return new UnaryOpNode((operation == TOKEN_DOUBLE_PLUS ? UnaryOpNode::Operator::POST_INCREMENT :
                                                                 UnaryOpNode::Operator::POST_DECREMENT),
                               left);
      }

      BinaryOpNode::Operator binaryOp;

      switch (operation)
      {
        case TOKEN_PLUS:          binaryOp = BinaryOpNode::Operator::ADD;       break;
        case TOKEN_MINUS:         binaryOp = BinaryOpNode::Operator::SUBTRACT;  break;
        case TOKEN_ASTERIX:       binaryOp = BinaryOpNode::Operator::MULTIPLY;  break;
        case TOKEN_SLASH:         binaryOp = BinaryOpNode::Operator::DIVIDE;    break;
        case TOKEN_DOUBLE_PLUS:   binaryOp = BinaryOpNode::Operator::DIVIDE;    break;
        case TOKEN_DOUBLE_MINUS:  binaryOp = BinaryOpNode::Operator::DIVIDE;    break;

        default:
        {
          RaiseError(parser.errorState, ICE_UNHANDLED_TOKEN_TYPE, "BinaryOpParselet", GetTokenName(parser.PeekToken().type));
          __builtin_unreachable();
        } break;
      }

      parser.NextToken(false);
      ASTNode* right = parser.ParseExpression(g_precedenceTable[operation]);
      Log(parser, "<-- [PARSELET] Binary operator\n");
      return new BinaryOpNode(binaryOp, left, right);
    };

  // Parses an array index
  g_infixMap[TOKEN_LEFT_BLOCK] =
    [](RooParser& parser, ASTNode* left) -> ASTNode*
    {
      Log(parser, "--> [PARSELET] Array index\n");
      parser.Consume(TOKEN_LEFT_BLOCK);
      ASTNode* indexParseExpression = parser.ParseExpression();
      parser.Consume(TOKEN_RIGHT_BLOCK);

      Log(parser, "<-- [PARSELET] Array index\n");
      return new BinaryOpNode(BinaryOpNode::Operator::INDEX_ARRAY, left, indexParseExpression);
    };

  // Parses a conditional
  g_infixMap[TOKEN_EQUALS_EQUALS]         =
  g_infixMap[TOKEN_BANG_EQUALS]           =
  g_infixMap[TOKEN_GREATER_THAN]          =
  g_infixMap[TOKEN_GREATER_THAN_EQUAL_TO] =
  g_infixMap[TOKEN_LESS_THAN]             =
  g_infixMap[TOKEN_LESS_THAN_EQUAL_TO]    =
    [](RooParser& parser, ASTNode* left) -> ASTNode*
    {
      Log(parser, "--> [PARSELET] Conditional\n");

      TokenType conditionToken = parser.PeekToken().type;
      parser.NextToken();
      ASTNode* right = parser.ParseExpression(g_precedenceTable[conditionToken]);

      ConditionNode::Condition condition;
      switch (conditionToken)
      {
        case TOKEN_EQUALS_EQUALS:           condition = ConditionNode::Condition::EQUAL;                  break;
        case TOKEN_BANG_EQUALS:             condition = ConditionNode::Condition::NOT_EQUAL;              break;
        case TOKEN_GREATER_THAN:            condition = ConditionNode::Condition::GREATER_THAN;           break;
        case TOKEN_GREATER_THAN_EQUAL_TO:   condition = ConditionNode::Condition::GREATER_THAN_OR_EQUAL;  break;
        case TOKEN_LESS_THAN:               condition = ConditionNode::Condition::LESS_THAN;              break;
        case TOKEN_LESS_THAN_EQUAL_TO:      condition = ConditionNode::Condition::LESS_THAN_OR_EQUAL;     break;

        default:
        {
          RaiseError(parser.errorState, ICE_UNHANDLED_TOKEN_TYPE, "ConditionalParselet", GetTokenName(parser.PeekToken().type));
          __builtin_unreachable();
        } break;
      }

      Log(parser, "<-- [PARSELET] Conditional\n");
      return new ConditionNode(condition, left, right);
    };

  // Parses a function call
  g_infixMap[TOKEN_LEFT_PAREN] =
    [](RooParser& parser, ASTNode* left) -> ASTNode*
    {
      Log(parser, "--> [PARSELET] Function Call\n");

      if (!IsNodeOfType<VariableNode>(left))
      {
        // FIXME: Print out the source that forms the expression
        RaiseError(parser.errorState, ERROR_EXPECTED_BUT_GOT, "function-name", "Potato");
      }

      VariableNode* leftAsVariable = reinterpret_cast<VariableNode*>(left);
      char* functionName = new char[strlen(leftAsVariable->name) + 1u];
      strcpy(functionName, leftAsVariable->name);
      delete[] left;

      std::vector<ASTNode*> params;
      parser.Consume(TOKEN_LEFT_PAREN);
      while (!parser.Match(TOKEN_RIGHT_PAREN))
      {
        params.push_back(parser.ParseExpression());
      }

      /*
       * XXX: We can't ignore new-lines here, but I think we should be able to? Maybe this is showing a bug
       * in the lexer where it doesn't roll back to TOKEN_LINEs correctly when we ask it to?
       */
      parser.Consume(TOKEN_RIGHT_PAREN, false);

      Log(parser, "<-- [PARSELET] Function call\n");
      return new CallNode(functionName, params);
    };

  // Parses a member access
  g_infixMap[TOKEN_DOT] =
    [](RooParser& parser, ASTNode* left) -> ASTNode*
    {
      Log(parser, "--> [PARSELET] Member access\n");

      if (!(IsNodeOfType<VariableNode>(left) || IsNodeOfType<MemberAccessNode>(left)))
      {
        // FIXME: Print out the source that forms the expression
        RaiseError(parser.errorState, ERROR_EXPECTED_BUT_GOT, "variable-binding or member-binding", "Potato");
      }

      parser.NextToken();
      ASTNode* child = parser.ParseExpression(P_MEMBER_ACCESS); 

      Log(parser, "<-- [PARSELET] Member access\n");
      return new MemberAccessNode(left, child);
    };

  // Parses a ternary expression
  g_infixMap[TOKEN_QUESTION_MARK] =
    [](RooParser& parser, ASTNode* left) -> ASTNode*
    {
      Log(parser, "--> [PARSELET] Ternary\n");
      
      if (!IsNodeOfType<ConditionNode>(left))
      {
        // FIXME: Print out the source that forms the expression
        RaiseError(parser.errorState, ERROR_UNEXPECTED_EXPRESSION, "conditional", "Potato");
      }

      ConditionNode* condition = reinterpret_cast<ConditionNode*>(left);

      parser.NextToken();
      ASTNode* thenBody = parser.ParseExpression(P_TERNARY-1u);
      parser.Consume(TOKEN_COLON);
      ASTNode* elseBody = parser.ParseExpression(P_TERNARY-1u);

      Log(parser, "<-- [PARSELET] Ternary\n");
      return new BranchNode(condition, thenBody, elseBody);
    };

  // Parses a variable assignment
  g_infixMap[TOKEN_EQUALS] =
    [](RooParser& parser, ASTNode* left) -> ASTNode*
    {
      Log(parser, "--> [PARSELET] Variable assignment\n");

      parser.NextToken();
      ASTNode* expression = parser.ParseExpression(P_ASSIGNMENT-1u);

      Log(parser, "<-- [PARSELET] Variable assignment\n");
      return new VariableAssignmentNode(left, expression, false);
    };
}
