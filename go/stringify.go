package slon

import (
	"fmt"
	"math"
	"reflect"
	"sort"
	"strings"
	"time"
)

// Stringify converts Go values back into SLON.
func Stringify(value any) (string, error) {
	switch v := value.(type) {
	case nil:
		return "null", nil
	case bool:
		if v {
			return "true", nil
		}
		return "false", nil
	case time.Time:
		return v.UTC().Format("2006-01-02/15:04:05.000"), nil
	case float32:
		f := float64(v)
		if math.IsNaN(f) || math.IsInf(f, 0) {
			return "", fmt.Errorf("non-finite float32")
		}
		return trimFloat(f), nil
	case float64:
		if math.IsNaN(v) || math.IsInf(v, 0) {
			return "", fmt.Errorf("non-finite float64")
		}
		return trimFloat(v), nil
	case int, int8, int16, int32, int64, uint, uint8, uint16, uint32, uint64:
		return fmt.Sprintf("%v", v), nil
	case string:
		return formatString(v), nil
	default:
		rv := reflect.ValueOf(value)
		switch rv.Kind() {
		case reflect.Slice, reflect.Array:
			return stringifyReflectSlice(rv)
		case reflect.Map:
			if rv.Type().Key().Kind() == reflect.String {
				return stringifyReflectMap(rv)
			}
		}
		return "", fmt.Errorf("unsupported type %T", value)
	}
}

func trimFloat(value float64) string {
	formatted := fmt.Sprintf("%f", value)
	formatted = strings.TrimSuffix(formatted, "0")
	formatted = strings.TrimSuffix(formatted, ".")
	return formatted
}

func stringifyReflectSlice(rv reflect.Value) (string, error) {
	parts := make([]string, rv.Len())
	for i := 0; i < rv.Len(); i++ {
		formatted, err := Stringify(rv.Index(i).Interface())
		if err != nil {
			return "", err
		}
		parts[i] = formatted
	}
	return "[" + strings.Join(parts, " | ") + "]", nil
}

func stringifyReflectMap(rv reflect.Value) (string, error) {
	keys := rv.MapKeys()
	stringKeys := make([]string, len(keys))
	for i, key := range keys {
		stringKeys[i] = key.String()
	}
	sort.Strings(stringKeys)
	parts := make([]string, 0, len(keys))
	for _, key := range stringKeys {
		name := key
		if requiresQuoting(name) {
			name = formatString(name)
		}
		value := rv.MapIndex(reflect.ValueOf(key)).Interface()
		formatted, err := Stringify(value)
		if err != nil {
			return "", err
		}
		parts = append(parts, fmt.Sprintf("%s: %s", name, formatted))
	}
	return "(" + strings.Join(parts, ", ") + ")", nil
}

func formatString(value string) string {
	replacer := strings.NewReplacer(
		"\\", "\\\\",
		"'", "\\'",
		"\n", "\\n",
		"\r", "\\r",
		"\t", "\\t",
	)
	return "'" + replacer.Replace(value) + "'"
}

func requiresQuoting(value string) bool {
	if len(value) == 0 {
		return true
	}
	for _, ch := range value {
		switch ch {
		case ':', ',', '(', ')', '[', ']', '|', '"', '\'':
			return true
		}
		if ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n' {
			return true
		}
	}
	return false
}
