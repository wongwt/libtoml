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
    // `entries` is written by finalize_table() and read today only by
    // tests via white-box access; the serializer (M1.4) / access API
    // (M1.5) add a real production reader
    // cppcheck-suppress unusedStructMember
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
} toml_error_s;

struct toml {
    toml_arena_s arena;
    toml_error_s error;
    toml_span_s source;  // Owned copy of the input, NUL-terminated
    toml_node_s *root;
};


// TOML Parser

static const toml_span_s EMPTY_SPAN = { 0 };

static void make_error(toml_t *toml, toml_errcode_e code, toml_span_s primary, toml_span_s secondary) {
    toml->error = (toml_error_s){
        .code = code,
        .primary = primary,
        .secondary = secondary,
    };
}

static bool spans_eq(toml_span_s a, toml_span_s b) {
    return a.len == b.len && memcmp(a.ptr, b.ptr, a.len) == 0;
}

static toml_span_s parse_key(toml_t *toml, token_s token) {
    switch (token.type) {
        case TOKEN_BARE_KEY:
        case TOKEN_S64:
        case TOKEN_TRUE:
        case TOKEN_FALSE:
            return token.text;
        default:
            make_error(toml, TOML_ERR_SYNTAX, token.text, EMPTY_SPAN);
            return EMPTY_SPAN;
    }
}

#define S64_TEXT_BUF_SIZE 21

static int64_t parse_s64_value(toml_span_s text) {
    char buf[S64_TEXT_BUF_SIZE];

    if (text.len >= sizeof(buf)) {
        return text.ptr[0] == '-' ? INT64_MIN : INT64_MAX;
    }

    memcpy(buf, text.ptr, text.len);
    buf[text.len] = '\0';

    return strtoll(buf, NULL, 10);
}

static toml_node_s *parse_val(toml_t *toml, token_s token) {
    toml_node_s *node = arena_alloc(&toml->arena, sizeof *node, ALIGNOF(toml_node_s));
    if (node == NULL) {
        make_error(toml, TOML_ERR_NOMEM, token.text, EMPTY_SPAN);
        return NULL;
    }

    *node = (toml_node_s){ 0 };

    switch (token.type) {
        case TOKEN_STR:
            node->type = TOML_STR;
            node->val.byte = (toml_span_s){
                .ptr = token.text.ptr + 1,
                .len = token.text.len - 2,
            };
            return node;
        case TOKEN_S64:
            node->type = TOML_S64;
            node->val.s64 = parse_s64_value(token.text);
            return node;
        case TOKEN_TRUE:
            node->type = TOML_BOOL;
            node->val.b = true;
            return node;
        case TOKEN_FALSE:
            node->type = TOML_BOOL;
            node->val.b = false;
            return node;
        default:
            make_error(toml, TOML_ERR_SYNTAX, token.text, EMPTY_SPAN);
            return NULL;
    }
}

static toml_node_s *parse_keyval(toml_t *toml, lexer_s *lexer, token_s key_tok) {
    toml_span_s key = parse_key(toml, key_tok);
    if (toml_has_error(toml)) {
        return NULL;
    }

    token_s eq_tok = lexer_next(lexer);
    if (eq_tok.type != TOKEN_EQUAL) {
        make_error(toml, TOML_ERR_SYNTAX, eq_tok.text, EMPTY_SPAN);
        return NULL;
    }

    toml_node_s *node = parse_val(toml, lexer_next(lexer));
    if (node == NULL) {
        return NULL;
    }

    node->key = key;
    return node;
}

typedef struct {
    toml_node_s *head;
    toml_node_s *tail;
    size_t count;
} node_list_s;

static void node_list_append(node_list_s *list, toml_node_s *entry) {
    if (list->head == NULL) {
        list->head = entry;
    } else {
        list->tail->next = entry;
    }
    list->tail = entry;
    list->count++;
}

static const toml_node_s *find_duplicate(const toml_node_s *head, toml_span_s key) {
    for (const toml_node_s *existing = head; existing != NULL; existing = existing->next) {
        if (spans_eq(existing->key, key)) {
            return existing;
        }
    }

    return NULL;
}

