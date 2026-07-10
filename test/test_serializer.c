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

// Parses `source`, serializes it back out, and asserts the result is
// byte-for-byte identical to the original
static void expect_round_trip(const char *source) {
    size_t len = strlen(source);
    toml_t *toml = toml_from_byte(source, len);

    EXPECT(toml != NULL);
    EXPECT(toml_has_error(toml) == false);

    char *out = malloc(len == 0 ? 1 : len);
    size_t written = toml_to_byte(toml, out);

    EXPECT(written == len);
    EXPECT(memcmp(out, source, len) == 0);

    free(out);
    toml_free(toml);
}

static void test_round_trip_empty_input(void) {
    expect_round_trip("");
}

static void test_round_trip_single_keyval(void) {
    expect_round_trip("answer = 42\n");
}

static void test_round_trip_no_trailing_newline(void) {
    expect_round_trip("answer = 42");
}

static void test_round_trip_multiple_keyvals(void) {
    expect_round_trip("a = 1\nb = true\nc = \"x\"\n");
}

static void test_round_trip_blank_lines_and_comments(void) {
    expect_round_trip("\n# hi\n\na = 1\n\n");
}

static void test_round_trip_trailing_comment_after_value(void) {
    expect_round_trip("a = 1 # comment\n");
}

static void test_round_trip_whitespace_around_equal(void) {
    expect_round_trip("a\t=   1\n");
}

static void test_round_trip_table_with_entries(void) {
    expect_round_trip("[server]\nport = 80\nhost = \"localhost\"\n");
}

static void test_round_trip_empty_table(void) {
    expect_round_trip("[server]\n");
}

static void test_round_trip_root_keys_then_table(void) {
    expect_round_trip("a = 1\n[server]\nport = 80\n");
}

static void test_round_trip_consecutive_tables(void) {
    expect_round_trip("[a]\nx = 1\n\n# between tables\n[b]\ny = 2\n");
}

static void test_round_trip_consecutive_empty_tables(void) {
    expect_round_trip("[a]\n[b]\n");
}

static void test_round_trip_crlf_line_endings(void) {
    expect_round_trip("a = 1\r\nb = 2\r\n");
}

static void test_round_trip_comment_only_document(void) {
    expect_round_trip("# just a comment, no entries\n");
}

static void test_round_trip_trailing_content_no_newline(void) {
    expect_round_trip("[a]\nx = 1\n# trailing, no newline");
}

static void test_to_byte_null_handle_returns_zero(void) {
    EXPECT(toml_to_byte(NULL, NULL) == 0);
}

static void test_to_byte_failed_parse_returns_zero(void) {
    toml_t *toml = toml_from_byte("a = 1\na = 2\n", 12);

    EXPECT(toml_has_error(toml) == true);
    EXPECT(toml_to_byte(toml, NULL) == 0);

    toml_free(toml);
}

int main(void) {
    test_round_trip_empty_input();
    test_round_trip_single_keyval();
    test_round_trip_no_trailing_newline();
    test_round_trip_multiple_keyvals();
    test_round_trip_blank_lines_and_comments();
    test_round_trip_trailing_comment_after_value();
    test_round_trip_whitespace_around_equal();
    test_round_trip_table_with_entries();
    test_round_trip_empty_table();
    test_round_trip_root_keys_then_table();
    test_round_trip_consecutive_tables();
    test_round_trip_consecutive_empty_tables();
    test_round_trip_crlf_line_endings();
    test_round_trip_comment_only_document();
    test_round_trip_trailing_content_no_newline();
    test_to_byte_null_handle_returns_zero();
    test_to_byte_failed_parse_returns_zero();

    printf("%d passed, %d failed\n", pass_count, fail_count);

    return (fail_count == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
