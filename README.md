# libtoml

A TOML v1.1 library in modern C with strict C99/POSIX compliance and optional
C11/C23 syntax sugar. MIT licensed.

> **Status:** pre-alpha, under active development. See the roadmap below.

## Quick example

````c
#include "toml.h"

toml_t *toml = toml_from_byte(byte, byte_len);

// `toml_has_error()` covers every failure mode, including a NULL handle
if (toml_has_error(toml)) {
    toml_err_print(toml);
    return EXIT_FAILURE;
}

// `toml_get_*_or()` lookup with explicit default value and clean syntax
int64_t port = toml_get_s64_or(toml, "server.port", 8080);
bool debug = toml_get_bool_or(toml, "debug", false);

// Guaranteed non-NULL: safe to pass straight into `printf()`
printf("host = %s\n", toml_get_str_or(toml, "server.host", "(unset)"));

// Plain getters return NULL/failure and the handle explains why
const char *token = toml_get_str(toml, "auth.token");
if (token == NULL) {
    toml_err_print(toml);
}

toml_free(toml);
````

Error state is sticky per handle: every successful call clears it,
every failing call overwrites it, so `toml_has_error()` always describes
the most recent operation.

## Building

````sh
make           # Release static library
make test      # Unit tests + toml-test
make test-san  # Same, under AddressSanitizer + UBSan
make check     # cppcheck static analysis
````

Requirements: a C99 compiler and POSIX environment. `gcc` and `clang` are
tested. Optional C11 conveniences (`_Generic` typed getters) activate
automatically and always have C99 fallbacks.

## Roadmap / TODO

Progress dashboard: toml-test pass count. Milestones in order, M0 through M8.

### M0 — Foundation
- [x] M0.1 Repo skeleton: `toml.c`, `toml.h`, LICENSE (MIT), Makefile
- [x] M0.2 Public API signatures stubbed: `toml_from_byte()`, `toml_t`
      handle, sticky `{code, span}` error + `toml_has_error()`,
      `toml_free()`, getter family declarations
- [x] M0.3 Bump allocator `arena_alloc()` with OOM path and alignment;
      unit tests include a 16-byte-buffer OOM test
- [ ] M0.4 Test runner: single entry point, unit tests + vendored
      toml-test corpus, skip-list mechanism
- [ ] M0.5 Targets: `test`, `test-san`, `test-valgrind`, `check`;
      `-Wall -Wextra -Werror`
- [ ] M0.6 GitHub Actions quick CI: check + test + test-san +
      skip-check on push

### M1 — Core dialect vertical slice
- [ ] M1.1 Lexer: bare keys, `=`, escape-free single-line basic
      strings, integers, booleans; comments/whitespace recorded as
      trivia spans; owned semantics with NUL sentinel
- [ ] M1.2 CST node finalized; intrusive child list + contiguous index
      backfill on container close
- [ ] M1.3 Parser: key-value pairs, `[table]`, duplicate detection
      with spans
- [ ] M1.4 Span-stitching serializer; **round-trip test
      `serialize(parse(x)) == x` goes live**
- [ ] M1.5 Access API: dotted-path lookup with array-index segments
      (`servers.0.host`) and quoted-segment rule documented;
      `toml_get_s64()` / `_or` family; `toml_has()`, `toml_type()`;
      sticky-error clear/overwrite semantics with unit tests
- [ ] M1.6 `test-release` target; first ~5 error codes;
      `toml_err_print()` minimal version (code + offset)

### M2 — Full TOML 1.0
- [ ] M2.1 `materialize_string`: escape decoding incl. `\uXXXX` /
      `\UXXXXXXXX`, surrogate rejection; UTF-8 boundary validation
- [ ] M2.2 Literal strings; multiline basic/literal (first-newline
      trim, line-ending backslash)
- [ ] M2.3 Floats (inf/nan/exponent/underscores); integer bases
- [ ] M2.4 `toml_get_str()` lazy materialization + cache flag;
      `toml_get_str_or()` non-NULL guarantee
