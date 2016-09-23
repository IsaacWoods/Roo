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

### Register Allocation - Function Level
For each function, a register state should be constructed. This is a table of registers and their states at each
point in the program.

A register can be:
* Free for general use
* Usuable, but for a specific purpose (RAX for returning things)
* In use - already in use by another variable
* Reserved

On entering a function, a initial register state should be constructed. This must mark registers being used by
parameters and those reserved by the stack frame. A register allocation algorithm can then be run over the AST to
add register states at different points in the function. The algorithm should take into account:
* Does something need to be returned? What's closest to the thing being returned, that should start in RAX?
