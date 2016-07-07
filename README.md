# Roo

Roo is a predominantly procedural programming language built both the experiment with a new language, and to
show the construction of a non-trivial compiler. Its construction is detailed in a (currently non-existant) series
of blog posts.

### Aims
Roo is based upon C, but tries to find better solutions to some of C's problems:
* No headers or separation of definition and implementation
* Better dependency management - something more like Rust's Cargo
* Terse syntax - no extranious semi-colons or extra punctionation
* Better safety - specific markers for unsafe operations, full type-safety otherwise

It looks something like this:

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
