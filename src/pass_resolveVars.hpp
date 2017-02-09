/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <cstdio>
#include <cstring>
#include <cassert>
#include <ast.hpp>
#include <error.hpp>

ast_pass PASS_resolveVars = {};

__attribute__((constructor))
void InitResolveVarsPass()
{
  PASS_resolveVars.iteratePolicy = CHILDREN_FIRST;

  PASS_resolveVars.f[VARIABLE_NODE] =
    [](parse_result& /*parse*/, thing_of_code* code, node* n)
    {
      if (n->variable.isResolved)
      {
        return;
      }

      error_state state = CreateErrorState(TRAVERSING_AST, code, n);

      for (auto* it = code->locals.head;
           it < code->locals.tail;
           it++)
      {
        variable_def* local = *it;

        if (strcmp(local->name, n->variable.name) == 0)
        {
          free(n->variable.name);
          n->variable.isResolved = true;
          n->variable.var = local;
          return;
        }
      }

      for (auto* it = code->params.head;
           it < code->params.tail;
           it++)
      {
        variable_def* param = *it;

        if (strcmp(param->name, n->variable.name) == 0)
        {
          free(n->variable.name);
          n->variable.isResolved = true;
          n->variable.var = param;
          return;
        }
      }

      RaiseError(state, ERROR_UNDEFINED_VARIABLE, n->variable.name);
    };

  PASS_resolveVars.f[MEMBER_ACCESS_NODE] =
    [](parse_result& /*parse*/, thing_of_code* code, node* n)
    {
      error_state state = CreateErrorState(TRAVERSING_AST, code, n);

      type_def* parentType = nullptr;
      switch (n->memberAccess.parent->type)
      {
        case VARIABLE_NODE:
        {
          assert(n->memberAccess.parent->variable.isResolved);
          assert(n->memberAccess.parent->variable.var->type.isResolved);
          parentType = n->memberAccess.parent->variable.var->type.def;
        } break;

        case MEMBER_ACCESS_NODE:
        {
          assert(n->memberAccess.parent->memberAccess.isResolved);
          assert(n->memberAccess.parent->memberAccess.member->type.isResolved);
          parentType = n->memberAccess.parent->memberAccess.member->type.def;
        } break;

        default:
        {
          RaiseError(state, ICE_UNHANDLED_NODE_TYPE, "PASS_resolveVars::MEMBER_ACCESS_NODE(parent)");
        } break;
      }

      if (n->memberAccess.child->type != VARIABLE_NODE)
      {
        RaiseError(state, ICE_UNHANDLED_NODE_TYPE, "PASS_resolveVars::MEMBER_ACCESS_NODE(child)");
      }
      
      assert(!(n->memberAccess.child->variable.isResolved));
      const char* childName = n->memberAccess.child->variable.name;

      for (auto* it = parentType->members.head;
           it < parentType->members.tail;
           it++)
      {
        variable_def* member = *it;

        if (strcmp(childName, member->name) == 0)
        {
          n->memberAccess.isResolved = true;
          n->memberAccess.member = member;
          return;
        }
      }

      RaiseError(state, ERROR_MEMBER_NOT_FOUND, childName, parentType->name);
    };
}
