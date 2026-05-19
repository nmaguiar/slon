using System.Collections;
using System.Globalization;
using System.Text;

namespace Slon;

public static class Slon
{
    private const string DateTimeFormat = "yyyy-MM-dd/HH:mm:ss.fff";

    public static object? Parse(string text)
    {
        var parser = new Parser(text ?? throw new ArgumentNullException(nameof(text)));
        var value = parser.Parse();
        parser.SkipWhitespace();
        if (!parser.IsEnd)
        {
            throw parser.Error("Unexpected trailing content");
        }

        return value;
    }

    public static string Stringify(object? value)
    {
        return Writer.Format(value);
    }

    private static bool IsDelimiter(char ch)
    {
        return ch is ':' or ',' or '(' or ')' or '[' or ']' or '|';
    }

    private sealed class Parser
    {
        private readonly string _text;
        private readonly int _length;
        private int _pos;

        internal Parser(string text)
        {
            _text = text;
            _length = text.Length;
            _pos = 0;
        }

        internal bool IsEnd => _pos >= _length;

        internal object? Parse()
        {
            SkipWhitespace();
            return ParseValue();
        }

        internal void SkipWhitespace()
        {
            while (!IsEnd && char.IsWhiteSpace(_text[_pos]))
            {
                _pos++;
            }
        }

        internal SlonException Error(string message)
        {
            return new SlonException($"{message} (at position {_pos})");
        }

        private char Peek()
        {
            return _text[_pos];
        }

        private bool Match(char expected)
        {
            if (!IsEnd && _text[_pos] == expected)
            {
                _pos++;
                return true;
            }

            return false;
        }

        private object? ParseValue()
        {
            SkipWhitespace();
            if (IsEnd)
            {
                throw Error("Unexpected end of input");
            }

            var ch = Peek();
            if (ch == '(')
            {
                return ParseObject();
            }

            if (ch == '[')
            {
                return ParseArray();
            }

            if (ch is '\'' or '"')
            {
                return ParseQuotedString();
            }

            if (ch == '-' || char.IsDigit(ch))
            {
                var instant = TryParseDateTime();
                if (instant.HasValue)
                {
                    return instant.Value;
                }

                return ParseNumber();
            }

            if (MatchLiteral("true"))
            {
                return true;
            }

            if (MatchLiteral("false"))
            {
                return false;
            }

            if (MatchLiteral("null"))
            {
                return null;
            }

            return ParseUnquotedString();
        }

        private Dictionary<string, object?> ParseObject()
        {
            Expect('(');
            var map = new Dictionary<string, object?>();
            SkipWhitespace();
            if (Match(')'))
            {
                return map;
            }

            while (true)
            {
                SkipWhitespace();
                var key = ParseStringLike();
                SkipWhitespace();
                Expect(':');
                var value = ParseValue();
                map[key] = value;
                SkipWhitespace();
                if (Match(','))
                {
                    continue;
                }

                if (Match(')'))
                {
                    break;
                }

                throw Error("Expected ',' or ')'");
            }

            return map;
        }

        private List<object?> ParseArray()
        {
            Expect('[');
            var list = new List<object?>();
            SkipWhitespace();
            if (Match(']'))
            {
                return list;
            }

            while (true)
            {
                var value = ParseValue();
                list.Add(value);
                SkipWhitespace();
                if (Match('|'))
                {
                    continue;
                }

                if (Match(']'))
                {
                    break;
                }

                throw Error("Expected '|' or ']'");
            }

            return list;
        }

        private string ParseQuotedString()
        {
            var quote = Peek();
            _pos++;
            var builder = new StringBuilder();
            while (!IsEnd)
            {
                var ch = Peek();
                if (ch == quote)
                {
                    _pos++;
                    return builder.ToString();
                }

                if (ch == '\\')
                {
                    _pos++;
                    if (IsEnd)
                    {
                        throw Error("Invalid escape sequence");
                    }

                    builder.Append(ParseEscape());
                    continue;
                }

                builder.Append(ch);
                _pos++;
            }

            throw Error("Unterminated string literal");
        }

