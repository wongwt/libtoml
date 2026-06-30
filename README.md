# libtoml

Modern C TOML v1.1 Parser Library

## Building

```sh
make            # builds build/libtoml.a and build/libtoml.so
make test       # builds the test binaries under build/test
make check      # builds and runs all tests
make clean      # removes the build/ directory
```

### Installing

```sh
make install                       # installs to /usr/local
make install prefix=/opt/libtoml   # or a custom prefix
make uninstall
```

## Usage

Include `toml.h` to use the library. There's no single required way to bring
it into your build:

- Link your program against `libtoml.a` or `libtoml.so` (see Building above), or
- Copy `src/toml.c` and `src/toml.h` directly into your own source tree and
  compile them as part of your project. The implementation is a single
  `.c`/`.h` pair with no dependencies beyond the C standard library, so it
  drops in cleanly without conflicting with the rest of your codebase.

```c
#include <stdio.h>
#include "toml.h"

int main(void) {
    toml_t *toml = toml_from_str("key = \"value\"\n");
    if (toml == NULL) {
        fprintf(stderr, "failed to parse\n");
        return 1;
    }

    // ... use toml ...

    toml_free(&toml);
    return 0;
}
```

## License

MIT. See [LICENSE](LICENSE).
