/* pmk_string.h

This library is meant to be an extremely simple and easy-to-use string library
for C programs. It consists entirely of this one file.

To use this library, do this in *one* C file:
    #define PMK_STRING_IMPL
    #include "pmk_string.h"

This library uses two types: String and StringBuilder.

The String type comprises a pointer to the data and a length. This type is
applicable when you simply want to refer to a specific sequence of bytes making
up a string. This data may be nul-terminated or not; it may overlap with other
strings or not; it may exist on the stack, the heap, BSS or data segment; it
may be read-only or read-write. The String data type doesn't tell you anything
other than that a sequence of len bytes beginning at the address pointed to by
data make up a string.

The StringBuilder type has a capacity in addition to the data pointer and
length. This type is applicable when you have a buffer with a specific capacity
which may be filled with bytes making up a string. The buffer may be fixed-size
or resizable. Generally, the contents can be assumed to be nul-terminated as
the functions which operate on them ensure this.

In general, if you need to concatenate strings or perform other operations
which may require memory allocation such as search and replace, you'll be using
the StringBuilder type and its associated functions. But if you just want to
refer to a read-only string or divide a string into substrings or make changes
which do not require allocation such as changing capitalization or replacing
all occurrences of one character with another, you'll be using the String type
and its functions.

IMPORTANT: String and StringBuilder both use int32_t to store the length (and
capacity in the case of StringBuilder). This means they are limited to
(2^31)-1, or about 2 billion bytes. Why not use size_t instead?  Primarily to
save space. Using int32_t instead of size_t means that on a 64-bit system, a
StringBuilder occupies 16 bytes instead of 24 bytes. Assuming a 64b cache line
size, each cache line can store 4 StringBuilders instead of an average of 2.5.
Why not use uint32_t and get twice the range? Because negative indices are
useful for various things and 2 billion is plenty big enough for 99% of cases.
If you really need longer strings, you probably want a more specialized
library.

## String Type

Here's an example of creating a String and initializing it to refer to a
read-only string:

    String name = { .data = "Paul", .len = 4 };

Obviously, it would be tedious and error-prone to have to manually count the
number of characters in the string literal. The str_lit macro does this for
you:

    String name = str_lit("Paul");

To print the string using printf, use the %.*s format specifier which expects
an int specifying the number of bytes in the string and a pointer to the
string:

    printf("%.*s\n", (int) name.len, name.data);

The len_data macro can be used to save typing:

    printf("%.*s\n", len_data(name));

Because string literals are automatically nul-terminated by the C compiler, we
could have printed the string like so:

    printf("%s\n", name.data);

But keep in mind that a String does not need to be nul-terminated, so this
should generally be avoided.

The fact that Strings do not have to be nul-terminated allows us to take
substrings very easily without doing any allocations. For example:

    String full_name  = str_lit("Paul Kennedy");
    String first_name = string_substr(full_name, 0, 4);
    String last_name  = string_substr(full_name, 5, 12);
    printf("First name: %.*s\n", len_data(first_name));
    printf("Last name: %.*s\n", len_data(last_name));

That's most of what you need to know about strings. For the most part, the
functions which operate on strings do not do any allocations with the exception
of string_dup(). Some of the functions modify the strings, such as
string_toupper(), but most of them simply compute some function of the input
string(s) with no side-effects, such as string_equal().

Note that some functions such as string_trim() sound like they modify the input
string but actually do not; string_trim() returns a substring of the input
string. To trim a string, it is necessary to capture the result:

    name = string_trim(name);

The set of functions has been loosely based on the string functions in the C
standard library with some omissions such as strlen() (which of course is
superfluous) and some additions such as string_starts_with(). Some functions
have been given different names from their C standard library counterparts,
such as string_find() which is analogous to strstr().

There are also some differences in functionality. For example, strchr() returns
a char *, and if the given character isn't found in the string, it returns
NULL, whereas string_char() returns an index indicating how far into the string
the character is found, and returns string.len if the character isn't found.

## StringBuilder Type

Here is an example of how you would use the StringBuilder type:

    StringBuilder builder = {0};
    builder_print(&builder, "My name is %s. I am %d years old.", "Paul", 33);
    printf("%.*s\n", len_data(builder));

Note the initialization of builder to {0}. This is important. Passing an
unitialized StringBuilder to any of the builder functions is undefined.

In general, the builder functions will dynamically allocate memory for the
StringBuilder as necessary. They will also nul-terminate the buffer for
convenience.

If you just want to append a String and don't need format specifiers, you can
use builder_append():

    String name = str_lit("Paul");
    builder_append(&builder, name);

You can easily create a String from a StringBuilder using the
builder_to_string macro.

    String bs = builder_to_string(builder);

This can be convenient if you want to pass the StringBuilder to one of the
String functions. For example:

    int32_t line_count = string_count(bs, '\n');

But be aware that if you then perform an operation on the StringBuilder which
requires reallocation, the String will no longer point to valid data.

In general, when adding to a StringBuilder, if the current capacity is
insufficient, the buffer will be doubled in size. In case doubling is not
enough, it will be expanded just enough to fit.

If you have some idea of how much memory you are likely to need, you can use
builder_reserve() to reserve a specific amount memory ahead of time to reduce
the number of reallocations:

    StringBuilder builder = {0};
    builder = builder_reserve(&builder, 1 << 12); // 4 KiB

To deallocate, call builder_destroy():

    builder_destroy(&builder);

This will deallocate the buffer and reinitialize to {0}, making it safe to
reuse.

If you wish to reuse a StringBuilder with its currently allocated buffer, you
can simply set the len field to 0:

    builder_print(&builder, "%s %d\n", "hello", 123);
    builder.len = 0;
    builder_print(&builder, "%s %d\n", "goodbyte", 456);

In addition to the functions mentioned so far, there are functions for
replacing substrings, splicing, reading lines of input from a stream and
reading files.

## Using Fixed-Size Buffers

As previously mentioned, the buffer for a StringBuilder does not have to be
dynamically allocated. It can be a fixed-sized buffer located on the stack,
BSS, or data segment.

Here is an example of how you would use a StringBuilder with a fixed-sized
buffer:

    char buffer[1024];
    StringBuilder builder = builder_from_fixed(buffer);
    builder_print(&builder, "%s %d\n", "hello", 123);

If the fixed-size buffer ever reaches capacity, it will automatically be
converted to a dynamically-allocated buffer.

StringBuilder's with fixed-sized buffers can safely be passed to
builder_destroy() which will merely set the length to 0.

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
- Use of the *_context() functions which accept a void * as their first
  parameter to pass arbitrary data to the allocator

By default, functions needing dynamic memory allocation will use realloc, but
you can replace this function by defining PMK_REALLOC. For example, if
my_realloc() is a function with the same signature as realloc, you can use it
like so:

    #define PMK_REALLOC(context, ptr, old_size, new_size) my_realloc(ptr, new_size)

Notice that in this example, the context and old_size macro parameters are being
discarded. If your allocator needs to be told the size of the allocation it is
resizing, you can simply pass in the old_size parameter.

The same thing applies for the context parameter. All the functions in this
library with names ending in _context take in a context parameter which they
will pass to PMK_REALLOC. This is useful for allocators which need additional
data in order to perform allocations, e.g. an arena allocator with separate
arenas can use the context parameter to know which arena to allocate from.

(If your allocator doesn't take a context parameter, it doesn't
make sense to use the _context functions because the parameter will be
discarded)

## Known Issues

- Naming: function names collide with reserved namespaces
- Error handling: functions are inconsistent with respect to how they indicate
  and handle errors
- Unicode: Nothing special has been done to support unicode
- Portability: Makes use of POSIX API
- OOM: does not attempt to detect or handle out-of-memory

*/

