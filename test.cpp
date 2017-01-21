struct Vec
{
  float x, y;

  Vec(float x, float y)
    :x(x)
    ,y(y)
  { }

  Vec operator +(const Vec& v)
  {
    return Vec(x+v.x, y+v.y);
  }
};

int main()
{
  Vec a(3, 4);
  Vec b(7, 6);
  Vec c = a + b;
  return 0;
}
