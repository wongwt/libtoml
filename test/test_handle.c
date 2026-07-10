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

#define _POSIX_C_SOURCE 200809L

#include "../src/toml.c"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

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

static void test_from_byte_populates_root(void) {
    const char *source = "answer = 42\n";
    toml_t *toml = toml_from_byte(source, strlen(source));

    EXPECT(toml != NULL);
    EXPECT(toml_has_error(toml) == false);
    EXPECT(toml->root != NULL);
    EXPECT(toml->root->type == TOML_TABLE);
    EXPECT(toml->root->val.t.count == 1);
    EXPECT(toml->root->val.t.entries[0]->type == TOML_S64);
    EXPECT(toml->root->val.t.entries[0]->val.s64 == 42);

    toml_free(toml);
}

static void test_from_byte_duplicate_key_sets_error(void) {
    const char *source = "a = 1\na = 2\n";
    toml_t *toml = toml_from_byte(source, strlen(source));

    EXPECT(toml != NULL);
    EXPECT(toml_has_error(toml) == true);
    EXPECT(toml->error.code == TOML_ERR_DUP_KEY);
    EXPECT(toml->root == NULL);

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

// Redirects stderr to a temp file for the duration of `fn(toml)`, then
// copies up to `buf_len - 1` captured bytes into `buf` as a C string
static void capture_stderr(void (*fn)(const toml_t *), const toml_t *toml, char *buf, size_t buf_len) {
    fflush(stderr);
    int saved_fd = dup(fileno(stderr));
    FILE *tmp = tmpfile();
    dup2(fileno(tmp), fileno(stderr));

    fn(toml);

    fflush(stderr);
    dup2(saved_fd, fileno(stderr));
    close(saved_fd);

    rewind(tmp);
    size_t n = fread(buf, 1, buf_len - 1, tmp);
    buf[n] = '\0';
    fclose(tmp);
}

static void test_err_print_null_handle_is_safe_and_silent(void) {
    char buf[128];
    capture_stderr(toml_err_print, NULL, buf, sizeof buf);

    EXPECT(buf[0] == '\0');
}

static void test_err_print_no_error_is_silent(void) {
    const char *source = "answer = 42\n";
    toml_t *toml = toml_from_byte(source, strlen(source));
    char buf[128];

    capture_stderr(toml_err_print, toml, buf, sizeof buf);

    EXPECT(buf[0] == '\0');

    toml_free(toml);
}

static void test_err_print_writes_code_and_offset(void) {
    const char *source = "a = 1\na = 2\n";  // Second "a" key starts at offset 6
    toml_t *toml = toml_from_byte(source, strlen(source));
    char buf[128];

    capture_stderr(toml_err_print, toml, buf, sizeof buf);

    EXPECT(strstr(buf, "TOML_ERR_DUP_KEY") != NULL);
    EXPECT(strstr(buf, "offset 6") != NULL);

    toml_free(toml);
}

static void test_err_print_type_error_offset(void) {
    const char *source = "answer = 42\n";  // "answer" key starts at offset 0
    toml_t *toml = toml_from_byte(source, strlen(source));
    toml_get_str(toml, "answer");  // Wrong type: sets TOML_ERR_TYPE
    char buf[128];

    capture_stderr(toml_err_print, toml, buf, sizeof buf);

    EXPECT(strstr(buf, "TOML_ERR_TYPE") != NULL);
    EXPECT(strstr(buf, "offset 0") != NULL);

    toml_free(toml);
}

int main(void) {
    test_has_error_null_is_safe_and_true();
    test_from_str_returns_usable_handle();
    test_from_byte_populates_root();
    test_from_byte_duplicate_key_sets_error();
    test_source_is_copied_not_aliased();
    test_source_has_nul_sentinel();
    test_from_byte_handles_input_larger_than_one_chunk();
    test_err_print_null_handle_is_safe_and_silent();
    test_err_print_no_error_is_silent();
    test_err_print_writes_code_and_offset();
    test_err_print_type_error_offset();

    printf("%d passed, %d failed\n", pass_count, fail_count);

    return (fail_count == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