#ifndef PMK_STRING_H
#define PMK_STRING_H

#include <stdint.h>

typedef   int8_t s8;
typedef  uint8_t u8;
typedef  int16_t s16;
typedef uint16_t u16;
typedef  int32_t s32;
typedef uint32_t u32;
typedef  int64_t s64;
typedef uint64_t u64;

#include <stddef.h>
#include <stdio.h>

typedef struct {
    char * data;
    s32 len;
} String;

#define str_lit(S)              (String) { .data = (S), .len = sizeof(S)-1 }
#define str_lit_const(S)                 { .data = (S), .len = sizeof(S)-1 }
#define str_cstr(S)             (String) { .data = (S), .len = strlen(S)   }
#define len_data_lit(S)         (int) sizeof(S)-1, (S)
#define len_data(S)             (int) (S).len, (S).data
#define str_left_adjust(S,N)    do { (S).data += N; (S).len -= N; } while (0)

int     string_equal        (String s1, String s2);
int     string_equaln       (String s1, String s2, s32 n);
int     string_compare      (String s1, String s2);
int     string_compare_qsort(const void * a, const void * b);
String  string_substr       (String string, s32 start, s32 end);
String  string_dup          (String string);
String  string_trim         (String string);
s32     string_char         (String string, char c);
s32     string_rchar        (String string, char c);
s32     string_span         (String string, String accept);
s32     string_cspan        (String string, String reject);
s32     string_find         (String haystack, String needle);
String  string_break        (String string, String accept);
String  string_tokenize     (String string, String delim, s32 * saveptr);
void    string_tr           (String string, char x, char y);
void    string_toupper      (String string);
void    string_tolower      (String string);
s32     string_count        (String string, char c);
int     string_starts_with  (String string, String prefix);
int     string_ends_with    (String string, String suffix);
int     string_parse_int    (String string, int * result);

