/*
 * Copyright (C) 2016-2017, Isaac Woods. All rights reserved.
 */

#pragma once

#include <ir.hpp>
#include <vector.hpp>

enum class task_type
{
  GENERATE_AIR,
};

struct task_info
{
  task_type type;
  union
  {
    thing_of_code* code;
  };
};

struct scheduler
{
  vector<task_info*> tasks;
};

void InitScheduler(scheduler& s);
void AddTask(scheduler& s, task_type type, ...);
task_info* GetTask(scheduler& s);
