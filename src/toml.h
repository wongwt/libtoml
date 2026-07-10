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

#ifndef LIBTOML_H
#define LIBTOML_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

// C11 support
#if defined(__STDC_VERSION__) && __STDC_VERSION__ >= 201112L
#define TOML_HAS_C11 1
#else
#define TOML_HAS_C11 0
#endif

#ifdef __cplusplus
extern "C" {
#endif

//! Opaque TOML document handle
typedef struct toml toml_t;

//! A byte range into either the immutable source buffer or the arena
typedef struct {
    const char *ptr; //!< First byte of the range
    size_t len; //!< Number of bytes in the range
} toml_span_s;

//! TOML error codes
typedef enum {
    TOML_OK = 0, //!< No error
    TOML_ERR_NOMEM, //!< Allocation failed
    TOML_ERR_SYNTAX, //!< Malformed input
    TOML_ERR_DUP_KEY //!< The same key is defined twice in one table
} toml_errcode_e;

//! Create TOML document from a byte buffer
//!
//! @param[in]  byte      Source bytes; need not be NUL-terminated
//! @param[in]  byte_len  Number of bytes in the source
//! @retval  NULL  The buffer cannot hold the document header
//! @return  TOML document handle
toml_t *toml_from_byte(const char *byte, size_t byte_len);

//! Serialize TOML document back to its exact source bytes
//!
//! @param[in]   toml  Document handle, or `NULL`
//! @param[out]  out   Destination buffer, sized to the original source
//! @retval  0  The handle is `NULL` or holds a failed parse
//! @return  Number of bytes written to `out`
size_t toml_to_byte(const toml_t *toml, char *out);

//! Tear down a TOML document handle
//!
//! @param[in]  toml  The handle to destroy, or `NULL` (ignored)
void toml_free(toml_t *toml);

//! Report whether the most recent operation failed
//!
//! @param[in]  toml  The handle to inspect, or `NULL`
//! @retval  true   The most recent operation failed, or the handle is `NULL`
//! @retval  false  The most recent operation succeeded
bool toml_has_error(const toml_t *toml);

//! Retrieve an integer value or return a fallback
//!
//! @param[in]  toml     TOML document handle
//! @param[in]  path     Dotted path to the value
//! @param[in]  def_val  Fallback when the path is absent
//! @return  The value at the path, or the fallback if absent
int64_t toml_get_s64_or(const toml_t *toml, const char *path, int64_t def_val);

//! Retrieve a floating-point value or return a fallback
//!
//! @param[in]  toml     TOML document handle
//! @param[in]  path     Dotted path to the value
//! @param[in]  def_val  Fallback when the path is absent
//! @return  The value at the path, or the fallback if absent
double toml_get_f64_or(const toml_t *toml, const char *path, double def_val);

//! Retrieve a boolean value or return a fallback
//!
//! @param[in]  toml     TOML document handle
//! @param[in]  path     Dotted path to the value
//! @param[in]  def_val  Fallback when the path is absent
//! @return  The value at the path, or the fallback if absent
bool toml_get_bool_or(const toml_t *toml, const char *path, bool def_val);

//! Retrieve a NUL-terminated string value or return a fallback
//!
//! @param[in]  toml     TOML document handle
//! @param[in]  path     Dotted path to the value
//! @param[in]  def_val  Fallback when the path is absent
//! @return  The value at the path, or the fallback if absent
const char *toml_get_str_or(const toml_t *toml, const char *path, const char *def_val);

//! Retrieve a NUL-terminated string value
//!
//! @param[in]  toml  TOML document handle
//! @param[in]  path  Dotted path to the value
//! @retval  NULL  The path is absent or holds the wrong type
//! @return  The value at the path
const char *toml_get_str(const toml_t *toml, const char *path);

#ifdef __cplusplus
}
#endif

#endif // LIBTOML_H
