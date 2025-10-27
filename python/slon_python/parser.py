import io
import re
from datetime import datetime, timezone
from typing import Any, Dict, Iterable, List, Union


class SLONDecodeError(ValueError):
    """Raised when parsing fails."""


_DATETIME_RE = re.compile(
    r"""
    (?P<year>\d{4})-
    (?P<month>\d{2})-
    (?P<day>\d{2})/
    (?P<hour>\d{2}):
    (?P<minute>\d{2}):
    (?P<second>\d{2})\.
    (?P<millisecond>\d{3})
    """,
    re.VERBOSE,
)

_NUMBER_RE = re.compile(r"-?(?:0|[1-9]\d*)(?:\.\d+)?(?:[eE][+-]?\d+)?")
_DELIMS = {":", ",", "(", ")", "[", "]", "|"}
_WS = {" ", "\t", "\r", "\n"}


class _Parser:
    def __init__(self, text: str):
        self.text = text
        self.length = len(text)
        self.pos = 0

    def parse(self) -> Any:
        self._skip_ws()
        value = self._parse_value()
        self._skip_ws()
        if self.pos != self.length:
            raise SLONDecodeError(f"Unexpected trailing content at position {self.pos}")
        return value

    def _peek(self) -> Union[str, None]:
        return self.text[self.pos] if self.pos < self.length else None

    def _advance(self, count: int = 1) -> None:
        self.pos += count

    def _skip_ws(self) -> None:
        while self.pos < self.length and self.text[self.pos] in _WS:
            self.pos += 1

    def _expect(self, char: str) -> None:
        if self._peek() != char:
            raise SLONDecodeError(f"Expected '{char}' at position {self.pos}")
        self._advance()

    def _parse_value(self) -> Any:
        self._skip_ws()
        ch = self._peek()
        if ch is None:
            raise SLONDecodeError("Unexpected end of input")
        if ch == "(":
            return self._parse_object()
        if ch == "[":
            return self._parse_array()
        if ch in {"'", '"'}:
            return self._parse_quoted_string()
        if ch in "-0123456789":
            dt = self._try_parse_datetime()
            if dt is not None:
                return dt
            return self._parse_number()
        if self._match_keyword("true"):
            return True
        if self._match_keyword("false"):
            return False
        if self._match_keyword("null"):
            return None
        return self._parse_unquoted_string()

    def _match_keyword(self, keyword: str) -> bool:
        end = self.pos + len(keyword)
        if self.text[self.pos:end] == keyword:
            boundary = end == self.length or self.text[end] in _DELIMS or self.text[end] in _WS
            if boundary:
                self.pos = end
                return True
        return False

    def _parse_object(self) -> Dict[str, Any]:
        self._expect("(")
        obj: Dict[str, Any] = {}
        self._skip_ws()
        if self._peek() == ")":
            self._advance()
            return obj
        while True:
            self._skip_ws()
            key = self._parse_string_like()
            self._skip_ws()
            self._expect(":")
            value = self._parse_value()
            obj[key] = value
            self._skip_ws()
            ch = self._peek()
            if ch == ",":
                self._advance()
                continue
            if ch == ")":
                self._advance()
                break
            raise SLONDecodeError(f"Expected ',' or ')' at position {self.pos}")
        return obj

    def _parse_array(self) -> List[Any]:
        self._expect("[")
        items: List[Any] = []
        self._skip_ws()
        if self._peek() == "]":
            self._advance()
            return items
        while True:
            value = self._parse_value()
            items.append(value)
            self._skip_ws()
            ch = self._peek()
            if ch == "|":
                self._advance()
                continue
            if ch == "]":
                self._advance()
                break
            raise SLONDecodeError(f"Expected '|' or ']' at position {self.pos}")
        return items

    def _parse_quoted_string(self) -> str:
        quote = self._peek()
        assert quote in {"'", '"'}
        self._advance()
        chars: List[str] = []
        while True:
            ch = self._peek()
            if ch is None:
                raise SLONDecodeError("Unterminated string literal")
            if ch == quote:
                self._advance()
                break
            if ch == "\\":
                self._advance()
                esc = self._peek()
                if esc is None:
                    raise SLONDecodeError("Invalid escape at end of string")
                chars.append(self._parse_escape(esc))
                self._advance()
            else:
                chars.append(ch)
                self._advance()
        return "".join(chars)

    def _parse_escape(self, esc: str) -> str:
        mapping = {
            '"': '"',
            "'": "'",
            "\\": "\\",
            "/": "/",
            "b": "\b",
            "f": "\f",
            "n": "\n",
            "r": "\r",
            "t": "\t",
        }
        if esc in mapping:
            return mapping[esc]
        if esc == "u":
            start = self.pos + 1
            end = start + 4
            if end > self.length:
                raise SLONDecodeError("Invalid unicode escape sequence")
            hex_part = self.text[start:end]
            if not re.fullmatch(r"[0-9a-fA-F]{4}", hex_part):
                raise SLONDecodeError("Invalid unicode escape sequence")
            self.pos = end - 1
            return chr(int(hex_part, 16))
        raise SLONDecodeError(f"Unknown escape '\\{esc}'")

    def _parse_unquoted_string(self) -> str:
        start = self.pos
        while self.pos < self.length:
            ch = self.text[self.pos]
            if ch in _DELIMS or ch in _WS:
                break
            self.pos += 1
        value = self.text[start:self.pos].strip()
        if not value:
            raise SLONDecodeError(f"Empty string value at position {start}")
        return value

    def _parse_string_like(self) -> str:
        ch = self._peek()
        if ch in {"'", '"'}:
            return self._parse_quoted_string()
        return self._parse_unquoted_string()

    def _parse_number(self) -> Union[int, float]:
        match = _NUMBER_RE.match(self.text, self.pos)
        if not match:
            raise SLONDecodeError(f"Invalid number at position {self.pos}")
        end = match.end()
        boundary = end == self.length or self.text[end] in _DELIMS or self.text[end] in _WS
        if not boundary:
            raise SLONDecodeError(f"Invalid number boundary at position {end}")
        number = match.group()
        self.pos = end
        if "." in number or "e" in number or "E" in number:
            return float(number)
        return int(number)

    def _try_parse_datetime(self) -> Union[datetime, None]:
        match = _DATETIME_RE.match(self.text, self.pos)
        if not match:
            return None
        end = match.end()
        boundary = end == self.length or self.text[end] in _DELIMS or self.text[end] in _WS
        if not boundary:
            return None
        groups = {k: int(v) for k, v in match.groupdict().items()}
        try:
            dt = datetime(
                groups["year"],
                groups["month"],
                groups["day"],
                groups["hour"],
                groups["minute"],
                groups["second"],
                groups["millisecond"] * 1000,
                tzinfo=timezone.utc,
            )
        except ValueError as exc:
            raise SLONDecodeError(f"Invalid datetime literal at position {self.pos}") from exc
        self.pos = end
        return dt


