/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#include <scheduler.hpp>
#include <cstdarg>

template<>
void Free<task_info*>(task_info*& task)
{
  free(task);
}

void InitScheduler(scheduler& s)
{
  InitVector<task_info*>(s.tasks);
}

template<>
void Free<scheduler>(scheduler& s)
{
  FreeVector<task_info*>(s.tasks);
}

void AddTask(scheduler& s, task_type type, ...)
{
  va_list args;
  va_start(args, type);

  task_info* task = static_cast<task_info*>(malloc(sizeof(task_info)));
  task->type = type;

  switch (type)
  {
    case task_type::GENERATE_AIR:
    {
      task->code = va_arg(args, thing_of_code*);
    } break;
  }

  Add<task_info*>(s.tasks, task);
  va_end(args);
}

task_info* GetTask(scheduler& s)
{
  return PopBack<task_info*>(s.tasks);
}