- [ ] M2.5 Datetime (all four kinds)
- [ ] M2.6 Arrays (nested, multiline, 1.0 comma rules);
      `toml_array_len()`
- [ ] M2.7 Inline tables; dotted keys with conflict detection
- [ ] M2.8 Array of tables
- [ ] **Gate: toml-test v1.0 valid + invalid at 100 percent; valgrind
      clean**

### M3 — TOML 1.1 + feature trimming
- [ ] M3.1 1.1 features, one dispatch point each: multiline inline
      tables, `\e`, `\x`, optional seconds
- [ ] M3.2 All `TOML_NO_*` macros; `TOML_SPEC_1_0` umbrella;
      `TOML_E_FEATURE_DISABLED` naming the macro
- [ ] M3.3 `toml_build_config()`; `test/skip/` files; `skip-check`
- [ ] M3.4 `matrix-trim`, `matrix-minimal`, `matrix-cc`; full CI layer
- [ ] M3.5 `coverage` target; trimmed paths verified unexecuted
- [ ] **Gate: toml-test v1.1 at 100 percent**

### M4 — Schema mapping
- [ ] M4.1 `toml_field_s` descriptors; flat `toml_map()` for s64, f64,
      bool, string-buffer, string-view; required/defaults
- [ ] M4.2 Constraints: min/max; buffer overflow is an error; failures
      carry value spans
- [ ] M4.3 Enum mapping tables
- [ ] M4.4 Nested tables; arrays (fixed capacity and callback forms)
- [ ] M4.5 Unknown-key policy: IGNORE / WARN / ERROR
- [ ] M4.6 Realistic example program; stretch: `toml_unmap()` reverse
      direction

### M5 — Diagnostics
- [ ] M5.1 Secondary spans (duplicate definitions, unclosed strings)
- [ ] M5.2 Renderer core: lazy line/column (Unicode scalar columns),
      caller-buffer, zero heap; `toml_err_print()` upgraded to full
      rendering
- [ ] M5.3 Long-line windowing; ANSI color flag; help table;
      `TOML_NO_ERROR_RENDER` gate
- [ ] M5.4 Golden tests over full invalid corpus + schema errors; one
      doc page per error code

### M6 — Editing and write-back
- [ ] M6.1 Edit foundation: arena-appended text + span repointing;
      replace value (same type)
- [ ] M6.2 `pick_string_style()` pure function with exhaustive unit
      tests; type-changing replacement
- [ ] M6.3 Add key to existing table (indentation mimicry)
- [ ] M6.4 Array append (style mimicry of previous element)
- [ ] M6.5 Delete key (line + trailing comment + attached comment
      block)
- [ ] M6.6 Add `[section]`; insertion position control
- [ ] M6.7 Property test: reparse-after-random-edit-sequence semantic
      equivalence; diff-minimality test

### M7 — Memory system completion
- [ ] M7.1 Borrow mode; `toml_parse_in_buffer()`
- [ ] M7.2 Fixed-mode `TOML_ERR_NOMEM` + `mem_needed`; documented
      upper bound formula
- [ ] M7.3 `toml_arena_reset()`; zero-allocation hot-reload loop test
- [ ] M7.4 Chunked dynamic backend (source stays contiguous); full
      suite under 16-byte chunks
- [ ] M7.5 Three-layer allocator; failure-injection tests
- [ ] **Gate: full suite green in all four modes (owned/borrow x
      fixed/dynamic)**

### M8 — Hardening and 1.0 release
- [ ] M8.1 Fuzz harnesses (parse + edit sequences); accumulate hours
- [ ] M8.2 DoS limits (nesting depth, total allocation), configurable
- [ ] M8.3 Benchmarks vs tomlc17 / toml-c
- [ ] M8.4 C11 `_Generic` sugar layer with verified C99 fallback
- [ ] M8.5 `dist`: single-header amalgamation
- [ ] M8.6 Docs: API reference, error-code manual, trimming table,
      badges

## License

MIT Licensed. See `LICENSE`.

---
