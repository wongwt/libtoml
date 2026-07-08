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

static void test_has_error_null_is_safe_and_true(void) {
    EXPECT(toml_has_error(NULL) == true);
}

static void test_from_str_returns_usable_handle(void) {
    const char *source = "answer = 42\n";
    toml_t *toml = toml_from_byte(source, 12);

    EXPECT(toml != NULL);
    EXPECT(toml_has_error(toml) == false);

    toml_free(toml);
}

int main(void) {
    test_has_error_null_is_safe_and_true();
    test_from_str_returns_usable_handle();

    printf("%d passed, %d failed\n", pass_count, fail_count);

    return (fail_count == 0) ? EXIT_SUCCESS : EXIT_FAILURE;
}
