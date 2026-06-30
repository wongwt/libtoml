//! @copyright  Copyright 2026 Wei-Te Wong. MIT Licensed.
#ifndef LIBTOML_H_
#define LIBTOML_H_

#include <stdio.h>

#if defined(__STDC_VERSION__)
#  if __STDC_VERSION__ >= 202311L
#    define TOML_HAS_C23 1
#  endif
#  if __STDC_VERSION__ >= 201112L
#    define TOML_HAS_C11 1
#  endif
#endif

#ifdef TOML_HAS_C23
#  define TOML_NODISCARD(msg) [[nodiscard(msg)]]
#else
#  define TOML_NODISCARD(msg) // nothing
#endif

#ifdef __cplusplus
extern "C" {
#endif

//! Opaque TOML object
typedef struct toml_s toml_t;

//! Initiate TOML object from NULL-terminated UTF-8 string
//!
//! @param[in]      src      NULL-terminated UTF-8 string
//! @return         Parsed TOML object, or NULL on parse error
//! @note           Must call `toml_free()` to release resources
//! @see            toml_free()
TOML_NODISCARD("call toml_free() to release resources")
toml_t *toml_from_str(const char *src);

//! Initiate TOML object from file
//!
//! @param[in]      fmt      `printf()`-style format string for the path to
//!                          open and parse
//! @param[in]      ...      Format arguments substituted into `fmt`
//! @return         Parsed TOML object, or NULL on open/parse error
//! @note           Must call `toml_free()` to release resources
//! @see            toml_free()
TOML_NODISCARD("call toml_free() to release resources")
toml_t *toml_from_file(const char *fmt, ...);

//! Initiate TOML object from file stream
//!
//! @param[in]      fp       File stream
//! @return         Parsed TOML object, or NULL on read/parse error
//! @note           Must call `toml_free()` to release resources
//! @see            toml_free()
TOML_NODISCARD("call toml_free() to release resources")
toml_t *toml_from_fp(FILE *fp);

//! Cleanup TOML object resources
//!
//! @param[in,out]  toml     Address of the TOML object to free and clear
//! @note           Safe to call with NULL or with *toml already NULL
void toml_free(toml_t **toml);

#ifdef __cplusplus
}
#endif

#endif  // LIBTOML_H_
