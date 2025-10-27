# SLON

## Introduction

SLON (Single Line Object Notation) is a lightweight variation of JSON designed for snippets that need to stay human-readable on a single line (for example notifications, logs, and in-line dashboards). It keeps JSON’s data model while swapping the most visually noisy delimiters for symbols that read closer to natural language. Think of it as a compact, single-line alternative to YAML.

Example of a weather notification using SLON:

````text
The current conditions are (condition: Moderate Rain, temp: 12.2, feelsLike: 14, sunLight: true, date: 2023-02-05/12:34:45.678)
````

The same payload expressed as JSON:

````json
The current conditions are {"condition":"Moderate Rain","temp":12.2,"feelsLike":14,"sunLight":true,"date":"2023-02-05T12:34:45.678Z"}
````

## Syntax Differences

The SLON grammar lives in `grammar/slon.pegjs`. Compared with JSON:

| Concept | JSON | SLON |
|---------|------|------|
| Object delimiters | `{` `}` | `(` `)` |
| Array delimiters | `[` `]` | `[` `]` (unchanged) |
| Array separator | `,` | `\|` |
| Key/value separator | `:` | `:` (unchanged) |
| String quoting | Always required | Optional when the value does not contain `:`, `,`, `(`, `)` or quotes |
| Datetime literal | ISO 8601 (`T`/`Z`) | `YYYY-MM-DD/HH:MM:SS.mmm` (parsed into a JavaScript `Date`) |

Additional conveniences:
- Whitespace is optional anywhere delimiters are allowed.
- Single quotes and double quotes are both supported for quoted strings, with the usual JSON escape sequences.
- Bare words are trimmed, so `(status:  ok )` normalises to `(status: ok)`.

## Supported Values

All JSON primitive types are supported, plus a dedicated datetime literal:

| Type | Example | Notes |
|------|---------|-------|
| Boolean | `(enabled: true)` | Same literals as JSON (`true`, `false`). |
| Null | `(value: null)` | Matches JSON. |
| Number | `(ratio: -12.45e-3)` | Parsed with `parseFloat`, so both integers and decimals work. |
| String | `(city: 'Lisbon')` | Unquoted strings are allowed if they contain none of `: , ( ) ' "`. |
| Array | `[1 \| 'two' \| 3]` | Uses the pipe character as the element separator. |
| Object | `(x: -1, y: 2)` | Parentheses make nested maps easy to read. |
| Datetime | `(generatedAt: 2024-02-15/09:30:45.123)` | Converted to a JavaScript `Date` by the parser. |

## Working with SLON

### Parsing

The grammar is implemented in PEG.js format. You can compile it with [Peggy](https://peggyjs.org/) (the actively maintained fork of PEG.js):

```bash
npx peggy --format commonjs --output dist/slon-parser.js grammar/slon.pegjs
```

The generated parser exposes a `parse` function that returns plain JavaScript values:

```javascript
const slon = require('./dist/slon-parser');

const payload = "(status: ok, metrics: [12 \| 43.5 \| 87], generatedAt: 2024-03-01/18:22:10.001)";
const parsed = slon.parse(payload);
// parsed => { status: "ok", metrics: [12, 43.5, 87], generatedAt: new Date("2024-03-01T18:22:10.001Z") }
```

### Converting to JSON

Because SLON is a strict subset of JSON’s data model, converting is trivial once you parse it:

```javascript
const parsed = slon.parse("(user: jane, roles: ['viewer' \| 'editor'])");
const jsonString = JSON.stringify(parsed); // {"user":"jane","roles":["viewer","editor"]}
```

### Creating SLON Strings

To produce SLON from existing data, you can format the JSON output manually or reuse the same delimiters:

```javascript
function toSlon(obj) {
  if (Array.isArray(obj)) {
    return `[${obj.map(toSlon).join(' | ')}]`;
  }
  if (obj && typeof obj === 'object') {
    return `(${Object.entries(obj).map(([key, value]) => `${key}: ${toSlon(value)}`).join(', ')})`;
  }
  if (obj instanceof Date) {
    const pad = (n, len) => String(n).padStart(len, '0');
    return `${obj.getUTCFullYear()}-${pad(obj.getUTCMonth() + 1, 2)}-${pad(obj.getUTCDate(), 2)}/${pad(obj.getUTCHours(), 2)}:${pad(obj.getUTCMinutes(), 2)}:${pad(obj.getUTCSeconds(), 2)}.${pad(obj.getUTCMilliseconds(), 3)}`;
  }
  if (typeof obj === 'string') {
    return /[:,"'()]/.test(obj) ? `'${obj.replace(/'/g, "\\'")}'` : obj;
  }
  return String(obj);
}
```

## Practical Examples

- **Log entry:** `(level: warn, message: 'Cluster is above threshold', currentUsage: 87.1, threshold: 75)`
- **Compact configuration:** `(service: api, replicas: 3, regions: [eu-west-1 | us-east-1], maintenance: 2024-06-10/01:00:00.000)`
- **Incident notification:** `(priority: high, title: 'Queue backlog', backlogSize: 1203, acknowledged: false)`

## Caveats and Tips

- Datetime literals are parsed as UTC timestamps. If you prefer local time keep a note in your consumer logic.
- Pipes (`|`) remain the array separator even when values are spread across multiple lines. This keeps the grammar simple but means commas inside arrays must be quoted.
- Unquoted strings stop at `:`, `,`, `(`, `)`, or quotes. Use single quotes if you expect punctuation.
- The grammar intentionally mirrors JSON’s number handling, so values such as `NaN` or `Infinity` are not valid.

## See Also

- `grammar/slon.pegjs` – canonical grammar definition.
- Language clients:
  - `python/` – Python parser and formatter.
  - `nodejs/` – Node.js runtime package.
  - `go/` – Go module exposing `Parse` and `Stringify`.
  - `rust/` – Rust crate with a strongly typed `Value` enum.
- [Peggy documentation](https://peggyjs.org/documentation.html) – tools and options for generating parsers.
- [JSON specification](https://www.json.org/json-en.html) – SLON diverges minimally, so JSON resources remain relevant.
