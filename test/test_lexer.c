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

static bool span_eq(toml_span_s span, const char *text) {
    size_t len = strlen(text);
    return span.len == len && memcmp(span.ptr, text, len) == 0;
}

static lexer_s make_lexer(const char *source) {
    lexer_s lexer;
    lexer_init(&lexer, source, strlen(source));
    return lexer;
}

static void test_bare_key_scans_until_boundary(void) {
    lexer_s lexer = make_lexer("answer=42");
    token_s token = lexer_next(&lexer);

    EXPECT(token.type == TOKEN_BARE_KEY);
    EXPECT(span_eq(token.text, "answer"));
    EXPECT(token.leading.len == 0);
}

static void test_bare_key_allows_digits_underscore_dash(void) {
    lexer_s lexer = make_lexer("my_key-2 =");
    token_s token = lexer_next(&lexer);

    EXPECT(token.type == TOKEN_BARE_KEY);
    EXPECT(span_eq(token.text, "my_key-2"));
}

static void test_leading_whitespace_becomes_trivia(void) {
    lexer_s lexer = make_lexer("   answer");
    token_s token = lexer_next(&lexer);

    EXPECT(span_eq(token.leading, "   "));
    EXPECT(span_eq(token.text, "answer"));
}

static void test_leading_comment_becomes_trivia_and_stops_before_newline(void) {
    lexer_s lexer = make_lexer("# hi\nanswer");
    token_s first = lexer_next(&lexer);

    EXPECT(first.type == TOKEN_NEWLINE);
    EXPECT(span_eq(first.leading, "# hi"));

    token_s second = lexer_next(&lexer);
    EXPECT(second.type == TOKEN_BARE_KEY);
    EXPECT(span_eq(second.text, "answer"));
}

static void test_equals_token(void) {
    lexer_s lexer = make_lexer("=");
    token_s token = lexer_next(&lexer);

    EXPECT(token.type == TOKEN_EQUAL);
    EXPECT(span_eq(token.text, "="));
}

static void test_newline_tokens_lf_and_crlf(void) {
    lexer_s lf = make_lexer("\n");
    token_s lf_token = lexer_next(&lf);
    EXPECT(lf_token.type == TOKEN_NEWLINE);
    EXPECT(lf_token.text.len == 1);

    lexer_s crlf = make_lexer("\r\n");
    token_s crlf_token = lexer_next(&crlf);
    EXPECT(crlf_token.type == TOKEN_NEWLINE);
    EXPECT(crlf_token.text.len == 2);
}

static void test_lone_cr_is_error(void) {
    lexer_s lexer = make_lexer("\rx");
    token_s token = lexer_next(&lexer);

    EXPECT(token.type == TOKEN_ERROR);
}

static void test_lone_cr_at_end_of_input_is_error(void) {
    lexer_s lexer = make_lexer("\r");
    token_s token = lexer_next(&lexer);

    EXPECT(token.type == TOKEN_ERROR);
    EXPECT(token.text.len == 1);
}

static void test_eof_token(void) {
    lexer_s lexer = make_lexer("");
    token_s token = lexer_next(&lexer);

    EXPECT(token.type == TOKEN_EOF);
}

static void test_basic_string_value(void) {
    lexer_s lexer = make_lexer("\"hello world\"");
    token_s token = lexer_next(&lexer);

    EXPECT(token.type == TOKEN_STR);
    EXPECT(span_eq(token.text, "\"hello world\""));
}

static void test_empty_basic_string(void) {
    lexer_s lexer = make_lexer("\"\"");
    token_s token = lexer_next(&lexer);

    EXPECT(token.type == TOKEN_STR);
    EXPECT(span_eq(token.text, "\"\""));
}

static void test_unterminated_basic_string_is_error(void) {
    lexer_s lexer = make_lexer("\"oops");
    token_s token = lexer_next(&lexer);

    EXPECT(token.type == TOKEN_ERROR);
}

static void test_basic_string_rejects_backslash(void) {
    lexer_s lexer = make_lexer("\"a\\nb\"");
    token_s token = lexer_next(&lexer);

    EXPECT(token.type == TOKEN_ERROR);
}

static void test_basic_string_rejects_embedded_newline(void) {
    lexer_s lexer = make_lexer("\"a\nb\"");
    token_s token = lexer_next(&lexer);

    EXPECT(token.type == TOKEN_ERROR);
}

static void test_integer_positive(void) {
    lexer_s lexer = make_lexer("42");
    token_s token = lexer_next(&lexer);

    EXPECT(token.type == TOKEN_S64);
    EXPECT(span_eq(token.text, "42"));
}

static void test_integer_negative(void) {
    lexer_s lexer = make_lexer("-7");
    token_s token = lexer_next(&lexer);

    EXPECT(token.type == TOKEN_S64);
    EXPECT(span_eq(token.text, "-7"));
}

