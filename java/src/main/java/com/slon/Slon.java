package com.slon;

import java.lang.reflect.Array;
import java.time.Instant;
import java.time.LocalDateTime;
import java.time.ZoneOffset;
import java.time.format.DateTimeFormatter;
import java.util.ArrayList;
import java.util.Collection;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.Objects;
import java.util.TreeMap;

/**
 * SLON (Single Line Object Notation) parser and formatter.
 */
public final class Slon {
    private static final DateTimeFormatter DATETIME_FORMATTER =
        DateTimeFormatter.ofPattern("yyyy-MM-dd/HH:mm:ss.SSS");

    private Slon() {
    }

    public static Map<String, Object> parse(String text) {
        Object value = parseValue(text);
        if (value instanceof Map) {
            @SuppressWarnings("unchecked")
            Map<String, Object> map = (Map<String, Object>) value;
            return map;
        }
        throw new SlonException("Root value is not an object");
    }

    public static Object parseValue(String text) {
        Parser parser = new Parser(text);
        Object value = parser.parse();
        parser.skipWhitespace();
        if (!parser.isEnd()) {
            throw parser.error("Unexpected trailing content");
        }
        return value;
    }

    public static String stringify(Object value) {
        return Writer.format(value);
    }

    private static boolean isDelimiter(char ch) {
        return ch == ':' || ch == ',' || ch == '(' || ch == ')' || ch == '[' || ch == ']' || ch == '|';
    }

    private static final class Parser {
        private final String text;
        private final int length;
        private int pos;

        Parser(String text) {
            this.text = Objects.requireNonNull(text, "text");
            this.length = text.length();
            this.pos = 0;
        }

        Object parse() {
            skipWhitespace();
            Object value = parseValue();
            skipWhitespace();
            return value;
        }

        boolean isEnd() {
            return pos >= length;
        }

        void skipWhitespace() {
            while (!isEnd()) {
                char ch = text.charAt(pos);
                if (!Character.isWhitespace(ch)) {
                    break;
                }
                pos++;
            }
        }

        char peek() {
            return text.charAt(pos);
        }

        boolean match(char expected) {
            if (!isEnd() && text.charAt(pos) == expected) {
                pos++;
                return true;
            }
            return false;
        }

        Object parseValue() {
            if (isEnd()) {
                throw error("Unexpected end of input");
            }
            char ch = peek();
            if (ch == '(') {
                return parseObject();
            }
            if (ch == '[') {
                return parseArray();
            }
            if (ch == '\'' || ch == '"') {
                return parseQuotedString();
            }
            if (ch == '-' || Character.isDigit(ch)) {
                Instant instant = tryParseDateTime();
                if (instant != null) {
                    return instant;
                }
                return parseNumber();
            }
            if (matchLiteral("true")) {
                return Boolean.TRUE;
            }
            if (matchLiteral("false")) {
                return Boolean.FALSE;
            }
            if (matchLiteral("null")) {
                return null;
            }
            return parseUnquotedString();
        }

        Map<String, Object> parseObject() {
            expect('(');
            Map<String, Object> map = new LinkedHashMap<>();
            skipWhitespace();
            if (match(')')) {
                return map;
            }
            while (true) {
                skipWhitespace();
                String key = parseStringLike();
                skipWhitespace();
                expect(':');
                Object value = parseValue();
                map.put(key, value);
                skipWhitespace();
                if (match(',')) {
                    continue;
                }
                if (match(')')) {
                    break;
                }
                throw error("Expected ',' or ')'");
            }
            return map;
        }

        List<Object> parseArray() {
            expect('[');
            List<Object> list = new ArrayList<>();
            skipWhitespace();
            if (match(']')) {
                return list;
            }
            while (true) {
                Object value = parseValue();
                list.add(value);
                skipWhitespace();
                if (match('|')) {
                    continue;
                }
                if (match(']')) {
                    break;
                }
                throw error("Expected '|' or ']'");
            }
            return list;
        }

