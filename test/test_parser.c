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

// Unit tests for the parser: parse_key(), parse_val(), parse_keyval(),
// parse_table_header(), and parse_toml(), including `[table]` sections
// and duplicate-key/duplicate-table-name detection.

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

static void test_parse_key_bare(void) {
    toml_t *toml = toml_from_byte("", 0);
    lexer_s lexer = make_lexer("host");

    toml_span_s key = parse_key(toml, lexer_next(&lexer));

    EXPECT(toml_has_error(toml) == false);
    EXPECT(span_eq(key, "host"));

    toml_free(toml);
}

static void test_parse_key_widens_s64_and_bool(void) {
    toml_t *toml = toml_from_byte("", 0);

    lexer_s a = make_lexer("123");
    toml_span_s ka = parse_key(toml, lexer_next(&a));
    EXPECT(toml_has_error(toml) == false);
    EXPECT(span_eq(ka, "123"));

    lexer_s b = make_lexer("true");
    toml_span_s kb = parse_key(toml, lexer_next(&b));
    EXPECT(toml_has_error(toml) == false);
    EXPECT(span_eq(kb, "true"));

    lexer_s c = make_lexer("false");
    toml_span_s kc = parse_key(toml, lexer_next(&c));
    EXPECT(toml_has_error(toml) == false);
    EXPECT(span_eq(kc, "false"));

    toml_free(toml);
}

static void test_parse_key_rejects_unexpected_token(void) {
    toml_t *toml = toml_from_byte("", 0);
    lexer_s lexer = make_lexer("=");

    parse_key(toml, lexer_next(&lexer));

    EXPECT(toml_has_error(toml) == true);
    EXPECT(toml->error.code == TOML_ERR_SYNTAX);

    toml_free(toml);
}

static void test_parse_val_str(void) {
    toml_t *toml = toml_from_byte("", 0);
    lexer_s lexer = make_lexer("\"hi\"");

    toml_node_s *node = parse_val(toml, lexer_next(&lexer));

    EXPECT(node != NULL);
    EXPECT(node->type == TOML_STR);
    EXPECT(span_eq(node->val.byte, "hi"));

    toml_free(toml);
}

static void test_parse_val_str_empty(void) {
    toml_t *toml = toml_from_byte("", 0);
    lexer_s lexer = make_lexer("\"\"");

    toml_node_s *node = parse_val(toml, lexer_next(&lexer));

    EXPECT(node != NULL);
    EXPECT(node->type == TOML_STR);
    EXPECT(node->val.byte.len == 0);

    toml_free(toml);
}

static void test_parse_val_s64_positive(void) {
    toml_t *toml = toml_from_byte("", 0);
    lexer_s lexer = make_lexer("42");

    toml_node_s *node = parse_val(toml, lexer_next(&lexer));

    EXPECT(node != NULL);
    EXPECT(node->type == TOML_S64);
    EXPECT(node->val.s64 == 42);

    toml_free(toml);
}

static void test_parse_val_s64_negative(void) {
    toml_t *toml = toml_from_byte("", 0);
    lexer_s lexer = make_lexer("-7");

    toml_node_s *node = parse_val(toml, lexer_next(&lexer));

    EXPECT(node != NULL);
    EXPECT(node->type == TOML_S64);
    EXPECT(node->val.s64 == -7);

    toml_free(toml);
}

// A digit run longer than S64_TEXT_BUF_SIZE can't be copied into the
// fixed decode buffer safely; parse_s64_value() must clamp instead of
// overflowing it. Full overflow semantics are still M2/M3 work -- this
// only guards the buffer itself
static void test_parse_val_s64_too_long_clamps_without_overflow(void) {
    toml_t *toml = toml_from_byte("", 0);
    lexer_s pos = make_lexer("99999999999999999999999999");
    lexer_s neg = make_lexer("-99999999999999999999999999");

    toml_node_s *pos_node = parse_val(toml, lexer_next(&pos));
    EXPECT(pos_node != NULL);
    EXPECT(pos_node->type == TOML_S64);
    EXPECT(pos_node->val.s64 == INT64_MAX);

    toml_node_s *neg_node = parse_val(toml, lexer_next(&neg));
    EXPECT(neg_node != NULL);
    EXPECT(neg_node->type == TOML_S64);
    EXPECT(neg_node->val.s64 == INT64_MIN);

    toml_free(toml);
}

