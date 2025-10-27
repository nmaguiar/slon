use chrono::{DateTime, NaiveDateTime, Utc};
use std::collections::BTreeMap;

#[derive(Debug, Clone, PartialEq)]
pub enum Value {
    Null,
    Bool(bool),
    Number(f64),
    String(String),
    Array(Vec<Value>),
    Object(BTreeMap<String, Value>),
    Date(DateTime<Utc>),
}

#[derive(Debug, Clone, PartialEq, Eq)]
pub struct Error {
    pub message: String,
    pub position: usize,
}

impl std::fmt::Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        write!(f, "{} at position {}", self.message, self.position)
    }
}

impl std::error::Error for Error {}

pub fn parse(input: &str) -> Result<Value, Error> {
    let mut parser = Parser::new(input);
    parser.skip_whitespace();
    let value = parser.parse_value()?;
    parser.skip_whitespace();
    if !parser.is_end() {
        return Err(parser.error("Unexpected trailing content"));
    }
    Ok(value)
}

pub fn stringify(value: &Value) -> String {
    format_value(value)
}

struct Parser<'a> {
    input: &'a [u8],
    len: usize,
    pos: usize,
}

impl<'a> Parser<'a> {
    fn new(input: &'a str) -> Self {
        Self {
            input: input.as_bytes(),
            len: input.len(),
            pos: 0,
        }
    }

    fn is_end(&self) -> bool {
        self.pos >= self.len
    }

    fn peek(&self) -> Option<u8> {
        if self.is_end() {
            None
        } else {
            Some(self.input[self.pos])
        }
    }

    fn advance(&mut self, count: usize) {
        self.pos += count;
    }

    fn skip_whitespace(&mut self) {
        while let Some(&c) = self.input.get(self.pos) {
            if !matches!(c, b' ' | b'\n' | b'\t' | b'\r') {
                break;
            }
            self.pos += 1;
        }
    }

    fn parse_value(&mut self) -> Result<Value, Error> {
        self.skip_whitespace();
        let ch = self.peek().ok_or_else(|| self.error("Unexpected end of input"))?;
        match ch {
            b'(' => self.parse_object(),
            b'[' => self.parse_array(),
            b'\'' | b'"' => self.parse_quoted_string(),
            b'-' | b'0'..=b'9' => {
                if let Some(value) = self.try_parse_datetime()? {
                    return Ok(Value::Date(value));
                }
                self.parse_number()
            }
            _ => {
                if let Some(value) = self.match_keyword(b"true") {
                    return Ok(Value::Bool(true));
                }
                if let Some(value) = self.match_keyword(b"false") {
                    return Ok(Value::Bool(false));
                }
                if let Some(value) = self.match_keyword(b"null") {
                    return Ok(Value::Null);
                }
                self.parse_unquoted_string().map(Value::String)
            }
        }
    }

    fn parse_object(&mut self) -> Result<Value, Error> {
        self.advance(1); // '('
        let mut map = BTreeMap::new();
        self.skip_whitespace();
        if self.peek() == Some(b')') {
            self.advance(1);
            return Ok(Value::Object(map));
        }
        loop {
            self.skip_whitespace();
            let key = self.parse_string_like()?;
            self.skip_whitespace();
            if self.peek() != Some(b':') {
                return Err(self.error("Expected ':' after key"));
            }
            self.advance(1);
            let value = self.parse_value()?;
            map.insert(key, value);
            self.skip_whitespace();
            match self.peek() {
                Some(b',') => {
                    self.advance(1);
                    continue;
                }
                Some(b')') => {
                    self.advance(1);
                    break;
                }
                _ => return Err(self.error("Expected ',' or ')'")),
            }
        }
        Ok(Value::Object(map))
    }

    fn parse_array(&mut self) -> Result<Value, Error> {
        self.advance(1); // '['
        let mut items = Vec::new();
        self.skip_whitespace();
        if self.peek() == Some(b']') {
            self.advance(1);
            return Ok(Value::Array(items));
        }
        loop {
            let value = self.parse_value()?;
            items.push(value);
            self.skip_whitespace();
            match self.peek() {
                Some(b'|') => {
                    self.advance(1);
                }
                Some(b']') => {
                    self.advance(1);
                    break;
                }
                _ => return Err(self.error("Expected '|' or ']'")),
            }
        }
        Ok(Value::Array(items))
    }

    fn parse_string_like(&mut self) -> Result<String, Error> {
        match self.peek() {
            Some(b'\'') | Some(b'"') => self.parse_quoted_string(),
            _ => self.parse_unquoted_string(),
        }
    }

    fn parse_quoted_string(&mut self) -> Result<String, Error> {
        let quote = self.peek().unwrap();
        self.advance(1);
        let mut result = String::new();
        while let Some(ch) = self.peek() {
            if ch == quote {
                self.advance(1);
                return Ok(result);
            }
            if ch == b'\\' {
                self.advance(1);
                let escaped = self.peek().ok_or_else(|| self.error("Invalid escape"))?;
                result.push(self.parse_escape(escaped)?);
                continue;
            }
            result.push(ch as char);
            self.advance(1);
        }
        Err(self.error("Unterminated string literal"))
    }