typedef struct {
    char * data;
    s32 len;
    s32 cap;
} StringBuilder;

#define builder_to_string(B)    (String) { .data = (B).data, .len = (B).len }
#define builder_from_fixed(F)   (StringBuilder) { .data = (F), .cap = DOWN_TO_ODD(sizeof(F)) }

#define builder_reserve(B,CAP)      builder_reserve_context     (NULL, B, CAP)
#define builder_destroy(B)          builder_destroy_context     (NULL, B)
#define builder_append(B,STR)       builder_append_context      (NULL, B, STR)
#define builder_print(B,FMT,...)    builder_print_context       (NULL, B, FMT, __VA_ARGS__)
#define builder_replace(B,X,Y)      builder_replace_context     (NULL, B, X, Y)
#define builder_splice(B,S,E,STR)   builder_splice_context      (NULL, B, S, E, STR)
#define builder_getline(B,F)        builder_getline_context     (NULL, B, F)
#define builder_read_file(B,F)      builder_read_file_context   (NULL, B, F)

void    builder_reserve_context     (void * context, StringBuilder * builder, s32 cap);
void    builder_destroy_context     (void * context, StringBuilder * builder);
void    builder_append_context      (void * context, StringBuilder * builder, String string);
int     builder_print_context       (void * context, StringBuilder * builder, const char * fmt, ...);
int     builder_replace_context     (void * context, StringBuilder * builder, String x, String y);
void    builder_splice_context      (void * context, StringBuilder * builder, s32 start, s32 end, String string);
int     builder_getline_context     (void * context, StringBuilder * builder, FILE * fp);
int     builder_read_file_context   (void * context, StringBuilder * builder, const char * filename);

#endif /* PMK_STRING_H */

#ifdef PMK_STRING_IMPL

#include <stdio.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifndef PMK_REALLOC
#include <stdlib.h>
#define PMK_REALLOC(c,p,os,ns) realloc(p,ns)
#endif
#define PMK_MALLOC(c,ns)    PMK_REALLOC(c,NULL,0,ns)
#define PMK_FREE(c,p)       (p = PMK_REALLOC(c,p,0,0))

#define XSTR(X)  #X
#define STR(X) XSTR(X)

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

#define UP_TO_EVEN(X)   (((X)+1) & ~1)
#define DOWN_TO_ODD(X)  (((X)-1) | 1)

#define IS_FIXED(B) ((B).cap & 1)

int
string_equal(String s1, String s2)
{
    if (s1.len != s2.len)
        return 0;
    return memcmp(s1.data, s2.data, s1.len) == 0;
}

// NOTE: if either string is shorter than n, returns false
int
string_equaln(String s1, String s2, s32 n)
{
    if (s1.len < n || s2.len < n)
        return 0;
    return memcmp(s1.data, s2.data, n) == 0;
}

int
string_compare(String s1, String s2)
{
    s32 min_len = s1.len < s2.len ? s1.len : s2.len;
    int cmp = memcmp(s1.data, s2.data, min_len);
    if (cmp == 0) {
        if (s1.len == s2.len)
            return 0;
        // When two strings of different lengths match up to the length of the
        // shorter string, the shorter string is considered to come first.
        return s1.len < s2.len ? -1 : 1;
    }
    return cmp;
}

int
string_compare_qsort(const void * a, const void * b)
{
    String * as = (String *) a;
    String * bs = (String *) b;
    return string_compare(*as, *bs);
}

String
string_substr(String string, s32 start, s32 end)
{
    if (start < 0) start += string.len;
    if (end < 0)   end   += string.len;
    assert(start >= 0);
    assert(end >= 0);
    assert(start <= end);
    assert(end <= string.len);
    return (String) {
        .data = string.data + start,
        .len = end - start
    };
}

// nul-terminates the copy
String
string_dup(String string)
{
    String result;
    result.data = PMK_MALLOC(NULL, string.len + 1);
    result.len = string.len;
    memcpy(result.data, string.data, string.len);
    result.data[result.len] = '\0';
    return result;
}

String string_ltrim(String string)
{
    s32 i = 0;
    for (; i < string.len; i++) {
        if (!isspace(string.data[i]))
            break;
    }
    string.data += i;
    string.len -= i;
    return string;
}

String string_rtrim(String string)
{
    s32 i = string.len;
    for (; i > 0; i--) {
        if (!isspace(string.data[i-1]))
            break;
    }
    string.len = i;
    return string;
}

String
string_trim(String string)
{
    return string_rtrim(string_ltrim(string));
}

