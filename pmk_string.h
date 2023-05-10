#ifndef PMK_STRING_H
#define PMK_STRING_H

/*
 * TODO:
 *
 *  - Document the API more
 *
 *  - Consider adding PMK_NO_SHORT_NAMES macro to help avoid name collisions.
 *
 *  - Consider using _Generic to allow for either String or char * for
 *    functions such as builder_append()
 *
 *  - Something similar to scscatrepr
 *
 *  - Certain functions such as string_substr() currently take start and end as
 *    parameters. Perhaps this should be start and length? Also, they allow
 *    start and end to be negative in which case they count from the end of the
 *    string, but this probably isn't as useful as it is in Python, say, where
 *    you can omit the end parameter to indicate that you want to include from
 *    start to the end of the string, e.g. s[-2:]. In our case, the best we could
 *    do would be s[-2:-1] which would leave off the last character.
 *
 *  - string_find() returns the index of the substring being searched for, but
 *    it might be preferable to return a pointer?
 *
 *  - Decide best way to handle and/or indicate errors. However it is done, it
 *    should be made consistent across all the functions. Perhaps the best way is
 *    to return -errno. That way, you can always just do if (f() < 0) to check
 *    for errors and you can just use perror() or strerror() to print a helpful
 *    error message.
 *
 *  - Decide if a more versatile way to control allocations is needed. Right
 *    now, there are only two ways to control it: defining PMK_REALLOC and
 *    passing a context pointer. That may or may not be sufficient.
 *
 *  - Consider adding flag to StringBuilder saying if buffer is dynamically
 *    allocated or not. Could maybe eliminate the _fixed functions.
 */

#include <stddef.h>
#include <stdio.h>

typedef struct {
    char * data;
    size_t len;
} String;

#define str_lit_const(S)                 { .data = (S), .len = sizeof(S)-1 }
#define str_lit(S)              (String) { .data = (S), .len = sizeof(S)-1 }
#define str_cstr(S)             (String) { .data = (S), .len = strlen(S)   }
#define len_data_lit(S)         (int) sizeof(S)-1, (S)
#define len_data(S)             (int) (S).len, (S).data
#define str_left_adjust(S,N)    do { (S).data += N; (S).len -= N; } while (0)

int     string_equal        (String s1, String s2);
int     string_equaln       (String s1, String s2, size_t n);
int     string_compare      (String s1, String s2);
int     string_compare_qsort(const void * a, const void * b);
String  string_substr       (String string, int start, int end);
String  string_dup          (String string);
String  string_trim         (String string);
size_t  string_char         (String string, char c);
size_t  string_rchar        (String string, char c);
size_t  string_span         (String string, String accept);
size_t  string_cspan        (String string, String reject);
size_t  string_find         (String haystack, String needle);
String  string_break        (String string, String accept);
String  string_tokenize     (String string, String delim, size_t * saveptr);
void    string_tr           (String string, char x, char y);
void    string_toupper      (String string);
void    string_tolower      (String string);
size_t  string_count        (String string, char c);
int     string_starts_with  (String string, String prefix);
int     string_ends_with    (String string, String suffix);
int     string_parse_int    (String string, int * result);

typedef struct {
    char * data;
    size_t len;
    size_t cap;
} StringBuilder;

#define builder_to_string(B)    (String) { .data = (B).data, .len = (B).len }
#define builder_from_fixed(F)   (StringBuilder) { .data = (F), .cap = sizeof(F) }

#define builder_reserve(B,CAP)      builder_reserve_context     (NULL, B, CAP)
#define builder_append(B,STR)       builder_append_context      (NULL, B, STR)
#define builder_print(B,FMT,...)    builder_print_context       (NULL, B, FMT, __VA_ARGS__)
#define builder_replace(B,X,Y)      builder_replace_context     (NULL, B, X, Y)
#define builder_splice(B,S,E,STR)   builder_splice_context      (NULL, B, S, E, STR)
#define builder_getline(B,F)        builder_getline_context     (NULL, B, F)
#define builder_read_file(B,F)      builder_read_file_context   (NULL, B, F)

void    builder_reserve_context     (void * context, StringBuilder * builder, size_t cap);
void    builder_append_context      (void * context, StringBuilder * builder, String string);
void    builder_print_context       (void * context, StringBuilder * builder, const char * fmt, ...);
void    builder_replace_context     (void * context, StringBuilder * builder, String x, String y);
void    builder_splice_context      (void * context, StringBuilder * builder, int start, int end, String string);
void    builder_getline_context     (void * context, StringBuilder * builder, FILE * fp);
void    builder_read_file_context   (void * context, StringBuilder * builder, const char * filename);

