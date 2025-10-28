# SLON for Swift

Swift Package Manager module for parsing and serialising SLON (Single Line Object Notation).

## Building

```bash
cd slon/swift
swift build
```

## Usage

```swift
import Slon

let payload = "(status: ok, metrics: [1 | 2 | 3], generatedAt: 2024-03-01/18:22:10.001)"
let value = try Slon.parse(payload)

if let object = value as? [String: Any],
   let status = object["status"] as? String {
    print(status)
}
```

Datetime literals parse into `Date` values in UTC. Use `Slon.stringify` to produce SLON strings from Swift values.
