# SLON for Java

Reference Java implementation of a SLON (Single Line Object Notation) parser and formatter.

## Build

Use Gradle (7+) to assemble the library:

```bash
cd java
gradle build
```

The compiled JAR is produced inside `build/libs/slon-java.jar`. If you prefer wrapper scripts, run `gradle wrapper` once and commit the generated files.

## Usage

```java
import com.slon.Slon;
import java.time.Instant;
import java.util.Map;

public class Example {
  public static void main(String[] args) {
    Map<String, Object> value = Slon.parse("(status: ok, generatedAt: 2024-03-01/18:22:10.001)");
    System.out.println(value.get("status")); // ok

    value.put("generatedAt", Instant.now());
    String slon = Slon.stringify(value);
    System.out.println(slon);
  }
}
```

Parsing returns plain Java collections (`Map`, `List`) as well as primitives. Datetime literals are converted to `Instant`. `Slon.stringify` performs the reverse operation.
