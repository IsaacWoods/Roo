/*
 * Copyright (C) 2017, Isaac Woods.
 * See LICENCE.md
 */

#pragma once

#include <cstring>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cassert>

template<typename T>
void Free(T&);

#define VECTOR_INITIAL_CAPACITY 8

template<typename T>
struct vector
{
  T* head;
  T* tail;
  unsigned int size;
  unsigned int capacity;

  T& operator[](unsigned int i)
  {
    return head[i];
  }

  const T& operator[](unsigned int i) const
  {
    return head[i];
  }
};

template<typename T>
void InitVector(vector<T>& v)
{
  v.head = nullptr;
  v.tail = nullptr;
  v.size = 0u;
  v.capacity = 0u;
}

// O(n)
template<typename T>
void FreeVector(vector<T>& v)
{
  for (auto* it = v.head;
       it < v.tail;
       it++)
  {
    Free<T>(*it);
  }

  free(v.head);
  v.head = nullptr;
  v.tail = nullptr;
  v.size = 0u;
  v.capacity = 0u;
}

// O(1)
template<typename T>
void DetachVector(vector<T>& v)
{
  free(v.head);
  v.head = nullptr;
  v.tail = nullptr;
  v.size = 0u;
  v.capacity = 0u;
}

// O(1)
template<typename T>
void Add(vector<T>& v, const T& thing)
{
  if (!(v.head))
  {
    v.capacity = VECTOR_INITIAL_CAPACITY;
    v.head = static_cast<T*>(malloc(sizeof(T) * v.capacity));
    v.tail = v.head;
  }
  else if ((v.size + 1u) > v.capacity)
  {
    T* oldHead = v.head;

    v.capacity *= 2u;
    v.head = static_cast<T*>(malloc(sizeof(T) * v.capacity));
    memcpy(v.head, oldHead, sizeof(T) * v.size);
    v.tail = v.head + (ptrdiff_t)v.size;

    free(oldHead);
  }

  *(v.tail) = thing;
  v.tail++;
  v.size++;
}

// O(n)
template<typename T>
void UnstableRemove(vector<T>& v, const T& thing)
{
  if (v.size == 0u)
  {
    return;
  }

  if (v.size == 1u)
  {
    if (*(v.head) == thing)
    {
      v.size--;
      v.tail--;
    }
  }
  else
  {
    for (auto* it = v.head;
         it < v.tail;
         it++)
    {
      if (*it == thing)
      {
        // Move the end thing into the place of the removed thing
        v.tail--;
        *it = *v.tail;
        v.size--;
        break;
      }
    }
  }
}

// O(n log(n))
template<typename T>
void StableRemove(vector<T>& v, const T& thing)
{
  if (v.size == 0u)
  {
    return;
  }

  if (v.size == 1u)
  {
    if (*(v.head) == thing)
    {
      v.size--;
      v.tail--;
    }
  }
  else
  {
    for (auto* it = v.head;
         it < v.tail;
         it++)
    {
      if (*it == thing)
      {
        // Move everything back a space, to maintain stability
        v.size--;
        v.tail--;
        auto* current = it;

        while (current < v.tail)
        {
          /*
           *  NOTE(Isaac): changing `current` more than once between sequence points is UB, so this
           *  must be split into two statements.
           */
          *current = *(current + 1);
          current++;
        }
        break;
      }
    }
  }
}

// O(1)
template<typename T>
T PopBack(vector<T>& v)
{
  assert(v.size > 0u);

  T last = v[v.size - 1u];
  v.size--;
  v.tail--;

  return last;
}

// O(n log(n))
template<typename T>
void SortVector(vector<T>& v, bool (*evaluationFn)(T& a, T& b))
{
  if (!(v.head) || v.capacity < 2u)
  {
    return;
  }

  for (auto* itA = v.head;
       itA < v.tail;
       itA++)
  {
    for (auto* itB = v.head;
         itB < v.tail;
         itB++)
    {
      if ((*evaluationFn)(*itA, *itB))
      {
        /*
         * NOTE(Isaac): This can't be a reference, because then it would change after the copy into itB
         */
        T temp = *itB;
        *itB = *itA;
        *itA = temp;
      }
    }
  }
}