s32
string_char(String string, char c)
{
    for (s32 i = 0; i < string.len; i++) {
        if (string.data[i] == c)
            return i;
    }
    return string.len;
}

s32
string_rchar(String string, char c)
{
    for (s32 i = string.len-1; i >= 0; i--) {
        if (string.data[i] == c)
            return i;
    }
    return string.len; // return -1?
}

s32
string_span(String string, String accept)
{
    s32 i, j;
    for (i = 0; i < string.len; i++) {
        for (j = 0; j < accept.len; j++) {
            if (string.data[i] == accept.data[j])
                break;
        }
        if (j == accept.len)
            break;
    }
    return i;
}


s32
string_cspan(String string, String reject)
{
    s32 i, j;
    for (i = 0; i < string.len; i++) {
        for (j = 0; j < reject.len; j++) {
            if (string.data[i] == reject.data[j])
                break;
        }
        if (j < reject.len)
            break;
    }
    return i;
}

String
string_break(String string, String accept)
{
    s32 idx = string_cspan(string, accept);
    string.data += idx;
    string.len -= idx;
    return string;
}

s32
string_find(String haystack, String needle)
{
    if (needle.len > haystack.len)
        return haystack.len;
    for (s32 i = 0; i <= haystack.len - needle.len; i++) {
        if (memcmp(haystack.data + i, needle.data, needle.len) == 0)
            return i;
    }
    return haystack.len;
}

String
string_tokenize(String string, String delim, s32 * saveptr)
{
    string.data += *saveptr;
    string.len  -= *saveptr;
    s32 start    = string_span(string, delim);
    string.data += start;
    string.len  -= start;
    s32 end      = string_cspan(string, delim);
    string.len   = end;
    *saveptr += start + end;
    return string;
}

void
string_tr(String string, char x, char y)
{
    for (s32 i = 0; i < string.len; i++) {
        if (string.data[i] == x)
            string.data[i] = y;
    }
}

void
string_toupper(String string)
{
    for (s32 i = 0; i < string.len; i++) {
        if (islower(string.data[i]))
            string.data[i] = toupper(string.data[i]);
    }
}

void
string_tolower(String string)
{
    for (s32 i = 0; i < string.len; i++) {
        if (isupper(string.data[i]))
            string.data[i] = tolower(string.data[i]);
    }
}

s32
string_count(String string, char c)
{
    s32 result = 0;
    for (s32 i = 0; i < string.len; i++) {
        if (string.data[i] == c)
            result++;
    }
    return result;
}

int
string_starts_with(String string, String prefix)
{
    return string_equaln(string, prefix, prefix.len);
}

int
string_ends_with(String string, String suffix)
{
    if (string.len < suffix.len)
        return 0;
    string.data += string.len - suffix.len;
    return string_equaln(string, suffix, suffix.len);
}

#define STRTOL_ERRORS \
    X(STRTOL_SUCCESS, ""                                ) \
    X(STRTOL_INVALID, "not a valid number"              ) \
    X(STRTOL_EXTRA  , "extra characters at end of input") \
    X(STRTOL_RANGE  , "out of range of type long"       ) \
    X(STRTOL_MAX    , "greater than INT_MAX"            ) \
    X(STRTOL_MIN    , "less than INT_MIN"               )

#define X(ENUM, STR) ENUM,
enum {
    STRTOL_ERRORS
    STRTOL_NUM
};
#undef X

#define X(ENUM, STR) [ENUM] = STR,
static const char * strtol_errors[] = {
    STRTOL_ERRORS
};
#undef X

const char *
parse_int_strerror(int errnum)
{
    assert(errnum < STRTOL_NUM);
    return strtol_errors[errnum];
}

// see https://wiki.sei.cmu.edu/confluence/display/c/ERR34-C.+Detect+errors+when+converting+a+string+to+a+number
// returns 0 on success, non-zero on failure
int
string_parse_int(String string, int * result) {
    char tmp[32];
    StringBuilder builder = builder_from_fixed(tmp);
    builder_append(&builder, string); // guarantee nul-termination
    char * end;
    errno = 0;
    const long sl = strtol(builder.data, &end, 0);
    if (end == string.data)                return STRTOL_INVALID;
    else if ('\0' != *end)                        return STRTOL_EXTRA;
    else if ((LONG_MIN == sl) && ERANGE == errno) return STRTOL_RANGE;
    else if ((LONG_MAX == sl) && ERANGE == errno) return STRTOL_RANGE;
    else if (sl > INT_MAX)                        return STRTOL_MAX;
    else if (sl < INT_MIN)                        return STRTOL_MIN;
    *result = (int) sl;
    return 0;
}

