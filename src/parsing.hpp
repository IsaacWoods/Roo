/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <stack>
#include <error.hpp>
#include <parser.hpp>
#include <ir.hpp>

struct ParseResult;
struct ScopeDef;
struct CodeThing;

enum RooKeyword
{
  KEYWORD_TYPE,
  KEYWORD_FN,
  KEYWORD_TRUE,
  KEYWORD_FALSE,
  KEYWORD_IMPORT,
  KEYWORD_BREAK,
  KEYWORD_RETURN,
  KEYWORD_IF,
  KEYWORD_ELSE,
  KEYWORD_WHILE,
  KEYWORD_MUT,
  KEYWORD_OPERATOR,
};

struct RooParser : Parser<RooKeyword>
{
  RooParser(ParseResult& result, const std::string& path);
  ~RooParser() { }

  ASTNode* ParseExpression(unsigned int precedence = 0u);
  TypeRef ParseTypeRef();
  void ParseParameterList(std::vector<VariableDef*>& params);
  VariableDef* ParseVariableDef();
  ASTNode* ParseStatement();
  ASTNode* ParseBlock();
  ASTNode* ParseIf();
  ASTNode* ParseWhile();
  void ParseTypeDef();
  void ParseImport();
  void ParseFunction(AttribSet& attribs);
  void ParseOperator(AttribSet& attribs);
  void ParseAttribute(AttribSet& attribs);

  ParseResult&          result;

  bool                  isInLoop;
  std::stack<ScopeDef*> scopeStack;
  CodeThing*            currentThing;
};
