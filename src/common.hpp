/*
 * Copyright (C) 2016, Isaac Woods. All rights reserved.
 */

#pragma once

#include <cstdlib>
#include <cstdint>
#include <cstdio>

// --- Global template for freeing stuff that needs to be freed ---
template<typename T>
void Free(T& thing);

// --- Linked list ---
template<typename T>
struct linked_list
{
  struct link
  {
    T payload;
    link* next;

    T& operator*()
    {
      return this->payload;
    }
  } *first;

  T& operator[](unsigned int i)
  {
    auto* it = this->first;

    for (unsigned int j = 0u;
         j < i;
         j++)
    {
      it = it->next;
    }

    return **it;
  }

  link* tail;
};

template<typename T>
void CreateLinkedList(linked_list<T>& list)
{
  list.first = nullptr;
  list.tail  = nullptr;
}

/*
 * Clears the list and deletes the links, without freeing the contents.
 * NOTE(Isaac): obviously this makes it very easy to leak everywhere; use with caution
 */
template<typename T>
void UnlinkLinkedList(linked_list<T>& list)
{
  while (list.first)
  {
    typename linked_list<T>::link* temp = list.first;
    list.first = list.first->next;
    free(temp);
  }

  list.first = nullptr;
  list.tail = nullptr;
}

// O(n)
template<typename T>
void FreeLinkedList(linked_list<T>& list)
{
  list.tail = nullptr;

  while (list.first)
  {
    typename linked_list<T>::link* temp = list.first;
    list.first = list.first->next;

    Free<T>(**temp);
    free(temp);
  }
}

// O(1)
template<typename T>
void AddToLinkedList(linked_list<T>& list, T thing)
{
  typename linked_list<T>::link* link = static_cast<typename linked_list<T>::link*>(malloc(sizeof(typename linked_list<T>::link)));
  link->next = nullptr;
  link->payload = thing;

  if (list.tail)
  {
    list.tail->next = link;
    list.tail = link;
  }
  else
  {
    list.first = list.tail = link;
  }
}

// O(n) (I think)
template<typename T>
void RemoveFromLinkedList(linked_list<T>& list, T thing)
{
  if ((**(list.first)) == thing)
  {
    typename linked_list<T>::link* newFirst = list.first->next;
    Free<T>(**(list.first));
    free(list.first);
    list.first = newFirst;
    return;
  }

  typename linked_list<T>::link* previous = list.first;
  for (auto* it = list.first->next;
       it;
       it = it->next)
  {
    if (**it == thing)
    {
      typename linked_list<T>::link* newNext = it->next;
      Free<T>(**it);
      free(it);
      previous->next = newNext;
      return;
    }

    previous = it;
  }
}

// O(n)
template<typename T>
unsigned int GetSizeOfLinkedList(linked_list<T>& list)
{
  unsigned int size = 0u;

  for (auto* i = list.first;
       i;
       i = i->next)
  {
    ++size;
  }

  return size;
}

// O(terrible)
template<typename T>
void SortLinkedList(linked_list<T>& list, bool (*evaluationFn)(T& a, T& b))
{
  if (!(list.first))
  {
    return;
  }

  typename linked_list<T>::link* head = nullptr;

  while (list.first)
  {
    typename linked_list<T>::link* current = list.first;
    list.first = list.first->next;

    if (!head || (*evaluationFn)(**current, **head))
    {
      // Insert onto the head of the sorted list
      current->next = head;
      head = current;
    }
    else
    {
      typename linked_list<T>::link* p = head;

      while (p)
      {
        if (!(p->next) || (*evaluationFn)(**current, **(p->next)))
        {
          // Insert into the middle of the sorted list
          current->next = p->next;
          p->next = current;
          break;
        }

        p = p->next;
      }
    }
  }

  list.first = head;
}

// O(n)
template<typename T>
void CopyLinkedList(linked_list<T>& dest, linked_list<T>& src)
{
  for (auto* it = src.first;
       it;
       it = it->next)
  {
    AddToLinkedList<T>(dest, **it);
  }
}

/*
 * Turn the linked-list into a contiguous array of the same elements.
 */
// O(n)
template<typename T>
T* LinearizeLinkedList(linked_list<T>& list, unsigned int* size)
{
  T* result = static_cast<T*>(malloc(sizeof(T) * GetSizeOfLinkedList<T>(list)));

  unsigned int i = 0u;

  for (auto* it = list.first;
       it;
       it = it->next)
  {
    result[i++] = **it;
  }

  *size = i;
  return result;
}

// --- Common functions ---
char* itoa(int num, char* str, int base);
char* ReadFile(const char* path);

enum compile_result
{
  SUCCESS,
  SYNTAX_ERROR,
  LINKING_ERROR
};

enum token_type
{
  // Keywords
  TOKEN_TYPE,
  TOKEN_FN,
  TOKEN_TRUE,
  TOKEN_FALSE,
  TOKEN_IMPORT,
  TOKEN_BREAK,
  TOKEN_RETURN,
  TOKEN_IF,
  TOKEN_ELSE,

  // Punctuation n' shit
  TOKEN_DOT,
  TOKEN_COMMA,
  TOKEN_COLON,
  TOKEN_LEFT_PAREN,
  TOKEN_RIGHT_PAREN,
  TOKEN_LEFT_BRACE,             // {
  TOKEN_RIGHT_BRACE,            // }
  TOKEN_LEFT_BLOCK,             // [
  TOKEN_RIGHT_BLOCK,            // ]
  TOKEN_ASTERIX,
  TOKEN_PLUS,
  TOKEN_MINUS,
  TOKEN_SLASH,
  TOKEN_EQUALS,
  TOKEN_BANG,                   // !
  TOKEN_TILDE,                  // ~
  TOKEN_PERCENT,
  TOKEN_QUESTION_MARK,
  TOKEN_POUND,                  // #

  TOKEN_YIELDS,                 // ->
  TOKEN_START_ATTRIBUTE,        // #[
  TOKEN_EQUALS_EQUALS,
  TOKEN_BANG_EQUALS,
  TOKEN_GREATER_THAN,
  TOKEN_GREATER_THAN_EQUAL_TO,
  TOKEN_LESS_THAN,
  TOKEN_LESS_THAN_EQUAL_TO,
  TOKEN_DOUBLE_PLUS,
  TOKEN_DOUBLE_MINUS,
  TOKEN_LEFT_SHIFT,
  TOKEN_RIGHT_SHIFT,
  TOKEN_LOGICAL_AND,
  TOKEN_LOGICAL_OR,
  TOKEN_BITWISE_AND,
  TOKEN_BITWISE_OR,
  TOKEN_BITWISE_XOR,

  // Other stuff
  TOKEN_IDENTIFIER,
  TOKEN_STRING,
  TOKEN_NUMBER_INT,
  TOKEN_NUMBER_FLOAT,
  TOKEN_CHAR_CONSTANT,
  TOKEN_LINE,
  TOKEN_INVALID,

  NUM_TOKENS
};

const char* GetTokenName(token_type type);