static bool finalize_table(toml_t *toml, node_list_s entries, toml_table_s *out) {
    toml_node_s **index = create_index(&toml->arena, entries.head, entries.count);
    if (index == NULL) {
        make_error(toml, TOML_ERR_NOMEM, EMPTY_SPAN, EMPTY_SPAN);
        return false;
    }

    *out = (toml_table_s){ .entries = index, .count = entries.count };
    return true;
}

static toml_node_s *make_table_node(toml_t *toml, node_list_s entries) {
    toml_table_s table;
    if (!finalize_table(toml, entries, &table)) {
        return NULL;
    }

    toml_node_s *node = arena_alloc(&toml->arena, sizeof *node, ALIGNOF(toml_node_s));
    if (node == NULL) {
        make_error(toml, TOML_ERR_NOMEM, EMPTY_SPAN, EMPTY_SPAN);
        return NULL;
    }

    *node = (toml_node_s){ .type = TOML_TABLE, .val.t = table };
    return node;
}

static toml_node_s *parse_table_header(toml_t *toml, lexer_s *lexer, node_list_s *root_entries) {
    toml_span_s name = parse_key(toml, lexer_next(lexer));
    if (toml_has_error(toml)) {
        return NULL;
    }

    token_s close_token = lexer_next(lexer);
    if (close_token.type != TOKEN_RBRACKET) {
        make_error(toml, TOML_ERR_SYNTAX, close_token.text, EMPTY_SPAN);
        return NULL;
    }

    token_s after_token = lexer_next(lexer);
    if (after_token.type != TOKEN_NEWLINE && after_token.type != TOKEN_EOF) {
        make_error(toml, TOML_ERR_SYNTAX, after_token.text, EMPTY_SPAN);
        return NULL;
    }

    const toml_node_s *dup = find_duplicate(root_entries->head, name);
    if (dup != NULL) {
        make_error(toml, TOML_ERR_DUP_KEY, name, dup->key);
        return NULL;
    }

    toml_node_s *table = arena_alloc(&toml->arena, sizeof *table, ALIGNOF(toml_node_s));
    if (table == NULL) {
        make_error(toml, TOML_ERR_NOMEM, EMPTY_SPAN, EMPTY_SPAN);
        return NULL;
    }

    *table = (toml_node_s){ .type = TOML_TABLE, .key = name };
    node_list_append(root_entries, table);

    return table;
}

static toml_node_s *parse_toml(toml_t *toml, lexer_s *lexer) {
    node_list_s root_entries = { 0 };
    node_list_s cur_entries = { 0 };
    toml_node_s *cur_table = NULL;  // NULL while collecting the root's own body

    token_s token = lexer_next(lexer);

    for (;;) {
        while (token.type == TOKEN_NEWLINE) {
            token = lexer_next(lexer);
        }

        if (token.type == TOKEN_EOF || token.type == TOKEN_LBRACKET) {
            if (cur_table == NULL) {
                root_entries = cur_entries;
            } else if (!finalize_table(toml, cur_entries, &cur_table->val.t)) {
                return NULL;
            }

            if (token.type == TOKEN_EOF) {
                break;
            }

            cur_table = parse_table_header(toml, lexer, &root_entries);
            if (cur_table == NULL) {
                return NULL;
            }

            cur_entries = (node_list_s){ 0 };
            token = lexer_next(lexer);
            continue;
        }

        toml_node_s *entry = parse_keyval(toml, lexer, token);
        if (entry == NULL) {
            return NULL;
        }

        const toml_node_s *dup = find_duplicate(cur_entries.head, entry->key);
        if (dup != NULL) {
            make_error(toml, TOML_ERR_DUP_KEY, entry->key, dup->key);
            return NULL;
        }

        node_list_append(&cur_entries, entry);

        token = lexer_next(lexer);
        if (token.type != TOKEN_NEWLINE && token.type != TOKEN_EOF) {
            make_error(toml, TOML_ERR_SYNTAX, token.text, EMPTY_SPAN);
            return NULL;
        }
    }

    return make_table_node(toml, root_entries);
}

toml_t *toml_from_byte(const char *byte, size_t byte_len) {
    size_t chunk_size = sizeof(toml_t) + ALIGNOF(toml_t) + byte_len + 1 + ARENA_CHUNK_SIZE;
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

    lexer_s lexer;
    lexer_init(&lexer, toml->source.ptr, toml->source.len);
    toml->root = parse_toml(toml, &lexer);

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
