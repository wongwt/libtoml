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

static bool streq(const char *a, const char *b) {
    return strcmp(a, b) == 0;
}

static const char *DOC =
    "answer = 42\n"
    "enabled = true\n"
    "name = \"gopher\"\n"
    "123 = \"numeric key\"\n"
    "\n"
    "[server]\n"
    "port = 8080\n"
    "host = \"localhost\"\n";


// toml_has()

static void test_has_root_key(void) {
    toml_t *toml = toml_from_byte(DOC, strlen(DOC));

    EXPECT(toml_has(toml, "answer") == true);
    EXPECT(toml_has_error(toml) == false);

    toml_free(toml);
}

static void test_has_nested_key(void) {
    toml_t *toml = toml_from_byte(DOC, strlen(DOC));

    EXPECT(toml_has(toml, "server.port") == true);
    EXPECT(toml_has_error(toml) == false);

    toml_free(toml);
}

static void test_has_missing_key_is_false_no_error(void) {
    toml_t *toml = toml_from_byte(DOC, strlen(DOC));

    EXPECT(toml_has(toml, "missing") == false);
    EXPECT(toml_has_error(toml) == false);

    toml_free(toml);
}

static void test_has_descends_into_scalar_is_false(void) {
    toml_t *toml = toml_from_byte(DOC, strlen(DOC));

    EXPECT(toml_has(toml, "answer.nested") == false);
    EXPECT(toml_has_error(toml) == false);

    toml_free(toml);
}

static void test_has_null_handle_is_false(void) {
    EXPECT(toml_has(NULL, "answer") == false);
}

static void test_has_failed_parse_is_false(void) {
    const char *source = "a = 1\na = 2\n";
    toml_t *toml = toml_from_byte(source, strlen(source));

    EXPECT(toml_has_error(toml) == true);
    EXPECT(toml_has(toml, "a") == false);
    EXPECT(toml->error.code == TOML_ERR_DUP_KEY);  // Sticky parse error untouched

    toml_free(toml);
}


// toml_type()

static void test_type_of_each_kind(void) {
    toml_t *toml = toml_from_byte(DOC, strlen(DOC));

    EXPECT(toml_type(toml, "answer") == TOML_S64);
    EXPECT(toml_type(toml, "enabled") == TOML_BOOL);
    EXPECT(toml_type(toml, "name") == TOML_STR);
    EXPECT(toml_type(toml, "server") == TOML_TABLE);
    EXPECT(toml_has_error(toml) == false);

    toml_free(toml);
}

static void test_type_of_missing_path_is_none(void) {
    toml_t *toml = toml_from_byte(DOC, strlen(DOC));

    EXPECT(toml_type(toml, "missing") == TOML_UNKNOWN);
    EXPECT(toml_has_error(toml) == false);

    toml_free(toml);
}

static void test_path_dotted_descends_tables(void) {
    toml_t *toml = toml_from_byte(DOC, strlen(DOC));

    EXPECT(toml_get_s64_or(toml, "server.port", 0) == 8080);
    EXPECT(streq(toml_get_str_or(toml, "server.host", ""), "localhost"));

    toml_free(toml);
}

static void test_path_quoted_segment_matches_numeric_key(void) {
    toml_t *toml = toml_from_byte(DOC, strlen(DOC));

    EXPECT(streq(toml_get_str_or(toml, "\"123\"", ""), "numeric key"));

    toml_free(toml);
}

static void test_path_unquoted_digits_is_index_never_matches_key(void) {
    toml_t *toml = toml_from_byte(DOC, strlen(DOC));

    EXPECT(toml_has(toml, "123") == false);
    EXPECT(toml_get_str_or(toml, "123", "fallback") != NULL);
    EXPECT(streq(toml_get_str_or(toml, "123", "fallback"), "fallback"));
    EXPECT(toml_has_error(toml) == false);  // Not-found, not a type error

    toml_free(toml);
}

