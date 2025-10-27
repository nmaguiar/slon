"use strict";

const DATETIME_RE =
  /^(\d{4})-(\d{2})-(\d{2})\/(\d{2}):(\d{2}):(\d{2})\.(\d{3})/;
const NUMBER_RE =
  /^-?(?:0|[1-9]\d*)(?:\.\d+)?(?:[eE][+-]?\d+)?/;
const DELIMS = new Set([":", ",", "(", ")", "[", "]", "|"]);
const WHITESPACE = /\s/;

class Parser {
  constructor(text) {
    this.text = text;
    this.length = text.length;
    this.pos = 0;
  }

  parse() {
    this.skipWhitespace();
    const value = this.parseValue();
    this.skipWhitespace();
    if (this.pos !== this.length) {
      throw new Error(`Unexpected trailing content at position ${this.pos}`);
    }
    return value;
  }

  peek() {
    return this.pos < this.length ? this.text[this.pos] : null;
  }

  advance(count = 1) {
    this.pos += count;
  }

  skipWhitespace() {
    while (this.pos < this.length && WHITESPACE.test(this.text[this.pos])) {
      this.pos += 1;
    }
  }

  expect(char) {
    if (this.peek() !== char) {
      throw new Error(`Expected '${char}' at position ${this.pos}`);
    }
    this.advance();
  }

  parseValue() {
    this.skipWhitespace();
    const ch = this.peek();
    if (ch === null) {
      throw new Error("Unexpected end of input");
    }
    if (ch === "(") {
      return this.parseObject();
    }
    if (ch === "[") {
      return this.parseArray();
    }
    if (ch === "'" || ch === '"') {
      return this.parseQuotedString();
    }
    if (ch === "-" || (ch >= "0" && ch <= "9")) {
      const datetime = this.tryParseDateTime();
      if (datetime !== null) {
        return datetime;
      }
      return this.parseNumber();
    }
    if (this.matchKeyword("true")) {
      return true;
    }
    if (this.matchKeyword("false")) {
      return false;
    }
    if (this.matchKeyword("null")) {
      return null;
    }
    return this.parseUnquotedString();
  }

  matchKeyword(keyword) {
    const end = this.pos + keyword.length;
    if (this.text.slice(this.pos, end) === keyword) {
      if (
        end === this.length ||
        DELIMS.has(this.text[end]) ||
        WHITESPACE.test(this.text[end])
      ) {
        this.pos = end;
        return true;
      }
    }
    return false;
  }

  parseObject() {
    this.expect("(");
    const result = {};
    this.skipWhitespace();
    if (this.peek() === ")") {
      this.advance();
      return result;
    }
    while (true) {
      this.skipWhitespace();
      const key = this.parseStringLike();
      this.skipWhitespace();
      this.expect(":");
      const value = this.parseValue();
      result[key] = value;
      this.skipWhitespace();
      const ch = this.peek();
      if (ch === ",") {
        this.advance();
        continue;
      }
      if (ch === ")") {
        this.advance();
        break;
      }
      throw new Error(`Expected ',' or ')' at position ${this.pos}`);
    }
    return result;
  }

  parseArray() {
    this.expect("[");
    const result = [];
    this.skipWhitespace();
    if (this.peek() === "]") {
      this.advance();
      return result;
    }
    while (true) {
      const value = this.parseValue();
      result.push(value);
      this.skipWhitespace();
      const ch = this.peek();
      if (ch === "|") {
        this.advance();
        continue;
      }
      if (ch === "]") {
        this.advance();
        break;
      }
      throw new Error(`Expected '|' or ']' at position ${this.pos}`);
    }
    return result;
  }

  parseStringLike() {
    const ch = this.peek();
    if (ch === "'" || ch === '"') {
      return this.parseQuotedString();
    }
    return this.parseUnquotedString();
  }

  parseQuotedString() {
    const quote = this.peek();
    this.advance();
    const chars = [];
    while (true) {
      const ch = this.peek();
      if (ch === null) {
        throw new Error("Unterminated string literal");
      }
      if (ch === quote) {
        this.advance();
        break;
      }
      if (ch === "\\") {
        this.advance();
        const esc = this.peek();
        if (esc === null) {
          throw new Error("Invalid escape at end of string");
        }
        chars.push(this.parseEscape(esc));
        this.advance();
      } else {
        chars.push(ch);
        this.advance();
      }
    }
    return chars.join("");
  }

