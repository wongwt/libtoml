//! @copyright  Copyright 2026 Wei-Te Wong. MIT Licensed.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "toml.c"

static int total = 0;
static int fails = 0;

#define EXPECT(cond) \
    do { \
        total++; \
        if (!(cond)) { \
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            fails++; \
        } \
    } while (0)


// lexer_emit_error()

static void test_emit_error_sets_has_err(void) {
    lexer_s lex = lexer_init("!", 1);
    token_pos_s head = lex.cur;

    lexer_emit_error(&lex, head, "Unexpected character");

    EXPECT(lex.has_err);
}

static void test_emit_error_sets_msg(void) {
    lexer_s lex = lexer_init("!", 1);
    token_pos_s head = lex.cur;

    lexer_emit_error(&lex, head, "Unexpected character");

    EXPECT(lex.err.msg != NULL);
    EXPECT(strcmp(lex.err.msg, "Unexpected character") == 0);
}

static void test_emit_error_returns_token_error(void) {
    lexer_s lex = lexer_init("!", 1);
    token_pos_s head = lex.cur;

    token_s tok = lexer_emit_error(&lex, head, "Unexpected character");

    EXPECT(tok.type == TOKEN_ERROR);
}

static void test_emit_error_span_head_matches_arg(void) {
    lexer_s lex = lexer_init("ab!", 3);
    lexer_move(&lex, 2);
    token_pos_s head = lex.cur;

    lexer_emit_error(&lex, head, "Unexpected character");

    EXPECT(lex.err.span.head.col == 3);
}

static void test_emit_error_null_lexer_returns_eof(void) {
    token_pos_s head = {0, 1, 1};
    token_s tok = lexer_emit_error(NULL, head, "oops");

    EXPECT(tok.type == TOKEN_EOF);
}


// lexer_emit_token()

static void test_emit_token_sets_type(void) {
    lexer_s lex = lexer_init("[", 1);
    token_pos_s head = lex.cur;
    lexer_move(&lex, 1);

    token_s tok = lexer_emit_token(&lex, head, TOKEN_LBRACKET);

    EXPECT(tok.type == TOKEN_LBRACKET);
}

static void test_emit_token_sets_ptr_and_len(void) {
    lexer_s lex = lexer_init("=", 1);
    token_pos_s head = lex.cur;
    lexer_move(&lex, 1);

    token_s tok = lexer_emit_token(&lex, head, TOKEN_EQUALS);

    EXPECT(tok.ptr == lex.doc);
    EXPECT(tok.len == 1);
}

static void test_emit_token_span_head_is_supplied_position(void) {
    lexer_s lex = lexer_init("ab", 2);
    lexer_move(&lex, 1);         // Advance past 'a'
    token_pos_s head = lex.cur;  // head = col 2
    lexer_move(&lex, 1);

    token_s tok = lexer_emit_token(&lex, head, TOKEN_BARE_KEY);

    EXPECT(tok.span.head.row == 1);
    EXPECT(tok.span.head.col == 2);
}

static void test_emit_token_span_tail_is_current_cursor(void) {
    lexer_s lex = lexer_init("abc", 3);
    token_pos_s head = lex.cur;
    lexer_move(&lex, 3);

    token_s tok = lexer_emit_token(&lex, head, TOKEN_BARE_KEY);

    EXPECT(tok.span.tail.row == 1);
    EXPECT(tok.span.tail.col == 4);
}

static void test_emit_token_null_lexer_returns_eof(void) {
    token_pos_s head = {0, 1, 1};
    token_s tok = lexer_emit_token(NULL, head, TOKEN_LBRACKET);

    EXPECT(tok.type == TOKEN_EOF);
}


// lexer_err_print()

static void test_err_print_silent_when_no_error(void) {
    lexer_s lex = lexer_init("a = 1\n", 6);
    lexer_err_print(&lex);  // Must not crash
}

static void test_err_print_silent_on_null(void) {
    lexer_err_print(NULL);  // Must not crash
}

static void test_err_print_outputs_error(void) {
    lexer_s lex = lexer_init("!\n", 2);
    lex.has_err       = true;
    lex.err.msg       = "Unexpected character";
    lex.err.span.head = (token_pos_s){0, 1, 1};
    lex.err.span.tail = (token_pos_s){1, 1, 2};

    FILE *dev_null = fopen("/dev/null", "w");
    EXPECT(dev_null != NULL);
    FILE *old_stderr = stderr;
    stderr = dev_null;
    lexer_err_print(&lex);
    stderr = old_stderr;
    fclose(dev_null);
}


// lexer_has_error()

static void test_has_error_false_on_fresh_lexer(void) {
    lexer_s lex = lexer_init("a = 1\n", 6);

    EXPECT(!lexer_has_error(&lex));
}

static void test_has_error_true_after_error_set(void) {
    lexer_s lex = lexer_init("!\n", 2);
    lex.has_err       = true;
    lex.err.msg       = "Unexpected character";
    lex.err.span.head = (token_pos_s){0, 1, 1};
    lex.err.span.tail = (token_pos_s){1, 1, 2};

    EXPECT(lexer_has_error(&lex));
}

static void test_has_error_null_lexer_returns_false(void) {
    EXPECT(!lexer_has_error(NULL));
}


// lexer_init()

static void test_init_sets_src_and_len(void) {
    const char *src = "key = 1\n";
    lexer_s lex = lexer_init(src, 8);

    EXPECT(lex.doc == src);
    EXPECT(lex.doc_len == 8);
}

static void test_init_cursor_starts_at_beginning(void) {
    lexer_s lex = lexer_init("x = 1\n", 6);

    EXPECT(lex.cur.pos == 0);
    EXPECT(lex.cur.row == 1);
    EXPECT(lex.cur.col == 1);
}

