//! @copyright  Copyright 2026 Wei-Te Wong. MIT Licensed.
#include <stdint.h>
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
            fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__,  #cond); \
            fails++; \
        } \
    } while (0)

static void test_init(void) {
    arena_s arena;
    arena_init(&arena, 64);

    EXPECT(arena.head == NULL);
    EXPECT(arena.chunk_size == 64);

    arena_free(&arena);
}

static void test_alloc_within_chunk_is_aligned_and_contiguous(void) {
    arena_s arena;
    arena_init(&arena, 64);

    char *p1 = arena_alloc(&arena, 1);
    char *p2 = arena_alloc(&arena, 3);
    char *p3 = arena_alloc(&arena, 8);

    EXPECT(p1 != NULL && p2 != NULL && p3 != NULL);
    EXPECT((p2 - p1) == 8);  // Size 1 rounds up to 8
    EXPECT((p3 - p2) == 8);  // Size 3 rounds up to 8
    EXPECT(arena.head != NULL);
    EXPECT(arena.head->used == 24);

    arena_free(&arena);
}

static void test_alloc_returns_8_byte_aligned_pointers(void) {
    arena_s arena;
    arena_init(&arena, 64);

    for (size_t i = 1; i <= 7; i++) {
        void *p = arena_alloc(&arena, i);
        EXPECT(((uintptr_t)p % ARENA_ALIGN_SIZE) == 0);
    }

    arena_free(&arena);
}

static void test_alloc_grows_new_chunk_when_full(void) {
    arena_s arena;
    arena_init(&arena, 64);

    // Fill the first chunk with 8 bytes
    arena_alloc(&arena, 8);
    chunk_s *first = arena.head;
    EXPECT(first->capacity == 64);
    EXPECT(first->used == 8);

    // Fill the remaining space in the first chunk
    arena_alloc(&arena, 56);
    EXPECT(arena.head == first);
    EXPECT(arena.head->used == 64);

    // Additional allocation should fall into a new chunk
    arena_alloc(&arena, 8);
    EXPECT(arena.head != first);
    EXPECT(arena.head->next == first);
    EXPECT(arena.head->used == 8);

    arena_free(&arena);
}

static void test_alloc_oversized_request_uses_2x_capacity(void) {
    arena_s arena;
    arena_init(&arena, 16);

    // Request far exceeds chunk_size, so capacity should be 2 * request
    void *p = arena_alloc(&arena, 1000);
    EXPECT(p != NULL);
    EXPECT(arena.head->capacity == 2000);

    arena_free(&arena);
}

static void test_chunk_size_doubles_after_growth(void) {
    arena_s arena;
    arena_init(&arena, 16);

    arena_alloc(&arena, 1);
    EXPECT(arena.chunk_size == 32);

    arena_alloc(&arena, 32);
    EXPECT(arena.head->capacity == 64);
    EXPECT(arena.chunk_size == 128);

    arena_free(&arena);
}

static void test_strdup_copies_and_terminates(void) {
    arena_s arena;
    arena_init(&arena, 64);

    const char *src = "hello, toml";
    char *dup = arena_strdup(&arena, src, strlen(src));

    EXPECT(dup != src);
    EXPECT(strcmp(dup, src) == 0);
    EXPECT(dup[strlen(src)] == '\0');

    arena_free(&arena);
}

static void test_strdup_handles_embedded_and_partial_length(void) {
    arena_s arena;
    arena_init(&arena, 64);

    char *dup = arena_strdup(&arena, "hello world", 5);

    EXPECT(strcmp(dup, "hello") == 0);

    arena_free(&arena);
}

static void test_multiple_allocations_do_not_overlap(void) {
    arena_s arena;
    arena_init(&arena, 4096);

    char *a = arena_strdup(&arena, "alpha", 5);
    char *b = arena_strdup(&arena, "beta", 4);
    char *c = arena_strdup(&arena, "gamma", 5);

    EXPECT(strcmp(a, "alpha") == 0);
    EXPECT(strcmp(b, "beta") == 0);
    EXPECT(strcmp(c, "gamma") == 0);

    arena_free(&arena);
}

static void test_free_releases_all_chunks(void) {
    arena_s arena;
    arena_init(&arena, 8);

    // Allocate several chunks
    for (int i = 0; i < 10; i++) {
        arena_alloc(&arena, 8);
    }
    EXPECT(arena.head != NULL);
    EXPECT(arena.head->next != NULL);

    arena_free(&arena);
    EXPECT(arena.head == NULL);
}

