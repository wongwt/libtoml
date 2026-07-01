//! @copyright  Copyright 2026 Wei-Te Wong. MIT Licensed.
#include "toml.h"

#include <limits.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>


#ifndef TOML_MALLOC
#define TOML_MALLOC malloc
#endif
#ifndef TOML_REALLOC
#define TOML_REALLOC realloc
#endif
#ifndef TOML_FREE
#define TOML_FREE free
#endif


// §1  Arena-based Memory Management

#define TOML_MIN(a, b) ((a) < (b) ? (a) : (b))
#define TOML_MAX(a, b) ((a) > (b) ? (a) : (b))

#define ARENA_ALIGN_SIZE 8u
#define ARENA_CHUNK_SIZE 4096u

typedef struct chunk_s {
    struct chunk_s *next;
    size_t capacity;
    size_t used;
    char data[];
} chunk_s;

typedef struct arena_s {
    chunk_s *head;
    size_t chunk_size;
} arena_s;

static void arena_init(arena_s *arena, size_t chunk_size) {
    if (arena == NULL) {
        return;
    }

    arena->head = NULL;
    arena->chunk_size = chunk_size == 0 ? ARENA_CHUNK_SIZE : chunk_size;
}

static size_t arena_align_size(size_t size) {
    if (size > SIZE_MAX - (ARENA_ALIGN_SIZE - 1)) {
        return SIZE_MAX;
    }

    return (size + (ARENA_ALIGN_SIZE - 1)) & ~(ARENA_ALIGN_SIZE - 1);
}

static bool arena_has_space(arena_s *arena, size_t aligned_size) {
    if (arena == NULL || arena->head == NULL) {
        return false;
    }

    return arena->head->capacity - arena->head->used >= aligned_size;
}

static chunk_s *arena_new_chunk(size_t capacity) {
    chunk_s *chunk = TOML_MALLOC(sizeof(chunk_s) + capacity);
    if (chunk == NULL) {
        return NULL;
    }
    chunk->next = NULL;
    chunk->capacity = capacity;
    chunk->used = 0;

    return chunk;
}

static bool arena_expand(arena_s *arena, size_t aligned_size) {
    // Clamp before doubling so aligned_size * 2 can never wrap around
    size_t doubled  = TOML_MIN(aligned_size, SIZE_MAX / 2) * 2;
    size_t capacity = TOML_MAX(doubled, arena->chunk_size);
    if (capacity > SIZE_MAX - sizeof(chunk_s)) {
        return false;
    }

    chunk_s *chunk = arena_new_chunk(capacity);
    if (chunk == NULL) {
        return false;
    }

    chunk->next = arena->head;
    arena->head = chunk;
    arena->chunk_size = TOML_MIN(capacity, SIZE_MAX / 2) * 2;

    return true;
}

static bool arena_ensure_capacity(arena_s *arena, size_t aligned_size) {
    if (arena_has_space(arena, aligned_size)) {
        return true;
    }

    return arena_expand(arena, aligned_size);
}

static void *arena_alloc(arena_s *arena, size_t size) {
    if (arena == NULL) {
        return NULL;
    }

    size_t aligned_size = arena_align_size(size);
    if (aligned_size == SIZE_MAX) {
        return NULL;
    }

    if (!arena_ensure_capacity(arena, aligned_size)) {
        return NULL;
    }

    chunk_s *chunk = arena->head;
    void *memory = chunk->data + chunk->used;
    chunk->used += aligned_size;

    return memory;
}

static char *arena_strdup(arena_s *arena, const char *str, size_t str_len) {
    if (arena == NULL || str == NULL) {
        return NULL;
    }

    if (str_len > SIZE_MAX - 1) {
        return NULL;
    }

    char *dup_str = arena_alloc(arena, str_len + 1);
    if (dup_str == NULL) {
        return NULL;
    }
    memcpy(dup_str, str, str_len);
    dup_str[str_len] = '\0';

    return dup_str;
}

