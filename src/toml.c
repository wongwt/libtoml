//! @copyright  Copyright 2026 Wei-Te Wong. MIT Licensed.
#include "toml.h"

#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#ifndef TOML_MALLOC
#define TOML_MALLOC malloc
#endif
#ifndef TOML_REALLOC
#define TOML_REALLOC realloc
#endif
#ifndef TOML_FREE
#define TOML_FREE free
#endif


// §1  Arena-based Memory Management

#define TOML_MIN(a, b) ((a) < (b) ? (a) : (b))
#define TOML_MAX(a, b) ((a) > (b) ? (a) : (b))

#define ARENA_ALIGN_SIZE 8u
#define ARENA_CHUNK_SIZE 4096u

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
    arena->chunk_size = chunk_size == 0 ? ARENA_CHUNK_SIZE : chunk_size;
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
    chunk_s *chunk = TOML_MALLOC(sizeof(chunk_s) + capacity);
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
        TOML_FREE(chunk);
        chunk = next;
    }
    arena->head = NULL;
}


// §2  TOML Object

#define TOML_READ_CHUNK_SIZE 4096u

struct toml_s {
    arena_s arena;
};

static toml_t *toml_new(const char *src, size_t src_len) {
    toml_t *toml = TOML_MALLOC(sizeof(toml_t));
    if (toml == NULL) {
        return NULL;
    }

    arena_init(&toml->arena, 0);

    (void)src;
    (void)src_len;

    return toml;
}

toml_t *toml_from_str(const char *src) {
    if (src == NULL) {
        return NULL;
    }

    return toml_new(src, strlen(src));
}

toml_t *toml_from_fp(FILE *fp) {
    if (fp == NULL) {
        return NULL;
    }

    size_t cap = TOML_READ_CHUNK_SIZE;
    size_t len = 0;

    char *buf = TOML_MALLOC(cap);
    if (buf == NULL) {
        return NULL;
    }

    size_t n;
    while ((n = fread(buf + len, 1, cap - len, fp)) > 0) {
        len += n;
        if (len == cap) {
            cap *= 2;
            char *new_buf = TOML_REALLOC(buf, cap);
            if (new_buf == NULL) {
                TOML_FREE(buf);
                return NULL;
            }
            buf = new_buf;
        }
    }

    if (ferror(fp)) {
        TOML_FREE(buf);
        return NULL;
    }

    toml_t *toml = toml_new(buf, len);
    TOML_FREE(buf);

    return toml;
}

toml_t *toml_from_file(const char *fmt, ...) {
    if (fmt == NULL) {
        return NULL;
    }

    char path[PATH_MAX] = {0};

    va_list args;
    va_start(args, fmt);
    int path_len = vsnprintf(path, sizeof(path), fmt, args);
    va_end(args);
    if (path_len < 0 || (size_t)path_len >= sizeof(path)) {
        return NULL;
    }

    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        return NULL;
    }

    toml_t *toml = toml_from_fp(fp);
    fclose(fp);

    return toml;
}

void toml_free(toml_t **toml) {
    if (toml == NULL || *toml == NULL) {
        return;
    }

    arena_free(&(*toml)->arena);
    TOML_FREE(*toml);
    *toml = NULL;
}
