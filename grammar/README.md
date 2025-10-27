# SLON Grammar

This directory contains the canonical [Peggy](https://peggyjs.org/) (PEG.js-compatible) grammar that defines the Single Line Object Notation syntax.

## Building a Parser

Generate a CommonJS parser with:

```bash
npx peggy --format commonjs --output ../dist/slon-parser.js slon.pegjs
```

Adjust `--output` to your preferred destination. The resulting module exposes a single `parse` function that returns JavaScript values (`Object`, `Array`, primitives, and `Date` instances for datetime literals).

## Grammar Highlights

- `SLON_text` is the entry rule; it trims leading/trailing whitespace before parsing a value.
- Objects use parentheses for delimiters, while arrays retain JSON’s square brackets but swap commas for pipe separators.
- Datetime literals follow `YYYY-MM-DD/HH:MM:SS.mmm` and are transformed into JavaScript `Date` objects in UTC.
- Unquoted strings absorb everything until `:`, `,`, `(`, `)`, or quotes; use single or double quotes when you need punctuation or whitespace preserved.
- All JSON escape sequences are supported, including Unicode (`\uXXXX`).

## Developing the Grammar

- Install Peggy locally for iterative work: `npm install --save-dev peggy`.
- Regenerate the parser after changes: `npx peggy --watch ...` can rebuild on save.
- Add your own unit tests by requiring the generated parser and asserting on the resulting JavaScript values.

Keeping the grammar small and explicit makes it easy to embed SLON support in environments where full JSON would be visually noisy.