int     builder_append_fixed        (StringBuilder * builder, String string);
int     builder_print_fixed         (StringBuilder * builder, const char * fmt, ...);
int     builder_replace_fixed       (StringBuilder * builder, String x, String y);
int     builder_splice_fixed        (StringBuilder * builder, int start, int end, String string);
int     builder_getline_fixed       (StringBuilder * builder, FILE * fp);
int     builder_read_file_fixed     (StringBuilder * builder, const char * filename);

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
#define PMK_REALLOC(c,p,s) realloc(p,s)
#endif
#define PMK_FREE(c,p) p = PMK_REALLOC(c,p,0)

#define XSTR(X)  #X
#define STR(X) XSTR(X)

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

int
string_equal(String s1, String s2)
{
    if (s1.len != s2.len)
        return 0;
    return memcmp(s1.data, s2.data, s1.len) == 0;
}

// NOTE: if either string is shorter than n, returns false
int
string_equaln(String s1, String s2, size_t n)
{
    if (s1.len < n || s2.len < n)
        return 0;
    return memcmp(s1.data, s2.data, n) == 0;
}

int
string_compare(String s1, String s2)
{
    size_t min_len = s1.len < s2.len ? s1.len : s2.len;
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
string_substr(String string, int start, int end)
{
    if (start < 0) start += string.len;
    if (end < 0)   end   += string.len;
    assert(start >= 0);
    assert(end >= 0);
    assert(start <= end);
    assert(end <= (int) string.len);
    return (String) {
        .data = string.data + start,
        .len = end - start
    };
}

String
string_dup(String string)
{
    String result;
    result.data = PMK_REALLOC(NULL, NULL, string.len);
    result.len = string.len;
    memcpy(result.data, string.data, string.len);
    return result;
}

String
string_trim(String string)
{
    size_t i;
    for (i = 0; i < string.len; i++) {
        if (!isspace(string.data[i])) {
            string.data += i;
            string.len -= i;
            break;
        }
    }
    size_t j = string.len-1;
    for (i = 0; i < string.len; i++, j--) {
        if (!isspace(string.data[j]))
            break;
    }
    string.len -= i;
    return string;
}

size_t
string_char(String string, char c)
{
    for (size_t i = 0; i < string.len; i++) {
        if (string.data[i] == c)
            return i;
    }
    return string.len;
}

size_t
string_rchar(String string, char c)
{
    for (size_t i = 0, j = string.len-1; i < string.len; i++, j--) {
        if (string.data[j] == c)
            return j;
    }
    return string.len;
}

size_t
string_span(String string, String accept)
{
    size_t i, j;
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


size_t
string_cspan(String string, String reject)
{
    size_t i, j;
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
    size_t idx = string_cspan(string, accept);
    string.data += idx;
    string.len -= idx;
    return string;
}

size_t
string_find(String haystack, String needle)
{
    if (needle.len > haystack.len)
        return haystack.len;
    for (size_t i = 0; i <= haystack.len - needle.len; i++) {
        if (memcmp(haystack.data + i, needle.data, needle.len) == 0)
            return i;
    }
    return haystack.len;
}

String
string_tokenize(String string, String delim, size_t * saveptr)
{
    string.data += *saveptr;
    string.len  -= *saveptr;
    size_t start = string_span(string, delim);
    string.data += start;
    string.len  -= start;
    size_t end   = string_cspan(string, delim);
    string.len   = end;
    *saveptr += start + end;
    return string;
}

void
string_tr(String string, char x, char y)
{
    for (size_t i = 0; i < string.len; i++) {
        if (string.data[i] == x)
            string.data[i] = y;
    }
}

void
string_toupper(String string)
{
    for (size_t i = 0; i < string.len; i++) {
        if (islower(string.data[i]))
            string.data[i] = toupper(string.data[i]);
    }
}

void
string_tolower(String string)
{
    for (size_t i = 0; i < string.len; i++) {
        if (isupper(string.data[i]))
            string.data[i] = tolower(string.data[i]);
    }
}

size_t
string_count(String string, char c)
{
    size_t result = 0;
    for (size_t i = 0; i < string.len; i++) {
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
    if (builder_append_fixed(&builder, string) < 0)
        return STRTOL_INVALID;
    char * end;
    errno = 0;
    const long sl = strtol(builder.data, &end, 0);
    if (end == string.data)                       return STRTOL_INVALID;
    else if ('\0' != *end)                        return STRTOL_EXTRA;
    else if ((LONG_MIN == sl) && ERANGE == errno) return STRTOL_RANGE;
    else if ((LONG_MAX == sl) && ERANGE == errno) return STRTOL_RANGE;
    else if (sl > INT_MAX)                        return STRTOL_MAX;
    else if (sl < INT_MIN)                        return STRTOL_MIN;
    *result = (int) sl;
    return 0;
}

void
builder_reserve_context(void * context, StringBuilder * builder, size_t cap)
{
    if (builder->cap >= cap)
        return;
    builder->cap = cap;
    builder->data = PMK_REALLOC(context, builder->data, cap);
}

// adds nul terminator
void
builder_append_context(void * context, StringBuilder * builder, String string)
{
    size_t new_len = builder->len + string.len;
    if (builder->cap < new_len + 1) {
        builder->cap = MAX(builder->cap * 2, new_len + 1);
        builder->data = PMK_REALLOC(context, builder->data, builder->cap);
    }
    assert(builder->cap >= new_len + 1);
    memcpy(builder->data + builder->len, string.data, string.len);
    builder->len += string.len;
    builder->data[builder->len] = '\0';
}

void
builder_print_context(void * context, StringBuilder * builder, const char * fmt, ...)
{
    va_list ap, aq;
    va_start(ap, fmt);
    va_copy(aq, ap);
    int additional_len = vsnprintf(NULL, 0, fmt, ap);
    assert(additional_len >= 0); // TODO: handle -1 on error
    va_end(ap);

    size_t new_len = builder->len + additional_len;
    if (builder->cap < new_len + 1) {
        builder->cap = MAX(builder->cap * 2, new_len + 1);
        builder->data = PMK_REALLOC(context, builder->data, builder->cap);
    }
    assert(builder->cap >= new_len + 1);

    char * dst = builder->data + builder->len;
    size_t rem = builder->cap - builder->len;
    vsnprintf(dst, rem, fmt, aq);
    va_end(aq);
    builder->len = new_len;
}

// returns 0 on success, -errno if insufficient space
int
builder_getline_fixed(StringBuilder * builder, FILE * fp)
{
    if (builder->cap == 0)
        return -(errno = ENOBUFS);
    char * dst = builder->data + builder->len;
    size_t avail = builder->cap - builder->len;
    if (fgets(dst, avail, fp) == NULL) {
        if (ferror(fp)) {
            perror(__FILE__ ":" STR(__LINE__));
            exit(EXIT_FAILURE);
        }
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
    return -(errno = ENOBUFS);
}

void
builder_getline_context(void * context, StringBuilder * builder, FILE * fp)
{
    if (builder->cap == 0)
        builder_reserve_context(context, builder, 128);
    while (builder_getline_fixed(builder, fp) < 0) {
        builder_reserve_context(context, builder, builder->cap * 2);
    }
}

// returns 0 on success, -errno on error
// TODO: decide best way to handle errors; should probably be consistent with builder_read_file_context
int
builder_read_file_fixed(StringBuilder * builder, const char * filename)
{
    errno = 0;
    FILE * fp = fopen(filename, "r");
    struct stat sb;

    if (fp == NULL)
        return -errno;
    if (fstat(fileno(fp), &sb) == -1)
        return -errno;
    if ((size_t) (sb.st_size) > builder->cap)
        return -(errno = ENOBUFS);

    if (fread(builder->data, 1, builder->cap, fp) != builder->cap)
        return -ENODATA;

    if (ferror(fp))
        return -errno;
    if (fclose(fp) != 0)
        return -errno;

    builder->len = sb.st_size;
    return 0;
}

// terminates on error
void
builder_read_file_context(void * context, StringBuilder * builder, const char * filename)
{
    FILE * fp = fopen(filename, "r");
    struct stat sb;

    if (fp == NULL) {
        perror(filename);
        exit(EXIT_FAILURE);
    }
    if (fstat(fileno(fp), &sb) == -1) {
        perror(filename);
        exit(EXIT_FAILURE);
    }

    builder_reserve_context(context, builder, (size_t) (sb.st_size));
    if (fread(builder->data, 1, builder->cap, fp) != builder->cap) {
        perror(filename);
        exit(EXIT_FAILURE);
    }
    if (ferror(fp)) {
        perror(filename);
        exit(EXIT_FAILURE);
    }
    if (fclose(fp) != 0) {
        perror(filename);
        exit(EXIT_FAILURE);
    }

    builder->len = (size_t) sb.st_size;
}

// adds nul terminator
int
builder_append_fixed(StringBuilder * builder, String string)
{
    if (builder->cap < builder->len + string.len + 1)
        return -(errno = ENOBUFS);
    memcpy(builder->data + builder->len, string.data, string.len);
    builder->len += string.len;
    builder->data[builder->len] = '\0';
    return 0;
}

// returns 0 on success, or number of bytes truncated
int
builder_print_fixed(StringBuilder * builder, const char * fmt, ...)
{
    va_list ap, aq;
    va_start(ap, fmt);
    va_copy(aq, ap);

    int required = vsnprintf(NULL, 0, fmt, ap) + 1;
    assert(required > 0); // TODO: handle -1 on error
    va_end(ap);

    char * dst = builder->data + builder->len;
    size_t rem = builder->cap - builder->len;
    vsnprintf(dst, rem, fmt, aq);
    va_end(aq);

    int num_trunc = required - rem;
    if (num_trunc > 0) {
        builder->len = builder->cap-1;
        return num_trunc;
    }
    builder->len += required-1;
    return 0;
}

void
builder_replace_context(void * context, StringBuilder * builder, String x, String y)
{
    if (x.len == 0)
        return;

    size_t new_len = builder->len - x.len + y.len;
    if (new_len + 1 > builder->cap) {
        size_t new_cap = MAX(builder->cap * 2, new_len + 1);
        builder_reserve_context(context, builder, new_cap);
    }
    assert(builder->cap >= new_len + 1);

    String b_as_s = builder_to_string(*builder);
    size_t x_in_b = string_find(b_as_s, x);
    if (x_in_b == b_as_s.len)
        return;

    char * rest_src = b_as_s.data + x_in_b + x.len;
    char * rest_dst = b_as_s.data + x_in_b + y.len;
    size_t rest_len = b_as_s.len  - x_in_b - x.len;

    memmove(rest_dst, rest_src, rest_len);
    memcpy(b_as_s.data + x_in_b, y.data, y.len);
    builder->len = new_len;
    builder->data[new_len] = '\0';
}

// TODO: should this be (start, count) instead?
void
builder_splice_context(void * context, StringBuilder * builder, int start, int end, String string)
{
    if (start < 0) start += builder->len;
    if (end < 0)   end   += builder->len;
    assert(start >= 0);
    assert(end >= 0);
    assert(start <= end);
    assert(end <= (int) builder->len);
    size_t nremove = end - start;
    size_t new_len = builder->len - nremove + string.len;
    if (new_len + 1 > builder->cap) {
        size_t new_cap = MAX(builder->cap * 2, new_len + 1);
        builder_reserve_context(context, builder, new_cap);
    }
    char * rest_src = builder->data + end;
    char * rest_dst = builder->data + start + string.len;
    size_t rest_len = builder->len - end;
    memmove(rest_dst, rest_src, rest_len);
    memcpy(builder->data + start, string.data, string.len);
    builder->len = new_len;
    builder->data[builder->len] = '\0';
}

// return 0 on success, return -1 if insufficient space or if x not found
int
builder_replace_fixed(StringBuilder * builder, String x, String y)
{
    if (x.len == 0)
        return -1;

    size_t new_len = builder->len - x.len + y.len;
    if (new_len + 1 > builder->cap)
        return -1;
    assert(builder->cap >= new_len + 1);

    String b_as_s = builder_to_string(*builder);
    size_t x_in_b = string_find(b_as_s, x);
    if (x_in_b == b_as_s.len)
        return -1;

    char * rest_src = b_as_s.data + x_in_b + x.len;
    char * rest_dst = b_as_s.data + x_in_b + y.len;
    size_t rest_len = b_as_s.len  - x_in_b - x.len;

    memmove(rest_dst, rest_src, rest_len);
    memcpy(b_as_s.data + x_in_b, y.data, y.len);
    builder->len = new_len;
    builder->data[new_len] = '\0';
    return 0;
}

// TODO: should this be (start, count) instead?
int
builder_splice_fixed(StringBuilder * builder, int start, int end, String string)
{
    if (start < 0) start += builder->len;
    if (end < 0)   end   += builder->len;
    assert(start >= 0);
    assert(end >= 0);
    assert(start <= end);
    assert(end <= (int) builder->len);
    size_t nremove = end - start;
    size_t new_len = builder->len - nremove + string.len;
    if (new_len + 1 > builder->cap)
        return -1;
    char * rest_src = builder->data + end;
    char * rest_dst = builder->data + start + string.len;
    size_t rest_len = builder->len - end;
    memmove(rest_dst, rest_src, rest_len);
    memcpy(builder->data + start, string.data, string.len);
    builder->len = new_len;
    builder->data[builder->len] = '\0';
    return 0;
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
gen_random_string(char * s, size_t len)
{
    for (size_t i = 0; i < len; i++) {
        s[i] = random_printable();
    }
    s[len] = '\0';
}

static size_t
change_random_char(char * s, size_t len)
{
    size_t index = rand() % len;
    char prev = s[index];
    int r;
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
        size_t save = 0;
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
        assert(builder.cap == 512);
        memset(builder.data, '\0', 512);
        PMK_FREE(NULL, builder.data);
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

        PMK_FREE(NULL, builder.data);
    }

    // builder_replace()
    {
        StringBuilder builder = {0};
        builder_reserve(&builder, 4);
        builder_append(&builder, str_lit("abc"));
        builder_replace(&builder, str_lit("b"), str_lit("def"));
        assert(string_equal(builder_to_string(builder), str_lit("adefc")));
        PMK_FREE(NULL, builder.data);
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

        PMK_FREE(NULL, builder.data);
    }

    // TODO: test builder_getline()
    // TODO: test builder_read_file()

    // builder_append_fixed(), builder_print_fixed(), builder_replace_fixed()
    {
        char char_buffer[32];
        StringBuilder builder = builder_from_fixed(char_buffer);
        builder_append_fixed(&builder, str_lit("good "));
        builder_append_fixed(&builder, str_lit("morning"));
        assert(string_equal(builder_to_string(builder), str_lit("good morning")));

        builder.len = 0;
        builder_print_fixed(&builder, "%d %s", 123, "red balloons");
        assert(string_equal(builder_to_string(builder), str_lit("123 red balloons")));

        builder_replace_fixed(&builder, str_lit("red"), str_lit("green"));
        assert(string_equal(builder_to_string(builder), str_lit("123 green balloons")));

        builder_replace_fixed(&builder, str_lit("red"), str_lit("yellow"));
        assert(string_equal(builder_to_string(builder), str_lit("123 green balloons")));
    }

    // builder_replace_fixed()
    {
        char char_buffer[4];
        StringBuilder builder = builder_from_fixed(char_buffer);
        builder_append_fixed(&builder, str_lit("abc"));
        assert(builder_replace_fixed(&builder, str_lit("b"), str_lit("def")) < 0);
    }

    // builder_splice_fixed()
    {
        char char_buffer[16];
        StringBuilder builder = builder_from_fixed(char_buffer);
        builder_append_fixed(&builder, str_lit("abc"));

        builder_splice_fixed(&builder, 1, 2, str_lit("def"));
        assert(string_equal(builder_to_string(builder), str_lit("adefc")));

        builder_splice_fixed(&builder, -4, -1, str_lit("b"));
        assert(string_equal(builder_to_string(builder), str_lit("abc")));

        builder_splice_fixed(&builder, 1, 1, str_lit("def"));
        assert(string_equal(builder_to_string(builder), str_lit("adefbc")));

        assert(builder_splice_fixed(&builder, 1, 1, str_lit("abcdefghijklmnop")) < 0);
    }

    // TODO: test builder_getline_fixed()
    // TODO: test builder_read_file_fixed()

    // TODO: do more random testing
    {
#define LEN 100
#define N 100
        char s1[LEN+1], s2[LEN+1];
        for (size_t i = 0; i < N; i++) {
            gen_random_string(s1, LEN);
            strncpy(s2, s1, LEN);
            size_t index = change_random_char(s1, LEN);
            String str1 = str_lit(s1);
            String str2 = str_lit(s2);
            assert(string_equal(str1, str2) == (strcmp(s1, s2) == 0));
            assert(string_equaln(str1, str2, index) == (strncmp(s1, s2, index) == 0));
            assert(string_compare(str1, str2) == strcmp(s1, s2));
        }
#undef LEN
#undef N
    }
}

#endif /* PMK_STRING_TEST */

#undef MIN
#undef MAX

#endif /* PMK_STRING_IMPL */

#endif /* PMK_STRING_H */