static void arena_free(arena_s *arena) {
    if (arena == NULL) {
        return;
    }

    chunk_s *chunk = arena->head;
    while (chunk != NULL) {
        chunk_s *next = chunk->next;
        TOML_FREE(chunk);
        chunk = next;
    }
    arena->head = NULL;
}


// §2  Lexer

typedef enum {
    TOKEN_EOF = 0,

    // Strings
    TOKEN_STR_BASIC,          // "..."
    TOKEN_STR_LITERAL,        // '...'
    TOKEN_STR_BASIC_ML,       // """..."""
    TOKEN_STR_LITERAL_ML,     // '''...'''

    // Integers
    TOKEN_S64_DEC,            // 42, 1_000
    TOKEN_S64_HEX,            // 0xFF
    TOKEN_S64_OCT,            // 0o77
    TOKEN_S64_BIN,            // 0b1010

    // Floats
    TOKEN_F64,                // 3.14, inf, +inf, -inf, nan, +nan, -nan

    // Others
    TOKEN_BOOL_TRUE,          // true
    TOKEN_BOOL_FALSE,         // false
    TOKEN_DATE,               // 2024-01-01
    TOKEN_TIME,               // 12:00:00
    TOKEN_DATETIME,           // 2024-01-01T12:00:00
    TOKEN_DATETIMETZ,         // 2024-01-01T12:00:00+08:00

    // Structural
    TOKEN_BARE_KEY,
    TOKEN_LBRACKET,           // [
    TOKEN_RBRACKET,           // ]
    TOKEN_LBRACE,             // {
    TOKEN_RBRACE,             // }
    TOKEN_EQUALS,             // =
    TOKEN_DOT,                // .
    TOKEN_COMMA,              // ,
    TOKEN_NEWLINE,            // \n or \r\n

    TOKEN_ERROR,
} token_type_e;

typedef struct {
    size_t pos;
    size_t row;
    size_t col;
} token_pos_s;

typedef struct {
    token_pos_s head;
    token_pos_s tail;
} token_span_s;

typedef struct {
    token_type_e type;
    token_span_s span;
    const char *ptr;
    size_t len;
} token_s;

typedef struct {
    const char *msg;
    token_span_s span;
} lexer_err_s;

typedef struct {
    const char *doc;
    size_t doc_len;
    token_pos_s cur;
    bool has_err;
    lexer_err_s err;
} lexer_s;

static bool lexer_has_error(const lexer_s *lexer) {
    if (lexer == NULL) {
        return false;
    }

    return lexer->has_err;
}

static void lexer_err_print(const lexer_s *lexer) {
    if (lexer == NULL || !lexer->has_err) {
        return;
    }

    fprintf(stderr, "[TOML::lexer] %zu:%zu: %s\n",
            lexer->err.span.head.row,
            lexer->err.span.head.col,
            lexer->err.msg);
}

static lexer_s lexer_init(const char *doc, size_t doc_len) {
    return (lexer_s){
        .doc     = doc,
        .doc_len = doc != NULL ? doc_len : 0,
        .cur     = {
            .pos = 0,
            .row = 1,
            .col = 1,
        },
    };
}

static char lexer_peek(const lexer_s *lexer, size_t offset) {
    if (lexer == NULL ||
        lexer->cur.pos >= lexer->doc_len ||
        offset >= lexer->doc_len - lexer->cur.pos) {
        return '\0';
    }

    return lexer->doc[lexer->cur.pos + offset];
}

static void lexer_move(lexer_s *lexer, size_t offset) {
    if (lexer == NULL) {
        return;
    }

    for (size_t i = 0; i < offset && lexer->cur.pos < lexer->doc_len; i++, lexer->cur.pos++) {
        if (lexer->doc[lexer->cur.pos] == '\n') {
            lexer->cur.row++;
            lexer->cur.col = 1;
        } else {
            lexer->cur.col++;
        }
    }
}

