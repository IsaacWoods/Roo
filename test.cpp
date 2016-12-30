#include <cstdio>
#include <vector.hpp>

template<>
void Free<unsigned int>(unsigned int&)
{
}

int main()
{
  vector<unsigned int> v;
  InitVector<unsigned int>(v);

  for (unsigned int i = 13u;
       i < 56u;
       i++)
  {
    Add<unsigned int>(v, i);
  }

  for (unsigned int i = 7u;
       i > 0u;
       i--)
  {
    Add<unsigned int>(v, i);
  }

  SortVector<unsigned int>(v, [](unsigned int& a, unsigned int& b)
      {
        return (a < b);
      });

  for (auto* it = v.head;
       it < v.tail;
       it++)
  {
    printf("%u\n", *it);
  }

  printf("Last: %u\n", v[v.size - 1]);
  printf("Size: %u Capacity: %u\n", v.size, v.capacity);

  FreeVector<unsigned int>(v);

  return 0;
}
