#include <stdio.h>

typedef struct
{
  int x;
  int y;
} vec_t;

int main()
{
  vec_t v{2, 5};
  int i = v.x + v.y;
}
