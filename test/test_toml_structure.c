//! @copyright  Copyright 2026 Wei-Te Wong. MIT Licensed.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "toml.h"

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

static void test_from_str_returns_object(void) {
    toml_t *toml = toml_from_str("key = \"value\"\n");

    EXPECT(toml != NULL);

    toml_free(&toml);
}

static void test_from_str_accepts_empty_document(void) {
    toml_t *toml = toml_from_str("");

    EXPECT(toml != NULL);

    toml_free(&toml);
}

static void test_from_str_rejects_null_src(void) {
    toml_t *toml = toml_from_str(NULL);

    EXPECT(toml == NULL);
}

static void test_from_fp_returns_object(void) {
    FILE *fp = tmpfile();
    EXPECT(fp != NULL);

    fputs("key = \"value\"\n", fp);
    rewind(fp);

    toml_t *toml = toml_from_fp(fp);
    EXPECT(toml != NULL);

    toml_free(&toml);
    fclose(fp);
}

static void test_from_fp_accepts_empty_stream(void) {
    FILE *fp = tmpfile();
    EXPECT(fp != NULL);

    toml_t *toml = toml_from_fp(fp);
    EXPECT(toml != NULL);

    toml_free(&toml);
    fclose(fp);
}

static void test_from_fp_rejects_null_fp(void) {
    toml_t *toml = toml_from_fp(NULL);

    EXPECT(toml == NULL);
}

static void test_from_fp_reads_input_larger_than_one_chunk(void) {
    FILE *fp = tmpfile();
    EXPECT(fp != NULL);

    // Larger than a single internal read chunk, forcing multiple fread() calls
    size_t want_len = 10000;
    char *want = malloc(want_len);
    EXPECT(want != NULL);
    memset(want, 'a', want_len);
    fwrite(want, 1, want_len, fp);
    rewind(fp);

    toml_t *toml = toml_from_fp(fp);
    EXPECT(toml != NULL);

    free(want);
    toml_free(&toml);
    fclose(fp);
}

static void test_from_file_returns_object(void) {
    char path[] = "/tmp/libtoml_test_XXXXXX";
    int fd = mkstemp(path);
    EXPECT(fd != -1);

    FILE *fp = fdopen(fd, "w");
    EXPECT(fp != NULL);
    fputs("key = \"value\"\n", fp);
    fclose(fp);

    toml_t *toml = toml_from_file("%s", path);
    EXPECT(toml != NULL);

    toml_free(&toml);
    remove(path);
}

static void test_from_file_builds_path_from_format_args(void) {
    char path[] = "/tmp/libtoml_test_XXXXXX";
    int fd = mkstemp(path);
    EXPECT(fd != -1);

    FILE *fp = fdopen(fd, "w");
    EXPECT(fp != NULL);
    fputs("key = \"value\"\n", fp);
    fclose(fp);

    const char *base = strrchr(path, '/') + 1;
    toml_t *toml = toml_from_file("/tmp/%s", base);
    EXPECT(toml != NULL);

    toml_free(&toml);
    remove(path);
}

static void test_from_file_rejects_missing_path(void) {
    toml_t *toml = toml_from_file("/nonexistent/path/does/not/exist.toml");

    EXPECT(toml == NULL);
}

static void test_from_file_rejects_null_path(void) {
    toml_t *toml = toml_from_file(NULL);

    EXPECT(toml == NULL);
}

static void test_free_clears_pointer(void) {
    toml_t *toml = toml_from_str("a = 1\n");
    EXPECT(toml != NULL);

    toml_free(&toml);
    EXPECT(toml == NULL);
}

static void test_free_is_safe_on_null_pointer_and_handle(void) {
    toml_free(NULL);

    toml_t *toml = NULL;
    toml_free(&toml);
    EXPECT(toml == NULL);
}

int main(void) {
    test_from_str_returns_object();
    test_from_str_accepts_empty_document();
    test_from_str_rejects_null_src();

    test_from_fp_returns_object();
    test_from_fp_accepts_empty_stream();
    test_from_fp_rejects_null_fp();
    test_from_fp_reads_input_larger_than_one_chunk();

    test_from_file_returns_object();
    test_from_file_builds_path_from_format_args();
    test_from_file_rejects_missing_path();
    test_from_file_rejects_null_path();

    test_free_clears_pointer();
    test_free_is_safe_on_null_pointer_and_handle();

    printf("%d/%d tests passed.\n", total - fails, total);

    return fails == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
