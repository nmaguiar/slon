# SLON for Node.js

Zero-dependency SLON parser and formatter for Node.js runtimes.

## Installation

```bash
npm install ./slon/nodejs
```

## Usage

```javascript
const { parse, stringify } = require('./nodejs');

const data = parse("(status: ok, items: [1 | 2 | 3], generatedAt: 2024-03-01/18:22:10.001)");
console.log(data.status); // "ok"

const slonString = stringify(data);
```

Datetime literals are converted into `Date` instances normalised to UTC.