static void test_path_empty_string_is_not_found(void) {
    toml_t *toml = toml_from_byte(DOC, strlen(DOC));

    EXPECT(toml_has(toml, "") == false);

    toml_free(toml);
}

static void test_path_leading_dot_is_malformed_not_found(void) {
    toml_t *toml = toml_from_byte(DOC, strlen(DOC));

    EXPECT(toml_has(toml, ".answer") == false);

    toml_free(toml);
}

static void test_path_trailing_dot_is_malformed_not_found(void) {
    toml_t *toml = toml_from_byte(DOC, strlen(DOC));

    EXPECT(toml_has(toml, "server.") == false);

    toml_free(toml);
}

static void test_path_doubled_dot_is_malformed_not_found(void) {
    toml_t *toml = toml_from_byte(DOC, strlen(DOC));

    EXPECT(toml_has(toml, "server..port") == false);

    toml_free(toml);
}

static void test_path_unterminated_quote_is_malformed_not_found(void) {
    toml_t *toml = toml_from_byte(DOC, strlen(DOC));

    EXPECT(toml_has(toml, "\"unterminated") == false);

    toml_free(toml);
}


// toml_get_s64_or()

static void test_get_s64_or_success(void) {
    toml_t *toml = toml_from_byte(DOC, strlen(DOC));

    EXPECT(toml_get_s64_or(toml, "answer", -1) == 42);
    EXPECT(toml_has_error(toml) == false);

    toml_free(toml);
}

static void test_get_s64_or_absent_returns_default_no_error(void) {
    toml_t *toml = toml_from_byte(DOC, strlen(DOC));

    EXPECT(toml_get_s64_or(toml, "missing", -1) == -1);
    EXPECT(toml_has_error(toml) == false);

    toml_free(toml);
}

static void test_get_s64_or_wrong_type_returns_default_and_error(void) {
    toml_t *toml = toml_from_byte(DOC, strlen(DOC));

    EXPECT(toml_get_s64_or(toml, "name", -1) == -1);
    EXPECT(toml_has_error(toml) == true);
    EXPECT(toml->error.code == TOML_ERR_TYPE);

    toml_free(toml);
}


// toml_get_bool_or()

static void test_get_bool_or_success(void) {
    toml_t *toml = toml_from_byte(DOC, strlen(DOC));

    EXPECT(toml_get_bool_or(toml, "enabled", false) == true);
    EXPECT(toml_has_error(toml) == false);

    toml_free(toml);
}

static void test_get_bool_or_wrong_type_returns_default_and_error(void) {
    toml_t *toml = toml_from_byte(DOC, strlen(DOC));

    EXPECT(toml_get_bool_or(toml, "answer", false) == false);
    EXPECT(toml_has_error(toml) == true);
    EXPECT(toml->error.code == TOML_ERR_TYPE);

    toml_free(toml);
}


// toml_get_str() / toml_get_str_or()

static void test_get_str_success(void) {
    toml_t *toml = toml_from_byte(DOC, strlen(DOC));

    const char *name = toml_get_str(toml, "name");

    EXPECT(name != NULL);
    EXPECT(streq(name, "gopher"));
    EXPECT(toml_has_error(toml) == false);

    toml_free(toml);
}

static void test_get_str_absent_returns_null_no_error(void) {
    toml_t *toml = toml_from_byte(DOC, strlen(DOC));

    EXPECT(toml_get_str(toml, "missing") == NULL);
    EXPECT(toml_has_error(toml) == false);

    toml_free(toml);
}

static void test_get_str_wrong_type_returns_null_and_error(void) {
    toml_t *toml = toml_from_byte(DOC, strlen(DOC));

    EXPECT(toml_get_str(toml, "answer") == NULL);
    EXPECT(toml_has_error(toml) == true);
    EXPECT(toml->error.code == TOML_ERR_TYPE);

    toml_free(toml);
}