def loads(text: str) -> Any:
    """Parse a SLON string into Python values."""
    return _Parser(text).parse()


def load(stream: io.TextIOBase) -> Any:
    """Parse SLON content from a file-like object."""
    return loads(stream.read())


def dumps(value: Any) -> str:
    """Serialize Python values into SLON."""
    return _stringify(value)


def dump(value: Any, stream: io.TextIOBase) -> None:
    """Serialize Python values as SLON into a file-like object."""
    stream.write(dumps(value))


def _stringify(value: Any) -> str:
    if value is None:
        return "null"
    if isinstance(value, bool):
        return "true" if value else "false"
    if isinstance(value, (int, float)):
        return repr(value)
    if isinstance(value, datetime):
        dt = value.astimezone(timezone.utc)
        return dt.strftime("%Y-%m-%d/%H:%M:%S.%f")[:-3]
    if isinstance(value, str):
        return _format_string(value)
    if isinstance(value, dict):
        items: List[str] = []
        for key in sorted(value.keys()):
            items.append(f"{_format_key(key)}: {_stringify(value[key])}")
        return f"({', '.join(items)})"
    if isinstance(value, (list, tuple)):
        return f"[{' | '.join(_stringify(v) for v in value)}]"
    raise TypeError(f"Unsupported type: {type(value)!r}")


def _format_key(key: str) -> str:
    if not key:
        return f"'{key}'"
    if any(ch in _DELIMS or ch in {"'", '"'} or ch.isspace() for ch in key):
        return _format_string(key)
    return key


def _format_string(value: str) -> str:
    escaped = value.replace("\\", "\\\\").replace("'", "\\'")
    escaped = escaped.replace("\n", "\\n").replace("\r", "\\r").replace("\t", "\\t")
    return f"'{escaped}'"
