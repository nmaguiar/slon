import Foundation

struct Parser {
    let text: String
    var index: String.Index

    init(text: String) {
        self.text = text
        self.index = text.startIndex
    }

    var isAtEnd: Bool {
        return index >= text.endIndex
    }

    var position: Int {
        return text.distance(from: text.startIndex, to: index)
    }

    mutating func skipWhitespace() {
        while let ch = peek(), ch.isWhitespace {
            advance()
        }
    }

    mutating func parseValue() throws -> Any {
        skipWhitespace()
        guard let ch = peek() else {
            throw SlonError("unexpected end of input")
        }
        switch ch {
        case "(":
            return try parseObject()
        case "[":
            return try parseArray()
        case "'", "\"":
            return try parseQuotedString()
        case "-", "+", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9":
            if let date = try tryParseDateTime() {
                return date
            }
            return try parseNumber()
        default:
            if let keyword = matchKeyword("true") {
                return keyword
            }
            if let keyword = matchKeyword("false") {
                return keyword
            }
            if let keyword = matchKeyword("null") {
                return keyword
            }
            return try parseUnquotedString()
        }
    }

    mutating func parseObject() throws -> [String: Any] {
        try expect("(")
        var result: [String: Any] = [:]
        skipWhitespace()
        if peek() == ")" {
            advance()
            return result
        }
        while true {
            skipWhitespace()
            let key = try parseStringLike()
            skipWhitespace()
            guard peek() == ":" else {
                throw SlonError("expected ':' after key at position \(position)")
            }
            advance()
            let value = try parseValue()
            result[key] = value
            skipWhitespace()
            guard let ch = peek() else {
                throw SlonError("unterminated object")
            }
            if ch == "," {
                advance()
                continue
            }
            if ch == ")" {
                advance()
                break
            }
            throw SlonError("expected ',' or ')' at position \(position)")
        }
        return result
    }

    mutating func parseArray() throws -> [Any] {
        try expect("[")
        var result: [Any] = []
        skipWhitespace()
        if peek() == "]" {
            advance()
            return result
        }
        while true {
            let value = try parseValue()
            result.append(value)
            skipWhitespace()
            guard let ch = peek() else {
                throw SlonError("unterminated array")
            }
            if ch == "|" {
                advance()
                continue
            }
            if ch == "]" {
                advance()
                break
            }
            throw SlonError("expected '|' or ']' at position \(position)")
        }
        return result
    }

    mutating func parseStringLike() throws -> String {
        if let ch = peek(), ch == "'" || ch == "\"" {
            return try parseQuotedString()
        }
        return try parseUnquotedString()
    }

    mutating func parseQuotedString() throws -> String {
        guard let quote = peek() else {
            throw SlonError("unterminated string literal")
        }
        advance()
        var result = ""
        while !isAtEnd {
            guard let ch = peek() else {
                throw SlonError("unterminated string literal")
            }
            advance()
            if ch == quote {
                return result
            }
            if ch == "\\" {
                let escaped = try parseEscapeSequence()
                result.append(escaped)
                continue
            }
            result.append(ch)
        }
        throw SlonError("unterminated string literal")
    }

    private mutating func parseEscapeSequence() throws -> String {
        guard let ch = peek() else {
            throw SlonError("invalid escape at position \(position)")
        }
        advance()
        switch ch {
        case "\"", "'", "\\", "/":
            return String(ch)
        case "b":
            return "\u{0008}"
        case "f":
            return "\u{000C}"
        case "n":
            return "\u{000A}"
        case "r":
            return "\u{000D}"
        case "t":
            return "\u{0009}"
        case "u":
            let start = index
            guard let end = text.index(start, offsetBy: 4, limitedBy: text.endIndex) else {
                throw SlonError("invalid unicode escape at position \(position)")
            }
            let hex = text[start..<end]
            guard let value = UInt32(hex, radix: 16), let scalar = UnicodeScalar(value) else {
                throw SlonError("invalid unicode escape at position \(position)")
            }
            index = end
            return String(Character(scalar))
        default:
            throw SlonError("unknown escape at position \(position)")
        }
    }

    mutating func parseUnquotedString() throws -> String {
        let start = index
        while let ch = peek(), !isDelimiter(ch) && !ch.isWhitespace {
            advance()
        }
        let raw = text[start..<index].trimmingCharacters(in: .whitespacesAndNewlines)
        if raw.isEmpty {
            throw SlonError("empty string at position \(text.distance(from: text.startIndex, to: start))")
        }
        return raw
    }

    mutating func parseNumber() throws -> Any {
        let start = index
        while let ch = peek(), ch == "+" || ch == "-" || ch == "e" || ch == "E" || ch == "." || ch.isNumber {
            advance()
        }
        let numberString = String(text[start..<index])
        if numberString.isEmpty {
            throw SlonError("invalid number at position \(text.distance(from: text.startIndex, to: start))")
        }
        if let boundary = peek(), !isDelimiter(boundary) && !boundary.isWhitespace {
            throw SlonError("invalid number boundary at position \(position)")
        }
        if numberString.contains(where: { $0 == "." || $0 == "e" || $0 == "E" }) {
            guard let value = Double(numberString), value.isFinite else {
                throw SlonError("invalid float at position \(text.distance(from: text.startIndex, to: start))")
            }
            return value
        }
        if let intValue = Int(numberString) {
            return intValue
        }
        if let uintValue = UInt64(numberString) {
            return uintValue
        }
        guard let doubleValue = Double(numberString), doubleValue.isFinite else {
            throw SlonError("invalid number at position \(text.distance(from: text.startIndex, to: start))")
        }
        return doubleValue
    }

    mutating func tryParseDateTime() throws -> Date? {
        guard let end = text.index(index, offsetBy: 23, limitedBy: text.endIndex) else {
            return nil
        }
        let candidate = String(text[index..<end])
        if let boundary = (end < text.endIndex ? text[end] : nil), !isDelimiter(boundary) && !boundary.isWhitespace {
            return nil
        }
        guard let date = Parser.dateFormatter.date(from: candidate) else {
            return nil
        }
        index = end
        return date
    }

    mutating func matchKeyword(_ keyword: String) -> Any? {
        guard text[index...].hasPrefix(keyword) else {
            return nil
        }
        let end = text.index(index, offsetBy: keyword.count)
        if end < text.endIndex {
            let boundary = text[end]
            if !isDelimiter(boundary) && !boundary.isWhitespace {
                return nil
            }
        }
        index = end
        switch keyword {
        case "true":
            return true
        case "false":
            return false
        case "null":
            return NSNull()
        default:
            return nil
        }
    }

    private mutating func expect(_ character: Character) throws {
        guard peek() == character else {
            throw SlonError("expected '\(character)' at position \(position)")
        }
        advance()
    }

    private mutating func advance() {
        guard index < text.endIndex else { return }
        index = text.index(after: index)
    }

    private func peek() -> Character? {
        guard index < text.endIndex else {
            return nil
        }
        return text[index]
    }

    private func isDelimiter(_ ch: Character) -> Bool {
        return ch == ":" || ch == "," || ch == "(" || ch == ")" || ch == "[" || ch == "]" || ch == "|"
    }

    private static let dateFormatter: DateFormatter = {
        let formatter = DateFormatter()
        formatter.dateFormat = "yyyy-MM-dd/HH:mm:ss.SSS"
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.timeZone = TimeZone(secondsFromGMT: 0)
        return formatter
    }()
}
