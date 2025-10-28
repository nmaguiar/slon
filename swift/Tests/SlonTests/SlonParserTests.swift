import XCTest
@testable import Slon

final class SlonParserTests: XCTestCase {
    func testParsesObjectWithDateAndArray() throws {
        let input = "(status: ok, metrics: [1 | 2.5 | 3], generatedAt: 2024-03-01/18:22:10.001)"
        let value = try Slon.parse(input)
        guard let object = value as? [String: Any] else {
            XCTFail("Expected dictionary")
            return
        }
        XCTAssertEqual(object["status"] as? String, "ok")
        if let metrics = object["metrics"] as? [Any] {
            XCTAssertEqual(metrics.count, 3)
            XCTAssertEqual(metrics[0] as? Int, 1)
            XCTAssertEqual(metrics[1] as? Double, 2.5)
            XCTAssertEqual(metrics[2] as? Int, 3)
        } else {
            XCTFail("Expected metrics array")
        }
        let formatter = DateFormatter()
        formatter.dateFormat = "yyyy-MM-dd/HH:mm:ss.SSS"
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.timeZone = TimeZone(secondsFromGMT: 0)
        XCTAssertEqual((object["generatedAt"] as? Date)?.timeIntervalSince1970, formatter.date(from: "2024-03-01/18:22:10.001")?.timeIntervalSince1970)
    }

    func testParsesKeywordsAndNull() throws {
        let input = "[true | false | null]"
        let value = try Slon.parse(input)
        guard let array = value as? [Any] else {
            XCTFail("Expected array")
            return
        }
        XCTAssertEqual(array.count, 3)
        XCTAssertEqual(array[0] as? Bool, true)
        XCTAssertEqual(array[1] as? Bool, false)
        XCTAssertTrue(array[2] is NSNull)
    }

    func testStringifyRoundTrip() throws {
        let formatter = DateFormatter()
        formatter.dateFormat = "yyyy-MM-dd/HH:mm:ss.SSS"
        formatter.locale = Locale(identifier: "en_US_POSIX")
        formatter.timeZone = TimeZone(secondsFromGMT: 0)
        let date = formatter.date(from: "2024-03-01/18:22:10.001")!
        let value: [String: Any] = [
            "status": "ok",
            "metrics": [1, 2.5, "three"],
            "generatedAt": date,
            "active": true,
            "notes": "Line\nbreak"
        ]
        let slonString = try Slon.stringify(value)
        let parsed = try Slon.parse(slonString)
        guard let object = parsed as? [String: Any] else {
            XCTFail("Expected dictionary after round trip")
            return
        }
        XCTAssertEqual(object["status"] as? String, "ok")
        XCTAssertEqual(object["active"] as? Bool, true)
        XCTAssertEqual((object["generatedAt"] as? Date)?.timeIntervalSince1970, date.timeIntervalSince1970)
        XCTAssertEqual(object["notes"] as? String, "Line\nbreak")
    }
}
