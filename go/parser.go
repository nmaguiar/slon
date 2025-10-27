package slon

import (
	"fmt"
	"math"
	"strconv"
	"strings"
	"time"
	"unicode"
)

// Parse converts a SLON string into Go values.
// Objects become map[string]any, arrays []any, datetimes time.Time in UTC.
func Parse(input string) (any, error) {
	p := &parser{text: input}
	p.skipWhitespace()
	value, err := p.parseValue()
	if err != nil {
		return nil, err
	}
	p.skipWhitespace()
	if !p.end() {
		return nil, fmt.Errorf("unexpected trailing content at position %d", p.pos)
	}
	return value, nil
}

type parser struct {
	text string
	pos  int
}

func (p *parser) end() bool {
	return p.pos >= len(p.text)
}

func (p *parser) peek() (rune, bool) {
	if p.end() {
		return 0, false
	}
	return rune(p.text[p.pos]), true
}

func (p *parser) advance(n int) {
	p.pos += n
}

func (p *parser) skipWhitespace() {
	for !p.end() {
		if !unicode.IsSpace(rune(p.text[p.pos])) {
			break
		}
		p.pos++
	}
}

func (p *parser) parseValue() (any, error) {
	p.skipWhitespace()
	ch, ok := p.peek()
	if !ok {
		return nil, fmt.Errorf("unexpected end of input")
	}
	switch ch {
	case '(':
		return p.parseObject()
	case '[':
		return p.parseArray()
	case '\'', '"':
		return p.parseQuotedString()
	case '-', '0', '1', '2', '3', '4', '5', '6', '7', '8', '9':
		if dt, consumed, err := p.tryParseDateTime(); err != nil {
			return nil, err
		} else if consumed {
			return dt, nil
		}
		return p.parseNumber()
	default:
		if keyword, consumed := p.matchKeyword("true"); consumed {
			return keyword, nil
		}
		if keyword, consumed := p.matchKeyword("false"); consumed {
			return keyword, nil
		}
		if keyword, consumed := p.matchKeyword("null"); consumed {
			return keyword, nil
		}
		return p.parseUnquotedString()
	}
}

func (p *parser) parseObject() (map[string]any, error) {
	p.advance(1) // skip '('
	result := make(map[string]any)
	p.skipWhitespace()
	if ch, ok := p.peek(); ok && ch == ')' {
		p.advance(1)
		return result, nil
	}
	for {
		p.skipWhitespace()
		key, err := p.parseStringLike()
		if err != nil {
			return nil, err
		}
		p.skipWhitespace()
		if ch, ok := p.peek(); !ok || ch != ':' {
			return nil, fmt.Errorf("expected ':' after key at position %d", p.pos)
		}
		p.advance(1)
		value, err := p.parseValue()
		if err != nil {
			return nil, err
		}
		result[key] = value
		p.skipWhitespace()
		ch, ok := p.peek()
		if !ok {
			return nil, fmt.Errorf("unterminated object")
		}
		if ch == ',' {
			p.advance(1)
			continue
		}
		if ch == ')' {
			p.advance(1)
			break
		}
		return nil, fmt.Errorf("expected ',' or ')' at position %d", p.pos)
	}
	return result, nil
}

func (p *parser) parseArray() ([]any, error) {
	p.advance(1) // skip '['
	var result []any
	p.skipWhitespace()
	if ch, ok := p.peek(); ok && ch == ']' {
		p.advance(1)
		return result, nil
	}
	for {
		value, err := p.parseValue()
		if err != nil {
			return nil, err
		}
		result = append(result, value)
		p.skipWhitespace()
		ch, ok := p.peek()
		if !ok {
			return nil, fmt.Errorf("unterminated array")
		}
		if ch == '|' {
			p.advance(1)
			continue
		}
		if ch == ']' {
			p.advance(1)
			break
		}
		return nil, fmt.Errorf("expected '|' or ']' at position %d", p.pos)
	}
	return result, nil
}

func (p *parser) parseStringLike() (string, error) {
	if ch, ok := p.peek(); ok && (ch == '\'' || ch == '"') {
		return p.parseQuotedString()
	}
	str, err := p.parseUnquotedString()
	return str, err
}