  parseEscape(esc) {
    const map = {
      '"': '"',
      "'": "'",
      "\\": "\\",
      "/": "/",
      b: "\b",
      f: "\f",
      n: "\n",
      r: "\r",
      t: "\t",
    };
    if (map[esc]) {
      return map[esc];
    }
    if (esc === "u") {
      const start = this.pos + 1;
      const hex = this.text.slice(start, start + 4);
      if (!/^[0-9a-fA-F]{4}$/.test(hex)) {
        throw new Error("Invalid unicode escape sequence");
      }
      this.pos = start + 3;
      return String.fromCharCode(parseInt(hex, 16));
    }
    throw new Error(`Unknown escape '\\${esc}'`);
  }

  parseUnquotedString() {
    const start = this.pos;
    while (this.pos < this.length) {
      const ch = this.text[this.pos];
      if (DELIMS.has(ch) || WHITESPACE.test(ch)) {
        break;
      }
      this.pos += 1;
    }
    const value = this.text.slice(start, this.pos).trim();
    if (!value) {
      throw new Error(`Empty string value at position ${start}`);
    }
    return value;
  }

  parseNumber() {
    const match = this.text.slice(this.pos).match(NUMBER_RE);
    if (!match) {
      throw new Error(`Invalid number at position ${this.pos}`);
    }
    const number = match[0];
    const end = this.pos + number.length;
    if (
      end !== this.length &&
      !DELIMS.has(this.text[end]) &&
      !WHITESPACE.test(this.text[end])
    ) {
      throw new Error(`Invalid number boundary at position ${end}`);
    }
    this.pos = end;
    return Number(number);
  }

  tryParseDateTime() {
    const match = this.text.slice(this.pos).match(DATETIME_RE);
    if (!match) {
      return null;
    }
    const [full, year, month, day, hour, minute, second, ms] = match;
    const end = this.pos + full.length;
    if (
      end !== this.length &&
      !DELIMS.has(this.text[end]) &&
      !WHITESPACE.test(this.text[end])
    ) {
      return null;
    }
    const date = new Date(
      Date.UTC(
        Number(year),
        Number(month) - 1,
        Number(day),
        Number(hour),
        Number(minute),
        Number(second),
        Number(ms)
      )
    );
    this.pos = end;
    return date;
  }
}

function parse(text) {
  return new Parser(text).parse();
}

function stringify(value) {
  return formatValue(value);
}

function formatValue(value) {
  if (value === null || value === undefined) {
    return "null";
  }
  if (value instanceof Date) {
    return formatDate(value);
  }
  if (Array.isArray(value)) {
    return `[${value.map((item) => formatValue(item)).join(" | ")}]`;
  }
  if (typeof value === "number" || typeof value === "bigint") {
    return String(value);
  }
  if (typeof value === "boolean") {
    return value ? "true" : "false";
  }
  if (typeof value === "string") {
    return formatString(value);
  }
  if (typeof value === "object") {
    const keys = Object.keys(value).sort();
    const parts = keys.map((key) => `${formatKey(key)}: ${formatValue(value[key])}`);
    return `(${parts.join(", ")})`;
  }
  throw new TypeError(`Unsupported type: ${typeof value}`);
}

function pad(n, width) {
  const s = String(n);
  return s.length >= width ? s : "0".repeat(width - s.length) + s;
}

function formatDate(value) {
  return (
    `${pad(value.getUTCFullYear(), 4)}-${pad(value.getUTCMonth() + 1, 2)}-${pad(value.getUTCDate(), 2)}` +
    `/${pad(value.getUTCHours(), 2)}:${pad(value.getUTCMinutes(), 2)}:${pad(value.getUTCSeconds(), 2)}.${pad(value.getUTCMilliseconds(), 3)}`
  );
}

function formatKey(key) {
  if (!key || /[:,"'()\[\]\|\s]/.test(key)) {
    return formatString(key);
  }
  return key;
}

function formatString(value) {
  return `'${value
    .replace(/\\/g, "\\\\")
    .replace(/'/g, "\\'")
    .replace(/\n/g, "\\n")
    .replace(/\r/g, "\\r")
    .replace(/\t/g, "\\t")}'`;
}

module.exports = {
  parse,
  stringify,
};
