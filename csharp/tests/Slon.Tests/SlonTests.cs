using System.Globalization;
using Xunit;

namespace Slon.Tests;

public class SlonTests
{
    [Fact]
    public void ParsesObjectWithDateAndArray()
    {
        const string input = "(status: ok, metrics: [1 | 2.5 | 3], generatedAt: 2024-03-01/18:22:10.001)";

        var value = Slon.Parse(input);

        var map = Assert.IsType<Dictionary<string, object?>>(value);
        Assert.Equal("ok", Assert.IsType<string>(map["status"]));

        var metrics = Assert.IsType<List<object?>>(map["metrics"]);
        Assert.Equal(3, metrics.Count);
        Assert.Equal(1L, Assert.IsType<long>(metrics[0]));
        Assert.Equal(2.5, Assert.IsType<double>(metrics[1]), 6);
        Assert.Equal(3L, Assert.IsType<long>(metrics[2]));

        var dt = Assert.IsType<DateTime>(map["generatedAt"]);
        var expected = DateTime.ParseExact(
            "2024-03-01/18:22:10.001",
            "yyyy-MM-dd/HH:mm:ss.fff",
            CultureInfo.InvariantCulture,
            DateTimeStyles.AssumeUniversal | DateTimeStyles.AdjustToUniversal);
        Assert.Equal(DateTimeKind.Utc, dt.Kind);
        Assert.Equal(expected, dt);
    }

    [Fact]
    public void ParsesKeywordsAndNull()
    {
        var value = Slon.Parse("[true | false | null]");

        var list = Assert.IsType<List<object?>>(value);
        Assert.Equal(3, list.Count);
        Assert.True(Assert.IsType<bool>(list[0]));
        Assert.False(Assert.IsType<bool>(list[1]));
        Assert.Null(list[2]);
    }

    [Fact]
    public void StringifyRoundTrip()
    {
        var date = DateTime.ParseExact(
            "2024-03-01/18:22:10.001",
            "yyyy-MM-dd/HH:mm:ss.fff",
            CultureInfo.InvariantCulture,
            DateTimeStyles.AssumeUniversal | DateTimeStyles.AdjustToUniversal);

        var value = new Dictionary<string, object?>
        {
            ["status"] = "ok",
            ["metrics"] = new object?[] { 1, 2.5, "three" },
            ["generatedAt"] = date,
            ["active"] = true,
            ["notes"] = "Line\nbreak"
        };

        var slonString = Slon.Stringify(value);
        var parsed = Assert.IsType<Dictionary<string, object?>>(Slon.Parse(slonString));

        Assert.Equal("ok", Assert.IsType<string>(parsed["status"]));
        Assert.True(Assert.IsType<bool>(parsed["active"]));
        Assert.Equal("Line\nbreak", Assert.IsType<string>(parsed["notes"]));
        Assert.Equal(date, Assert.IsType<DateTime>(parsed["generatedAt"]));
    }
}