func (p *parser) parseQuotedString() (string, error) {
	ch, _ := p.peek()
	quote := byte(ch)
	p.advance(1)
	var builder strings.Builder
	for !p.end() {
		current := p.text[p.pos]
		if current == quote {
			p.advance(1)
			return builder.String(), nil
		}
		if current == '\\' {
			if p.pos+1 >= len(p.text) {
				return "", fmt.Errorf("invalid escape at position %d", p.pos)
			}
			next := p.text[p.pos+1]
			switch next {
			case '"', '\'', '\\', '/':
				builder.WriteByte(next)
			case 'b':
				builder.WriteByte('\b')
			case 'f':
				builder.WriteByte('\f')
			case 'n':
				builder.WriteByte('\n')
			case 'r':
				builder.WriteByte('\r')
			case 't':
				builder.WriteByte('\t')
			case 'u':
				if p.pos+5 >= len(p.text) {
					return "", fmt.Errorf("invalid unicode escape at position %d", p.pos)
				}
				hex := p.text[p.pos+2 : p.pos+6]
				r, err := strconv.ParseInt(hex, 16, 32)
				if err != nil {
					return "", fmt.Errorf("invalid unicode escape at position %d", p.pos)
				}
				builder.WriteRune(rune(r))
				p.advance(4)
			default:
				return "", fmt.Errorf("unknown escape at position %d", p.pos)
			}
			p.advance(2)
			continue
		}
		builder.WriteByte(current)
		p.advance(1)
	}
	return "", fmt.Errorf("unterminated string literal")
}

func (p *parser) parseUnquotedString() (string, error) {
	start := p.pos
	for !p.end() {
		ch := p.text[p.pos]
		if isDelimiter(ch) || unicode.IsSpace(rune(ch)) {
			break
		}
		p.pos++
	}
	raw := strings.TrimSpace(p.text[start:p.pos])
	if raw == "" {
		return "", fmt.Errorf("empty string at position %d", start)
	}
	return raw, nil
}

func (p *parser) parseNumber() (any, error) {
	start := p.pos
	for !p.end() {
		ch := p.text[p.pos]
		if !(ch == '+' || ch == '-' || ch == 'e' || ch == 'E' || ch == '.' || (ch >= '0' && ch <= '9')) {
			break
		}
		p.pos++
	}
	number := p.text[start:p.pos]
	if number == "" {
		return nil, fmt.Errorf("invalid number at position %d", start)
	}
	if p.pos < len(p.text) {
		if ch := p.text[p.pos]; !isDelimiter(ch) && !unicode.IsSpace(rune(ch)) {
			return nil, fmt.Errorf("invalid number boundary at position %d", p.pos)
		}
	}
	if strings.ContainsAny(number, ".eE") {
		value, err := strconv.ParseFloat(number, 64)
		if err != nil || math.IsNaN(value) || math.IsInf(value, 0) {
			return nil, fmt.Errorf("invalid float at position %d", start)
		}
		return value, nil
	}
	value, err := strconv.ParseInt(number, 10, 64)
	if err == nil {
		return value, nil
	}
	unsigned, errUnsigned := strconv.ParseUint(number, 10, 64)
	if errUnsigned != nil {
		return nil, fmt.Errorf("invalid integer at position %d", start)
	}
	return unsigned, nil
}

func (p *parser) tryParseDateTime() (time.Time, bool, error) {
	if p.pos+23 > len(p.text) {
		return time.Time{}, false, nil
	}
	candidate := p.text[p.pos : p.pos+23]
	layout := "2006-01-02/15:04:05.000"
	if p.pos+23 < len(p.text) {
		next := p.text[p.pos+23]
		if !isDelimiter(next) && !unicode.IsSpace(rune(next)) {
			return time.Time{}, false, nil
		}
	}
	t, err := time.ParseInLocation(layout, candidate, time.UTC)
	if err != nil {
		return time.Time{}, false, nil
	}
	p.pos += 23
	return t, true, nil
}

func (p *parser) matchKeyword(keyword string) (any, bool) {
	if strings.HasPrefix(p.text[p.pos:], keyword) {
		end := p.pos + len(keyword)
		if end == len(p.text) || isDelimiter(p.text[end]) || unicode.IsSpace(rune(p.text[end])) {
			p.pos = end
			switch keyword {
			case "true":
				return true, true
			case "false":
				return false, true
			case "null":
				return nil, true
			}
		}
	}
	return nil, false
}

func isDelimiter(ch byte) bool {
	switch ch {
	case ':', ',', '(', ')', '[', ']', '|':
		return true
	default:
		return false
	}
}