static void test_init_has_no_error(void) {
    lexer_s lex = lexer_init("x = 1\n", 6);

    EXPECT(!lex.has_err);
}

static void test_init_empty_src(void) {
    lexer_s lex = lexer_init("", 0);

    EXPECT(lex.doc != NULL);
    EXPECT(lex.doc_len == 0);
    EXPECT(lex.cur.pos == 0);
    EXPECT(!lex.has_err);
}

static void test_init_null_src_clamps_len_to_zero(void) {
    lexer_s lex = lexer_init(NULL, 42);

    EXPECT(lex.doc == NULL);
    EXPECT(lex.doc_len == 0);
}

static void test_init_null_src_peek_returns_nul(void) {
    lexer_s lex = lexer_init(NULL, 0);

    EXPECT(lexer_peek(&lex, 0) == '\0');
}


// lexer_move()

static void test_move_advances_pos(void) {
    lexer_s lex = lexer_init("abc", 3);

    lexer_move(&lex, 1);

    EXPECT(lex.cur.pos == 1);
}

static void test_move_increments_col(void) {
    lexer_s lex = lexer_init("abc", 3);

    lexer_move(&lex, 1);

    EXPECT(lex.cur.col == 2);
}

static void test_move_by_multiple_increments_col(void) {
    lexer_s lex = lexer_init("abcd", 4);

    lexer_move(&lex, 3);

    EXPECT(lex.cur.pos == 3);
    EXPECT(lex.cur.col == 4);
}

static void test_move_on_lf_increments_row_resets_col(void) {
    lexer_s lex = lexer_init("\nabc", 4);

    lexer_move(&lex, 1);

    EXPECT(lex.cur.row == 2);
    EXPECT(lex.cur.col == 1);
}

static void test_move_by_two_on_crlf_increments_row_once(void) {
    // '\r' is a plain byte (col++), '\n' triggers row increment
    lexer_s lex = lexer_init("\r\nabc", 5);

    lexer_move(&lex, 2);

    EXPECT(lex.cur.row == 2);
    EXPECT(lex.cur.col == 1);
    EXPECT(lex.cur.pos == 2);
}

static void test_move_peek_after_move_returns_next_char(void) {
    lexer_s lex = lexer_init("abc", 3);

    lexer_move(&lex, 1);

    EXPECT(lexer_peek(&lex, 0) == 'b');
}

static void test_move_peek_after_move_to_eof_returns_nul(void) {
    lexer_s lex = lexer_init("a", 1);

    lexer_move(&lex, 1);

    EXPECT(lexer_peek(&lex, 0) == '\0');
}

static void test_move_clamps_at_eof(void) {
    lexer_s lex = lexer_init("ab", 2);

    lexer_move(&lex, 999);

    EXPECT(lex.cur.pos == 2);
}

static void test_move_by_zero_is_noop(void) {
    lexer_s lex = lexer_init("abc", 3);

    lexer_move(&lex, 0);

    EXPECT(lex.cur.pos == 0);
    EXPECT(lex.cur.row == 1);
    EXPECT(lex.cur.col == 1);
}

static void test_move_null_lexer_is_noop(void) {
    lexer_move(NULL, 1);  // Must not crash
}


// lexer_next(): bare keys and booleans

static void test_next_bool_true(void) {
    lexer_s lex = lexer_init("true", 4);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_BOOL_TRUE);
    EXPECT(tok.len  == 4);
}

static void test_next_bool_false(void) {
    lexer_s lex = lexer_init("false", 5);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_BOOL_FALSE);
    EXPECT(tok.len  == 5);
}

static void test_next_bool_uppercase_is_bare_key(void) {
    // TOML booleans are lowercase only
    const char *doc = "True";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_BARE_KEY);
}

static void test_next_bare_key_simple(void) {
    const char *doc = "name";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_BARE_KEY);
    EXPECT(tok.len  == 4);
}

static void test_next_bare_key_with_dash(void) {
    const char *doc = "key-name";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_BARE_KEY);
    EXPECT(tok.len  == 8);
}

static void test_next_bare_key_with_underscore(void) {
    const char *doc = "_private";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_BARE_KEY);
    EXPECT(tok.len  == 8);
}

static void test_next_bare_key_true_prefix(void) {
    // Starts with "true" but has more chars
    const char *doc = "truevalue";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_BARE_KEY);
    EXPECT(tok.len  == 9);
}

static void test_next_bare_key_false_prefix(void) {
    const char *doc = "falsely";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_BARE_KEY);
    EXPECT(tok.len  == 7);
}

static void test_next_bare_key_then_equals(void) {
    const char *doc = "name=";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_BARE_KEY);
    EXPECT(tok.len  == 4);
    tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_EQUALS);
}


// lexer_next(): Unicode bare keys

static void test_next_bare_key_latin_extended(void) {
    // U+00E9 LATIN SMALL LETTER E WITH ACUTE (2 bytes: 0xC3 0xA9)
    // "caf\xC3\xA9" = 5 bytes, 4 codepoints
    const char *doc = "caf\xC3\xA9";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_BARE_KEY);
    EXPECT(tok.len  == 5);  // Byte length
}

static void test_next_bare_key_cjk(void) {
    // U+540D U+524D (名前, "name" in Japanese): 3 bytes each = 6 bytes total
    const char *doc = "\xE5\x90\x8D\xE5\x89\x8D";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_BARE_KEY);
    EXPECT(tok.len  == 6);
}

static void test_next_bare_key_mixed_ascii_unicode(void) {
    // "k\xC3\xA9y" = ASCII 'k', U+00E9, ASCII 'y' = 4 bytes
    const char *doc = "k\xC3\xA9y";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_BARE_KEY);
    EXPECT(tok.len  == 4);
}