static void test_integer_explicit_positive_sign(void) {
    lexer_s lexer = make_lexer("+7");
    token_s token = lexer_next(&lexer);

    EXPECT(token.type == TOKEN_S64);
    EXPECT(span_eq(token.text, "+7"));
}

static void test_lone_dash_is_bare_key(void) {
    lexer_s lexer = make_lexer("-");
    token_s token = lexer_next(&lexer);

    EXPECT(token.type == TOKEN_BARE_KEY);
    EXPECT(span_eq(token.text, "-"));
}

static void test_lone_plus_is_error(void) {
    lexer_s lexer = make_lexer("+");
    token_s token = lexer_next(&lexer);

    EXPECT(token.type == TOKEN_ERROR);
}

static void test_plus_prefixed_non_digit_is_error(void) {
    lexer_s lexer = make_lexer("+7x");
    token_s token = lexer_next(&lexer);

    EXPECT(token.type == TOKEN_ERROR);
    EXPECT(span_eq(token.text, "+7"));
}

static void test_bool_true(void) {
    lexer_s lexer = make_lexer("true");
    token_s token = lexer_next(&lexer);

    EXPECT(token.type == TOKEN_TRUE);
    EXPECT(span_eq(token.text, "true"));
}

static void test_bool_false(void) {
    lexer_s lexer = make_lexer("false");
    token_s token = lexer_next(&lexer);

    EXPECT(token.type == TOKEN_FALSE);
    EXPECT(span_eq(token.text, "false"));
}

static void test_bool_keyword_prefix_is_bare_key(void) {
    lexer_s lexer = make_lexer("truer");
    token_s token = lexer_next(&lexer);

    EXPECT(token.type == TOKEN_BARE_KEY);
    EXPECT(span_eq(token.text, "truer"));
}

static void test_invalid_start_char_is_error(void) {
    lexer_s lexer = make_lexer("@nope");
    token_s token = lexer_next(&lexer);

    EXPECT(token.type == TOKEN_ERROR);
}

static void test_lbracket_token(void) {
    lexer_s lexer = make_lexer("[");
    token_s token = lexer_next(&lexer);

    EXPECT(token.type == TOKEN_LBRACKET);
    EXPECT(span_eq(token.text, "["));
}

static void test_rbracket_token(void) {
    lexer_s lexer = make_lexer("]");
    token_s token = lexer_next(&lexer);

    EXPECT(token.type == TOKEN_RBRACKET);
    EXPECT(span_eq(token.text, "]"));
}

static void test_table_header_line(void) {
    lexer_s lexer = make_lexer("[server]\n");

    token_s open = lexer_next(&lexer);
    EXPECT(open.type == TOKEN_LBRACKET);

    token_s name = lexer_next(&lexer);
    EXPECT(name.type == TOKEN_BARE_KEY);
    EXPECT(span_eq(name.text, "server"));

    token_s close = lexer_next(&lexer);
    EXPECT(close.type == TOKEN_RBRACKET);

    token_s newline = lexer_next(&lexer);
    EXPECT(newline.type == TOKEN_NEWLINE);
}

static void test_full_key_value_line(void) {
    lexer_s lexer = make_lexer("answer = 42\n");

    token_s key = lexer_next(&lexer);
    EXPECT(key.type == TOKEN_BARE_KEY);
    EXPECT(span_eq(key.text, "answer"));

    token_s eq = lexer_next(&lexer);
    EXPECT(eq.type == TOKEN_EQUAL);
    EXPECT(span_eq(eq.leading, " "));

    token_s value = lexer_next(&lexer);
    EXPECT(value.type == TOKEN_S64);
    EXPECT(span_eq(value.text, "42"));
    EXPECT(span_eq(value.leading, " "));

    token_s newline = lexer_next(&lexer);
    EXPECT(newline.type == TOKEN_NEWLINE);
}

int main(void) {
    test_bare_key_scans_until_boundary();
    test_bare_key_allows_digits_underscore_dash();
    test_leading_whitespace_becomes_trivia();
    test_leading_comment_becomes_trivia_and_stops_before_newline();
    test_equals_token();
    test_newline_tokens_lf_and_crlf();
    test_lone_cr_is_error();
    test_lone_cr_at_end_of_input_is_error();
    test_eof_token();
    test_basic_string_value();
    test_empty_basic_string();
    test_unterminated_basic_string_is_error();
    test_basic_string_rejects_backslash();
    test_basic_string_rejects_embedded_newline();
    test_integer_positive();
    test_integer_negative();
    test_integer_explicit_positive_sign();
    test_lone_dash_is_bare_key();
    test_lone_plus_is_error();
    test_plus_prefixed_non_digit_is_error();
    test_bool_true();
    test_bool_false();
    test_bool_keyword_prefix_is_bare_key();
    test_invalid_start_char_is_error();
    test_lbracket_token();
    test_rbracket_token();
    test_table_header_line();
    test_full_key_value_line();

    printf("%d passed, %d failed\n", pass_count, fail_count);

    return (fail_count == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