        private char ParseEscape()
        {
            var esc = Peek();
            _pos++;
            return esc switch
            {
                '"' or '\'' or '\\' or '/' => esc,
                'b' => '\b',
                'f' => '\f',
                'n' => '\n',
                'r' => '\r',
                't' => '\t',
                'u' => ParseUnicodeEscape(),
                _ => throw Error("Unknown escape sequence")
            };
        }

        private char ParseUnicodeEscape()
        {
            EnsureRemaining(4);
            var hex = _text.Substring(_pos, 4);
            _pos += 4;
            if (!ushort.TryParse(hex, NumberStyles.HexNumber, CultureInfo.InvariantCulture, out var code))
            {
                throw Error("Invalid unicode escape");
            }

            return (char)code;
        }

        private string ParseUnquotedString()
        {
            var start = _pos;
            while (!IsEnd)
            {
                var ch = _text[_pos];
                if (IsDelimiter(ch) || char.IsWhiteSpace(ch))
                {
                    break;
                }

                _pos++;
            }

            var value = _text.Substring(start, _pos - start).Trim();
            if (string.IsNullOrEmpty(value))
            {
                throw Error("Empty string value");
            }

            return value;
        }

        private string ParseStringLike()
        {
            if (!IsEnd)
            {
                var ch = Peek();
                if (ch is '\'' or '"')
                {
                    return ParseQuotedString();
                }
            }

            return ParseUnquotedString();
        }

        private object ParseNumber()
        {
            var start = _pos;
            while (!IsEnd)
            {
                var ch = _text[_pos];
                if (char.IsDigit(ch) || ch is '-' or '+' or '.' or 'e' or 'E')
                {
                    _pos++;
                }
                else
                {
                    break;
                }
            }

            var number = _text.Substring(start, _pos - start);
            if (number.Length == 0)
            {
                throw Error("Invalid number");
            }

            if (!IsBoundary(_pos))
            {
                throw Error("Invalid number boundary");
            }

            if (number.IndexOfAny(new[] { '.', 'e', 'E' }) >= 0)
            {
                if (!double.TryParse(number, NumberStyles.Float, CultureInfo.InvariantCulture, out var floatValue) ||
                    double.IsNaN(floatValue) ||
                    double.IsInfinity(floatValue))
                {
                    throw Error("Invalid number");
                }

                return floatValue;
            }

            if (long.TryParse(number, NumberStyles.Integer, CultureInfo.InvariantCulture, out var intValue))
            {
                return intValue;
            }

            if (double.TryParse(number, NumberStyles.Float, CultureInfo.InvariantCulture, out var fallback) &&
                !double.IsNaN(fallback) &&
                !double.IsInfinity(fallback))
            {
                return fallback;
            }

            throw Error("Invalid number");
        }

        private DateTime? TryParseDateTime()
        {
            const int requiredLength = 23;
            if (_pos + requiredLength > _length)
            {
                return null;
            }

            var candidate = _text.Substring(_pos, requiredLength);
            if (!LooksLikeDateTime(candidate) || !IsBoundary(_pos + requiredLength))
            {
                return null;
            }

            if (DateTime.TryParseExact(
                candidate,
                DateTimeFormat,
                CultureInfo.InvariantCulture,
                DateTimeStyles.AssumeUniversal | DateTimeStyles.AdjustToUniversal,
                out var parsed))
            {
                _pos += requiredLength;
                return DateTime.SpecifyKind(parsed, DateTimeKind.Utc);
            }

            return null;
        }

        private static bool LooksLikeDateTime(string value)
        {
            if (value.Length != 23)
            {
                return false;
            }

            for (var i = 0; i < value.Length; i++)
            {
                var ch = value[i];
                if (i is 4 or 7)
                {
                    if (ch != '-')
                    {
                        return false;
                    }
                }
                else if (i == 10)
                {
                    if (ch != '/')
                    {
                        return false;
                    }
                }
                else if (i is 13 or 16)
                {
                    if (ch != ':')
                    {
                        return false;
                    }
                }
                else if (i == 19)
                {
                    if (ch != '.')
                    {
                        return false;
                    }
                }
                else if (!char.IsDigit(ch))
                {
                    return false;
                }
            }

            return true;
        }