static void test_next_bare_key_4byte_codepoint(void) {
    // U+1F600 GRINNING FACE (4 bytes: 0xF0 0x9F 0x98 0x80)
    const char *doc = "\xF0\x9F\x98\x80";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_BARE_KEY);
    EXPECT(tok.len  == 4);
}

static void test_next_bare_key_unicode_col_counts_codepoints(void) {
    // After scanning a 3-byte CJK key (1 codepoint), col should be 2 (started at 1)
    const char *doc = "\xE5\x90\x8D";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_BARE_KEY);
    EXPECT(tok.span.tail.col == 2);  // col 1 → consumed 1 codepoint → col 2
}


// lexer_next(): datetime

static void test_next_date(void) {
    const char *doc = "2024-01-15";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_DATE);
    EXPECT(tok.len  == 10);
}

static void test_next_time(void) {
    const char *doc = "12:30:45";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_TIME);
    EXPECT(tok.len  == 8);
}

static void test_next_time_with_frac(void) {
    const char *doc = "12:30:45.123456";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_TIME);
    EXPECT(tok.len  == 15);
}

static void test_next_time_no_seconds(void) {
    const char *doc = "12:30";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_TIME);
    EXPECT(tok.len  == 5);
}

static void test_next_datetime_no_seconds(void) {
    const char *doc = "2024-01-15T12:30";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_DATETIME);
    EXPECT(tok.len  == 16);
}

static void test_next_datetimetz_no_seconds_z(void) {
    const char *doc = "2024-01-15T12:30Z";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_DATETIMETZ);
    EXPECT(tok.len  == 17);
}

static void test_next_datetime(void) {
    const char *doc = "2024-01-15T12:30:45";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_DATETIME);
    EXPECT(tok.len  == 19);
}

static void test_next_datetime_space_sep(void) {
    const char *doc = "2024-01-15 12:30:45";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_DATETIME);
    EXPECT(tok.len  == 19);
}

static void test_next_datetime_with_frac(void) {
    const char *doc = "2024-01-15T12:30:45.123";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_DATETIME);
    EXPECT(tok.len  == 23);
}

static void test_next_datetimetz_z(void) {
    const char *doc = "2024-01-15T12:30:45Z";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_DATETIMETZ);
    EXPECT(tok.len  == 20);
}

static void test_next_datetimetz_pos_offset(void) {
    const char *doc = "2024-01-15T12:30:45+05:30";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_DATETIMETZ);
    EXPECT(tok.len  == 25);
}

static void test_next_datetimetz_neg_offset(void) {
    const char *doc = "2024-01-15T12:30:45-08:00";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_DATETIMETZ);
    EXPECT(tok.len  == 25);
}

static void test_next_datetimetz_frac_and_z(void) {
    const char *doc = "2024-01-15T12:30:45.123Z";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_DATETIMETZ);
    EXPECT(tok.len  == 24);
}

static void test_next_date_followed_by_space_and_comment(void) {
    const char *doc = "2024-01-15 # comment";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_DATE);
    EXPECT(tok.len  == 10);  // YYYY-MM-DD only, space not consumed
    EXPECT(!lexer_has_error(&lex));
}

static void test_next_date_followed_by_space_and_newline(void) {
    const char *doc = "2024-01-15 \n";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_DATE);
    EXPECT(!lexer_has_error(&lex));
}

static void test_next_datetime_T_invalid_time_is_error(void) {
    const char *doc = "2024-01-15Tgarbage";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_ERROR);
    EXPECT(lexer_has_error(&lex));
}

static void test_next_numeral_not_confused_for_date(void) {
    const char *doc = "1234";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_S64_DEC);
    EXPECT(tok.len  == 4);
}

static void test_next_numeral_not_confused_for_time(void) {
    const char *doc = "12";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_S64_DEC);
    EXPECT(tok.len  == 2);
}


// lexer_next(): numbers

static void test_next_dec_int_simple(void) {
    lexer_s lex = lexer_init("42", 2);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_S64_DEC);
    EXPECT(tok.len  == 2);
}

static void test_next_dec_int_positive(void) {
    const char *doc = "+99";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_S64_DEC);
    EXPECT(tok.len  == 3);
}

static void test_next_dec_int_negative(void) {
    const char *doc = "-17";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_S64_DEC);
    EXPECT(tok.len  == 3);
}

static void test_next_dec_int_with_underscore(void) {
    const char *doc = "1_000_000";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_S64_DEC);
    EXPECT(tok.len  == 9);
}

static void test_next_dec_int_zero(void) {
    lexer_s lex = lexer_init("0", 1);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_S64_DEC);
    EXPECT(tok.len  == 1);
}

static void test_next_hex_int(void) {
    const char *doc = "0xFF";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_S64_HEX);
    EXPECT(tok.len  == 4);
}

static void test_next_hex_int_with_underscore(void) {
    const char *doc = "0xDEAD_BEEF";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_S64_HEX);
    EXPECT(tok.len  == 11);
}

static void test_next_oct_int(void) {
    const char *doc = "0o77";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_S64_OCT);
    EXPECT(tok.len  == 4);
}

static void test_next_bin_int(void) {
    const char *doc = "0b1010";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_S64_BIN);
    EXPECT(tok.len  == 6);
}

static void test_next_float_simple(void) {
    const char *doc = "3.14";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_F64);
    EXPECT(tok.len  == 4);
}

static void test_next_float_exponent(void) {
    const char *doc = "6.626e-34";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_F64);
    EXPECT(tok.len  == 9);
}

static void test_next_float_frac_and_exp(void) {
    const char *doc = "3.14e5";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_F64);
    EXPECT(tok.len  == 6);
}