static void test_parse_val_bool(void) {
    toml_t *toml = toml_from_byte("", 0);

    lexer_s t = make_lexer("true");
    toml_node_s *nt = parse_val(toml, lexer_next(&t));
    EXPECT(nt != NULL);
    EXPECT(nt->type == TOML_BOOL);
    EXPECT(nt->val.b == true);

    lexer_s f = make_lexer("false");
    toml_node_s *nf = parse_val(toml, lexer_next(&f));
    EXPECT(nf != NULL);
    EXPECT(nf->type == TOML_BOOL);
    EXPECT(nf->val.b == false);

    toml_free(toml);
}

static void test_parse_val_rejects_unexpected_token(void) {
    toml_t *toml = toml_from_byte("", 0);
    lexer_s lexer = make_lexer("=");

    toml_node_s *node = parse_val(toml, lexer_next(&lexer));

    EXPECT(node == NULL);
    EXPECT(toml_has_error(toml) == true);
    EXPECT(toml->error.code == TOML_ERR_SYNTAX);

    toml_free(toml);
}

static void test_parse_keyval(void) {
    toml_t *toml = toml_from_byte("", 0);
    lexer_s lexer = make_lexer("answer = 42");
    token_s key_tok = lexer_next(&lexer);

    toml_node_s *node = parse_keyval(toml, &lexer, key_tok);

    EXPECT(node != NULL);
    EXPECT(span_eq(node->key, "answer"));
    EXPECT(node->type == TOML_S64);
    EXPECT(node->val.s64 == 42);

    toml_free(toml);
}

static void test_parse_keyval_missing_equal_is_syntax_error(void) {
    toml_t *toml = toml_from_byte("", 0);
    lexer_s lexer = make_lexer("answer 42");
    token_s key_tok = lexer_next(&lexer);

    toml_node_s *node = parse_keyval(toml, &lexer, key_tok);

    EXPECT(node == NULL);
    EXPECT(toml_has_error(toml) == true);
    EXPECT(toml->error.code == TOML_ERR_SYNTAX);

    toml_free(toml);
}

static void test_parse_toml_single_keyval(void) {
    toml_t *toml = toml_from_byte("", 0);
    lexer_s lexer = make_lexer("answer = 42\n");

    toml_node_s *root = parse_toml(toml, &lexer);

    EXPECT(root != NULL);
    EXPECT(toml_has_error(toml) == false);
    EXPECT(root->type == TOML_TABLE);
    EXPECT(root->val.t.count == 1);
    EXPECT(span_eq(root->val.t.entries[0]->key, "answer"));
    EXPECT(root->val.t.entries[0]->type == TOML_S64);
    EXPECT(root->val.t.entries[0]->val.s64 == 42);

    toml_free(toml);
}

static void test_parse_toml_multiple_keyvals(void) {
    toml_t *toml = toml_from_byte("", 0);
    lexer_s lexer = make_lexer("a = 1\nb = true\nc = \"x\"\n");

    toml_node_s *root = parse_toml(toml, &lexer);

    EXPECT(root != NULL);
    EXPECT(toml_has_error(toml) == false);
    EXPECT(root->val.t.count == 3);
    EXPECT(span_eq(root->val.t.entries[0]->key, "a"));
    EXPECT(root->val.t.entries[0]->val.s64 == 1);
    EXPECT(span_eq(root->val.t.entries[1]->key, "b"));
    EXPECT(root->val.t.entries[1]->val.b == true);
    EXPECT(span_eq(root->val.t.entries[2]->key, "c"));
    EXPECT(span_eq(root->val.t.entries[2]->val.byte, "x"));

    toml_free(toml);
}

static void test_parse_toml_empty_input(void) {
    toml_t *toml = toml_from_byte("", 0);
    lexer_s lexer = make_lexer("");

    toml_node_s *root = parse_toml(toml, &lexer);

    EXPECT(root != NULL);
    EXPECT(toml_has_error(toml) == false);
    EXPECT(root->val.t.count == 0);

    toml_free(toml);
}

static void test_parse_toml_blank_lines_and_comments_ignored(void) {
    toml_t *toml = toml_from_byte("", 0);
    lexer_s lexer = make_lexer("\n# hi\n\na = 1\n\n");

    toml_node_s *root = parse_toml(toml, &lexer);

    EXPECT(root != NULL);
    EXPECT(toml_has_error(toml) == false);
    EXPECT(root->val.t.count == 1);
    EXPECT(span_eq(root->val.t.entries[0]->key, "a"));

    toml_free(toml);
}

