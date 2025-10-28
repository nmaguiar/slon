// swift-tools-version: 5.7
import PackageDescription

let package = Package(
    name: "Slon",
    platforms: [
        .macOS(.v12),
        .iOS(.v15),
        .tvOS(.v15),
        .watchOS(.v8)
    ],
    products: [
        .library(
            name: "Slon",
            targets: ["Slon"]
        )
    ],
    targets: [
        .target(
            name: "Slon"
        ),
        .testTarget(
            name: "SlonTests",
            dependencies: ["Slon"]
        )
    ]
)