        private bool MatchLiteral(string literal)
        {
            if (_pos + literal.Length > _length || !string.Equals(_text.Substring(_pos, literal.Length), literal, StringComparison.Ordinal))
            {
                return false;
            }

            var end = _pos + literal.Length;
            if (!IsBoundary(end))
            {
                return false;
            }

            _pos = end;
            return true;
        }

        private bool IsBoundary(int index)
        {
            if (index >= _length)
            {
                return true;
            }

            var ch = _text[index];
            return IsDelimiter(ch) || char.IsWhiteSpace(ch);
        }

        private void Expect(char expected)
        {
            if (!Match(expected))
            {
                throw Error($"Expected '{expected}'");
            }
        }

        private void EnsureRemaining(int count)
        {
            if (_pos + count > _length)
            {
                throw Error("Unexpected end of input");
            }
        }
    }

    private static class Writer
    {
        internal static string Format(object? value)
        {
            return value switch
            {
                null => "null",
                bool boolValue => boolValue ? "true" : "false",
                byte or sbyte or short or ushort or int or uint or long or ulong => Convert.ToString(value, CultureInfo.InvariantCulture)!,
                float floatValue => FormatFloat(floatValue),
                double doubleValue => FormatDouble(doubleValue),
                decimal decimalValue => decimalValue.ToString(CultureInfo.InvariantCulture),
                string stringValue => Quote(stringValue),
                DateTime dateTimeValue => dateTimeValue.ToUniversalTime().ToString(DateTimeFormat, CultureInfo.InvariantCulture),
                DateTimeOffset dateTimeOffsetValue => dateTimeOffsetValue.UtcDateTime.ToString(DateTimeFormat, CultureInfo.InvariantCulture),
                IDictionary dictionary => FormatDictionary(dictionary),
                IEnumerable enumerable when value is not string => FormatEnumerable(enumerable),
                _ => throw new SlonException($"Unsupported type: {value.GetType()}")
            };
        }

        private static string FormatFloat(float value)
        {
            if (float.IsNaN(value) || float.IsInfinity(value))
            {
                throw new SlonException("Non-finite number");
            }

            return value.ToString("R", CultureInfo.InvariantCulture);
        }

        private static string FormatDouble(double value)
        {
            if (double.IsNaN(value) || double.IsInfinity(value))
            {
                throw new SlonException("Non-finite number");
            }

            return value.ToString("R", CultureInfo.InvariantCulture);
        }

        private static string FormatEnumerable(IEnumerable values)
        {
            var items = new List<string>();
            foreach (var item in values)
            {
                items.Add(Format(item));
            }

            return $"[{string.Join(" | ", items)}]";
        }

        private static string FormatDictionary(IDictionary map)
        {
            var entries = new List<KeyValuePair<string, object?>>();
            foreach (DictionaryEntry entry in map)
            {
                if (entry.Key is not string key)
                {
                    throw new SlonException("Object keys must be strings");
                }

                entries.Add(new KeyValuePair<string, object?>(key, entry.Value));
            }

            entries.Sort((a, b) => string.CompareOrdinal(a.Key, b.Key));
            var parts = new List<string>(entries.Count);
            foreach (var entry in entries)
            {
                var renderedKey = RequiresQuoting(entry.Key) ? Quote(entry.Key) : entry.Key;
                parts.Add($"{renderedKey}: {Format(entry.Value)}");
            }

            return $"({string.Join(", ", parts)})";
        }

        private static string Quote(string value)
        {
            var escaped = value
                .Replace("\\", "\\\\", StringComparison.Ordinal)
                .Replace("'", "\\'", StringComparison.Ordinal)
                .Replace("\n", "\\n", StringComparison.Ordinal)
                .Replace("\r", "\\r", StringComparison.Ordinal)
                .Replace("\t", "\\t", StringComparison.Ordinal);
            return $"'{escaped}'";
        }

        private static bool RequiresQuoting(string key)
        {
            if (string.IsNullOrEmpty(key))
            {
                return true;
            }

            foreach (var ch in key)
            {
                if (IsDelimiter(ch) || ch is '\'' or '"' || char.IsWhiteSpace(ch))
                {
                    return true;
                }
            }

            return false;
        }
    }
}
