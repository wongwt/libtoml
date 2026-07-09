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

static toml_arena_s make_arena(unsigned char *storage, size_t capacity) {
    return (toml_arena_s){ .chunk = storage, .capacity = capacity };
}

static void test_create_index_backfills_contiguous_array(void) {
    unsigned char storage[512];
    toml_arena_s arena = make_arena(storage, sizeof storage);

    toml_node_s a = { .type = TOML_S64, .val.s64 = 1 };
    toml_node_s b = { .type = TOML_S64, .val.s64 = 2 };
    toml_node_s c = { .type = TOML_S64, .val.s64 = 3 };
    a.next = &b;
    b.next = &c;
    c.next = NULL;

    toml_node_s **children = create_index(&arena, &a, 3);

    EXPECT(children != NULL);
    EXPECT(children[0] == &a);
    EXPECT(children[1] == &b);
    EXPECT(children[2] == &c);
}

static void test_create_index_handles_empty_list(void) {
    unsigned char storage[512];
    toml_arena_s arena = make_arena(storage, sizeof storage);

    toml_node_s **children = create_index(&arena, NULL, 0);

    EXPECT(children != NULL);
}

static void test_create_index_returns_null_on_oom(void) {
    unsigned char storage[4];
    toml_arena_s arena = make_arena(storage, sizeof storage);

    toml_node_s a = { .type = TOML_S64, .val.s64 = 1 };
    toml_node_s b = { .type = TOML_S64, .val.s64 = 2 };
    toml_node_s c = { .type = TOML_S64, .val.s64 = 3 };
    a.next = &b;
    b.next = &c;
    c.next = NULL;

    toml_node_s **children = create_index(&arena, &a, 3);

    EXPECT(children == NULL);
}

int main(void) {
    test_create_index_backfills_contiguous_array();
    test_create_index_handles_empty_list();
    test_create_index_returns_null_on_oom();

    printf("%d passed, %d failed\n", pass_count, fail_count);

    return (fail_count == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