    fn parse_escape(&mut self, escape: u8) -> Result<char, Error> {
        self.advance(1);
        let ch = match escape {
            b'"' => '"',
            b'\'' => '\'',
            b'\\' => '\\',
            b'/' => '/',
            b'b' => '\u{0008}',
            b'f' => '\u{000c}',
            b'n' => '\n',
            b'r' => '\r',
            b't' => '\t',
            b'u' => {
                let start = self.pos;
                let end = start + 4;
                if end > self.len {
                    return Err(self.error("Invalid unicode escape"));
                }
                let hex = &self.input[start..end];
                let hex_str = std::str::from_utf8(hex).unwrap();
                let code = u16::from_str_radix(hex_str, 16)
                    .map_err(|_| self.error("Invalid unicode escape"))?;
                self.advance(4);
                char::from_u32(code as u32).ok_or_else(|| self.error("Invalid unicode scalar value"))?
            }
            _ => return Err(self.error("Unknown escape sequence")),
        };
        Ok(ch)
    }

    fn parse_unquoted_string(&mut self) -> Result<String, Error> {
        let start = self.pos;
        while let Some(ch) = self.peek() {
            if is_delimiter(ch) || is_whitespace(ch) {
                break;
            }
            self.advance(1);
        }
        let slice = &self.input[start..self.pos];
        let trimmed = std::str::from_utf8(slice).unwrap().trim();
        if trimmed.is_empty() {
            return Err(self.error("Empty string value"));
        }
        Ok(trimmed.to_owned())
    }

    fn parse_number(&mut self) -> Result<Value, Error> {
        let start = self.pos;
        while let Some(ch) = self.peek() {
            if matches!(ch, b'+' | b'-' | b'.' | b'e' | b'E' | b'0'..=b'9') {
                self.advance(1);
            } else {
                break;
            }
        }
        let end = self.pos;
        let slice = &self.input[start..end];
        let s = std::str::from_utf8(slice).unwrap();
        if end < self.len {
            let next = self.input[end];
            if !is_delimiter(next) && !is_whitespace(next) {
                return Err(self.error("Invalid number boundary"));
            }
        }
        let number = s.parse::<f64>().map_err(|_| self.error("Invalid number"))?;
        if !number.is_finite() {
            return Err(self.error("Non-finite number"));
        }
        Ok(Value::Number(number))
    }

    fn try_parse_datetime(&mut self) -> Result<Option<DateTime<Utc>>, Error> {
        if self.pos + 23 > self.len {
            return Ok(None);
        }
        let candidate = &self.input[self.pos..self.pos + 23];
        let slice = std::str::from_utf8(candidate).unwrap();
        if let Ok(naive) = NaiveDateTime::parse_from_str(slice, "%Y-%m-%d/%H:%M:%S.%3f") {
            if self.pos + 23 < self.len {
                let boundary = self.input[self.pos + 23];
                if !is_delimiter(boundary) && !is_whitespace(boundary) {
                    return Ok(None);
                }
            }
            self.advance(23);
            return Ok(Some(DateTime::<Utc>::from_utc(naive, Utc)));
        }
        Ok(None)
    }

    fn match_keyword(&mut self, keyword: &[u8]) -> Option<Value> {
        if self.input[self.pos..].starts_with(keyword) {
            let end = self.pos + keyword.len();
            if end == self.len || is_delimiter(self.input[end]) || is_whitespace(self.input[end]) {
                self.pos = end;
                match keyword {
                    b"true" => return Some(Value::Bool(true)),
                    b"false" => return Some(Value::Bool(false)),
                    b"null" => return Some(Value::Null),
                    _ => {}
                }
            }
        }
        None
    }

    fn error(&self, message: &str) -> Error {
        Error {
            message: message.to_owned(),
            position: self.pos,
        }
    }
}

fn is_delimiter(ch: u8) -> bool {
    matches!(ch, b':' | b',' | b'(' | b')' | b'[' | b']' | b'|')
}

fn is_whitespace(ch: u8) -> bool {
    matches!(ch, b' ' | b'\t' | b'\r' | b'\n')
}

fn format_value(value: &Value) -> String {
    match value {
        Value::Null => "null".to_owned(),
        Value::Bool(true) => "true".to_owned(),
        Value::Bool(false) => "false".to_owned(),
        Value::Number(num) => {
            if num.fract() == 0.0 {
                format!("{:.0}", num)
            } else {
                num.to_string()
            }
        }
        Value::String(s) => format_string(s),
        Value::Array(items) => {
            let joined = items
                .iter()
                .map(format_value)
                .collect::<Vec<_>>()
                .join(" | ");
            format!("[{}]", joined)
        }
        Value::Object(map) => {
            let joined = map
                .iter()
                .map(|(key, value)| format!("{}: {}", format_key(key), format_value(value)))
                .collect::<Vec<_>>()
                .join(", ");
            format!("({})", joined)
        }
        Value::Date(dt) => dt.format("%Y-%m-%d/%H:%M:%S.%3f").to_string(),
    }
}

fn format_key(key: &str) -> String {
    if key.is_empty() || key.chars().any(|c| matches!(c, ':' | ',' | '(' | ')' | '[' | ']' | '|' | '"' | '\'' ) || c.is_whitespace()) {
        format_string(key)
    } else {
        key.to_owned()
    }
}

fn format_string(value: &str) -> String {
    let escaped = value
        .replace('\\', "\\\\")
        .replace('\'', "\\'")
        .replace('\n', "\\n")
        .replace('\r', "\\r")
        .replace('\t', "\\t");
    format!("'{}'", escaped)
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn parses_object() {
        let value = parse("(status: ok, count: 3)").unwrap();
        if let Value::Object(map) = value {
            assert_eq!(map.get("status"), Some(&Value::String("ok".to_owned())));
        } else {
            panic!("expected object");
        }
    }

    #[test]
    fn parses_datetime() {
        let value = parse("(at: 2024-03-01/18:22:10.001)").unwrap();
        if let Value::Object(map) = value {
            assert!(matches!(map.get("at").unwrap(), Value::Date(_)));
        } else {
            panic!("expected object");
        }
    }
}
