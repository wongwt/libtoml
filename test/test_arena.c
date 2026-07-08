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

#include "../src/toml.c"

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

static int pass_count = 0;
static int fail_count = 0;

#define EXPECT(cond) \
    do { \
        if (cond) { \
            pass_count++; \
        } else { \
            fail_count++; \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
        } \
    } while (0)

static void test_alloc_returns_null_on_oom_with_16_byte_buffer(void) {
    unsigned char storage[16];
    toml_arena_s arena = {
        .chunk = storage,
        .capacity = sizeof storage,
    };

    void *first = arena_alloc(&arena, 12, 1);
    EXPECT(first != NULL);

    void *second = arena_alloc(&arena, 5, 1);
    EXPECT(second == NULL);

    void *third = arena_alloc(&arena, 4, 1);
    EXPECT(third != NULL);
}

static void test_alloc_respects_alignment(void) {
    unsigned char storage[64];
    toml_arena_s arena = {
        .chunk = storage,
        .capacity = sizeof storage,
    };

    // Misalign the bump pointer by one byte before requesting 8-byte
    // alignment, so the padding logic actually gets exercised.
    void *filler = arena_alloc(&arena, 1, 1);
    EXPECT(filler != NULL);

    void *aligned = arena_alloc(&arena, 8, 8);
    EXPECT(aligned != NULL);
    EXPECT((uintptr_t)aligned % 8 == 0);
}

static void test_alloc_does_not_overlap_sequential_allocations(void) {
    unsigned char storage[64];
    toml_arena_s arena = {
        .chunk = storage,
        .capacity = sizeof storage,
    };

    unsigned char *first = arena_alloc(&arena, 8, 1);
    unsigned char *second = arena_alloc(&arena, 8, 1);

    EXPECT(first != NULL);
    EXPECT(second != NULL);
    EXPECT(second >= first + 8);
}

int main(void) {
    test_alloc_returns_null_on_oom_with_16_byte_buffer();
    test_alloc_respects_alignment();
    test_alloc_does_not_overlap_sequential_allocations();

    printf("%d passed, %d failed\n", pass_count, fail_count);

    return (fail_count == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