static void test_next_float_no_frac(void) {
    const char *doc = "1e10";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_F64);
    EXPECT(tok.len  == 4);
}

static void test_next_float_negative(void) {
    const char *doc = "-0.5";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_F64);
    EXPECT(tok.len  == 4);
}

static void test_next_pos_inf(void) {
    const char *doc = "inf";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_F64);
    EXPECT(tok.len  == 3);
}

static void test_next_explicit_pos_inf(void) {
    const char *doc = "+inf";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_F64);
    EXPECT(tok.len  == 4);
}

static void test_next_neg_inf(void) {
    const char *doc = "-inf";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_F64);
    EXPECT(tok.len  == 4);
}

static void test_next_nan(void) {
    const char *doc = "nan";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_F64);
    EXPECT(tok.len  == 3);
}

static void test_next_pos_nan(void) {
    const char *doc = "+nan";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_F64);
    EXPECT(tok.len  == 4);
}

static void test_next_neg_nan(void) {
    const char *doc = "-nan";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_F64);
    EXPECT(tok.len  == 4);
}

static void test_next_dash_bare_key_not_neg_int(void) {
    // '-' followed by non-digit stays a bare key
    const char *doc = "-foo";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_BARE_KEY);
    EXPECT(tok.len  == 4);
}


// lexer_next(): whitespace and comment skipping

static void test_next_skips_leading_spaces(void) {
    lexer_s lex = lexer_init("   [", 4);

    token_s tok = lexer_next(&lex);

    EXPECT(tok.type == TOKEN_LBRACKET);
}

static void test_next_skips_leading_tabs(void) {
    lexer_s lex = lexer_init("\t\t=", 3);

    token_s tok = lexer_next(&lex);

    EXPECT(tok.type == TOKEN_EQUALS);
}

static void test_next_skips_comment_to_end_of_row(void) {
    lexer_s lex = lexer_init("# comment\n=", 11);

    token_s tok = lexer_next(&lex);

    // Comment stops before \n; next token is the newline
    EXPECT(tok.type == TOKEN_NEWLINE);
}

static void test_next_skips_inline_comment_after_whitespace(void) {
    lexer_s lex = lexer_init("  # comment\n[", 13);

    token_s tok = lexer_next(&lex);

    EXPECT(tok.type == TOKEN_NEWLINE);
}

static void test_next_comment_at_eof_returns_eof(void) {
    lexer_s lex = lexer_init("# no newline", 12);

    token_s tok = lexer_next(&lex);

    EXPECT(tok.type == TOKEN_EOF);
}

static void test_next_comment_tab_is_allowed(void) {
    // Tab (U+0009) is the one control character permitted in comments
    lexer_s lex = lexer_init("#\t tab\n=", 8);

    token_s tok = lexer_next(&lex);

    EXPECT(tok.type == TOKEN_NEWLINE);
    EXPECT(!lexer_has_error(&lex));
}

static void test_next_comment_control_char_is_error(void) {
    // U+0001 is a forbidden control character inside a comment
    lexer_s lex = lexer_init("# bad\x01 char\n", 12);

    token_s tok = lexer_next(&lex);

    EXPECT(tok.type == TOKEN_ERROR);
    EXPECT(lexer_has_error(&lex));
}

static void test_next_comment_del_is_error(void) {
    // U+007F DEL is also forbidden in comments
    lexer_s lex = lexer_init("#\x7F\n", 3);

    token_s tok = lexer_next(&lex);

    EXPECT(tok.type == TOKEN_ERROR);
    EXPECT(lexer_has_error(&lex));
}

static void test_next_comment_crlf_is_not_error(void) {
    // \r before \n terminates the comment without triggering control-char error
    lexer_s lex = lexer_init("# text\r\n=", 9);

    token_s tok = lexer_next(&lex);

    EXPECT(tok.type == TOKEN_NEWLINE);
    EXPECT(!lexer_has_error(&lex));
}

static void test_next_bom_at_start_is_skipped(void) {
    // UTF-8 BOM (0xEF 0xBB 0xBF) at position 0 must be consumed silently
    const char *doc = "\xEF\xBB\xBF" "key";
    lexer_s lex = lexer_init(doc, strlen(doc));

    token_s tok = lexer_next(&lex);

    EXPECT(tok.type == TOKEN_BARE_KEY);
    EXPECT(tok.len  == 3);
}

static void test_next_bom_not_at_start_is_bare_key(void) {
    // BOM mid-document is just a Unicode bare key codepoint
    const char *doc = "a\xEF\xBB\xBF" "b";
    lexer_s lex = lexer_init(doc, strlen(doc));

    token_s tok = lexer_next(&lex);

    EXPECT(tok.type == TOKEN_BARE_KEY);
    EXPECT(tok.len  == 5);  // 'a' + 3 BOM bytes + 'b'
}

// lexer_next(): single-char tokens

static void test_next_emits_lbracket(void) {
    lexer_s lex = lexer_init("[", 1);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_LBRACKET);
    EXPECT(tok.len  == 1);
}

static void test_next_emits_rbracket(void) {
    lexer_s lex = lexer_init("]", 1);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_RBRACKET);
    EXPECT(tok.len  == 1);
}

static void test_next_emits_lbrace(void) {
    lexer_s lex = lexer_init("{", 1);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_LBRACE);
    EXPECT(tok.len  == 1);
}

static void test_next_emits_rbrace(void) {
    lexer_s lex = lexer_init("}", 1);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_RBRACE);
    EXPECT(tok.len  == 1);
}

static void test_next_emits_equals(void) {
    lexer_s lex = lexer_init("=", 1);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_EQUALS);
    EXPECT(tok.len  == 1);
}

