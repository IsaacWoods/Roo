# Roo
[![Build Status](https://travis-ci.org/IsaacWoods/Roo.svg?branch=master)](https://travis-ci.org/IsaacWoods/Roo)

Roo is a predominantly procedural programming language built to experiment with a new language.
Its central goal is to design a language with good programmers in mind, instead of protecting against
bad ones. It aims to avoid the kitchen-sink-esque hodgepodge of languages like C++ but also not to become
obsessed with a theoretical ideal like functional programming languages have tended to.

### Aims
Roo is based upon C, but tries to find better solutions to some of C's problems:
* No separation of definition and implementation (no headers)
* Easier build process - nice dependency management and source management
* Terse, expressive syntax - no extranious semi-colons or extra punctionation
* Access to the 'sharp tools' - doesn't shy away from raw pointers or manual memory management

It looks something like this:

``` roo
import roo.io

type color
{
  r : float
  g : float
  b : float
  a : float
}

fn PrintColor(color : color&)
{
  PrintFmt("Color: (%f, %f, %f, %f)", color.r, color.g, color.b, color.a)
}

#[Entry]
fn Main() -> int
{
  #[Debug]
  Print("Hello, World!")

  // Make and print a color
  myColor : color(1.0f, 0.0f, 1.0f, 1.0f)
  PrintColor(myColor)
}
```

### Using the compiler
At the moment, the compiler only produces executables usable with 64-bit, System-V, ELF-compatible systems.
Running `./roo` in a directory of `.roo` files will compile and link them, producing an executable in the
current directory. It will also produce various DOT files, which may be converted to images with:
`dot -Tpng -o {someName}.png {someName}.dot`
