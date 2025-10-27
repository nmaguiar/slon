# SLON for Go

Idiomatic Go parser and formatter for SLON (Single Line Object Notation).

## Installation

```bash
cd slon/go
go mod tidy
```

Add the module to your project using a `replace` directive if you consume it from a different repository:

```go
require github.com/nunoaguiar/slon/go/slon v0.0.0

replace github.com/nunoaguiar/slon/go/slon => ../path/to/slon/go
```

## Usage

```go
package main

import (
  "fmt"
  "github.com/nunoaguiar/slon/go/slon"
)

func main() {
  value, err := slon.Parse("(status: ok, metrics: [1 | 2 | 3], generatedAt: 2024-03-01/18:22:10.001)")
  if err != nil {
    panic(err)
  }
  fmt.Println(value.(map[string]any)["status"])
}
```

Datetime literals are parsed into `time.Time` instances in UTC. Use `slon.Stringify` to serialise Go values back into SLON format.
