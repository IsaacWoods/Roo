# Roo
[![Build Status](https://travis-ci.org/IsaacWoods/Roo.svg?branch=master)](https://travis-ci.org/IsaacWoods/Roo)

Roo is a imperitive, toy programming language that takes concepts from functional programming. Its central goal
is to avoid the kitchen-sink-esque hodgepodge of languages like C++, instead expressing logic through fewer, more powerful
features.

### Aims
* No separation of definition and implementation (no headers)
* Easier build process - nice source and dependency management
* Safety - strong type-checking and powerful expression of programmer intent
* Terse, expressive syntax - no extranious punctuation

It looks something like this:
``` roo
#[Name(example)]

import Prelude

type color
{
  r : float
  g : float
  b : float
  a : float
}

fn PrintColor(color : color&)
{
  PrintFmt("Color: ({}, {}, {}, {})\n", color.r, color.g, color.b, color.a)
}

#[Entry]
fn Main() -> int
{
  #[Debug]
  Print("Hello, World!\n")

  // Make and print a color
  myColor : color(1.0f, 0.0f, 1.0f, 1.0f)
  PrintColor(myColor)

  return 0
}
```

### Using the compiler
* At the moment, The compiler can only produce executables usable on x86_64, System-V, ELF-compatible systems
* (Temporary step) Run `make prelude` to build `Prelude` (our standard library)
* Run `./roo` to compile and link all the files in the current directory
* Various DOT files will also be produced, which may be converted to PNG with `dot -Tpng -o {file}.png {file}.dot`

### Contributing
Contributions are welcome. Current tasks and long-term goals can be found on the [Trello](https://trello.com/b/zxHvpzTz/roo). Please keep to the style of the existing code.