static void test_next_emits_dot(void) {
    lexer_s lex = lexer_init(".", 1);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_DOT);
    EXPECT(tok.len  == 1);
}

static void test_next_emits_comma(void) {
    lexer_s lex = lexer_init(",", 1);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_COMMA);
    EXPECT(tok.len  == 1);
}

static void test_next_emits_newline_lf(void) {
    lexer_s lex = lexer_init("\n", 1);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_NEWLINE);
    EXPECT(tok.len  == 1);
}

static void test_next_emits_newline_crlf(void) {
    lexer_s lex = lexer_init("\r\n", 2);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_NEWLINE);
    EXPECT(tok.len  == 2);
}

static void test_next_bare_cr_is_error(void) {
    lexer_s lex = lexer_init("\r", 1);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_ERROR);
    EXPECT(lexer_has_error(&lex));
}

static void test_next_emits_eof_on_empty(void) {
    lexer_s lex = lexer_init("", 0);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_EOF);
}

static void test_next_emits_eof_after_all_tokens(void) {
    lexer_s lex = lexer_init("[", 1);
    lexer_next(&lex);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_EOF);
}

static void test_next_unknown_char_is_error(void) {
    lexer_s lex = lexer_init("@", 1);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_ERROR);
    EXPECT(lexer_has_error(&lex));
}

static void test_next_span_and_ptr_correct_for_single_char(void) {
    lexer_s lex = lexer_init("  =", 3);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type          == TOKEN_EQUALS);
    EXPECT(tok.span.head.col == 3);
    EXPECT(tok.ptr           == lex.doc + 2);
    EXPECT(tok.len           == 1);
}

static void test_next_sequence_bracket_equals_bracket(void) {
    lexer_s lex = lexer_init("[=]", 3);
    EXPECT(lexer_next(&lex).type == TOKEN_LBRACKET);
    EXPECT(lexer_next(&lex).type == TOKEN_EQUALS);
    EXPECT(lexer_next(&lex).type == TOKEN_RBRACKET);
    EXPECT(lexer_next(&lex).type == TOKEN_EOF);
}

static void test_next_returns_eof_after_error(void) {
    lexer_s lex = lexer_init("!", 1);
    token_pos_s head = lex.cur;
    lexer_emit_error(&lex, head, "Unexpected character");

    token_s tok = lexer_next(&lex);

    EXPECT(tok.type == TOKEN_EOF);
}

static void test_next_null_lexer_returns_eof(void) {
    token_s tok = lexer_next(NULL);

    EXPECT(tok.type == TOKEN_EOF);
}


// lexer_next(): strings

static void test_next_str_basic_empty(void) {
    lexer_s lex = lexer_init("\"\"", 2);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_STR_BASIC);
    EXPECT(tok.len  == 2);
    EXPECT(tok.ptr  == lex.doc);
}

static void test_next_str_basic_simple(void) {
    lexer_s lex = lexer_init("\"hello\"", 7);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_STR_BASIC);
    EXPECT(tok.len  == 7);
    EXPECT(tok.ptr  == lex.doc);
}

static void test_next_str_basic_escaped_quote(void) {
    // "a\"b" — 6 bytes: " a \ " b "
    lexer_s lex = lexer_init("\"a\\\"b\"", 6);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_STR_BASIC);
    EXPECT(tok.len  == 6);
}

static void test_next_str_basic_escaped_backslash(void) {
    // "a\\b" — 6 bytes: " a \ \ b "
    lexer_s lex = lexer_init("\"a\\\\b\"", 6);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_STR_BASIC);
    EXPECT(tok.len  == 6);
}

static void test_next_str_basic_unterminated_eof(void) {
    lexer_s lex = lexer_init("\"hello", 6);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_ERROR);
    EXPECT(lexer_has_error(&lex));
}

static void test_next_str_basic_unterminated_newline(void) {
    lexer_s lex = lexer_init("\"hello\n", 7);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_ERROR);
    EXPECT(lexer_has_error(&lex));
}

static void test_next_str_basic_ml_empty(void) {
    lexer_s lex = lexer_init("\"\"\"\"\"\"", 6);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_STR_BASIC_ML);
    EXPECT(tok.len  == 6);
    EXPECT(tok.ptr  == lex.doc);
}

static void test_next_str_basic_ml_simple(void) {
    lexer_s lex = lexer_init("\"\"\"hello\"\"\"", 11);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_STR_BASIC_ML);
    EXPECT(tok.len  == 11);
}

static void test_next_str_basic_ml_with_newline(void) {
    const char *doc = "\"\"\"hello\nworld\"\"\"";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_STR_BASIC_ML);
    EXPECT(tok.len  == strlen(doc));
}

static void test_next_str_basic_ml_trims_leading_lf(void) {
    // Opening """ immediately followed by \n, newline is trimmed (consumed)
    const char *doc = "\"\"\"\nhello\"\"\"";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_STR_BASIC_ML);
    EXPECT(tok.len  == strlen(doc));
    EXPECT(tok.ptr  == lex.doc);
}

static void test_next_str_basic_ml_trims_leading_crlf(void) {
    const char *doc = "\"\"\"\r\nhello\"\"\"";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_STR_BASIC_ML);
    EXPECT(tok.len  == strlen(doc));
}

static void test_next_str_basic_ml_embedded_quote_mid(void) {
    // """" a """: one quote mid-body, not at close
    const char *doc = "\"\"\"\"a\"\"\"";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_STR_BASIC_ML);
    EXPECT(tok.len  == strlen(doc));
}

static void test_next_str_basic_ml_two_embedded_quotes_mid(void) {
    // """""a""": two quotes mid-body
    const char *doc = "\"\"\"\"\"a\"\"\"";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_STR_BASIC_ML);
    EXPECT(tok.len  == strlen(doc));
}