void
builder_reserve_context(void * context, StringBuilder * builder, s32 cap)
{
    if (builder->cap >= cap)
        return;

    char * orig_data = builder->data;
    s32    orig_cap  = builder->cap;

    // if switching to dynamically-allocated, pass NULL
    // to realloc (to do malloc), then copy the data over
    int switch_to_dyn = IS_FIXED(*builder);
    cap = UP_TO_EVEN(cap);
    char * ptr = switch_to_dyn ? NULL : builder->data;
    char * new_data = PMK_REALLOC(context, ptr, orig_cap, cap);
    if (switch_to_dyn)
        memcpy(new_data, orig_data, orig_cap);

    builder->data = new_data;
    builder->cap = cap;
}

void
builder_destroy_context(void * context, StringBuilder * builder)
{
    if (IS_FIXED(*builder)) {
        builder->len = 0;
        return;
    }
    PMK_FREE(context, builder->data);
    *builder = (StringBuilder) {0};
}

// adds nul terminator
void
builder_append_context(void * context, StringBuilder * builder, String string)
{
    s32 new_len = builder->len + string.len;
    if (builder->cap < new_len + 1) {
        s32 new_cap = MAX(builder->cap * 2, new_len + 1);
        builder_reserve_context(context, builder, new_cap);
    }
    assert(builder->cap >= new_len + 1);
    memcpy(builder->data + builder->len, string.data, string.len);
    builder->len += string.len;
    builder->data[builder->len] = '\0';
}

int
builder_print_context(void * context, StringBuilder * builder, const char * fmt, ...)
{
    va_list ap, aq;
    va_start(ap, fmt);
    va_copy(aq, ap);
    int additional_len = vsnprintf(NULL, 0, fmt, ap);
    if (additional_len < 0)
        return additional_len;
    va_end(ap);

    s32 new_len = builder->len + additional_len;
    if (builder->cap < new_len + 1) {
        s32 new_cap = MAX(builder->cap * 2, new_len + 1);
        builder_reserve_context(context, builder, new_cap);
    }
    assert(builder->cap >= new_len + 1);

    char * dst = builder->data + builder->len;
    s32 rem = builder->cap - builder->len;
    vsnprintf(dst, rem, fmt, aq);
    va_end(aq);
    builder->len = new_len;
    return 0;
}

int
builder_getline_context(void * context, StringBuilder * builder, FILE * fp)
{
    if (builder->cap == 0)
        builder_reserve_context(context, builder, 128);

    while (1) {
        char * dst = builder->data + builder->len;
        s32 avail = builder->cap - builder->len;
        if (fgets(dst, avail, fp) == NULL) {
            if (ferror(fp))
                return -errno;
            // only possibility: EOF with 0 chars read
            builder->len = 0;
            return 0;
        }
        builder->len = strlen(builder->data);
        assert(builder->len > 0);
        if (builder->data[builder->len-1] == '\n') {
            builder->len--;
            return 0;
        }
        if (feof(fp)) // file ended without a newline
            return 0;
        // there's more data but the buffer is full
        builder_reserve_context(context, builder, 128);
    }
}

// terminates on error
int
builder_read_file_context(void * context, StringBuilder * builder, const char * filename)
{
    FILE * fp = fopen(filename, "r");
    struct stat sb;

    if (fp == NULL)
        return -errno;
    if (fstat(fileno(fp), &sb) == -1)
        return -errno;

    builder_reserve_context(context, builder, (s32) (sb.st_size));
    fread(builder->data, 1, builder->cap, fp);

    if (ferror(fp))
        return -errno;
    if (fclose(fp) != 0)
        return -errno;

    builder->len = (s32) sb.st_size;
    return 0;
}

int
builder_replace_context(void * context, StringBuilder * builder, String x, String y)
{
    if (x.len == 0)
        return -1;

    s32 new_len = builder->len - x.len + y.len;
    if (new_len + 1 > builder->cap) {
        s32 new_cap = MAX(builder->cap * 2, new_len + 1);
        builder_reserve_context(context, builder, new_cap);
    }
    assert(builder->cap >= new_len + 1);

    String b_as_s = builder_to_string(*builder);
    s32 x_in_b = string_find(b_as_s, x);
    if (x_in_b == b_as_s.len)
        return -1;

    char * rest_src = b_as_s.data + x_in_b + x.len;
    char * rest_dst = b_as_s.data + x_in_b + y.len;
    s32    rest_len = b_as_s.len  - x_in_b - x.len;

    memmove(rest_dst, rest_src, rest_len);
    memcpy(b_as_s.data + x_in_b, y.data, y.len);
    builder->len = new_len;
    builder->data[new_len] = '\0';
    return 0;
}