static void lexer_move_codepoint(lexer_s *lexer, size_t seq_len) {
    if (lexer == NULL || seq_len == 0) {
        return;
    }

    // Lead byte counts as one column unit
    if (lexer->cur.pos < lexer->doc_len) {
        lexer->cur.pos++;
        lexer->cur.col++;
    }

    // Continuous bytes advance position only
    for (size_t i = 1; i < seq_len && lexer->cur.pos < lexer->doc_len; i++) {
        lexer->cur.pos++;
    }
}

static token_s lexer_emit_token(const lexer_s *lexer, token_pos_s head, token_type_e type) {
    if (lexer == NULL) {
        return (token_s){0};
    }

    return (token_s){
        .type = type,
        .span = {
            .head = head,
            .tail = lexer->cur,
        },
        .ptr = lexer->doc + head.pos,
        .len = lexer->cur.pos - head.pos,
    };
}

static token_s lexer_emit_error(lexer_s *lexer, token_pos_s head, const char *msg) {
    if (lexer == NULL) {
        return (token_s){0};
    }

    lexer->has_err = true;
    lexer->err = (lexer_err_s){
        .msg  = msg,
        .span = {
            .head = head,
            .tail = lexer->cur,
        },
    };

    return lexer_emit_token(lexer, head, TOKEN_ERROR);
}

static void lexer_skip_utf8_bom(lexer_s *lexer) {
    if (lexer == NULL || lexer->cur.pos != 0) {
        return;
    }

    if (lexer_peek(lexer, 0) == '\xEF' &&
        lexer_peek(lexer, 1) == '\xBB' &&
        lexer_peek(lexer, 2) == '\xBF') {
        lexer_move(lexer, 3);
    }
}

static bool is_comment_char(char c) {
    unsigned char b = (unsigned char)c;
    return !((b <= 0x08) || (b >= 0x0B && b <= 0x1F) || b == 0x7F);
}

static void lexer_skip_comment(lexer_s *lexer) {
    while (lexer_peek(lexer, 0) != '\n' &&
           lexer_peek(lexer, 0) != '\r' &&
           lexer_peek(lexer, 0) != '\0') {
        token_pos_s err_head = lexer->cur;
        char c = lexer_peek(lexer, 0);
        lexer_move(lexer, 1);
        if (!is_comment_char(c)) {
            lexer_emit_error(lexer, err_head, "Invalid comment character");
            return;
        }
    }
}

static token_pos_s lexer_skip_trivia(lexer_s *lexer) {
    if (lexer == NULL) {
        return (token_pos_s){0};
    }

    lexer_skip_utf8_bom(lexer);

    for (;;) {
        switch (lexer_peek(lexer, 0)) {
            case ' ':
            case '\t':
                lexer_move(lexer, 1);
                break;
            case '#':
                lexer_move(lexer, 1);
                lexer_skip_comment(lexer);
                if (lexer_has_error(lexer)) {
                    return lexer->cur;
                }
                break;
            default:
                return lexer->cur;
        }
    }
}

static token_s lexer_scan_str(lexer_s *lexer, token_pos_s head, char q) {
    if (lexer == NULL) {
        return (token_s){0};
    }

    token_type_e sl = (q == '"') ? TOKEN_STR_BASIC    : TOKEN_STR_LITERAL;
    token_type_e ml = (q == '"') ? TOKEN_STR_BASIC_ML : TOKEN_STR_LITERAL_ML;

    size_t quote = 1 + (lexer_peek(lexer, 1) == q) + (lexer_peek(lexer, 2) == q);
    if (quote == 2) {
        lexer_move(lexer, 2);
        return lexer_emit_token(lexer, head, sl);
    }

    token_type_e type = (quote == 3) ? ml : sl;
    if (type == ml) {
        if (lexer_peek(lexer, 3) == '\n') {
            lexer_move(lexer, 1);
        }
        else if (lexer_peek(lexer, 3) == '\r' && lexer_peek(lexer, 4) == '\n') {
            lexer_move(lexer, 2);
        }
    }

    lexer_move(lexer, quote);

    for (;;) {
        char c = lexer_peek(lexer, 0);
        if (c == '\0' || (quote == 1 && c == '\n')) {
            return lexer_emit_error(lexer, head, "Unterminated string");
        }
        if (c == q) {
            size_t n = 0;
            while (lexer_peek(lexer, n) == q) n++;
            if (n >= quote) {
                lexer_move(lexer, n);
                return lexer_emit_token(lexer, head, type);
            }
            lexer_move(lexer, n);
        } else if (q == '"' && c == '\\') {
            lexer_move(lexer, 1);
            if (lexer_peek(lexer, 0) != '\0') {
                lexer_move(lexer, 1);
            }
        } else {
            lexer_move(lexer, 1);
        }
    }
}

