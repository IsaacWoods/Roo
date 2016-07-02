# Roo

Roo is a predominantly procedural programming language built both the experiment with a new language, and to
show the construction of a non-trivial compiler. Its construction is detailed in a (currently non-existant) series
of blog posts. It looks something like this:

``` roo
type Color
{
  Float r
  Float g
  Float b
  Float a
}

fn PrintColor(Color& color)
{
  PrintFmt("Color: (%f, %f, %f, %f)", color.r, color.g, color.b, color.a)
}

fn Main() -> Int
{
  Print("Hello, World!")

  // Make and print a color
  Color myColor(1.0f, 0.0f, 1.0f, 1.0f)
  PrintColor(myColor)
}
```