// TODO: should this be (start, count) instead?
void
builder_splice_context(void * context, StringBuilder * builder, s32 start, s32 end, String string)
{
    if (start < 0) start += builder->len;
    if (end < 0)   end   += builder->len;
    assert(start >= 0);
    assert(end >= 0);
    assert(start <= end);
    assert(end <= builder->len);
    s32 nremove = end - start;
    s32 new_len = builder->len - nremove + string.len;
    if (new_len + 1 > builder->cap) {
        s32 new_cap = MAX(builder->cap * 2, new_len + 1);
        builder_reserve_context(context, builder, new_cap);
    }
    char * rest_src = builder->data + end;
    char * rest_dst = builder->data + start + string.len;
    s32    rest_len = builder->len - end;
    memmove(rest_dst, rest_src, rest_len);
    memcpy(builder->data + start, string.data, string.len);
    builder->len = new_len;
    builder->data[builder->len] = '\0';
}

#ifdef PMK_STRING_TEST

static char
random_printable()
{
    int r = (rand() % ('~' - ' ' + 3)) + ' ';
    assert(r >= ' ');
    assert(r <= '~' + 2);
    if (r ==  '~' + 1) r = '\n';
    if (r ==  '~' + 2) r = '\t';
    return r;
}

static void
gen_random_cstring(char * s, s32 len)
{
    for (s32 i = 0; i < len; i++) {
        s[i] = random_printable();
    }
    s[len] = '\0';
}

static s32
change_random_char(char * s, s32 len)
{
    s32 index = rand() % len;
    char prev = s[index];
    char r;
    do {
        r = random_printable();
    } while (r == prev);
    s[index] = r;
    return index;
}

