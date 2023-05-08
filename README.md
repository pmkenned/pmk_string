# String Library

This library is meant to be an extremely simple and easy-to-use string library
for C programs. It consists of only one file, `pmk_string.h` which should
compile with GCC and clang using `-std=c99`. *Note*: MSVC has not yet been
tested.

To get started, add `pmk_string.h` to your project and add the following two
lines to exactly one of your translation units:

```
#define PMK_STRING_IMPL
#include "pmk_string.h"
```

All other files which use the library must have only the `#include`.

## Overview

This library is meant to be very minimal. It provides two data types defined
like so:

```
typedef struct {
    char * data;
    size_t len;
} String;

typedef struct {
    char * data;
    size_t len;
    size_t cap;
} StringBuilder;
```

The `String` type is applicable when you simply want to refer to a specific
sequence of bytes making up a string. This data may be nul-terminated or not;
it may overlap with other strings or not; it may exist on the stack, the heap,
BSS or data segment; it may be read-only or read-write. The `String` data type
doesn't tell you anything other than that a sequence of `len` bytes beginning
at the address pointed to by `data` make up a string.

The `StringBuilder` type is applicable when you have a buffer with a specific
capacity which may be filled with bytes making up a string. The buffer may be
fixed-size or resizable. Generally, the contents can be assumed to be
nul-terminated as the functions which operate on them ensure this.

## Example Usage

The following is a complete program which uses the `String` type to refer to a
string literal and print it using `printf()`:

```
// Example 1
#define PMK_STRING_IMPL
#include "pmk_string.h"
#include <stdio.h>

int main()
{
    String name = str_lit("Paul");
    printf("Hello, %.*s\n", len_data(name));
    return 0;
}
```

Note the use of the `%.*s` format specifier. This expects two arguments: an
`int` specifying the number of characters from the string to print and a
pointer to the string. The `len_data` macro is used to insert these two
arguments.

Technically, we could have used `%s` with `name.data` because string literals
are automatically nul-terminated, but there is no requirement that a `String`
refer to a nul-terminated string (it could, for example, be used to refer to a
substring of some other string), so it is always a good idea to print them
using the `%.*s` format specifier and `len_data` macro.

The next example shows how to use the `StringBuilder` type:

```
// Example 2
void string_builder_example()
{
    String month = str_lit("May");
    int day_of_month = 9, year = 2023;

    StringBuilder builder = {0}; // must be initialized
    builder = builder_reserve(1 << 10); // optional

    builder_append(&builder, str_lit("Hello, "));
    builder_print(&builder, "today is %.*s ", len_data(month));
    builder_print(&builder, "%d, %d.", day_of_month, year);

    printf("%.*s\n", len_data(builder));

    free(builder.data);
    builder = {0};
}
```

The `builder_append()` and `builder_print()` functions used above will
automatically allocate enough memory to hold the result, resizing as necessary.

If you have some idea of how much memory you are likely to need, you can use
`builder_reserve()` to reserve a specific amount memory ahead of time to reduce
the number of reallocations.

There are a few things to note about Example 2:

- It is important to initialize a `StringBuilder` by assigning to `{0}`. If a
  `StringBuilder` is not initialized, the functions which operate it will
  likely crash.
- The same `len_data` macro that is used when printing a `String` may also be
  used when printing the contents of a `StringBuilder`.
- The `builder_append()` function is used for appending a `String`, but this
  can also be accomplished using the `builder_print()` function.
- Like `builder_append()`, `builder_print()` will append to the string being
  built. If you wish to reuse the `StringBuilder`, you can simply set the `len`
  field to 0. Subsequent operations will overwrite any existing data in the
  buffer.

As mentioned in the overview, a `StringBuilder` may use a fixed-sized buffer.
This is useful in situations where you want to avoid dynamic memory
allocations.

To initialize a `StringBuilder` to use a fixed-sized buffer, use the
`builder_from_fixed()` macro as in Example 3 below.

```
// Example 3
void string_builder_fixed_example()
{
    String month = str_lit("May");
    int day_of_month = 9, year = 2023;

    char buffer[1024];
    StringBuilder builder = {0};
    builder = builder_from_fixed(buffer);

    builder_append_fixed(&builder, str_lit("Hello, "));
    builder_print_fixed(&builder, "today is %.*s ", len_data(month));
    builder_print_fixed(&builder, "%d, %d.", day_of_month, year);

    printf("%.*s\n", len_data(builder));
}
```

Note that when using a fixed-sized buffer, you must use the `*_fixed`
functions. The non-fixed functions (e.g. `builder_print`) assume that the
string builder is dynamically allocated and will attempt to resize if
necessary. The `*_fixed` functions, however, will simply return an error if
they run out of space in the buffer.

Also note that we do not free the string builder since no allocations have been
performed.

For more examples, see `examples.c`.

## Error Handling

For the most part, functions which can indicate errors do so by returning
`-errno`.  This makes it easy to check for errors: simply check `f() < 0`.

Note that better error handling is on the TODO list. Currently, some functions
terminate the program on certain kinds of errors or return positive numbers for
errors or just return -1.

## Dynamic Memory Allocation

This library is meant to be versatile when it comes to dynamic memory
allocation. This is accomplished with the following two features:

- Replacing the default allocator through a preprocessor macro
- Use of the `*_context()` functions which accept a `void *` as their first
  parameter to pass arbitrary data to the allocator

By default, functions needing dynamic memory allocation will use `realloc`, but
you can replace this function by defining `PMK_REALLOC`. For example, if
`my_realloc()` is a function with the same signature as `realloc`, you can use
this allocator like so:

```
#define PMK_REALLOC(context, pointer, size) my_realloc(pointer, size)
```

Notice that the `context` argument is discarded. This parameter is
used by the `*_context()` functions. If you do not use these functions, you
should have no problem.

But if your allocator needs additional information to perform allocations, then
you must pass this pointer to your allocator:

```
#define PMK_REALLOC(context, pointer, size) my_realloc(context, pointer, size)
```

The file `pmk_arena.h` has been included to show an example of overriding the
default allocator with a simple arena allocator.

## Known Issues

- Naming: function names collide with reserved namespaces
- Error handling: functions are inconsistent with respect to how they indicate
  and handle errors
- MSVC: this code has not been tested on MSVC and may not compile
- Unicode: Nothing special has been done to support unicode
- Portability: Makes use of POSIX API
- OOM: does not attempt to detect or handle out-of-memory
