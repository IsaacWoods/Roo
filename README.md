# Roo
[![Build Status](https://travis-ci.org/IsaacWoods/Roo.svg?branch=master)](https://travis-ci.org/IsaacWoods/Roo)

Roo is a toy programming language that is mainly imperitive, but takes concepts from functional languages.
Its central goal is to design a language with good programmers in mind, instead of protecting against
bad ones. It aims to avoid the kitchen-sink-esque hodgepodge of languages like C++, expressing the logic
of a program through a few powerful features.

### Aims
Roo is based upon C, but tries to find better solutions to some of C's problems:
* No separation of definition and implementation (no headers)
* Easier build process - nice dependency management and source management
* Safety - strong type-checking and powerful expression of programmer intent
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

  return 0
}
```

### Using the compiler
At the moment, the compiler only produces executables usable with x86_64, System-V, ELF-compatible systems.
Running `./roo` in a directory of `.roo` files will compile and link them, producing an executable in the
current directory. It will also produce various DOT files, which may be converted to images with:
`dot -Tpng -o {someName}.png {someName}.dot`