static void test_get_str_or_never_null(void) {
    toml_t *toml = toml_from_byte(DOC, strlen(DOC));

    EXPECT(streq(toml_get_str_or(toml, "name", "(unset)"), "gopher"));
    EXPECT(streq(toml_get_str_or(toml, "missing", "(unset)"), "(unset)"));
    EXPECT(streq(toml_get_str_or(toml, "answer", "(unset)"), "(unset)"));

    toml_free(toml);
}


static void test_sticky_error_success_clears_prior_failure(void) {
    toml_t *toml = toml_from_byte(DOC, strlen(DOC));

    toml_get_s64_or(toml, "name", -1);  // Wrong type: sets TOML_ERR_TYPE
    EXPECT(toml_has_error(toml) == true);

    toml_get_s64_or(toml, "answer", -1);  // Success: must clear it
    EXPECT(toml_has_error(toml) == false);

    toml_free(toml);
}

static void test_sticky_error_failure_overwrites_prior_success(void) {
    toml_t *toml = toml_from_byte(DOC, strlen(DOC));

    toml_get_s64_or(toml, "answer", -1);  // Success
    EXPECT(toml_has_error(toml) == false);

    toml_get_s64_or(toml, "name", -1);  // Wrong type: overwrites
    EXPECT(toml_has_error(toml) == true);
    EXPECT(toml->error.code == TOML_ERR_TYPE);

    toml_free(toml);
}

static void test_sticky_error_reflects_most_recent_across_getters(void) {
    toml_t *toml = toml_from_byte(DOC, strlen(DOC));

    toml_get_str(toml, "answer");  // Wrong type
    EXPECT(toml_has_error(toml) == true);

    toml_has(toml, "missing");  // Always clears, regardless of found
    EXPECT(toml_has_error(toml) == false);

    toml_free(toml);
}

static void test_to_byte_unaffected_by_prior_getter_error(void) {
    toml_t *toml = toml_from_byte(DOC, strlen(DOC));

    toml_get_s64_or(toml, "name", -1);  // Wrong type: sets an error
    EXPECT(toml_has_error(toml) == true);

    char *out = malloc(strlen(DOC));
    size_t written = toml_to_byte(toml, out);

    EXPECT(written == strlen(DOC));
    EXPECT(memcmp(out, DOC, strlen(DOC)) == 0);
    EXPECT(toml_has_error(toml) == false);  // toml_to_byte succeeded, so it clears

    free(out);
    toml_free(toml);
}

int main(void) {
    test_has_root_key();
    test_has_nested_key();
    test_has_missing_key_is_false_no_error();
    test_has_descends_into_scalar_is_false();
    test_has_null_handle_is_false();
    test_has_failed_parse_is_false();
    test_type_of_each_kind();
    test_type_of_missing_path_is_none();
    test_path_dotted_descends_tables();
    test_path_quoted_segment_matches_numeric_key();
    test_path_unquoted_digits_is_index_never_matches_key();
    test_path_empty_string_is_not_found();
    test_path_leading_dot_is_malformed_not_found();
    test_path_trailing_dot_is_malformed_not_found();
    test_path_doubled_dot_is_malformed_not_found();
    test_path_unterminated_quote_is_malformed_not_found();
    test_get_s64_or_success();
    test_get_s64_or_absent_returns_default_no_error();
    test_get_s64_or_wrong_type_returns_default_and_error();
    test_get_bool_or_success();
    test_get_bool_or_wrong_type_returns_default_and_error();
    test_get_str_success();
    test_get_str_absent_returns_null_no_error();
    test_get_str_wrong_type_returns_null_and_error();
    test_get_str_or_never_null();
    test_sticky_error_success_clears_prior_failure();
    test_sticky_error_failure_overwrites_prior_success();
    test_sticky_error_reflects_most_recent_across_getters();
    test_to_byte_unaffected_by_prior_getter_error();

    printf("%d passed, %d failed\n", pass_count, fail_count);

    return (fail_count == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