static void test_free_on_empty_arena_is_safe(void) {
    arena_s arena;
    arena_init(&arena, 64);

    arena_free(&arena);
    EXPECT(arena.head == NULL);
}

static void test_null_arena_arguments_are_safe(void) {
    // None of these should crash; arena_init/arena_free silently no-op,
    // arena_alloc/arena_strdup return NULL
    arena_init(NULL, 64);
    arena_free(NULL);

    EXPECT(arena_alloc(NULL, 8) == NULL);
    EXPECT(arena_strdup(NULL, "hi", 2) == NULL);

    arena_s arena;
    arena_init(&arena, 64);

    EXPECT(arena_strdup(&arena, NULL, 2) == NULL);

    arena_free(&arena);
}

static void test_alloc_overflowing_size_returns_null(void) {
    arena_s arena;
    arena_init(&arena, 64);

    EXPECT(arena_alloc(&arena, SIZE_MAX) == NULL);
    EXPECT(arena_alloc(&arena, SIZE_MAX - 1) == NULL);

    // Arena must remain usable after a rejected oversized request
    EXPECT(arena_alloc(&arena, 8) != NULL);

    arena_free(&arena);
}

static void test_strdup_overflowing_length_returns_null(void) {
    arena_s arena;
    arena_init(&arena, 64);

    EXPECT(arena_strdup(&arena, "x", SIZE_MAX) == NULL);

    arena_free(&arena);
}

static void test_init_with_zero_chunk_size(void) {
    arena_s arena;
    arena_init(&arena, 0);

    EXPECT(arena.head == NULL);
    EXPECT(arena.chunk_size == 0);

    // First allocation still creates a usable chunk, sized off the
    // request rather than the (zero) configured chunk_size
    void *p = arena_alloc(&arena, 8);
    EXPECT(p != NULL);
    EXPECT(arena.head != NULL);
    EXPECT(arena.head->capacity == 16);  // max(2 * 8, 0)

    arena_free(&arena);
}

static void test_alloc_zero_size(void) {
    arena_s arena;
    arena_init(&arena, 64);

    void *p1 = arena_alloc(&arena, 0);
    EXPECT(p1 != NULL);
    EXPECT(arena.head != NULL);
    EXPECT(arena.head->used == 0);

    // A second zero-size allocation aliases the first: no space was
    // consumed, so both calls return the same address
    void *p2 = arena_alloc(&arena, 0);
    EXPECT(p2 == p1);

    // The arena must remain fully usable for real allocations afterward
    void *p3 = arena_alloc(&arena, 8);
    EXPECT(p3 == p1);
    EXPECT(arena.head->used == 8);

    arena_free(&arena);
}

static void test_alloc_zero_size_on_empty_arena_creates_chunk(void) {
    arena_s arena;
    arena_init(&arena, 0);

    // Even chunk_size == 0 and size == 0 must still produce a valid
    // (zero-capacity) chunk rather than leaving arena.head dangling
    void *p = arena_alloc(&arena, 0);
    EXPECT(p != NULL);
    EXPECT(arena.head != NULL);
    EXPECT(arena.head->capacity == 0);
    EXPECT(arena.head->used == 0);

    arena_free(&arena);
}

int main(void) {
    test_init();

    test_alloc_within_chunk_is_aligned_and_contiguous();
    test_alloc_returns_8_byte_aligned_pointers();
    test_alloc_grows_new_chunk_when_full();
    test_alloc_oversized_request_uses_2x_capacity();
    test_chunk_size_doubles_after_growth();
    test_strdup_copies_and_terminates();
    test_strdup_handles_embedded_and_partial_length();
    test_multiple_allocations_do_not_overlap();
    test_free_releases_all_chunks();
    test_free_on_empty_arena_is_safe();
    test_null_arena_arguments_are_safe();
    test_alloc_overflowing_size_returns_null();
    test_strdup_overflowing_length_returns_null();
    test_init_with_zero_chunk_size();
    test_alloc_zero_size();
    test_alloc_zero_size_on_empty_arena_creates_chunk();

    printf("%d/%d tests passed.\n", total - fails, total);

    return fails == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
