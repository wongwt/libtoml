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
#include <string.h>

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

static void test_has_error_null_is_safe_and_true(void) {
    EXPECT(toml_has_error(NULL) == true);
}

static void test_from_str_returns_usable_handle(void) {
    const char *source = "answer = 42\n";
    toml_t *toml = toml_from_byte(source, strlen(source));

    EXPECT(toml != NULL);
    EXPECT(toml_has_error(toml) == false);

    toml_free(toml);
}

static void test_source_is_copied_not_aliased(void) {
    char source[] = "answer = 42\n";
    size_t len = strlen(source);
    toml_t *toml = toml_from_byte(source, len);

    EXPECT(toml->source.ptr != source);
    EXPECT(toml->source.len == len);
    EXPECT(memcmp(toml->source.ptr, source, len) == 0);

    // Mutating the caller's buffer must not affect the handle's own copy
    source[0] = 'X';
    EXPECT(toml->source.ptr[0] == 'a');

    toml_free(toml);
}

static void test_source_has_nul_sentinel(void) {
    const char *source = "answer = 42\n";
    size_t len = strlen(source);
    toml_t *toml = toml_from_byte(source, len);

    EXPECT(toml->source.ptr[len] == '\0');

    toml_free(toml);
}

// Exercises the chunk_size = min_size branch in toml_from_byte(): the
// input alone is larger than one arena chunk, so the arena must grow
// to fit the handle, the copy, and the sentinel. The padding lives
// inside a string value so the document still parses successfully
static void test_from_byte_handles_input_larger_than_one_chunk(void) {
    const char *prefix = "a = \"";
    const char *suffix = "\"\n";
    size_t padding_len = ARENA_CHUNK_SIZE + 1024;
    size_t len = strlen(prefix) + padding_len + strlen(suffix);

    char *source = malloc(len);
    memcpy(source, prefix, strlen(prefix));
    memset(source + strlen(prefix), 'a', padding_len);
    memcpy(source + strlen(prefix) + padding_len, suffix, strlen(suffix));

    toml_t *toml = toml_from_byte(source, len);

    EXPECT(toml != NULL);
    EXPECT(toml_has_error(toml) == false);
    EXPECT(toml->source.len == len);
    EXPECT(memcmp(toml->source.ptr, source, len) == 0);
    EXPECT(toml->source.ptr[len] == '\0');

    toml_free(toml);
    free(source);
}

int main(void) {
    test_has_error_null_is_safe_and_true();
    test_from_str_returns_usable_handle();
    test_source_is_copied_not_aliased();
    test_source_has_nul_sentinel();
    test_from_byte_handles_input_larger_than_one_chunk();

    printf("%d passed, %d failed\n", pass_count, fail_count);

    return (fail_count == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