        String parseQuotedString() {
            char quote = peek();
            pos++;
            StringBuilder builder = new StringBuilder();
            while (!isEnd()) {
                char ch = peek();
                if (ch == quote) {
                    pos++;
                    return builder.toString();
                }
                if (ch == '\\') {
                    pos++;
                    if (isEnd()) {
                        throw error("Invalid escape sequence");
                    }
                    builder.append(parseEscape());
                    continue;
                }
                builder.append(ch);
                pos++;
            }
            throw error("Unterminated string literal");
        }

        char parseEscape() {
            char esc = peek();
            pos++;
            switch (esc) {
                case '"':
                case '\'':
                case '\\':
                case '/':
                    return esc;
                case 'b':
                    return '\b';
                case 'f':
                    return '\f';
                case 'n':
                    return '\n';
                case 'r':
                    return '\r';
                case 't':
                    return '\t';
                case 'u':
                    ensureRemaining(4);
                    String hex = text.substring(pos, pos + 4);
                    pos += 4;
                    try {
                        int code = Integer.parseInt(hex, 16);
                        return (char) code;
                    } catch (NumberFormatException ex) {
                        throw error("Invalid unicode escape");
                    }
                default:
                    throw error("Unknown escape sequence");
            }
        }

        String parseUnquotedString() {
            int start = pos;
            while (!isEnd()) {
                char ch = text.charAt(pos);
                if (isDelimiter(ch) || Character.isWhitespace(ch)) {
                    break;
                }
                pos++;
            }
            String value = text.substring(start, pos).trim();
            if (value.isEmpty()) {
                throw error("Empty string value");
            }
            return value;
        }

        String parseStringLike() {
            if (isEnd()) {
                throw error("Unexpected end of input");
            }
            char ch = peek();
            if (ch == '\'' || ch == '"') {
                return parseQuotedString();
            }
            return parseUnquotedString();
        }

        Number parseNumber() {
            int start = pos;
            while (!isEnd()) {
                char ch = text.charAt(pos);
                if (Character.isDigit(ch) || ch == '-' || ch == '+' || ch == '.' || ch == 'e' || ch == 'E') {
                    pos++;
                } else {
                    break;
                }
            }
            String number = text.substring(start, pos);
            if (number.isEmpty()) {
                throw error("Invalid number");
            }
            if (!isBoundary(pos)) {
                throw error("Invalid number boundary");
            }
            if (number.indexOf('.') >= 0 || number.indexOf('e') >= 0 || number.indexOf('E') >= 0) {
                try {
                    double value = Double.parseDouble(number);
                    if (!Double.isFinite(value)) {
                        throw error("Non-finite number");
                    }
                    return value;
                } catch (NumberFormatException ex) {
                    throw error("Invalid number");
                }
            }
            try {
                return Long.parseLong(number);
            } catch (NumberFormatException ex) {
                try {
                    double value = Double.parseDouble(number);
                    if (!Double.isFinite(value)) {
                        throw error("Invalid number");
                    }
                    return value;
                } catch (NumberFormatException nested) {
                    throw error("Invalid number");
                }
            }
        }

        Instant tryParseDateTime() {
            final int requiredLength = 23;
            if (pos + requiredLength > length) {
                return null;
            }
            String candidate = text.substring(pos, pos + requiredLength);
            if (!looksLikeDateTime(candidate)) {
                return null;
            }
            if (!isBoundary(pos + requiredLength)) {
                return null;
            }
            try {
                LocalDateTime dateTime = LocalDateTime.parse(candidate, DATETIME_FORMATTER);
                pos += requiredLength;
                return dateTime.toInstant(ZoneOffset.UTC);
            } catch (Exception ex) {
                return null;
            }
        }

        boolean looksLikeDateTime(String value) {
            if (value.length() != 23) {
                return false;
            }
            for (int i = 0; i < value.length(); i++) {
                char ch = value.charAt(i);
                if (i == 4 || i == 7) {
                    if (ch != '-') {
                        return false;
                    }
                } else if (i == 10) {
                    if (ch != '/') {
                        return false;
                    }
                } else if (i == 13 || i == 16) {
                    if (ch != ':') {
                        return false;
                    }
                } else if (i == 19) {
                    if (ch != '.') {
                        return false;
                    }
                } else if (!Character.isDigit(ch)) {
                    return false;
                }
            }
            return true;
        }