static void test_next_str_basic_ml_four_quotes_at_close(void) {
    // """hello"""": one embedded quote right at the close (n=4: consume 1 content + 3 close)
    const char *doc = "\"\"\"hello\"\"\"\"";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_STR_BASIC_ML);
    EXPECT(tok.len  == strlen(doc));
}

static void test_next_str_basic_ml_five_quotes_at_close(void) {
    // """hello""""": two embedded quotes right at the close (n=5: consume 2 content + 3 close)
    const char *doc = "\"\"\"hello\"\"\"\"\"";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_STR_BASIC_ML);
    EXPECT(tok.len  == strlen(doc));
}

static void test_next_str_basic_ml_unterminated(void) {
    lexer_s lex = lexer_init("\"\"\"hello", 8);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_ERROR);
    EXPECT(lexer_has_error(&lex));
}

static void test_next_str_literal_empty(void) {
    lexer_s lex = lexer_init("''", 2);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_STR_LITERAL);
    EXPECT(tok.len  == 2);
    EXPECT(tok.ptr  == lex.doc);
}

static void test_next_str_literal_simple(void) {
    lexer_s lex = lexer_init("'hello'", 7);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_STR_LITERAL);
    EXPECT(tok.len  == 7);
    EXPECT(tok.ptr  == lex.doc);
}

static void test_next_str_literal_no_escape_processing(void) {
    // Backslash is a literal character in literal strings
    lexer_s lex = lexer_init("'a\\nb'", 6);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_STR_LITERAL);
    EXPECT(tok.len  == 6);
}

static void test_next_str_literal_unterminated_eof(void) {
    lexer_s lex = lexer_init("'hello", 6);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_ERROR);
    EXPECT(lexer_has_error(&lex));
}

static void test_next_str_literal_unterminated_newline(void) {
    lexer_s lex = lexer_init("'hello\n", 7);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_ERROR);
    EXPECT(lexer_has_error(&lex));
}

static void test_next_str_literal_ml_empty(void) {
    lexer_s lex = lexer_init("''''''", 6);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_STR_LITERAL_ML);
    EXPECT(tok.len  == 6);
    EXPECT(tok.ptr  == lex.doc);
}

static void test_next_str_literal_ml_simple(void) {
    lexer_s lex = lexer_init("'''hello'''", 11);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_STR_LITERAL_ML);
    EXPECT(tok.len  == 11);
}

static void test_next_str_literal_ml_with_newline(void) {
    const char *doc = "'''hello\nworld'''";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_STR_LITERAL_ML);
    EXPECT(tok.len  == strlen(doc));
}

static void test_next_str_literal_ml_trims_leading_lf(void) {
    const char *doc = "'''\nhello'''";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_STR_LITERAL_ML);
    EXPECT(tok.len  == strlen(doc));
    EXPECT(tok.ptr  == lex.doc);
}

static void test_next_str_literal_ml_trims_leading_crlf(void) {
    const char *doc = "'''\r\nhello'''";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_STR_LITERAL_ML);
    EXPECT(tok.len  == strlen(doc));
}

static void test_next_str_literal_ml_two_embedded_quotes_mid(void) {
    // '''''a''': two quotes mid-body
    const char *doc = "'''''a'''";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_STR_LITERAL_ML);
    EXPECT(tok.len  == strlen(doc));
}

static void test_next_str_literal_ml_four_quotes_at_close(void) {
    // '''hello'''': one embedded quote at close (n=4: consume 1 content + 3 close)
    const char *doc = "'''hello''''";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_STR_LITERAL_ML);
    EXPECT(tok.len  == strlen(doc));
}

static void test_next_str_literal_ml_five_quotes_at_close(void) {
    // '''hello''''': two embedded quotes at close (n=5: consume 2 content + 3 close)
    const char *doc = "'''hello'''''";
    lexer_s lex = lexer_init(doc, strlen(doc));
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_STR_LITERAL_ML);
    EXPECT(tok.len  == strlen(doc));
}

static void test_next_str_literal_ml_unterminated(void) {
    lexer_s lex = lexer_init("'''hello", 8);
    token_s tok = lexer_next(&lex);
    EXPECT(tok.type == TOKEN_ERROR);
    EXPECT(lexer_has_error(&lex));
}

// lexer_peek()

static void test_peek_returns_first_char(void) {
    lexer_s lex = lexer_init("abc", 3);

    EXPECT(lexer_peek(&lex, 0) == 'a');
}

static void test_peek_does_not_advance(void) {
    lexer_s lex = lexer_init("abc", 3);

    lexer_peek(&lex, 0);
    lexer_peek(&lex, 0);

    EXPECT(lex.cur.pos == 0);
    EXPECT(lex.cur.col == 1);
}

static void test_peek_returns_nul_on_empty_input(void) {
    lexer_s lex = lexer_init("", 0);

    EXPECT(lexer_peek(&lex, 0) == '\0');
}

static void test_peek_returns_nul_at_eof(void) {
    lexer_s lex = lexer_init("a", 1);
    lex.cur.pos = 1;

    EXPECT(lexer_peek(&lex, 0) == '\0');
}

static void test_peek_with_offset_returns_nth_char(void) {
    lexer_s lex = lexer_init("abc", 3);

    EXPECT(lexer_peek(&lex, 0) == 'a');
    EXPECT(lexer_peek(&lex, 1) == 'b');
    EXPECT(lexer_peek(&lex, 2) == 'c');
}

static void test_peek_offset_past_end_returns_nul(void) {
    lexer_s lex = lexer_init("ab", 2);

    EXPECT(lexer_peek(&lex, 2) == '\0');
    EXPECT(lexer_peek(&lex, 99) == '\0');
}

