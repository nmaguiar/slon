# SLON for Rust

Reference Rust implementation of the SLON (Single Line Object Notation) grammar.

## Setup

```bash
cd slon/rust
cargo test
```

Add it to your project by including a path dependency:

```toml
[dependencies]
slon = { path = "../path/to/slon/rust" }
```

## Usage

```rust
use slon::{parse, stringify, Value};

fn main() -> Result<(), Box<dyn std::error::Error>> {
    let value = parse("(status: ok, metrics: [1 | 2 | 3], generatedAt: 2024-03-01/18:22:10.001)")?;
    if let Value::Object(obj) = value {
        println!("{:?}", obj.get("status"));
    }
    Ok(())
}
```

Datetime literals are parsed into `chrono::DateTime<Utc>` values. The `Value` enum mirrors JSON with an additional `Date` variant. Use `stringify` to emit SLON strings from parsed data.