static bool is_bare_key_char(char c) {
    return (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '-';
}

static size_t lexer_utf8_seq_len(unsigned char b) {
    if (b <= 0x7F)              return 1;
    if (b >= 0xC2 && b <= 0xDF) return 2;
    if (b >= 0xE0 && b <= 0xEF) return 3;
    if (b >= 0xF0 && b <= 0xF4) return 4;

    return 0;
}

static void lexer_scan_bare_key(lexer_s *lexer) {
    if (lexer == NULL) {
        return;
    }

    for (;;) {
        unsigned char b = (unsigned char)lexer_peek(lexer, 0);
        if (is_bare_key_char((char)b)) {
            lexer_move(lexer, 1);
        } else if (b >= 0xC2) {
            lexer_move_codepoint(lexer, lexer_utf8_seq_len(b));
        } else {
            break;
        }
    }
}

static token_s lexer_scan_text(lexer_s *lexer, token_pos_s head) {
    if (lexer == NULL) {
        return (token_s){0};
    }

    size_t key_start = lexer->cur.pos;
    lexer_scan_bare_key(lexer);

    size_t len = lexer->cur.pos - key_start;
    const char *ptr = lexer->doc + key_start;
    if (len == 4 && memcmp(ptr, "true",  4) == 0) {
        return lexer_emit_token(lexer, head, TOKEN_BOOL_TRUE);
    }
    if (len == 5 && memcmp(ptr, "false", 5) == 0) {
        return lexer_emit_token(lexer, head, TOKEN_BOOL_FALSE);
    }
    if (len == 3 && memcmp(ptr, "inf",   3) == 0) {
        return lexer_emit_token(lexer, head, TOKEN_F64);
    }
    if (len == 3 && memcmp(ptr, "nan",   3) == 0) {
        return lexer_emit_token(lexer, head, TOKEN_F64);
    }

    return lexer_emit_token(lexer, head, TOKEN_BARE_KEY);
}

static bool lexer_see_sign(char c) {
    return c == '+' || c == '-';
}

static bool is_dec_digit(char c) {
    return c >= '0' && c <= '9';
}

static bool is_hex_digit(char c) {
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

static bool is_oct_digit(char c) {
    return c >= '0' && c <= '7';
}

static bool is_bin_digit(char c) {
    return c == '0' || c == '1';
}

static void lexer_scan_digits(lexer_s *lexer, bool (*is_digit_fn)(char)) {
    if (lexer == NULL || is_digit_fn == NULL) {
        return;
    }

    while (is_digit_fn(lexer_peek(lexer, 0)) || lexer_peek(lexer, 0) == '_') {
        lexer_move(lexer, 1);
    }
}

static bool lexer_peek_digits(const lexer_s *lexer, size_t offset, size_t count) {
    if (lexer == NULL) {
        return false;
    }

    for (size_t i = 0; i < count; i++) {
        if (!is_dec_digit(lexer_peek(lexer, offset + i))) {
            return false;
        }
    }

    return true;
}

static void lexer_scan_subseconds(lexer_s *lexer) {
    if (lexer == NULL) {
        return;
    }

    if (lexer_peek(lexer, 0) == '.') {
        lexer_move(lexer, 1);
        while (is_dec_digit(lexer_peek(lexer, 0))) {
            lexer_move(lexer, 1);
        }
    }
}

static token_s lexer_scan_time(lexer_s *lexer, token_pos_s head) {
    if (lexer == NULL) {
        return (token_s){0};
    }

    if (!lexer_peek_digits(lexer, 0, 2) || lexer_peek(lexer, 2) != ':' ||
        !lexer_peek_digits(lexer, 3, 2)) {
        return lexer_emit_error(lexer, head, "Invalid time");
    }

    if (lexer_peek(lexer, 5) == ':' && lexer_peek_digits(lexer, 6, 2)) {
        lexer_move(lexer, 8);  // HH:MM:SS
        lexer_scan_subseconds(lexer);
    } else {
        lexer_move(lexer, 5);  // HH:MM
    }

    return lexer_emit_token(lexer, head, TOKEN_TIME);
}

static token_s lexer_scan_date(lexer_s *lexer, token_pos_s head) {
    if (lexer == NULL) {
        return (token_s){0};
    }

    if (!lexer_peek_digits(lexer, 0, 4) || lexer_peek(lexer, 4) != '-' ||
        !lexer_peek_digits(lexer, 5, 2) || lexer_peek(lexer, 7) != '-' ||
        !lexer_peek_digits(lexer, 8, 2)) {
        return lexer_emit_error(lexer, head, "Invalid date");
    }

    lexer_move(lexer, 10);  // YYYY-MM-DD

    char sep = lexer_peek(lexer, 0);
    if (sep != 'T' && sep != 't' && sep != ' ') {
        return lexer_emit_token(lexer, head, TOKEN_DATE);
    }

    if (!lexer_peek_digits(lexer, 1, 2) || lexer_peek(lexer, 3) != ':' ||
        !lexer_peek_digits(lexer, 4, 2)) {
        if (sep == ' ') {
            return lexer_emit_token(lexer, head, TOKEN_DATE);
        } else {
            return lexer_emit_error(lexer, head, "Invalid datetime");
        }
    }

    if (lexer_peek(lexer, 6) == ':' && lexer_peek_digits(lexer, 7, 2)) {
        lexer_move(lexer, 9);  // sep+HH:MM:SS
        lexer_scan_subseconds(lexer);
    } else {
        lexer_move(lexer, 6);  // sep+HH:MM
    }

    char tz = lexer_peek(lexer, 0);
    if (tz == 'Z' || tz == 'z') {
        lexer_move(lexer, 1);  // Z
        return lexer_emit_token(lexer, head, TOKEN_DATETIMETZ);
    }
    if (lexer_see_sign(tz)) {
        if (!lexer_peek_digits(lexer, 1, 2) || lexer_peek(lexer, 3) != ':' ||
            !lexer_peek_digits(lexer, 4, 2)) {
            return lexer_emit_error(lexer, head, "Invalid timezone offset");
        }
        lexer_move(lexer, 6);  // +HH:MM
        return lexer_emit_token(lexer, head, TOKEN_DATETIMETZ);
    }

    return lexer_emit_token(lexer, head, TOKEN_DATETIME);
}

static bool lexer_see_date(const lexer_s *lexer) {
    if (lexer == NULL) {
        return false;
    }

    return lexer_peek(lexer, 4) == '-';
}

static bool lexer_see_time(const lexer_s *lexer) {
    if (lexer == NULL) {
        return false;
    }

    return lexer_peek(lexer, 2) == ':';
}


static token_s lexer_scan_numeral(lexer_s *lexer, token_pos_s head) {
    if (lexer == NULL) {
        return (token_s){0};
    }

    if (lexer_see_sign(lexer_peek(lexer, 0))) {
        lexer_move(lexer, 1);
    }

    if (lexer_peek(lexer, 0) == '0') {
        if (lexer_peek(lexer, 1) == 'x') {
            lexer_move(lexer, 2);
            lexer_scan_digits(lexer, is_hex_digit);
            return lexer_emit_token(lexer, head, TOKEN_S64_HEX);
        }
        if (lexer_peek(lexer, 1) == 'o') {
            lexer_move(lexer, 2);
            lexer_scan_digits(lexer, is_oct_digit);
            return lexer_emit_token(lexer, head, TOKEN_S64_OCT);
        }
        if (lexer_peek(lexer, 1) == 'b') {
            lexer_move(lexer, 2);
            lexer_scan_digits(lexer, is_bin_digit);
            return lexer_emit_token(lexer, head, TOKEN_S64_BIN);
        }
    }

    if (lexer_see_date(lexer)) {
        return lexer_scan_date(lexer, head);
    }
    if (lexer_see_time(lexer)) {
        return lexer_scan_time(lexer, head);
    }

    lexer_scan_digits(lexer, is_dec_digit);

    bool is_float = false;
    if (lexer_peek(lexer, 0) == '.' && is_dec_digit(lexer_peek(lexer, 1))) {
        is_float = true;
        lexer_move(lexer, 1);
        lexer_scan_digits(lexer, is_dec_digit);
    }
    if (lexer_peek(lexer, 0) == 'e' || lexer_peek(lexer, 0) == 'E') {
        is_float = true;
        lexer_move(lexer, 1);
        if (lexer_see_sign(lexer_peek(lexer, 0))) {
            lexer_move(lexer, 1);
        }
        lexer_scan_digits(lexer, is_dec_digit);
    }

    return lexer_emit_token(lexer, head, is_float ? TOKEN_F64 : TOKEN_S64_DEC);
}

static token_s lexer_scan(lexer_s *lexer, token_pos_s head) {
    if (lexer == NULL) {
        return (token_s){0};
    }

    switch (lexer_peek(lexer, 0)) {
        case '\0':
            return lexer_emit_token(lexer, head, TOKEN_EOF);
        case '"':
            return lexer_scan_str(lexer, head, '"');
        case '\'':
            return lexer_scan_str(lexer, head, '\'');
        case '0':
        case '1':
        case '2':
        case '3':
        case '4':
        case '5':
        case '6':
        case '7':
        case '8':
        case '9':
            return lexer_scan_numeral(lexer, head);
        case '+':
        case '-':
            if (is_dec_digit(lexer_peek(lexer, 1))) {
                return lexer_scan_numeral(lexer, head);
            }
            lexer_move(lexer, 1);
            return lexer_scan_text(lexer, head);
        case '[':
            lexer_move(lexer, 1);
            return lexer_emit_token(lexer, head, TOKEN_LBRACKET);
        case ']':
            lexer_move(lexer, 1);
            return lexer_emit_token(lexer, head, TOKEN_RBRACKET);
        case '{':
            lexer_move(lexer, 1);
            return lexer_emit_token(lexer, head, TOKEN_LBRACE);
        case '}':
            lexer_move(lexer, 1);
            return lexer_emit_token(lexer, head, TOKEN_RBRACE);
        case '=':
            lexer_move(lexer, 1);
            return lexer_emit_token(lexer, head, TOKEN_EQUALS);
        case '.':
            lexer_move(lexer, 1);
            return lexer_emit_token(lexer, head, TOKEN_DOT);
        case ',':
            lexer_move(lexer, 1);
            return lexer_emit_token(lexer, head, TOKEN_COMMA);
        case '\n':
            lexer_move(lexer, 1);
            return lexer_emit_token(lexer, head, TOKEN_NEWLINE);
        case '\r':
            if (lexer_peek(lexer, 1) == '\n') {
                lexer_move(lexer, 2);
                return lexer_emit_token(lexer, head, TOKEN_NEWLINE);
            } else {
                lexer_move(lexer, 1);
                return lexer_emit_error(lexer, head, "Bare CR is not allowed");
            }
        default: {
            unsigned char b = (unsigned char)lexer_peek(lexer, 0);
            if (is_bare_key_char((char)b) || b >= 0xC2) {
                return lexer_scan_text(lexer, head);
            }
            lexer_move(lexer, 1);
            return lexer_emit_error(lexer, head, "Unexpected character");
        }
    }
}

static token_s lexer_next(lexer_s *lexer) {
    if (lexer == NULL || lexer->has_err) {
        return (token_s){0};
    }

    token_pos_s head = lexer_skip_trivia(lexer);
    if (lexer_has_error(lexer)) {
        return lexer_emit_token(lexer, lexer->err.span.head, TOKEN_ERROR);
    }

    return lexer_scan(lexer, head);
}

static bool lexer_see_valid_utf8(const char *doc, size_t doc_len) {
    if (doc == NULL) {
        return doc_len == 0;
    }

    const unsigned char *p   = (const unsigned char *)doc;
    const unsigned char *end = p + doc_len;

    while (p < end) {
        unsigned char b = *p;

        // NULL prohibited in TOML
        if (b == 0x00) {
            return false;
        }

        if (b <= 0x7F) { p++; continue; }

        size_t seq_len = lexer_utf8_seq_len(b);

        // Overlong/surrogate lead or > U+10FFFF; truncated sequence
        if (seq_len == 0 || (size_t)(end - p) < seq_len) {
            return false;
        }
        if (seq_len >= 2) {
            if ((p[1] & 0xC0) != 0x80)     return false;
        }
        if (seq_len >= 3) {
            if ((p[2] & 0xC0) != 0x80)     return false;
            if (b == 0xE0 && p[1] < 0xA0)  return false;  // Overlong (< U+0800)
            if (b == 0xED && p[1] >= 0xA0) return false;  // Surrogate (U+D800–U+DFFF)
        }
        if (seq_len == 4) {
            if ((p[3] & 0xC0) != 0x80)     return false;
            if (b == 0xF0 && p[1] < 0x90)  return false;  // Overlong (< U+10000)
            if (b == 0xF4 && p[1] > 0x8F)  return false;  // > U+10FFFF
        }

        p += seq_len;
    }

    return true;
}


// §3  TOML Object

#define TOML_READ_CHUNK_SIZE 4096u

struct toml_s {
    arena_s arena;
};

static toml_t *toml_new(const char *src, size_t src_len) {
    if (!lexer_see_valid_utf8(src, src_len)) {
        return NULL;
    }

    toml_t *toml = TOML_MALLOC(sizeof(toml_t));
    if (toml == NULL) {
        return NULL;
    }

    arena_init(&toml->arena, 0);

    lexer_s lexer = lexer_init(src, src_len);
    (void)lexer;

    return toml;
}

toml_t *toml_from_str(const char *src) {
    if (src == NULL) {
        return NULL;
    }

    return toml_new(src, strlen(src));
}

toml_t *toml_from_fp(FILE *fp) {
    if (fp == NULL) {
        return NULL;
    }

    size_t cap = TOML_READ_CHUNK_SIZE;
    size_t len = 0;

    char *buf = TOML_MALLOC(cap);
    if (buf == NULL) {
        return NULL;
    }

    size_t n;
    while ((n = fread(buf + len, 1, cap - len, fp)) > 0) {
        len += n;
        if (len == cap) {
            if (cap > SIZE_MAX / 2) {
                TOML_FREE(buf);
                return NULL;
            }
            cap *= 2;
            char *new_buf = TOML_REALLOC(buf, cap);
            if (new_buf == NULL) {
                TOML_FREE(buf);
                return NULL;
            }
            buf = new_buf;
        }
    }

    if (ferror(fp)) {
        TOML_FREE(buf);
        return NULL;
    }

    toml_t *toml = toml_new(buf, len);
    TOML_FREE(buf);

    return toml;
}

toml_t *toml_from_file(const char *fmt, ...) {
    if (fmt == NULL) {
        return NULL;
    }

    char path[PATH_MAX] = {0};

    va_list args;
    va_start(args, fmt);
    int path_len = vsnprintf(path, sizeof(path), fmt, args);
    va_end(args);
    if (path_len < 0 || (size_t)path_len >= sizeof(path)) {
        return NULL;
    }

    FILE *fp = fopen(path, "rb");
    if (fp == NULL) {
        return NULL;
    }

    toml_t *toml = toml_from_fp(fp);
    fclose(fp);

    return toml;
}

void toml_free(toml_t **toml) {
    if (toml == NULL || *toml == NULL) {
        return;
    }

    arena_free(&(*toml)->arena);
    TOML_FREE(*toml);
    *toml = NULL;
}
