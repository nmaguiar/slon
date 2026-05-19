# SLON for C#

Reference .NET implementation of a SLON (Single Line Object Notation) parser and formatter.

## Build

```bash
cd csharp/tests/Slon.Tests
dotnet test
```

## Usage

```csharp
using Slon;
using System;
using System.Collections.Generic;

var value = Slon.Parse("(status: ok, generatedAt: 2024-03-01/18:22:10.001)");
var map = (Dictionary<string, object?>)value!;
Console.WriteLine(map["status"]); // ok

map["generatedAt"] = DateTime.UtcNow;
var slon = Slon.Stringify(map);
Console.WriteLine(slon);
```

Parsing returns standard .NET types (`Dictionary<string, object?>`, `List<object?>`, primitives). Datetime literals are converted to UTC `DateTime` values. `Slon.Stringify` performs the reverse operation.
