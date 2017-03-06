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

### Primitives and Literals
Roo defines the following primitives:
| Name      | Description                                               | Example Literal   |
|:---------:|:---------------------------------------------------------:|:-----------------:|
| uint      | Generic unsigned integer                                  | `14u`             |
| int       | Generic signed integer                                    | `-7`              |
| u8 / i8   | 1-byte wide integers (unsigned and signed, respectively)  | `4`               |
| u16 / i16 | 2-byte wide integers (unsigned and signed, respectively)  | `255`             |
| u32 / i32 | 4-byte wide integers (unsigned and signed, respectively)  | `836234`          |
| u64 / i64 | 8-byte wide integers (unsigned and signed, respectively)  | `1e14`            |
| float     | floating-pointer number (can either be set as SP or DP)   | `2.76`            |
| char      | unsigned integer to represent characters (UTF-8 probably) | `'a'`             |

### Types
A custom type can be defined like:
``` roo
type Color
{
  r : u8
  g : u8
  b : u8
}
```

Types can be constructed and their members accessed like:
``` roo
cyan : Color(0, 255, 255)
amountOfGreenInCyan : u8 = cyan.g
```

### References
A reference to type `T` is a `T&`. References actually have two levels of mutablility, one that talks about the reference,
and another that talks about the thing being  referenced. A mutable reference can be used to change the thing it points to (as long as
that thing is mutable itself).

* An immutable reference to an immutable thing is a `T&`
* A mutable reference to an immutable thing is a `mut T&`
* An immutable reference to a mutable thing is a `T mut&`
* A mutable reference to a mutable thing is a `mut T mut&`

A variable may be referenced by as many immutable references as the programmer sees fit, **or** a single mutable reference. This, similiarly
to how Rust's borrowing system does so, prevents any chance or race conditions and increases safety.

### Arrays
An array of type `T` and size `16` is a `T[16u]`. The mutable qualifier in `mut T[16u]` references the mutability of the underlying elements.
An array's size is fixed at compile time and it will be allocated on the stack. Arrays can be indexed with the `[{uint}]` operator and bounds
checking, if possible, will occur at compile time.
The array size always appears after the type name, so a mutable array of 16 mutable references to `T` is a
`mut T[16u] mut&`. An array can be initialised as a literal with `{expression, expression, expression}`.

### Traits
Traits are abstract versions of types that define a set of members. Types can *implement* traits to inherit their
members. The trait can then be used as an alias for any type that implements it.

### Range Syntaxic Sugar
A range can be created with the syntax `(a..b)`. This can be used to iterate from `a` (inclusive) and `b` (exclusive).
They implement the `Iterable` trait, and so can be iterated over like so, where the type of `n` is inferred from the range.
``` roo
for (n : uint in <uint>(1..10))
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
