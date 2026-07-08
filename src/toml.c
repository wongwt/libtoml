//
// MIT License
//
// Copyright (c) 2026 Wei-Te Wong
//
// Permission is hereby granted, free of charge, to any person obtaining a
// copy of this software and associated documentation files (the
// "Software"), to deal in the Software without restriction, including
// without limitation the rights to use, copy, modify, merge, publish,
// distribute, sublicense, and/or sell copies of the Software, and to
// permit persons to whom the Software is furnished to do so, subject to
// the following conditions:
//
// The above copyright notice and this permission notice shall be included
// in all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
// OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
// MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
// IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY
// CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,
// TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
// SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.

#include "toml.h"

#include <assert.h>
#include <stdlib.h>

#if TOML_HAS_C11
#define ALIGNOF(type) _Alignof(type)
#else
#define ALIGNOF(type) offsetof(struct { char byte; type value; }, value)
#endif

// One chunk for now; the chunked dynamic backend arriving in M7 reuses
// this same size per chunk
static const size_t ARENA_CHUNK_SIZE = 4096;

typedef struct {
    unsigned char *chunk;
    size_t capacity;
    size_t offset;
} toml_arena_s;

static size_t align_to(size_t value, size_t align) {
    assert(align > 0 && (align & (align - 1)) == 0);

    return (value + align - 1) & ~(align - 1);
}

static void *arena_alloc(toml_arena_s *arena, size_t size, size_t align) {
    size_t aligned_offset = align_to(arena->offset, align);

    if (aligned_offset > arena->capacity) {
        return NULL;
    }

    if (size > arena->capacity - aligned_offset) {
        return NULL;
    }

    void *ptr = arena->chunk + aligned_offset;
    arena->offset = aligned_offset + size;

    return ptr;
}

typedef struct {
    toml_errcode_e code;
    toml_span_s primary;
    toml_span_s secondary;
    const char *detail;
} toml_error_s;

struct toml {
    toml_arena_s arena;
    toml_error_s error;
};

toml_t *toml_from_byte(const char *byte, size_t byte_len) {
    (void)byte;
    (void)byte_len;

    unsigned char *chunk = malloc(ARENA_CHUNK_SIZE);
    if (chunk == NULL) {
        return NULL;
    }

    toml_arena_s arena = {
        .chunk = chunk,
        .capacity = ARENA_CHUNK_SIZE,
    };

    toml_t *toml = arena_alloc(&arena, sizeof *toml, ALIGNOF(toml_t));
    if (toml == NULL) {
        free(chunk);
        return NULL;
    }

    toml->arena = arena;
    toml->error = (toml_error_s){ .code = TOML_OK };

    return toml;
}

void toml_free(toml_t *toml) {
    if (toml == NULL) {
        return;
    }

    free(toml->arena.chunk);
}

bool toml_has_error(const toml_t *toml) {
    if (toml == NULL) {
        return true;
    }

    return toml->error.code != TOML_OK;
}