static void test_peek_null_lexer_returns_nul(void) {
    EXPECT(lexer_peek(NULL, 0) == '\0');
}

// lexer_see_valid_utf8()

static void test_utf8_valid_empty(void) {
    EXPECT(lexer_see_valid_utf8("", 0));
}

static void test_utf8_valid_ascii(void) {
    EXPECT(lexer_see_valid_utf8("hello = 1\n", 10));
}

static void test_utf8_valid_2byte(void) {
    // U+00E9 LATIN SMALL LETTER E WITH ACUTE: 0xC3 0xA9
    EXPECT(lexer_see_valid_utf8("\xC3\xA9", 2));
}

static void test_utf8_valid_3byte(void) {
    // U+4E2D CJK UNIFIED IDEOGRAPH: 0xE4 0xB8 0xAD
    EXPECT(lexer_see_valid_utf8("\xE4\xB8\xAD", 3));
}

static void test_utf8_valid_4byte(void) {
    // U+1F600 GRINNING FACE: 0xF0 0x9F 0x98 0x80
    EXPECT(lexer_see_valid_utf8("\xF0\x9F\x98\x80", 4));
}

static void test_utf8_rejects_null_byte(void) {
    EXPECT(!lexer_see_valid_utf8("\x00", 1));
}

static void test_utf8_rejects_null_byte_embedded(void) {
    EXPECT(!lexer_see_valid_utf8("abc\x00" "def", 7));
}

static void test_utf8_rejects_bare_continuation(void) {
    // 0x80 is a continuation byte and invalid as a sequence start
    EXPECT(!lexer_see_valid_utf8("\x80", 1));
}

static void test_utf8_rejects_overlong_2byte(void) {
    // 0xC1 0x80 encodes U+0040 as a 2-byte sequence (overlong)
    EXPECT(!lexer_see_valid_utf8("\xC1\x80", 2));
}

static void test_utf8_rejects_overlong_3byte(void) {
    // 0xE0 0x80 0x80 encodes U+0000 as a 3-byte sequence (overlong)
    EXPECT(!lexer_see_valid_utf8("\xE0\x80\x80", 3));
}

static void test_utf8_rejects_surrogate(void) {
    // U+D800: 0xED 0xA0 0x80 — high surrogate, forbidden in UTF-8
    EXPECT(!lexer_see_valid_utf8("\xED\xA0\x80", 3));
}

static void test_utf8_rejects_above_u10ffff(void) {
    // 0xF4 0x90 0x80 0x80 encodes U+110000, one above the maximum
    EXPECT(!lexer_see_valid_utf8("\xF4\x90\x80\x80", 4));
}

static void test_utf8_rejects_invalid_lead_f5(void) {
    EXPECT(!lexer_see_valid_utf8("\xF5\x80\x80\x80", 4));
}

static void test_utf8_rejects_truncated_2byte(void) {
    EXPECT(!lexer_see_valid_utf8("\xC3", 1));
}

static void test_utf8_rejects_truncated_3byte(void) {
    EXPECT(!lexer_see_valid_utf8("\xE4\xB8", 2));
}

static void test_utf8_rejects_truncated_4byte(void) {
    EXPECT(!lexer_see_valid_utf8("\xF0\x9F\x98", 3));
}

static void test_utf8_null_doc_zero_len(void) {
    EXPECT(lexer_see_valid_utf8(NULL, 0));
}

static void test_utf8_null_doc_nonzero_len(void) {
    EXPECT(!lexer_see_valid_utf8(NULL, 1));
}


static void test_peek_offset_overflow_returns_nul(void) {
    // cur.pos near SIZE_MAX: (SIZE_MAX - 1) + 2 wraps to 1 under the old
    // addition-based check, which would pass the bounds test and read garbage.
    // The subtraction-based check catches this via cur.pos >= doc_len first.
    lexer_s lex = lexer_init("abc", 3);
    lex.cur.pos = SIZE_MAX - 1;
    EXPECT(lexer_peek(&lex, 2) == '\0');
}


