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

#include "toml.h"

#include <assert.h>
#include <stdlib.h>
#include <string.h>

#if TOML_HAS_C11
#define ALIGNOF(type) _Alignof(type)
#else
#define ALIGNOF(type) offsetof(struct { char byte; type value; }, value)
#endif

// Silences -Wunused-function for entry points with no production caller
// yet; the M1.3 parser wires these in, at which point this goes away
#if defined(__GNUC__) || defined(__clang__)
#define TOML_NOT_YET_CALLED __attribute__((unused))
#else
#define TOML_NOT_YET_CALLED
#endif

// One chunk for now; the chunked dynamic backend arriving in M7 reuses
// this same size per chunk
static const size_t ARENA_CHUNK_SIZE = 4096;

typedef struct {
    unsigned char *chunk;
    size_t capacity;
    size_t offset;
} toml_arena_s;

static size_t align_to(size_t value, size_t align) {
    assert(align > 0 && (align & (align - 1)) == 0);

    return (value + align - 1) & ~(align - 1);
}

static void *arena_alloc(toml_arena_s *arena, size_t size, size_t align) {
    size_t aligned_offset = align_to(arena->offset, align);

    if (aligned_offset > arena->capacity) {
        return NULL;
    }

    if (size > arena->capacity - aligned_offset) {
        return NULL;
    }

    void *ptr = arena->chunk + aligned_offset;
    arena->offset = aligned_offset + size;

    return ptr;
}


// TOML Lexer

typedef enum {
    TOKEN_EOF,
    TOKEN_ERROR,
    TOKEN_NEWLINE,
    TOKEN_EQUAL,
    TOKEN_LBRACKET,
    TOKEN_RBRACKET,
    TOKEN_BARE_KEY,
    TOKEN_STR,
    TOKEN_S64,
    TOKEN_TRUE,
    TOKEN_FALSE,
} token_type_e;

typedef struct {
    token_type_e type;
    toml_span_s leading;     // Trivia before the token
    toml_span_s text;        // Exact source bytes of the token, quotes included
} token_s;

typedef struct {
    const char *cur;
    const char *end;
} lexer_s;

// `source[len]` must be a readable sentinel byte other than '\n' (a
// NUL terminator works). This lets lookahead like the CRLF check in
// lexer_next() read one byte past `end` without a bounds check
TOML_NOT_YET_CALLED
static void lexer_init(lexer_s *lexer, const char *source, size_t len) {
    lexer->cur = source;
    lexer->end = source + len;
}

static void lexer_move(lexer_s *lexer, size_t offset) {
    lexer->cur += offset;
}

static char lexer_peek(const lexer_s *lexer, size_t offset) {
    return lexer->cur[offset];
}

static toml_span_s make_span(const char *head, const char *tail) {
    return (toml_span_s){
        .ptr = head,
        .len = (size_t)(tail - head)
    };
}

static token_s make_token(token_type_e type, const char *head, const char *tail, toml_span_s leading) {
    return (token_s){
        .type = type,
        .leading = leading,
        .text = make_span(head, tail),
    };
}

static bool lexer_eof(const lexer_s *lexer) {
    return lexer->cur >= lexer->end;
}

static bool is_bare_key_char(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
           (c >= '0' && c <= '9') || c == '_' || c == '-';
}

static bool is_dec_digit(char c) {
    return (c >= '0' && c <= '9');
}

static bool is_newline(char c) {
    return c == '\n' || c == '\r';
}

static toml_span_s lexer_scan_trivia(lexer_s *lexer) {
    const char *head = lexer->cur;

    while (!lexer_eof(lexer)) {
        switch (lexer_peek(lexer, 0)) {
            case ' ':
            case '\t':
                lexer_move(lexer, 1);
                break;
            case '#':
                while (!lexer_eof(lexer) && !is_newline(lexer_peek(lexer, 0))) {
                    lexer_move(lexer, 1);
                }
                break;
            default:
                return make_span(head, lexer->cur);
        }
    }

    return make_span(head, lexer->cur);
}

static bool span_eq_literal(toml_span_s span, const char *literal) {
    size_t len = strlen(literal);
    return span.len == len && memcmp(span.ptr, literal, len) == 0;
}

static token_s lexer_scan_bare_key(lexer_s *lexer, toml_span_s leading) {
    const char *head = lexer->cur;

    while (!lexer_eof(lexer) && is_bare_key_char(lexer_peek(lexer, 0))) {
        lexer_move(lexer, 1);
    }

    if (lexer->cur == head) {
        // Move past the erroneous byte
        lexer_move(lexer, 1);
        return make_token(TOKEN_ERROR, head, lexer->cur, leading);
    }

    toml_span_s text = make_span(head, lexer->cur);

    if (span_eq_literal(text, "true")) {
        return make_token(TOKEN_TRUE, head, lexer->cur, leading);
    }

    if (span_eq_literal(text, "false")) {
        return make_token(TOKEN_FALSE, head, lexer->cur, leading);
    }

    return make_token(TOKEN_BARE_KEY, head, lexer->cur, leading);
}

static token_s lexer_scan_s64(lexer_s *lexer, toml_span_s leading) {
    const char *head = lexer->cur;

    bool has_plus = lexer_peek(lexer, 0) == '+';
    if (has_plus || lexer_peek(lexer, 0) == '-') {
        lexer_move(lexer, 1);
    }

    const char *first_digit = lexer->cur;

    while (!lexer_eof(lexer) && is_dec_digit(lexer_peek(lexer, 0))) {
        lexer_move(lexer, 1);
    }

    bool has_digit = lexer->cur != first_digit;
    bool clean_end = lexer_eof(lexer) || !is_bare_key_char(lexer_peek(lexer, 0));
    if (has_digit && clean_end) {
        return make_token(TOKEN_S64, head, lexer->cur, leading);
    }

    if (has_plus) {
        return make_token(TOKEN_ERROR, head, lexer->cur, leading);
    } else {
        lexer->cur = head;
        return lexer_scan_bare_key(lexer, leading);
    }
}