static void
pmk_string_test()
{
    // string_equal()
    assert( string_equal(   str_lit("hello"),   str_lit("hello"))       );
    assert( string_equal(   str_lit(""),        str_lit(""))            );
    assert(!string_equal(   str_lit("hello!"),  str_lit("hello?"))      );
    assert(!string_equal(   str_lit(""),        str_lit("hello"))       );
    assert(!string_equal(   str_lit("hello"),   str_lit(""))            );
    assert(!string_equal(   str_lit("hello"),   str_lit("hello there")) );
    assert(!string_equal(   str_lit("hello!"),  str_lit("hello"))       );

    // string_equaln()
    assert( string_equaln(  str_lit("hello!"),  str_lit("hello?"),  5)  );
    assert( string_equaln(  str_lit(""),        str_lit(""),        0)  );
    assert(!string_equaln(  str_lit("hello!"),  str_lit("hello?"),  6)  );
    assert(!string_equaln(  str_lit("hello"),   str_lit("hello"),   6)  );

    // string_compare()
    assert(string_compare(  str_lit("aaa"),     str_lit("bbb")) < 0     );
    assert(string_compare(  str_lit("bbb"),     str_lit("aaa")) > 0     );
    assert(string_compare(  str_lit("aaa"),     str_lit("aaa")) == 0    );
    assert(string_compare(  str_lit("aa"),      str_lit("aaa")) < 0     );
    assert(string_compare(  str_lit("aa"),      str_lit(""))    > 0     );

    // string_compare_qsort()
    assert(string_compare_qsort(    &str_lit("aaa"), &str_lit("bbb")) < 0   );
    assert(string_compare_qsort(    &str_lit("bbb"), &str_lit("aaa")) > 0   );
    assert(string_compare_qsort(    &str_lit("aaa"), &str_lit("aaa")) == 0  );
    assert(string_compare_qsort(    &str_lit("aa"),  &str_lit("aaa")) < 0   );
    assert(string_compare_qsort(    &str_lit("aa"),  &str_lit("")   ) > 0   );

    // string_substr()
    assert(string_equal(string_substr(  str_lit("hello"), 0, 5),    str_lit("hello"))   );
    assert(string_equal(string_substr(  str_lit("hello"), 0, 0),    str_lit(""))        );
    assert(string_equal(string_substr(  str_lit("hello"), -1, 5),   str_lit("o"))       );
    assert(string_equal(string_substr(  str_lit("hello"), -2, -1),  str_lit("l"))       );

    // string_dup()
    String dup_str = string_dup(str_lit("hello"));
    assert(string_equal(dup_str, str_lit("hello")));
    PMK_FREE(NULL, dup_str.data);

    // string_trim()
    assert(string_equal(string_trim(str_lit("  good morning \n \t ")), str_lit("good morning")));
    assert(string_equal(string_trim(str_lit("  ")), str_lit("")));

    // string_char()
    assert(string_char(str_lit("hello"), 'l') == 2);
    assert(string_char(str_lit("hello"), 'x') == 5);
    assert(string_char(str_lit(""), 'x') == 0);

    // string_rchar()
    assert(string_rchar(str_lit("hello"), 'l') == 3);
    assert(string_rchar(str_lit("hello"), 'x') == 5);
    assert(string_rchar(str_lit(""), 'x') == 0);

    // string_span()
    assert(string_span(     str_lit("good morning"),    str_lit("gdX o"))    == 5   );
    assert(string_span(     str_lit("good morning"),    str_lit("gn mrodi")) == 12  );
    assert(string_span(     str_lit("good morning"),    str_lit("XYZ"))      == 0   );
    assert(string_span(     str_lit("good morning"),    str_lit(""))         == 0   );
    assert(string_span(     str_lit(""),                str_lit("abc"))      == 0   );

    // string_cspan()
    assert(string_cspan(    str_lit("good morning"),    str_lit("mr"))      == 5    );
    assert(string_cspan(    str_lit("good morning"),    str_lit("abc"))     == 12   );
    assert(string_cspan(    str_lit("good morning"),    str_lit("Xg"))      == 0    );
    assert(string_cspan(    str_lit("good morning"),    str_lit(""))        == 12   );
    assert(string_cspan(    str_lit(""),                str_lit("abc"))     == 0    );

    // string_find()
    assert(string_find(     str_lit("good morning"),    str_lit("morn"))    == 5    );
    assert(string_find(     str_lit("good morning"),    str_lit("fish"))    == 12   );
    assert(string_find(     str_lit("good morning"),    str_lit(""))        == 0    );
    assert(string_find(     str_lit(""),                str_lit("fish"))    == 0    );
    assert(string_find(     str_lit(""),                str_lit(""))        == 0    );

    // string_break()
    assert(string_equal(string_break(   str_lit("good morning"),    str_lit("mr")) , str_lit("morning")));
    assert(string_equal(string_break(   str_lit("good morning"),    str_lit("abc")), str_lit("")));
    assert(string_equal(string_break(   str_lit("good morning"),    str_lit("Xg")) , str_lit("good morning")));
    assert(string_equal(string_break(   str_lit("good morning"),    str_lit(""))   , str_lit("")));
    assert(string_equal(string_break(   str_lit(""),                str_lit("abc")), str_lit("")));

    // string_tokenize()
    {
        String input = str_lit("  good \t morning \t ");
        String delim = str_lit(" \t");
        s32 save = 0;
        assert(string_equal(string_tokenize(input, delim, &save), str_lit("good")));
        assert(string_equal(string_tokenize(input, delim, &save), str_lit("morning")));
    }

    // string_tr()
    {
        char cstring[] = "feet, seen, ten";
        String string = str_lit(cstring);
        string_tr(string, 'e', 'o');
        assert(string_equal(string, str_lit("foot, soon, ton")));
    }

    // string_toupper(), string_tolower()
    {
        char cstring[] = "Good morning";
        String string = str_lit(cstring);
        string_toupper(string);
        assert(string_equal(string, str_lit("GOOD MORNING")));
        string_tolower(string);
        assert(string_equal(string, str_lit("good morning")));
    }

    // string_count()
    assert(string_count(str_lit("good morning"), 'o') == 3);
    assert(string_count(str_lit(""), 'o') == 0);

    // string_starts_with()
    assert( string_starts_with(str_lit("good morning"), str_lit("good")));
    assert(!string_starts_with(str_lit("good morning"), str_lit("bad")));

    // string_ends_with()
    assert( string_ends_with(str_lit("good morning"), str_lit("morning")));
    assert(!string_ends_with(str_lit("good morning"), str_lit("evening")));

    // string_parse_int()
    {
        int result;
        assert(string_parse_int(str_lit("123"), &result) == 0);
        assert(result == 123);
        assert(string_parse_int(str_lit("-123"), &result) == 0);
        assert(result == -123);
        assert(string_parse_int(str_lit("  2"), &result) == 0);
        assert(result == 2);
        assert(string_parse_int(str_lit(" +2"), &result) == 0);
        assert(result == 2);
        assert(string_parse_int(str_lit("3.2"), &result) != 0);
    }

    // builder_reserve()
    {
        StringBuilder builder = {0};
        builder_reserve(&builder, 512);
        assert(builder.cap >= 512);
        memset(builder.data, '\0', 512);
        builder_destroy(&builder);
    }

    // builder_append(), builder_print(), builder_replace()
    {
        StringBuilder builder = {0};
        builder_append(&builder, str_lit("good "));
        builder_append(&builder, str_lit("morning"));
        assert(string_equal(builder_to_string(builder), str_lit("good morning")));

        builder.len = 0;
        builder_print(&builder, "%d %s", 123, "red balloons");
        assert(string_equal(builder_to_string(builder), str_lit("123 red balloons")));

        builder_replace(&builder, str_lit("red"), str_lit("green"));
        assert(string_equal(builder_to_string(builder), str_lit("123 green balloons")));

        builder_replace(&builder, str_lit("red"), str_lit("yellow"));
        assert(string_equal(builder_to_string(builder), str_lit("123 green balloons")));

        builder_destroy(&builder);
    }

    // builder_replace()
    {
        StringBuilder builder = {0};
        builder_reserve(&builder, 4);
        builder_append(&builder, str_lit("abc"));
        builder_replace(&builder, str_lit("b"), str_lit("def"));
        assert(string_equal(builder_to_string(builder), str_lit("adefc")));
        builder_destroy(&builder);
    }

    // builder_splice()
    {
        StringBuilder builder = {0};
        builder_reserve(&builder, 4);
        builder_append(&builder, str_lit("abc"));

        builder_splice(&builder, 1, 2, str_lit("def"));
        assert(string_equal(builder_to_string(builder), str_lit("adefc")));

        builder_splice(&builder, -4, -1, str_lit("b"));
        assert(string_equal(builder_to_string(builder), str_lit("abc")));

        builder_splice(&builder, 1, 1, str_lit("def"));
        assert(string_equal(builder_to_string(builder), str_lit("adefbc")));

        builder_destroy(&builder);
    }

    // TODO: test builder_getline()
    // TODO: test builder_read_file()

    // builder_append(), builder_print(), builder_replace() with fixed-sized buffers
    {
        char char_buffer[32];
        StringBuilder builder = builder_from_fixed(char_buffer);
        builder_append(&builder, str_lit("good "));
        builder_append(&builder, str_lit("morning"));
        assert(string_equal(builder_to_string(builder), str_lit("good morning")));

        builder.len = 0;
        builder_print(&builder, "%d %s", 123, "red balloons");
        assert(string_equal(builder_to_string(builder), str_lit("123 red balloons")));

        builder_replace(&builder, str_lit("red"), str_lit("green"));
        assert(string_equal(builder_to_string(builder), str_lit("123 green balloons")));

        builder_replace(&builder, str_lit("red"), str_lit("yellow"));
        assert(string_equal(builder_to_string(builder), str_lit("123 green balloons")));

        builder_destroy(&builder);
    }

    // builder_replace() with fixed-sized buffer
    {
        char char_buffer[4];
        StringBuilder builder = builder_from_fixed(char_buffer);
        builder_append(&builder, str_lit("abc"));
        builder_replace(&builder, str_lit("b"), str_lit("def"));
        assert(string_equal(builder_to_string(builder), str_lit("adefc")));
        builder_destroy(&builder);
    }

    // builder_splice() with fixed-sized buffer
    {
        char char_buffer[16];
        StringBuilder builder = builder_from_fixed(char_buffer);
        builder_append(&builder, str_lit("abc"));

        builder_splice(&builder, 1, 2, str_lit("def"));
        assert(string_equal(builder_to_string(builder), str_lit("adefc")));

        builder_splice(&builder, -4, -1, str_lit("b"));
        assert(string_equal(builder_to_string(builder), str_lit("abc")));

        builder_splice(&builder, 1, 1, str_lit("def"));
        assert(string_equal(builder_to_string(builder), str_lit("adefbc")));

        builder_splice(&builder, 1, 1, str_lit("abcdefghijklmnop"));
        assert(string_equal(builder_to_string(builder), str_lit("aabcdefghijklmnopdefbc")));

        builder_destroy(&builder);
    }

    // TODO: do more random testing

    // string_equal(), string_equaln(), string_compare()
    {
#define LEN 100
#define N 10
        char s1[LEN+1], s2[LEN+1];
        for (s32 i = 0; i < N; i++) {

            // create a random string, create a copy with one random character changed
            gen_random_cstring(s1, LEN);
            memcpy(s2, s1, LEN);
            s32 index = change_random_char(s1, LEN);

            String str1 = str_lit(s1);
            String str2 = str_lit(s2);

            assert(string_equal (str1, str2)        ==  (strcmp(s1, s2)         == 0));
            assert(string_equaln(str1, str2, index) ==  (strncmp(s1, s2, index) == 0));

            int compare = string_compare(str1, str2);
            int cmp     = strcmp(s1, s2);

            // constrain results to -1, 0, 1
            compare = (compare < 0) ? -1 : (compare > 0) ? 1 : 0;
            cmp = (compare < 0) ? -1 : (compare > 0) ? 1 : 0;

            assert(compare == cmp);
        }
#undef LEN
#undef N
    }
}

#endif /* PMK_STRING_TEST */

#undef MIN
#undef MAX

#endif /* PMK_STRING_IMPL */

/*
MIT License

Copyright (c) 2023 Paul Kennedy

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/
