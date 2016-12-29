#include <cstdio>
#include <vector.hpp>

template<>
void Free<unsigned int*>(unsigned int*& i)
{
  free(i);
}

int main()
{
  vector<unsigned int*> v;
  InitVector<unsigned int*>(v);

  for (unsigned int i = 0u;
       i < 50u;
       i++)
  {
    Add<unsigned int*>(v, new unsigned int(i));
  }

  for (auto* it = v.head;
       it < v.tail;
       it++)
  {
    printf("%u\n", **it);
  }

  printf("Last: %u\n", *(v[v.size - 1]));
  printf("Size: %u Capacity: %u\n", v.size, v.capacity);

  FreeVector<unsigned int*>(v);

  return 0;
}