static token_s lexer_scan_str(lexer_s *lexer, toml_span_s leading) {
    const char *head = lexer->cur;

    // Skip opening quote
    lexer_move(lexer, 1);

    while (!lexer_eof(lexer) && lexer_peek(lexer, 0) != '"') {
        if (is_newline(lexer_peek(lexer, 0)) || lexer_peek(lexer, 0) == '\\') {
            return make_token(TOKEN_ERROR, head, lexer->cur, leading);
        }
        lexer_move(lexer, 1);
    }

    if (lexer_eof(lexer)) {
        return make_token(TOKEN_ERROR, head, lexer->cur, leading);
    }

    // Skip closing quote
    lexer_move(lexer, 1);

    return make_token(TOKEN_STR, head, lexer->cur, leading);
}

TOML_NOT_YET_CALLED
static token_s lexer_next(lexer_s *lexer) {
    toml_span_s leading = lexer_scan_trivia(lexer);

    const char *head = lexer->cur;

    if (lexer_eof(lexer)) {
        return make_token(TOKEN_EOF, head, head, leading);
    }

    switch (lexer_peek(lexer, 0)) {
        case '\n':
            lexer_move(lexer, 1);
            return make_token(TOKEN_NEWLINE, head, lexer->cur, leading);
        case '\r':
            if (lexer_peek(lexer, 1) != '\n') {
                lexer_move(lexer, 1);
                return make_token(TOKEN_ERROR, head, lexer->cur, leading);
            } else {
                lexer_move(lexer, 2);
                return make_token(TOKEN_NEWLINE, head, lexer->cur, leading);
            }
        case '=':
            lexer_move(lexer, 1);
            return make_token(TOKEN_EQUAL, head, lexer->cur, leading);
        case '[':
            lexer_move(lexer, 1);
            return make_token(TOKEN_LBRACKET, head, lexer->cur, leading);
        case ']':
            lexer_move(lexer, 1);
            return make_token(TOKEN_RBRACKET, head, lexer->cur, leading);
        case '"':
            return lexer_scan_str(lexer, leading);
        case '0': case '1': case '2': case '3': case '4': case '5':
        case '6': case '7': case '8': case '9': case '+': case '-':
            return lexer_scan_s64(lexer, leading);
        default:
            return lexer_scan_bare_key(lexer, leading);
    }
}


// TOML CST

typedef enum {
    TOML_STR,
    TOML_S64,
    TOML_BOOL,
    TOML_TABLE,
} toml_type_e;

typedef struct {
    struct toml_node **entries;
    size_t count;
} toml_table_s;

typedef struct toml_node {
    toml_type_e type;
    toml_span_s key;         // This node's key span (empty for the root)
    toml_span_s leading;     // Trivia before this node
    toml_span_s trailing;    // Trivia after, up to end of line
    union {
        int64_t s64;
        bool b;
        toml_span_s byte;    // Non-NUL-terminated
        toml_table_s t;
    } val;
    struct toml_node *next;
} toml_node_s;

TOML_NOT_YET_CALLED
static toml_node_s **create_index(toml_arena_s *arena, toml_node_s *head, size_t count) {
    size_t size  = count * sizeof(toml_node_s *);
    size_t align = ALIGNOF(toml_node_s *);
    toml_node_s **children = arena_alloc(arena, size, align);
    if (children == NULL) {
        return NULL;
    }

    toml_node_s *node = head;
    for (size_t i = 0; i < count; i++) {
        children[i] = node;
        node = node->next;
    }

    return children;
}

typedef struct {
    toml_errcode_e code;
    toml_span_s primary;
    toml_span_s secondary;
    const char *detail;
} toml_error_s;

struct toml {
    toml_arena_s arena;
    toml_error_s error;
    toml_span_s source;  // Owned copy of the input, NUL-terminated
};

toml_t *toml_from_byte(const char *byte, size_t byte_len) {
    size_t min_size = sizeof(toml_t) + ALIGNOF(toml_t) + byte_len + 1;
    size_t chunk_size = min_size > ARENA_CHUNK_SIZE ? min_size : ARENA_CHUNK_SIZE;

    unsigned char *chunk = malloc(chunk_size);
    if (chunk == NULL) {
        return NULL;
    }

    toml_arena_s arena = {
        .chunk = chunk,
        .capacity = chunk_size,
    };

    toml_t *toml = arena_alloc(&arena, sizeof *toml, ALIGNOF(toml_t));
    if (toml == NULL) {
        free(chunk);
        return NULL;
    }

    char *copy = arena_alloc(&arena, byte_len + 1, 1);
    if (copy == NULL) {
        free(chunk);
        return NULL;
    }

    memcpy(copy, byte, byte_len);
    copy[byte_len] = '\0';

    toml->arena = arena;
    toml->error = (toml_error_s){ .code = TOML_OK };
    toml->source = make_span(copy, copy + byte_len);

    return toml;
}

void toml_free(toml_t *toml) {
    if (toml == NULL) {
        return;
    }

    free(toml->arena.chunk);
}

bool toml_has_error(const toml_t *toml) {
    if (toml == NULL) {
        return true;
    }

    return toml->error.code != TOML_OK;
}
