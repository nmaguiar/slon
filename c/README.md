# SLON for C

Portable C99 parser and formatter for SLON (Single Line Object Notation). The library parses SLON into an in-memory tree and can serialise the structure back to SLON text.

## Build

```bash
cd c
make
```

This produces `libslon.a` inside `build/`. Update `CFLAGS` in the `Makefile` to suit your environment.

## Usage

```c
#include "slon.h"
#include <stdio.h>

int main(void) {
  const char *text = "(status: ok, generatedAt: 2024-03-01/18:22:10.001)";
  slon_value *value = NULL;
  slon_error error;

  if (slon_parse(text, &value, &error) != 0) {
    fprintf(stderr, "Parse error at %d: %s\n", error.position, error.message);
    return 1;
  }

  char *roundtrip = slon_stringify(value);
  printf("%s\n", roundtrip);

  slon_stringify_free(roundtrip);
  slon_value_free(value);
  return 0;
}
```

Datetime literals are converted into `slon_datetime` structures (UTC). Remember to call `slon_value_free` on all parsed values to release heap memory.