int main(void) {
    test_emit_error_sets_has_err();
    test_emit_error_sets_msg();
    test_emit_error_returns_token_error();
    test_emit_error_span_head_matches_arg();
    test_emit_error_null_lexer_returns_eof();

    test_emit_token_sets_type();
    test_emit_token_sets_ptr_and_len();
    test_emit_token_span_head_is_supplied_position();
    test_emit_token_span_tail_is_current_cursor();
    test_emit_token_null_lexer_returns_eof();

    test_err_print_silent_when_no_error();
    test_err_print_silent_on_null();
    test_err_print_outputs_error();

    test_has_error_false_on_fresh_lexer();
    test_has_error_true_after_error_set();
    test_has_error_null_lexer_returns_false();

    test_init_sets_src_and_len();
    test_init_cursor_starts_at_beginning();
    test_init_has_no_error();
    test_init_empty_src();
    test_init_null_src_clamps_len_to_zero();
    test_init_null_src_peek_returns_nul();

    test_move_advances_pos();
    test_move_increments_col();
    test_move_by_multiple_increments_col();
    test_move_on_lf_increments_row_resets_col();
    test_move_by_two_on_crlf_increments_row_once();
    test_move_peek_after_move_returns_next_char();
    test_move_peek_after_move_to_eof_returns_nul();
    test_move_clamps_at_eof();
    test_move_by_zero_is_noop();
    test_move_null_lexer_is_noop();

    test_next_bool_true();
    test_next_bool_false();
    test_next_bool_uppercase_is_bare_key();
    test_next_bare_key_simple();
    test_next_bare_key_with_dash();
    test_next_bare_key_with_underscore();
    test_next_bare_key_true_prefix();
    test_next_bare_key_false_prefix();
    test_next_bare_key_then_equals();
    test_next_bare_key_latin_extended();
    test_next_bare_key_cjk();
    test_next_bare_key_mixed_ascii_unicode();
    test_next_bare_key_4byte_codepoint();
    test_next_bare_key_unicode_col_counts_codepoints();

    test_next_date();
    test_next_time();
    test_next_time_with_frac();
    test_next_time_no_seconds();
    test_next_datetime_no_seconds();
    test_next_datetimetz_no_seconds_z();
    test_next_datetime();
    test_next_datetime_space_sep();
    test_next_datetime_with_frac();
    test_next_datetimetz_z();
    test_next_datetimetz_pos_offset();
    test_next_datetimetz_neg_offset();
    test_next_datetimetz_frac_and_z();
    test_next_date_followed_by_space_and_comment();
    test_next_date_followed_by_space_and_newline();
    test_next_datetime_T_invalid_time_is_error();
    test_next_numeral_not_confused_for_date();
    test_next_numeral_not_confused_for_time();

    test_next_dec_int_simple();
    test_next_dec_int_positive();
    test_next_dec_int_negative();
    test_next_dec_int_with_underscore();
    test_next_dec_int_zero();
    test_next_hex_int();
    test_next_hex_int_with_underscore();
    test_next_oct_int();
    test_next_bin_int();
    test_next_float_simple();
    test_next_float_exponent();
    test_next_float_frac_and_exp();
    test_next_float_no_frac();
    test_next_float_negative();
    test_next_pos_inf();
    test_next_explicit_pos_inf();
    test_next_neg_inf();
    test_next_nan();
    test_next_pos_nan();
    test_next_neg_nan();
    test_next_dash_bare_key_not_neg_int();

    test_next_skips_leading_spaces();
    test_next_skips_leading_tabs();
    test_next_skips_comment_to_end_of_row();
    test_next_skips_inline_comment_after_whitespace();
    test_next_comment_at_eof_returns_eof();
    test_next_comment_tab_is_allowed();
    test_next_comment_control_char_is_error();
    test_next_comment_del_is_error();
    test_next_comment_crlf_is_not_error();
    test_next_bom_at_start_is_skipped();
    test_next_bom_not_at_start_is_bare_key();
    test_next_emits_lbracket();
    test_next_emits_rbracket();
    test_next_emits_lbrace();
    test_next_emits_rbrace();
    test_next_emits_equals();
    test_next_emits_dot();
    test_next_emits_comma();
    test_next_emits_newline_lf();
    test_next_emits_newline_crlf();
    test_next_bare_cr_is_error();
    test_next_emits_eof_on_empty();
    test_next_emits_eof_after_all_tokens();
    test_next_unknown_char_is_error();
    test_next_span_and_ptr_correct_for_single_char();
    test_next_sequence_bracket_equals_bracket();
    test_next_returns_eof_after_error();
    test_next_null_lexer_returns_eof();

    test_next_str_basic_empty();
    test_next_str_basic_simple();
    test_next_str_basic_escaped_quote();
    test_next_str_basic_escaped_backslash();
    test_next_str_basic_unterminated_eof();
    test_next_str_basic_unterminated_newline();
    test_next_str_basic_ml_empty();
    test_next_str_basic_ml_simple();
    test_next_str_basic_ml_with_newline();
    test_next_str_basic_ml_trims_leading_lf();
    test_next_str_basic_ml_trims_leading_crlf();
    test_next_str_basic_ml_embedded_quote_mid();
    test_next_str_basic_ml_two_embedded_quotes_mid();
    test_next_str_basic_ml_four_quotes_at_close();
    test_next_str_basic_ml_five_quotes_at_close();
    test_next_str_basic_ml_unterminated();
    test_next_str_literal_empty();
    test_next_str_literal_simple();
    test_next_str_literal_no_escape_processing();
    test_next_str_literal_unterminated_eof();
    test_next_str_literal_unterminated_newline();
    test_next_str_literal_ml_empty();
    test_next_str_literal_ml_simple();
    test_next_str_literal_ml_with_newline();
    test_next_str_literal_ml_trims_leading_lf();
    test_next_str_literal_ml_trims_leading_crlf();
    test_next_str_literal_ml_two_embedded_quotes_mid();
    test_next_str_literal_ml_four_quotes_at_close();
    test_next_str_literal_ml_five_quotes_at_close();
    test_next_str_literal_ml_unterminated();

    test_utf8_valid_empty();
    test_utf8_valid_ascii();
    test_utf8_valid_2byte();
    test_utf8_valid_3byte();
    test_utf8_valid_4byte();
    test_utf8_rejects_null_byte();
    test_utf8_rejects_null_byte_embedded();
    test_utf8_rejects_bare_continuation();
    test_utf8_rejects_overlong_2byte();
    test_utf8_rejects_overlong_3byte();
    test_utf8_rejects_surrogate();
    test_utf8_rejects_above_u10ffff();
    test_utf8_rejects_invalid_lead_f5();
    test_utf8_rejects_truncated_2byte();
    test_utf8_rejects_truncated_3byte();
    test_utf8_rejects_truncated_4byte();
    test_utf8_null_doc_zero_len();
    test_utf8_null_doc_nonzero_len();

    test_peek_returns_first_char();
    test_peek_does_not_advance();
    test_peek_returns_nul_on_empty_input();
    test_peek_returns_nul_at_eof();
    test_peek_with_offset_returns_nth_char();
    test_peek_offset_past_end_returns_nul();
    test_peek_null_lexer_returns_nul();
    test_peek_offset_overflow_returns_nul();

    printf("%d/%d tests passed.\n", total - fails, total);

    return fails == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