// A trailing comment already works by construction: lexer_scan_trivia()
// treats '#...' as trivia regardless of position, so it becomes the
// following newline token's leading trivia. This proves it end-to-end
// through parse_toml() rather than only at the lexer level
static void test_parse_toml_trailing_comment_after_value(void) {
    toml_t *toml = toml_from_byte("", 0);
    lexer_s lexer = make_lexer("a = 1 # comment\n");

    toml_node_s *root = parse_toml(toml, &lexer);

    EXPECT(root != NULL);
    EXPECT(toml_has_error(toml) == false);
    EXPECT(root->val.t.count == 1);
    EXPECT(span_eq(root->val.t.entries[0]->key, "a"));
    EXPECT(root->val.t.entries[0]->val.s64 == 1);

    toml_free(toml);
}

static void test_parse_toml_duplicate_key_reports_both_spans(void) {
    toml_t *toml = toml_from_byte("", 0);
    lexer_s lexer = make_lexer("a = 1\na = 2\n");

    toml_node_s *root = parse_toml(toml, &lexer);

    EXPECT(root == NULL);
    EXPECT(toml_has_error(toml) == true);
    EXPECT(toml->error.code == TOML_ERR_DUP_KEY);
    EXPECT(span_eq(toml->error.primary, "a"));
    EXPECT(span_eq(toml->error.secondary, "a"));
    EXPECT(toml->error.primary.ptr != toml->error.secondary.ptr);

    toml_free(toml);
}

static void test_parse_toml_syntax_error_missing_equal(void) {
    toml_t *toml = toml_from_byte("", 0);
    lexer_s lexer = make_lexer("a 1\n");

    toml_node_s *root = parse_toml(toml, &lexer);

    EXPECT(root == NULL);
    EXPECT(toml_has_error(toml) == true);
    EXPECT(toml->error.code == TOML_ERR_SYNTAX);

    toml_free(toml);
}

static void test_parse_toml_trailing_garbage_after_value_is_syntax_error(void) {
    toml_t *toml = toml_from_byte("", 0);
    lexer_s lexer = make_lexer("a = 1 2\n");

    toml_node_s *root = parse_toml(toml, &lexer);

    EXPECT(root == NULL);
    EXPECT(toml_has_error(toml) == true);
    EXPECT(toml->error.code == TOML_ERR_SYNTAX);

    toml_free(toml);
}

static void test_parse_toml_table_with_one_entry(void) {
    toml_t *toml = toml_from_byte("", 0);
    lexer_s lexer = make_lexer("[server]\nport = 80\n");

    toml_node_s *root = parse_toml(toml, &lexer);

    EXPECT(root != NULL);
    EXPECT(toml_has_error(toml) == false);
    EXPECT(root->val.t.count == 1);

    toml_node_s *server = root->val.t.entries[0];
    EXPECT(span_eq(server->key, "server"));
    EXPECT(server->type == TOML_TABLE);
    EXPECT(server->val.t.count == 1);
    EXPECT(span_eq(server->val.t.entries[0]->key, "port"));
    EXPECT(server->val.t.entries[0]->val.s64 == 80);

    toml_free(toml);
}

static void test_parse_toml_empty_table(void) {
    toml_t *toml = toml_from_byte("", 0);
    lexer_s lexer = make_lexer("[server]\n");

    toml_node_s *root = parse_toml(toml, &lexer);

    EXPECT(root != NULL);
    EXPECT(toml_has_error(toml) == false);
    EXPECT(root->val.t.count == 1);
    EXPECT(root->val.t.entries[0]->type == TOML_TABLE);
    EXPECT(root->val.t.entries[0]->val.t.count == 0);

    toml_free(toml);
}

static void test_parse_toml_root_keys_then_table(void) {
    toml_t *toml = toml_from_byte("", 0);
    lexer_s lexer = make_lexer("a = 1\n[server]\nport = 80\n");

    toml_node_s *root = parse_toml(toml, &lexer);

    EXPECT(root != NULL);
    EXPECT(toml_has_error(toml) == false);
    EXPECT(root->val.t.count == 2);
    EXPECT(span_eq(root->val.t.entries[0]->key, "a"));
    EXPECT(root->val.t.entries[0]->type == TOML_S64);
    EXPECT(span_eq(root->val.t.entries[1]->key, "server"));
    EXPECT(root->val.t.entries[1]->type == TOML_TABLE);

    toml_free(toml);
}

static void test_parse_toml_consecutive_tables(void) {
    toml_t *toml = toml_from_byte("", 0);
    lexer_s lexer = make_lexer("[a]\nx = 1\n[b]\ny = 2\n");

    toml_node_s *root = parse_toml(toml, &lexer);

    EXPECT(root != NULL);
    EXPECT(toml_has_error(toml) == false);
    EXPECT(root->val.t.count == 2);

    toml_node_s *a = root->val.t.entries[0];
    EXPECT(span_eq(a->key, "a"));
    EXPECT(a->val.t.count == 1);
    EXPECT(span_eq(a->val.t.entries[0]->key, "x"));
    EXPECT(a->val.t.entries[0]->val.s64 == 1);

    toml_node_s *b = root->val.t.entries[1];
    EXPECT(span_eq(b->key, "b"));
    EXPECT(b->val.t.count == 1);
    EXPECT(span_eq(b->val.t.entries[0]->key, "y"));
    EXPECT(b->val.t.entries[0]->val.s64 == 2);

    toml_free(toml);
}

