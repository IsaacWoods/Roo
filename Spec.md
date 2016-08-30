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
Roo aims not to limit the power of the programmer. You want to dereference unchecked pointer arithmetic
fuelled from a random number generator? Be my guest. It'll segfault, and Roo doesn't complain. It's a
language designed around **good** programmers, not **bad** ones.

### Importing
Imports tell the compiler what you'd like to link against when you create your executable. Depending on
where it's located, there are multiple ways of specifying dependencies.

A local dependency:
``` roo
import roo.random
```

A remote Git repository (automatically cloned to latest commit):
``` roo
import 'https://github.com/IsaacWoods/SomeRooLibrary.git
```
