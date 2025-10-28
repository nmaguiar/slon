import Foundation

public struct SlonError: Error, CustomStringConvertible {
    public let message: String

    public init(_ message: String) {
        self.message = message
    }

    public var description: String { message }
}

public enum Slon {
    public static func parse(_ text: String) throws -> Any {
        var parser = Parser(text: text)
        parser.skipWhitespace()
        let value = try parser.parseValue()
        parser.skipWhitespace()
        if !parser.isAtEnd {
            throw SlonError("unexpected trailing content at position \(parser.position)")
        }
        return value
    }

    public static func stringify(_ value: Any) throws -> String {
        return try SlonStringifier.stringify(value)
    }
}
