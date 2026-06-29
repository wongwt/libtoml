//! @copyright  Copyright 2026 Wei-Te Wong. MIT Licensed.
#include "toml.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>


// §1  Arena-based Memory Management

#define TOML_MIN(a, b) ((a) < (b) ? (a) : (b))
#define TOML_MAX(a, b) ((a) > (b) ? (a) : (b))

#define ARENA_FREE free
#define ARENA_MALLOC malloc

#define ARENA_ALIGN_SIZE 8u

typedef struct chunk_s {
    struct chunk_s *next;
    size_t capacity;
    size_t used;
    char data[];
} chunk_s;

typedef struct arena_s {
    chunk_s *head;
    size_t chunk_size;
} arena_s;

static void arena_init(arena_s *arena, size_t chunk_size) {
    if (arena == NULL) {
        return;
    }

    arena->head = NULL;
    arena->chunk_size = chunk_size;
}

static size_t arena_align_size(size_t size) {
    if (size > SIZE_MAX - (ARENA_ALIGN_SIZE - 1)) {
        return SIZE_MAX;
    }

    return (size + (ARENA_ALIGN_SIZE - 1)) & ~(ARENA_ALIGN_SIZE - 1);
}

static bool arena_has_space(arena_s *arena, size_t aligned_size) {
    if (arena == NULL || arena->head == NULL) {
        return false;
    }

    return arena->head->capacity - arena->head->used >= aligned_size;
}

static chunk_s *arena_new_chunk(size_t capacity) {
    chunk_s *chunk = ARENA_MALLOC(sizeof(chunk_s) + capacity);
    if (chunk == NULL) {
        return NULL;
    }
    chunk->next = NULL;
    chunk->capacity = capacity;
    chunk->used = 0;

    return chunk;
}

static bool arena_expand(arena_s *arena, size_t aligned_size) {
    // Clamp before doubling so aligned_size * 2 can never wrap around
    size_t doubled  = TOML_MIN(aligned_size, SIZE_MAX / 2) * 2;
    size_t capacity = TOML_MAX(doubled, arena->chunk_size);
    if (capacity > SIZE_MAX - sizeof(chunk_s)) {
        return false;
    }

    chunk_s *chunk = arena_new_chunk(capacity);
    if (chunk == NULL) {
        return false;
    }

    chunk->next = arena->head;
    arena->head = chunk;
    arena->chunk_size = TOML_MIN(capacity, SIZE_MAX / 2) * 2;

    return true;
}

static bool arena_ensure_capacity(arena_s *arena, size_t aligned_size) {
    if (arena_has_space(arena, aligned_size)) {
        return true;
    }

    return arena_expand(arena, aligned_size);
}

static void *arena_alloc(arena_s *arena, size_t size) {
    if (arena == NULL) {
        return NULL;
    }

    size_t aligned_size = arena_align_size(size);
    if (aligned_size == SIZE_MAX) {
        return NULL;
    }

    if (!arena_ensure_capacity(arena, aligned_size)) {
        return NULL;
    }

    chunk_s *chunk = arena->head;
    void *memory = chunk->data + chunk->used;
    chunk->used += aligned_size;

    return memory;
}

static char *arena_strdup(arena_s *arena, const char *str, size_t str_len) {
    if (arena == NULL || str == NULL) {
        return NULL;
    }

    if (str_len > SIZE_MAX - 1) {
        return NULL;
    }

    char *dup_str = arena_alloc(arena, str_len + 1);
    if (dup_str == NULL) {
        return NULL;
    }
    memcpy(dup_str, str, str_len);
    dup_str[str_len] = '\0';

    return dup_str;
}

static void arena_free(arena_s *arena) {
    if (arena == NULL) {
        return;
    }

    chunk_s *chunk = arena->head;
    while (chunk != NULL) {
        chunk_s *next = chunk->next;
        ARENA_FREE(chunk);
        chunk = next;
    }
    arena->head = NULL;
}
