# File format for Roo Modules
Information about Roo modules are stored in a `.roomod` file (later on, this may be included in the ELF
relocatable itself). This is because Roo lacks header files, but still needs to know information at compile-time
that either is required before linking, or isn't present in the relocatable (such as template information and
types).

### Overview
A `.roomod` file encodes information about:
* Types
* Things of code (functions and operators)

They start with a header, that specifies the number of types, functions and operators in the file.

### Binary format
| Offset  | Size (bytes)  | Name                | Description                           |
|---------|---------------|---------------------|---------------------------------------|
| 0x00    | 4             | Magic               | `0x7F 'R' 'O' 'O'`                    |
| 0x04    | 1             | Version             | Version of the file format used       |
| 0x05    | 4             | `type_count`        | Number of type entries in the file    |
| 0x09    | 4             | `code_thing_count`  | Number of thing entries in the file   |

The rest of the file starts from 0xD.

### String encoding
**NOTE: This format doesn't allow encoding of non-ASCII or strings longer than 255 characters**
String are encoded as a 1-byte length (in bytes), including the null-terminator, followed by the ASCII bytes of the string
and null-terminated by `'\0'`.

### Vector encoding
**NOTE: This format doesn't allow encoding of vectors of more than 255 elements**
A vector of length `N` of type `T` is encoded as a 1-byte length (in elements), followed by `N` elements of size `sizeof(T)`.

### Variable Definition encoding
| Field                 | Type            | Size (bytes)  | Description                           |
|-----------------------|-----------------|---------------|---------------------------------------|
| `name`                | String          | variable      | The name of the variable              |
| `typeName`            | String          | variable      | The name of the type of the variable  |
| `isMutable`           | Boolean         | 1             | Whether the variable is mutable       |
| `isReference`         | Boolean         | 1             | Whether the variable is a reference   |
| `isReferenceMutable`  | Boolean         | 1             | Whether the reference is mutable      |
| `arraySize`           | Unsigned        | 4             | If `0`, it's not an array             |

### Type entries
| Field     | Type            | Size (bytes)  | Description                 |
|-----------|-----------------|---------------|-----------------------------|
| `name`    | String          | variable      | The name of the type        |
| `members` | Vector<Var-Def> | variable      | The type's member variables |
| `size`    | Unsigned        | 4             | The type's size in bytes    |

### Thing entries
| Field       | Type            | Size (bytes)  | Description                             |
|-------------|-----------------|---------------|-----------------------------------------|
| `type`      | Enum            | 1             | `0` for functions, `1` for operators    |
| `name`(f)   | String          | variable      | The name of the function                |
| `token`(op) | Enum            | 4             | The token of the operator               |
| `params`    | Vector<Var-Def> | variable      | The parameters to the function/operator |
| `symbol`    | String          | variable      | The symbol of the function/operator     |

Only either `name` or `token` is present, the former for functions and the latter for operators.
