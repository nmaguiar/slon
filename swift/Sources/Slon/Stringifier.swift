import Foundation
import CoreFoundation

enum SlonStringifier {
    static func stringify(_ value: Any) throws -> String {
        switch value {
        case is NSNull:
            return "null"
        case let bool as Bool:
            return bool ? "true" : "false"
        case let date as Date:
            return SlonStringifier.dateFormatter.string(from: date)
        case let string as String:
            return formatString(string)
        case let double as Double:
            return try formatDouble(double)
        case let float as Float:
            return try formatDouble(Double(float))
        case let int as Int:
            return String(int)
        case let int as Int8:
            return String(int)
        case let int as Int16:
            return String(int)
        case let int as Int32:
            return String(int)
        case let int as Int64:
            return String(int)
        case let uint as UInt:
            return String(uint)
        case let uint as UInt8:
            return String(uint)
        case let uint as UInt16:
            return String(uint)
        case let uint as UInt32:
            return String(uint)
        case let uint as UInt64:
            return String(uint)
        case let decimal as Decimal:
            return try formatDecimal(decimal)
        case let array as [Any]:
            return try stringifyArray(array)
        case let dict as [String: Any]:
            return try stringifyDictionary(dict)
        default:
            if let array = value as? NSArray {
                return try stringifyArray(array.map { $0 })
            }
            if let dict = value as? NSDictionary {
                var swiftDict: [String: Any] = [:]
                for (key, value) in dict {
                    guard let key = key as? String else {
                        throw SlonError("dictionary keys must be strings, found \(type(of: key))")
                    }
                    swiftDict[key] = value
                }
                return try stringifyDictionary(swiftDict)
            }
            if let number = value as? NSNumber {
                if CFGetTypeID(number) == CFBooleanGetTypeID() {
                    return number.boolValue ? "true" : "false"
                }
                return try formatDouble(number.doubleValue)
            }
            throw SlonError("unsupported type \(type(of: value))")
        }
    }

    private static func stringifyArray(_ array: [Any]) throws -> String {
        let parts = try array.map { try stringify($0) }
        return "[" + parts.joined(separator: " | ") + "]"
    }

    private static func stringifyDictionary(_ dict: [String: Any]) throws -> String {
        let sortedKeys = dict.keys.sorted()
        let parts = try sortedKeys.map { key -> String in
            let formattedKey: String
            if requiresQuoting(key) {
                formattedKey = formatString(key)
            } else {
                formattedKey = key
            }
            let value = dict[key]!
            let formattedValue = try stringify(value)
            return "\(formattedKey): \(formattedValue)"
        }
        return "(" + parts.joined(separator: ", ") + ")"
    }

    private static func requiresQuoting(_ value: String) -> Bool {
        if value.isEmpty {
            return true
        }
        for ch in value {
            switch ch {
            case ":", ",", "(", ")", "[", "]", "|", "\"", "'":
                return true
            default:
                if ch.isWhitespace {
                    return true
                }
            }
        }
        return false
    }

    private static func formatString(_ value: String) -> String {
        var result = "'"
        for scalar in value.unicodeScalars {
            switch scalar {
            case "\\":
                result.append("\\\\")
            case "'":
                result.append("\\'")
            case "\n":
                result.append("\\n")
            case "\r":
                result.append("\\r")
            case "\t":
                result.append("\\t")
            default:
                result.append(Character(scalar))
            }
        }
        result.append("'")
        return result
    }

    private static func formatDouble(_ value: Double) throws -> String {
        guard value.isFinite else {
            throw SlonError("non-finite floating-point value")
        }
        if let formatted = decimalFormatter.string(from: NSNumber(value: value)) {
            return stripTrailingZeros(formatted)
        }
        return stripTrailingZeros(String(value))
    }

    private static func formatDecimal(_ value: Decimal) throws -> String {
        let number = NSDecimalNumber(decimal: value)
        if number == NSDecimalNumber.notANumber {
            throw SlonError("non-finite decimal value")
        }
        if let formatted = decimalFormatter.string(from: number) {
            return stripTrailingZeros(formatted)
        }
        return stripTrailingZeros(number.stringValue)
    }

    private static func stripTrailingZeros(_ value: String) -> String {
        guard value.contains(".") else {
            return value
        }
        var trimmed = value
        while trimmed.last == "0" {
            trimmed.removeLast()
        }
        if trimmed.last == "." {
            trimmed.removeLast()
        }
        return trimmed
    }

    private static let decimalFormatter: NumberFormatter = {
        let formatter = NumberFormatter()
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.numberStyle = .decimal
        formatter.usesGroupingSeparator = false
        formatter.maximumFractionDigits = 15
        formatter.maximumSignificantDigits = 17
        formatter.minimumFractionDigits = 0
        formatter.minimumIntegerDigits = 1
        return formatter
    }()

    private static let dateFormatter: DateFormatter = {
        let formatter = DateFormatter()
        formatter.dateFormat = "yyyy-MM-dd/HH:mm:ss.SSS"
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.timeZone = TimeZone(secondsFromGMT: 0)
        return formatter
    }()
}
