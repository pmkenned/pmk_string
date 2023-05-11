#define _POSIX_C_SOURCE 1

#define PMK_ARENA_IMPL
#define DEFAULT_ARENA_CAP 16
#define PMK_DEBUG 0
#include "pmk_arena.h"

#define PMK_STRING_IMPL
#define PMK_REALLOC(c,p,s) arena_realloc_into(c,p,s)
#include "pmk_string.h"

#include <stdio.h>
#include <stdlib.h>

#define NELEM(X) (sizeof(X)/sizeof(X[0]))
#define NELEMS(X) (ptrdiff_t)(sizeof(X)/sizeof(X[0]))

static void
example1()
{
    String name = str_lit("Paul");

    printf("Example1: Hello, %.*s\n", len_data(name));
}

static void
example2()
{
    String name = str_lit("Paul");

    StringBuilder builder = {0};
    builder_append(&builder, str_lit("Hello, "));
    builder_append(&builder, name);

    printf("Example2: %.*s\n", len_data(builder));

    //free(builder.data);
}

static void
example3()
{
    String name = str_lit("Paul");
    int age = 33;
    String favorite_color = str_lit("Orange");

    StringBuilder builder = {0};

    builder_print(&builder, "My name is %.*s.", len_data(name));
    builder_print(&builder, " I am %d years old and my favorite color is %.*s.", age, len_data(favorite_color));

    printf("Example3: %.*s\n", len_data(builder));

    //free(builder.data);
}

static void
example4()
{
    String message = str_lit("   Hello there, \t you  .  ");
    s32 save = 0;
    String token;
    printf("Example4: [%s]\n", message.data);
    while (1) {
        token = string_tokenize(message, str_lit(" \t"), &save);
        if (token.len == 0)
            break;
        printf("  token: [%.*s]\n", len_data(token));
    }
}

static void
example5()
{
    String animals[] = {
        str_lit("dog"),
        str_lit("fish"),
        str_lit("cat"),
        str_lit("monkey"),
        str_lit("horse"),
        str_lit("duck"),
        str_lit("goose"),
        str_lit("cow"),
        str_lit("pig"),
        str_lit("sheep"),
        str_lit("donkey"),
    };

    qsort(animals, NELEM(animals), sizeof(animals[0]), string_compare_qsort);

    StringBuilder builder = {0};
    for (s32 i = 0; i < NELEMS(animals); i++) {
        if (i > 0)
            builder_append(&builder, str_lit(", "));
        builder_append(&builder, animals[i]);
    }
    printf("Example5: %.*s\n", len_data(builder));
}

static void
example6()
{
    Arena arena = {0};
    StringBuilder builder = {0};
    String name = str_lit("Paul");
    builder_print_context(&arena, &builder, "Hello, %.*s. This is in an arena.", len_data(name));
    printf("Example6: %.*s\n", len_data(builder));
    arena_destroy(&arena);
}

static void
example7()
{
    Arena arena1 = arena_create(1<<5);
    Arena arena2 = arena_create(1<<4);
    Arena arena3 = {0};

    StringBuilder builder = {0};
    String name = str_lit("Paul");

    builder_print_context(&arena1, &builder, "Hello, %.*s.", len_data(name));
    builder_print_context(&arena2, &builder, " Nice to meet you, %.*s.", len_data(name));

    builder.data = arena_realloc_into(&arena3, builder.data, builder.cap);

    arena_destroy(&arena1);
    arena_destroy(&arena2);

    printf("Example7: %.*s\n", len_data(builder));
    arena_destroy(&arena3);
}

static void
example8()
{
    StringBuilder builder = {0};
    builder_append(&builder, str_lit("Hello, good morning, how are you?"));
    builder_replace(&builder, str_lit("good"), str_lit("what a lovely"));
    printf("Example8: %.*s\n", len_data(builder));

    builder_splice(&builder, 21, 28, str_lit("evening"));
    printf("Example8: %.*s\n", len_data(builder));
}

static void
example9()
{
    StringBuilder builder = {0};
    printf("What is your name? ");
    builder_getline(&builder, stdin);
    String name = builder_to_string(builder);
    name = string_trim(name);
    printf("Example9: Hello, %.*s!\n", len_data(name));
}

static void
example10()
{
    StringBuilder builder = {0};
    printf("How old are you? ");
    builder_getline(&builder, stdin);
    String age = builder_to_string(builder);
    age = string_trim(age);
    int age_int;
    if (string_parse_int(age, &age_int) != 0 || age_int <= 0) {
        printf("That's not a valid age.\n");
        return;
    }
    printf("Example10: Next year, you will be %d years old!\n", age_int+1);
}

static void
example11()
{
    const char * filename = "README.md";
    StringBuilder builder = {0};
    builder_read_file(&builder, filename);
    s32 line_count = string_count(builder_to_string(builder), '\n');
    printf("Example11: there are %d bytes and %d lines in %s\n", builder.len, line_count, filename);
}

static void
example12()
{
    const char * filename = "README.md";
    char buffer[1 << 13];
    StringBuilder builder = builder_from_fixed(buffer);
    if (builder_read_file_fixed(&builder, filename) < 0) {
        perror("example12()");
        exit(EXIT_FAILURE);
    }
    s32 line_count = string_count(builder_to_string(builder), '\n');
    printf("Example12: there are %d bytes and %d lines in %s\n", builder.len, line_count, filename);
}

int main()
{
#if PMK_STRING_TEST
    pmk_string_test();
    fprintf(stderr, "All tests passed.\n");
    return 0;
#endif
    example1();
    example2();
    example3();
    example4();
    example5();
    example6();
    example7();
    example8();
    //example9();
    //example10();
    example11();
    example12();

    arena_destroy(&default_arena);

    return 0;
}
