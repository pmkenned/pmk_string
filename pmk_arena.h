#ifndef PMK_ARENA_H
#define PMK_ARENA_H

#include <stddef.h>

#ifndef DEFAULT_ARENA_CAP
#define DEFAULT_ARENA_CAP (1<<20)
#endif

typedef struct Arena Arena;

struct Arena {
    char * data;
    size_t len;
    size_t cap;
    Arena * next_arena;
};

extern Arena default_arena;

Arena   arena_create        (size_t cap);
void *  arena_realloc_into  (Arena * arena, void * ptr, size_t size);
void *  arena_realloc       (void * ptr, size_t size);
void    arena_destroy       (Arena * arena);
//void    arena_free_after    (void * ptr); // TODO

#endif /* PMK_ARENA_H */

#ifdef PMK_ARENA_IMPL

#include <stdlib.h>
#include <string.h>
#include <assert.h>
#include <stdio.h>

#define MIN(X, Y) (((X) < (Y)) ? (X) : (Y))
#define MAX(X, Y) (((X) > (Y)) ? (X) : (Y))

#ifndef ALIGNMENT
#define ALIGNMENT 8
#endif

#define ALIGN_UP(X,Y) (((X) + ((Y)-1)) & ~((Y)-1))

#ifndef PMK_DEBUG
#define PMK_DEBUG 0
#endif

#if PMK_DEBUG
#define DEBUG_MSG(FMT, ...) fprintf(stderr, FMT, __VA_ARGS__)
#else
#define DEBUG_MSG(FMT, ...)
#endif

Arena default_arena;

Arena
arena_create(size_t cap)
{
    Arena arena = {
        .data = malloc(cap),
        .len = 0,
        .cap = cap,
        .next_arena = NULL
    };
    //DEBUG_MSG("arena_create(%zu), buffer at %p\n", cap, arena.data);
    return arena;
}

// TODO: bump downwards
// TODO: use arena stack?
void *
arena_realloc_into(Arena * arena, void * ptr, size_t size)
{
    DEBUG_MSG("arena_realloc_into(%p, %p, %zu)\n", arena, ptr, size);
    if (ptr == NULL && size == 0)
        return NULL;

    if (arena == NULL)
        arena = &default_arena;

    size_t required = ALIGN_UP(size + sizeof(size_t), ALIGNMENT);
    size_t avail = arena->cap - arena->len;
    if (arena->cap == 0) {
        size_t new_arena_cap = MAX(required, DEFAULT_ARENA_CAP);
        //DEBUG_MSG("Arena has no buffer. Allocating new arena buffer with cap=%zu...\n", new_arena_cap);
        *arena = arena_create(new_arena_cap);
    } else if (required > avail) {
        Arena * old_arena = malloc(sizeof(*old_arena));
        *old_arena = *arena;
        size_t new_arena_cap = MAX(required, DEFAULT_ARENA_CAP);
        //DEBUG_MSG("Insufficient space: %zu > %zu. Allocating new arena buffer with cap=%zu...\n", required, avail, new_arena_cap);
        *arena = arena_create(new_arena_cap);
        arena->next_arena = old_arena;
    }

    *((size_t *) (arena->data + arena->len)) = size;
    arena->len += sizeof(size_t);
    void * result = arena->data + arena->len;
    arena->len = ALIGN_UP(arena->len + size, ALIGNMENT);

    if (ptr && size > 0) {
        // TODO: if size < old_size, maybe just leave in place?
        size_t old_size = *((size_t *) ptr - 1);
        size_t min_size = MIN(old_size, size);
        memcpy(result, ptr, min_size);
    } else if (ptr && size == 0) {
        // cannot free individual allocations from an arena
#if PMK_DEBUG
        size_t old_size = *((size_t *) ptr - 1);
        memset(ptr, 0xcd, old_size);
#endif
    }

    //DEBUG_MSG("Allocated %zu bytes from arena at %p (buffer at %p) at %p (from %p)\n", size, arena, arena->data, result, ptr);
    return result;
}

void *
arena_realloc(void * ptr, size_t size) {
    return arena_realloc_into(&default_arena, ptr, size);
}

static void
_arena_destroy(Arena * arena)
{
    //DEBUG_MSG("Destroy arena (%zu)\n", arena->cap);
    if (arena->next_arena)
        _arena_destroy(arena->next_arena);
    free(arena->data);
    free(arena); // assumes arena is dynamically allocated
}

void
arena_destroy(Arena * arena)
{
    //DEBUG_MSG("Reset arena %p with cap %zu, buffer at %p\n", arena, arena->cap, arena->data);
    if (arena->next_arena)
        _arena_destroy(arena->next_arena);
    free(arena->data);
    *arena = (Arena) {0}; // resets the arena
}

#undef MIN
#undef MAX

#endif /* PMK_ARENA_IMPL */