        boolean matchLiteral(String literal) {
            if (!text.startsWith(literal, pos)) {
                return false;
            }
            int end = pos + literal.length();
            if (!isBoundary(end)) {
                return false;
            }
            pos = end;
            return true;
        }

        boolean isBoundary(int index) {
            if (index >= length) {
                return true;
            }
            char ch = text.charAt(index);
            return isDelimiter(ch) || Character.isWhitespace(ch);
        }

        void expect(char expected) {
            if (!match(expected)) {
                throw error("Expected '" + expected + "'");
            }
        }

        void ensureRemaining(int count) {
            if (pos + count > length) {
                throw error("Unexpected end of input");
            }
        }

        SlonException error(String message) {
            return new SlonException(message + " (at position " + pos + ")");
        }
    }

    private static final class Writer {
        private Writer() {
        }

        static String format(Object value) {
            if (value == null) {
                return "null";
            }
            if (value instanceof Boolean) {
                return Boolean.TRUE.equals(value) ? "true" : "false";
            }
            if (value instanceof Byte || value instanceof Short || value instanceof Integer || value instanceof Long) {
                return Long.toString(((Number) value).longValue());
            }
            if (value instanceof Number) {
                double number = ((Number) value).doubleValue();
                if (!Double.isFinite(number)) {
                    throw new SlonException("Non-finite number");
                }
                return Double.toString(number);
            }
            if (value instanceof CharSequence) {
                return quote(value.toString());
            }
            if (value instanceof Instant) {
                LocalDateTime dateTime = LocalDateTime.ofInstant((Instant) value, ZoneOffset.UTC);
                return DATETIME_FORMATTER.format(dateTime);
            }
            if (value instanceof Map) {
                @SuppressWarnings("unchecked")
                Map<String, Object> map = (Map<String, Object>) value;
                return formatMap(map);
            }
            if (value instanceof Collection) {
                return formatCollection((Collection<?>) value);
            }
            if (value.getClass().isArray()) {
                return formatArray(value);
            }
            throw new SlonException("Unsupported type: " + value.getClass());
        }

        private static String formatCollection(Collection<?> values) {
            List<String> parts = new ArrayList<>(values.size());
            for (Object value : values) {
                parts.add(format(value));
            }
            return "[" + String.join(" | ", parts) + "]";
        }

        private static String formatArray(Object array) {
            int length = Array.getLength(array);
            List<String> parts = new ArrayList<>(length);
            for (int i = 0; i < length; i++) {
                parts.add(format(Array.get(array, i)));
            }
            return "[" + String.join(" | ", parts) + "]";
        }

        private static String formatMap(Map<String, Object> map) {
            Map<String, Object> ordered = map instanceof LinkedHashMap ? map : new TreeMap<>(map);
            List<String> parts = new ArrayList<>(ordered.size());
            for (Map.Entry<String, Object> entry : ordered.entrySet()) {
                String key = entry.getKey();
                String renderedKey = requiresQuoting(key) ? quote(key) : key;
                parts.add(renderedKey + ": " + format(entry.getValue()));
            }
            return "(" + String.join(", ", parts) + ")";
        }

        private static String quote(String value) {
            String escaped = value
                .replace("\\", "\\\\")
                .replace("'", "\\'")
                .replace("\n", "\\n")
                .replace("\r", "\\r")
                .replace("\t", "\\t");
            return "'" + escaped + "'";
        }

        private static boolean requiresQuoting(String key) {
            if (key == null || key.isEmpty()) {
                return true;
            }
            for (char ch : key.toCharArray()) {
                if (isDelimiter(ch) || ch == '\'' || ch == '"' || Character.isWhitespace(ch)) {
                    return true;
                }
            }
            return false;
        }
    }
}
