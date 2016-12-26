# Specification
Roo is a toy programming language. It's base belief is that C had the right idea, but was executed
in a way that ended up with numerous compromises. Roo aims to create a programming language with the
power of languages like C and C++, but without some of the limitations of powerful languages.

### Aims
* Mostly procedural
* No separation of definition and implementation
* Easy build process - nice dependency and source management
* Terse and expressive syntax - no extranious punctuation and clear, concise meaning
* Access to the 'sharp' tools - no shying away from raw pointers or manual memory management

### The Vision
Roo aims not to limit the power of the programmer. You want to dereference pointer arithmetic fuelled by a random
number generator? Be my guest. It'll segfault, and Roo doesn't complain. It's a language designed around **good**
programmers, not **bad** ones.

### Importing
Imports tell the compiler what you'd like to link against when you create your executable. Depending on
where it's located, there are multiple ways of specifying dependencies.

A local dependency:
``` roo
import roo.random
```

A remote Git repository (automatically cloned to latest commit):
``` roo
import "https://github.com/IsaacWoods/SomeRooLibrary.git"
```

### Traits
Traits are abstract versions of types that define a set of members. Types can *implement* traits to inherit their
members. The trait can then be used as an alias for any type that implements it.

### Range Syntaxic Sugar
A range can be created with the syntax `(a..b)`. This can be used to iterate from `a` (inclusive) and `b` (exclusive).
They implement the `Iterable` trait, and so can be iterated over like so, where the type of `n` is inferred from the range.
``` roo
for (n : <uint>(1..10))
{
  PrintFmt("%u", n);
}
```

### Calling Convention - System V
| Register | Usage   | Saved By |
|:--------:|:-------:|:--------:|
| RAX      | General | Caller   |
| RBX      | General | Callee   |
| RCX      | General | Caller   |
| RDX      | General | Caller   |
| RSI      | General | Caller   |
| RDI      | General | Caller   |
| RSP      | General | Caller   |
| RBP      | General | Callee   |
| R8       | General | Caller   |
| R9       | General | Caller   |
| R10      | General | Caller   |
| R11      | General | Caller   |
| R12      | General | Callee   |
| R13      | General | Callee   |
| R14      | General | Callee   |
| R15      | General | Callee   |