// Two consecutive empty tables: closing the first one's (empty) body
// happens purely because the second '[' was seen, not because it had
// any entries
static void test_parse_toml_consecutive_empty_tables(void) {
    toml_t *toml = toml_from_byte("", 0);
    lexer_s lexer = make_lexer("[a]\n[b]\n");

    toml_node_s *root = parse_toml(toml, &lexer);

    EXPECT(root != NULL);
    EXPECT(toml_has_error(toml) == false);
    EXPECT(root->val.t.count == 2);
    EXPECT(root->val.t.entries[0]->val.t.count == 0);
    EXPECT(root->val.t.entries[1]->val.t.count == 0);

    toml_free(toml);
}

static void test_parse_toml_duplicate_table_name(void) {
    toml_t *toml = toml_from_byte("", 0);
    lexer_s lexer = make_lexer("[server]\n[server]\n");

    toml_node_s *root = parse_toml(toml, &lexer);

    EXPECT(root == NULL);
    EXPECT(toml_has_error(toml) == true);
    EXPECT(toml->error.code == TOML_ERR_DUP_KEY);

    toml_free(toml);
}

// A table name colliding with an already-defined top-level key is the
// same kind of collision as two identical keys
static void test_parse_toml_table_name_collides_with_existing_key(void) {
    toml_t *toml = toml_from_byte("", 0);
    lexer_s lexer = make_lexer("server = 1\n[server]\n");

    toml_node_s *root = parse_toml(toml, &lexer);

    EXPECT(root == NULL);
    EXPECT(toml_has_error(toml) == true);
    EXPECT(toml->error.code == TOML_ERR_DUP_KEY);

    toml_free(toml);
}

static void test_parse_toml_table_header_missing_rbracket_is_syntax_error(void) {
    toml_t *toml = toml_from_byte("", 0);
    lexer_s lexer = make_lexer("[server\n");

    toml_node_s *root = parse_toml(toml, &lexer);

    EXPECT(root == NULL);
    EXPECT(toml_has_error(toml) == true);
    EXPECT(toml->error.code == TOML_ERR_SYNTAX);

    toml_free(toml);
}

static void test_parse_toml_table_header_trailing_garbage_is_syntax_error(void) {
    toml_t *toml = toml_from_byte("", 0);
    lexer_s lexer = make_lexer("[server] extra\n");

    toml_node_s *root = parse_toml(toml, &lexer);

    EXPECT(root == NULL);
    EXPECT(toml_has_error(toml) == true);
    EXPECT(toml->error.code == TOML_ERR_SYNTAX);

    toml_free(toml);
}

int main(void) {
    test_parse_key_bare();
    test_parse_key_widens_s64_and_bool();
    test_parse_key_rejects_unexpected_token();
    test_parse_val_str();
    test_parse_val_str_empty();
    test_parse_val_s64_positive();
    test_parse_val_s64_negative();
    test_parse_val_s64_too_long_clamps_without_overflow();
    test_parse_val_bool();
    test_parse_val_rejects_unexpected_token();
    test_parse_keyval();
    test_parse_keyval_missing_equal_is_syntax_error();
    test_parse_toml_single_keyval();
    test_parse_toml_multiple_keyvals();
    test_parse_toml_empty_input();
    test_parse_toml_blank_lines_and_comments_ignored();
    test_parse_toml_trailing_comment_after_value();
    test_parse_toml_duplicate_key_reports_both_spans();
    test_parse_toml_syntax_error_missing_equal();
    test_parse_toml_trailing_garbage_after_value_is_syntax_error();
    test_parse_toml_table_with_one_entry();
    test_parse_toml_empty_table();
    test_parse_toml_root_keys_then_table();
    test_parse_toml_consecutive_tables();
    test_parse_toml_consecutive_empty_tables();
    test_parse_toml_duplicate_table_name();
    test_parse_toml_table_name_collides_with_existing_key();
    test_parse_toml_table_header_missing_rbracket_is_syntax_error();
    test_parse_toml_table_header_trailing_garbage_is_syntax_error();

    printf("%d passed, %d failed\n", pass_count, fail_count);

    return (fail_count == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
