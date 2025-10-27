#include "slon.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define SLON_ERROR_OUT_OF_MEMORY "Out of memory"
#define SLON_ERROR_UNEXPECTED "Unexpected token"

typedef struct {
    const char *text;
    size_t length;
    size_t pos;
    slon_error *error;
} slon_parser;

typedef struct {
    char *data;
    size_t length;
    size_t capacity;
} slon_buffer;

static slon_value *parse_value(slon_parser *parser);
static slon_value *parse_object(slon_parser *parser);
static slon_value *parse_array(slon_parser *parser);
static slon_value *parse_string_like(slon_parser *parser);
static slon_value *parse_number(slon_parser *parser);
static slon_value *parse_datetime(slon_parser *parser);
static slon_value *parse_keyword(slon_parser *parser);

static void parser_skip_ws(slon_parser *parser);
static int parser_match(slon_parser *parser, char ch);
static int parser_expect(slon_parser *parser, char ch);
static int parser_error(slon_parser *parser, const char *message);
static int parser_is_boundary(slon_parser *parser, size_t index);
static int is_delimiter(char ch);
static char *parse_quoted_string(slon_parser *parser);
static char *parse_unquoted_string(slon_parser *parser);
static char *slon_strndup(const char *text, size_t length);

static slon_value *slon_value_new(slon_type type);
static int object_append(slon_value *object, char *key, slon_value *value);
static int array_append(slon_value *array, slon_value *value);

static int buffer_append_char(slon_buffer *buffer, char ch);
static int buffer_append_str(slon_buffer *buffer, const char *text);
static int buffer_append_len(slon_buffer *buffer, const char *text, size_t length);
static void buffer_free(slon_buffer *buffer);
static int stringify_value(const slon_value *value, slon_buffer *buffer);
static int stringify_string(const char *value, slon_buffer *buffer);
static int stringify_key(const char *key, slon_buffer *buffer);
static int requires_quoting(const char *key);

int slon_parse(const char *text, slon_value **out_value, slon_error *error) {
    if (out_value == NULL || text == NULL) {
        if (error) {
            error->position = 0;
            error->message = "Invalid arguments";
        }
        return 1;
    }

    slon_parser parser = {
        .text = text,
        .length = strlen(text),
        .pos = 0,
        .error = error
    };

    if (error) {
        error->position = 0;
        error->message = NULL;
    }

    parser_skip_ws(&parser);
    slon_value *value = parse_value(&parser);
    if (value == NULL) {
        return 1;
    }
    parser_skip_ws(&parser);
    if (parser.pos != parser.length) {
        parser_error(&parser, "Unexpected trailing content");
        slon_value_free(value);
        return 1;
    }
    *out_value = value;
    return 0;
}

void slon_value_free(slon_value *value) {
    if (value == NULL) {
        return;
    }
    switch (value->type) {
        case SLON_STRING:
            free(value->as.string);
            break;
        case SLON_ARRAY:
            for (size_t i = 0; i < value->as.array.length; i++) {
                slon_value_free(value->as.array.items[i]);
            }
            free(value->as.array.items);
            break;
        case SLON_OBJECT:
            for (size_t i = 0; i < value->as.object.length; i++) {
                free(value->as.object.entries[i].key);
                slon_value_free(value->as.object.entries[i].value);
            }
            free(value->as.object.entries);
            break;
        default:
            break;
    }
    free(value);
}

char *slon_stringify(const slon_value *value) {
    if (value == NULL) {
        return NULL;
    }
    slon_buffer buffer = {0};
    if (stringify_value(value, &buffer) != 0) {
        buffer_free(&buffer);
        return NULL;
    }
    if (buffer_append_char(&buffer, '\0') != 0) {
        buffer_free(&buffer);
        return NULL;
    }
    return buffer.data;
}

void slon_stringify_free(char *text) {
    free(text);
}

static slon_value *parse_value(slon_parser *parser) {
    parser_skip_ws(parser);
    if (parser->pos >= parser->length) {
        parser_error(parser, "Unexpected end of input");
        return NULL;
    }
    char ch = parser->text[parser->pos];
    if (ch == '(') {
        return parse_object(parser);
    }
    if (ch == '[') {
        return parse_array(parser);
    }
    if (ch == '\'' || ch == '"') {
        char *string = parse_quoted_string(parser);
        if (string == NULL) {
            return NULL;
        }
        slon_value *value = slon_value_new(SLON_STRING);
        if (value == NULL) {
            free(string);
            parser_error(parser, SLON_ERROR_OUT_OF_MEMORY);
            return NULL;
        }
        value->as.string = string;
        return value;
    }
    if (ch == '-' || isdigit((unsigned char) ch)) {
        slon_value *datetime = parse_datetime(parser);
        if (datetime != NULL) {
            return datetime;
        }
        return parse_number(parser);
    }
    slon_value *keyword = parse_keyword(parser);
    if (keyword != NULL) {
        return keyword;
    }
    char *unquoted = parse_unquoted_string(parser);
    if (unquoted == NULL) {
        return NULL;
    }
    slon_value *value = slon_value_new(SLON_STRING);
    if (value == NULL) {
        free(unquoted);
        parser_error(parser, SLON_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    value->as.string = unquoted;
    return value;
}

static slon_value *parse_keyword(slon_parser *parser) {
    const char *rest = parser->text + parser->pos;
    if (strncmp(rest, "true", 4) == 0 && parser_is_boundary(parser, parser->pos + 4)) {
        parser->pos += 4;
        slon_value *value = slon_value_new(SLON_BOOL);
        if (value == NULL) {
            parser_error(parser, SLON_ERROR_OUT_OF_MEMORY);
            return NULL;
        }
        value->as.boolean = 1;
        return value;
    }
    if (strncmp(rest, "false", 5) == 0 && parser_is_boundary(parser, parser->pos + 5)) {
        parser->pos += 5;
        slon_value *value = slon_value_new(SLON_BOOL);
        if (value == NULL) {
            parser_error(parser, SLON_ERROR_OUT_OF_MEMORY);
            return NULL;
        }
        value->as.boolean = 0;
        return value;
    }
    if (strncmp(rest, "null", 4) == 0 && parser_is_boundary(parser, parser->pos + 4)) {
        parser->pos += 4;
        return slon_value_new(SLON_NULL);
    }
    return NULL;
}

static slon_value *parse_object(slon_parser *parser) {
    if (!parser_expect(parser, '(')) {
        return NULL;
    }
    slon_value *object = slon_value_new(SLON_OBJECT);
    if (object == NULL) {
        parser_error(parser, SLON_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    object->as.object.length = 0;
    object->as.object.entries = NULL;

    parser_skip_ws(parser);
    if (parser_match(parser, ')')) {
        return object;
    }

    while (1) {
        parser_skip_ws(parser);
        slon_value *key_value = parse_string_like(parser);
        if (key_value == NULL || key_value->type != SLON_STRING) {
            slon_value_free(object);
            if (key_value) {
                slon_value_free(key_value);
            }
            return NULL;
        }
        char *key = key_value->as.string;
        free(key_value);

        parser_skip_ws(parser);
        if (!parser_expect(parser, ':')) {
            free(key);
            slon_value_free(object);
            return NULL;
        }

        slon_value *value = parse_value(parser);
        if (value == NULL) {
            free(key);
            slon_value_free(object);
            return NULL;
        }
        if (object_append(object, key, value) != 0) {
            free(key);
            slon_value_free(value);
            slon_value_free(object);
            parser_error(parser, SLON_ERROR_OUT_OF_MEMORY);
            return NULL;
        }

        parser_skip_ws(parser);
        if (parser_match(parser, ',')) {
            continue;
        }
        if (parser_match(parser, ')')) {
            break;
        }
        parser_error(parser, "Expected ',' or ')'");
        slon_value_free(object);
        return NULL;
    }
    return object;
}

static slon_value *parse_array(slon_parser *parser) {
    if (!parser_expect(parser, '[')) {
        return NULL;
    }
    slon_value *array = slon_value_new(SLON_ARRAY);
    if (array == NULL) {
        parser_error(parser, SLON_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    array->as.array.length = 0;
    array->as.array.items = NULL;

    parser_skip_ws(parser);
    if (parser_match(parser, ']')) {
        return array;
    }
    while (1) {
        slon_value *value = parse_value(parser);
        if (value == NULL) {
            slon_value_free(array);
            return NULL;
        }
        if (array_append(array, value) != 0) {
            slon_value_free(value);
            slon_value_free(array);
            parser_error(parser, SLON_ERROR_OUT_OF_MEMORY);
            return NULL;
        }
        parser_skip_ws(parser);
        if (parser_match(parser, '|')) {
            continue;
        }
        if (parser_match(parser, ']')) {
            break;
        }
        parser_error(parser, "Expected '|' or ']'");
        slon_value_free(array);
        return NULL;
    }
    return array;
}

static slon_value *parse_string_like(slon_parser *parser) {
    parser_skip_ws(parser);
    if (parser->pos >= parser->length) {
        parser_error(parser, "Unexpected end of input");
        return NULL;
    }
    char ch = parser->text[parser->pos];
    char *string;
    if (ch == '\'' || ch == '"') {
        string = parse_quoted_string(parser);
    } else {
        string = parse_unquoted_string(parser);
    }
    if (string == NULL) {
        return NULL;
    }
    slon_value *value = slon_value_new(SLON_STRING);
    if (value == NULL) {
        free(string);
        parser_error(parser, SLON_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    value->as.string = string;
    return value;
}

static slon_value *parse_number(slon_parser *parser) {
    const char *start = parser->text + parser->pos;
    char *endptr;
    double number = strtod(start, &endptr);
    if (endptr == start) {
        parser_error(parser, "Invalid number");
        return NULL;
    }
    size_t consumed = (size_t) (endptr - start);
    if (!parser_is_boundary(parser, parser->pos + consumed)) {
        parser_error(parser, "Invalid number boundary");
        return NULL;
    }
    parser->pos += consumed;
    if (!isfinite(number)) {
        parser_error(parser, "Non-finite number");
        return NULL;
    }
    slon_value *value = slon_value_new(SLON_NUMBER);
    if (value == NULL) {
        parser_error(parser, SLON_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    value->as.number = number;
    return value;
}

static int parse_digits(const char *text, size_t start, size_t length) {
    int result = 0;
    for (size_t i = 0; i < length; i++) {
        result = result * 10 + (text[start + i] - '0');
    }
    return result;
}

static int match_datetime_pattern(const char *text) {
    return isdigit((unsigned char) text[0]) &&
           isdigit((unsigned char) text[1]) &&
           isdigit((unsigned char) text[2]) &&
           isdigit((unsigned char) text[3]) &&
           text[4] == '-' &&
           isdigit((unsigned char) text[5]) &&
           isdigit((unsigned char) text[6]) &&
           text[7] == '-' &&
           isdigit((unsigned char) text[8]) &&
           isdigit((unsigned char) text[9]) &&
           text[10] == '/' &&
           isdigit((unsigned char) text[11]) &&
           isdigit((unsigned char) text[12]) &&
           text[13] == ':' &&
           isdigit((unsigned char) text[14]) &&
           isdigit((unsigned char) text[15]) &&
           text[16] == ':' &&
           isdigit((unsigned char) text[17]) &&
           isdigit((unsigned char) text[18]) &&
           text[19] == '.' &&
           isdigit((unsigned char) text[20]) &&
           isdigit((unsigned char) text[21]) &&
           isdigit((unsigned char) text[22]);
}

static slon_value *parse_datetime(slon_parser *parser) {
    if (parser->pos + 23 > parser->length) {
        return NULL;
    }
    const char *text = parser->text + parser->pos;
    if (!match_datetime_pattern(text)) {
        return NULL;
    }
    if (!parser_is_boundary(parser, parser->pos + 23)) {
        return NULL;
    }
    slon_value *value = slon_value_new(SLON_DATETIME);
    if (value == NULL) {
        parser_error(parser, SLON_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    value->as.datetime.year = parse_digits(text, 0, 4);
    value->as.datetime.month = parse_digits(text, 5, 2);
    value->as.datetime.day = parse_digits(text, 8, 2);
    value->as.datetime.hour = parse_digits(text, 11, 2);
    value->as.datetime.minute = parse_digits(text, 14, 2);
    value->as.datetime.second = parse_digits(text, 17, 2);
    value->as.datetime.millisecond = parse_digits(text, 20, 3);
    parser->pos += 23;
    return value;
}

static void parser_skip_ws(slon_parser *parser) {
    while (parser->pos < parser->length) {
        if (!isspace((unsigned char) parser->text[parser->pos])) {
            break;
        }
        parser->pos++;
    }
}

static int parser_match(slon_parser *parser, char ch) {
    if (parser->pos < parser->length && parser->text[parser->pos] == ch) {
        parser->pos++;
        return 1;
    }
    return 0;
}

static int parser_expect(slon_parser *parser, char ch) {
    if (!parser_match(parser, ch)) {
        char message[32];
        snprintf(message, sizeof(message), "Expected '%c'", ch);
        parser_error(parser, message);
        return 0;
    }
    return 1;
}

static int parser_error(slon_parser *parser, const char *message) {
    if (parser->error != NULL) {
        parser->error->position = (int) parser->pos;
        parser->error->message = message;
    }
    return 0;
}

static int parser_is_boundary(slon_parser *parser, size_t index) {
    if (index >= parser->length) {
        return 1;
    }
    char ch = parser->text[index];
    return is_delimiter(ch) || isspace((unsigned char) ch);
}

static int is_delimiter(char ch) {
    return ch == ':' || ch == ',' || ch == '(' || ch == ')' ||
           ch == '[' || ch == ']' || ch == '|';
}

static char hex_value(char ch) {
    if (ch >= '0' && ch <= '9') {
        return (char) (ch - '0');
    }
    if (ch >= 'a' && ch <= 'f') {
        return (char) (10 + (ch - 'a'));
    }
    if (ch >= 'A' && ch <= 'F') {
        return (char) (10 + (ch - 'A'));
    }
    return -1;
}

static char *parse_quoted_string(slon_parser *parser) {
    char quote = parser->text[parser->pos++];
    slon_buffer buffer = {0};
    while (parser->pos < parser->length) {
        char ch = parser->text[parser->pos++];
        if (ch == quote) {
            if (buffer_append_char(&buffer, '\0') != 0) {
                buffer_free(&buffer);
                parser_error(parser, SLON_ERROR_OUT_OF_MEMORY);
                return NULL;
            }
            return buffer.data;
        }
        if (ch == '\\') {
            if (parser->pos >= parser->length) {
                buffer_free(&buffer);
                parser_error(parser, "Invalid escape sequence");
                return NULL;
            }
            char esc = parser->text[parser->pos++];
            switch (esc) {
                case '"':
                case '\'':
                case '\\':
                case '/':
                    if (buffer_append_char(&buffer, esc) != 0) {
                        buffer_free(&buffer);
                        parser_error(parser, SLON_ERROR_OUT_OF_MEMORY);
                        return NULL;
                    }
                    break;
                case 'b':
                    if (buffer_append_char(&buffer, '\b') != 0) {
                        buffer_free(&buffer);
                        parser_error(parser, SLON_ERROR_OUT_OF_MEMORY);
                        return NULL;
                    }
                    break;
                case 'f':
                    if (buffer_append_char(&buffer, '\f') != 0) {
                        buffer_free(&buffer);
                        parser_error(parser, SLON_ERROR_OUT_OF_MEMORY);
                        return NULL;
                    }
                    break;
                case 'n':
                    if (buffer_append_char(&buffer, '\n') != 0) {
                        buffer_free(&buffer);
                        parser_error(parser, SLON_ERROR_OUT_OF_MEMORY);
                        return NULL;
                    }
                    break;
                case 'r':
                    if (buffer_append_char(&buffer, '\r') != 0) {
                        buffer_free(&buffer);
                        parser_error(parser, SLON_ERROR_OUT_OF_MEMORY);
                        return NULL;
                    }
                    break;
                case 't':
                    if (buffer_append_char(&buffer, '\t') != 0) {
                        buffer_free(&buffer);
                        parser_error(parser, SLON_ERROR_OUT_OF_MEMORY);
                        return NULL;
                    }
                    break;
                case 'u':
                {
                    if (parser->pos + 4 > parser->length) {
                        buffer_free(&buffer);
                        parser_error(parser, "Invalid unicode escape");
                        return NULL;
                    }
                    char h1 = hex_value(parser->text[parser->pos]);
                    char h2 = hex_value(parser->text[parser->pos + 1]);
                    char h3 = hex_value(parser->text[parser->pos + 2]);
                    char h4 = hex_value(parser->text[parser->pos + 3]);
                    if (h1 < 0 || h2 < 0 || h3 < 0 || h4 < 0) {
                        buffer_free(&buffer);
                        parser_error(parser, "Invalid unicode escape");
                        return NULL;
                    }
                    parser->pos += 4;
                    unsigned int code = ((unsigned int) h1 << 12) |
                                        ((unsigned int) h2 << 8) |
                                        ((unsigned int) h3 << 4) |
                                        (unsigned int) h4;
                    if (code <= 0x7F) {
                        if (buffer_append_char(&buffer, (char) code) != 0) {
                            buffer_free(&buffer);
                            parser_error(parser, SLON_ERROR_OUT_OF_MEMORY);
                            return NULL;
                        }
                    } else {
                        char utf8[4];
                        int written = 0;
                        if (code <= 0x7FF) {
                            utf8[0] = (char) (0xC0 | ((code >> 6) & 0x1F));
                            utf8[1] = (char) (0x80 | (code & 0x3F));
                            written = 2;
                        } else {
                            utf8[0] = (char) (0xE0 | ((code >> 12) & 0x0F));
                            utf8[1] = (char) (0x80 | ((code >> 6) & 0x3F));
                            utf8[2] = (char) (0x80 | (code & 0x3F));
                            written = 3;
                        }
                        if (buffer_append_len(&buffer, utf8, (size_t) written) != 0) {
                            buffer_free(&buffer);
                            parser_error(parser, SLON_ERROR_OUT_OF_MEMORY);
                            return NULL;
                        }
                    }
                    break;
                }
                default:
                    buffer_free(&buffer);
                    parser_error(parser, "Unknown escape sequence");
                    return NULL;
            }
        } else {
            if (buffer_append_char(&buffer, ch) != 0) {
                buffer_free(&buffer);
                parser_error(parser, SLON_ERROR_OUT_OF_MEMORY);
                return NULL;
            }
        }
    }
    buffer_free(&buffer);
    parser_error(parser, "Unterminated string literal");
    return NULL;
}

static char *parse_unquoted_string(slon_parser *parser) {
    size_t start = parser->pos;
    while (parser->pos < parser->length) {
        char ch = parser->text[parser->pos];
        if (is_delimiter(ch) || isspace((unsigned char) ch)) {
            break;
        }
        parser->pos++;
    }
    size_t end = parser->pos;
    while (start < end && isspace((unsigned char) parser->text[start])) {
        start++;
    }
    while (end > start && isspace((unsigned char) parser->text[end - 1])) {
        end--;
    }
    if (start == end) {
        parser_error(parser, "Empty string value");
        return NULL;
    }
    size_t length = end - start;
    char *copy = slon_strndup(parser->text + start, length);
    if (copy == NULL) {
        parser_error(parser, SLON_ERROR_OUT_OF_MEMORY);
        return NULL;
    }
    return copy;
}

static char *slon_strndup(const char *text, size_t length) {
    char *copy = (char *) malloc(length + 1);
    if (copy == NULL) {
        return NULL;
    }
    memcpy(copy, text, length);
    copy[length] = '\0';
    return copy;
}

static slon_value *slon_value_new(slon_type type) {
    slon_value *value = (slon_value *) calloc(1, sizeof(slon_value));
    if (value == NULL) {
        return NULL;
    }
    value->type = type;
    return value;
}

static int object_append(slon_value *object, char *key, slon_value *value) {
    size_t count = object->as.object.length;
    slon_pair *entries = (slon_pair *) realloc(object->as.object.entries, (count + 1) * sizeof(slon_pair));
    if (entries == NULL) {
        return 1;
    }
    object->as.object.entries = entries;
    object->as.object.entries[count].key = key;
    object->as.object.entries[count].value = value;
    object->as.object.length = count + 1;
    return 0;
}

static int array_append(slon_value *array, slon_value *value) {
    size_t count = array->as.array.length;
    slon_value **items = (slon_value **) realloc(array->as.array.items, (count + 1) * sizeof(slon_value *));
    if (items == NULL) {
        return 1;
    }
    array->as.array.items = items;
    array->as.array.items[count] = value;
    array->as.array.length = count + 1;
    return 0;
}

static int buffer_reserve(slon_buffer *buffer, size_t additional) {
    size_t needed = buffer->length + additional;
    if (needed <= buffer->capacity) {
        return 0;
    }
    size_t capacity = buffer->capacity ? buffer->capacity : 64;
    while (capacity < needed) {
        capacity *= 2;
    }
    char *data = (char *) realloc(buffer->data, capacity);
    if (data == NULL) {
        return 1;
    }
    buffer->data = data;
    buffer->capacity = capacity;
    return 0;
}

static int buffer_append_char(slon_buffer *buffer, char ch) {
    if (buffer_reserve(buffer, 1) != 0) {
        return 1;
    }
    buffer->data[buffer->length++] = ch;
    return 0;
}

static int buffer_append_str(slon_buffer *buffer, const char *text) {
    return buffer_append_len(buffer, text, strlen(text));
}

static int buffer_append_len(slon_buffer *buffer, const char *text, size_t length) {
    if (buffer_reserve(buffer, length) != 0) {
        return 1;
    }
    memcpy(buffer->data + buffer->length, text, length);
    buffer->length += length;
    return 0;
}

static void buffer_free(slon_buffer *buffer) {
    free(buffer->data);
    buffer->data = NULL;
    buffer->length = 0;
    buffer->capacity = 0;
}

static int stringify_datetime(const slon_datetime *dt, slon_buffer *buffer) {
    char temp[32];
    int written = snprintf(temp, sizeof(temp), "%04d-%02d-%02d/%02d:%02d:%02d.%03d",
                           dt->year, dt->month, dt->day,
                           dt->hour, dt->minute, dt->second, dt->millisecond);
    if (written < 0) {
        return 1;
    }
    return buffer_append_len(buffer, temp, (size_t) written);
}

static int stringify_value(const slon_value *value, slon_buffer *buffer) {
    switch (value->type) {
        case SLON_NULL:
            return buffer_append_str(buffer, "null");
        case SLON_BOOL:
            return buffer_append_str(buffer, value->as.boolean ? "true" : "false");
        case SLON_NUMBER: {
            char temp[32];
            int written = snprintf(temp, sizeof(temp), "%.15g", value->as.number);
            if (written < 0) {
                return 1;
            }
            return buffer_append_len(buffer, temp, (size_t) written);
        }
        case SLON_STRING:
            return stringify_string(value->as.string, buffer);
        case SLON_ARRAY: {
            if (buffer_append_char(buffer, '[') != 0) {
                return 1;
            }
            for (size_t i = 0; i < value->as.array.length; i++) {
                if (i > 0) {
                    if (buffer_append_str(buffer, " | ") != 0) {
                        return 1;
                    }
                }
                if (stringify_value(value->as.array.items[i], buffer) != 0) {
                    return 1;
                }
            }
            return buffer_append_char(buffer, ']');
        }
        case SLON_OBJECT: {
            if (buffer_append_char(buffer, '(') != 0) {
                return 1;
            }
            for (size_t i = 0; i < value->as.object.length; i++) {
                if (i > 0) {
                    if (buffer_append_str(buffer, ", ") != 0) {
                        return 1;
                    }
                }
                const char *key = value->as.object.entries[i].key;
                if (stringify_key(key, buffer) != 0) {
                    return 1;
                }
                if (buffer_append_str(buffer, ": ") != 0) {
                    return 1;
                }
                if (stringify_value(value->as.object.entries[i].value, buffer) != 0) {
                    return 1;
                }
            }
            return buffer_append_char(buffer, ')');
        }
        case SLON_DATETIME:
            return stringify_datetime(&value->as.datetime, buffer);
        default:
            return 1;
    }
}

static int stringify_string(const char *value, slon_buffer *buffer) {
    if (buffer_append_char(buffer, '\'') != 0) {
        return 1;
    }
    for (const char *ptr = value; *ptr; ptr++) {
        char ch = *ptr;
        switch (ch) {
            case '\\':
                if (buffer_append_str(buffer, "\\\\") != 0) return 1;
                break;
            case '\'':
                if (buffer_append_str(buffer, "\\'") != 0) return 1;
                break;
            case '\n':
                if (buffer_append_str(buffer, "\\n") != 0) return 1;
                break;
            case '\r':
                if (buffer_append_str(buffer, "\\r") != 0) return 1;
                break;
            case '\t':
                if (buffer_append_str(buffer, "\\t") != 0) return 1;
                break;
            default:
                if (buffer_append_char(buffer, ch) != 0) return 1;
                break;
        }
    }
    return buffer_append_char(buffer, '\'');
}

static int stringify_key(const char *key, slon_buffer *buffer) {
    if (!requires_quoting(key)) {
        return buffer_append_str(buffer, key);
    }
    return stringify_string(key, buffer);
}

static int requires_quoting(const char *key) {
    if (key == NULL || key[0] == '\0') {
        return 1;
    }
    for (const char *ptr = key; *ptr; ptr++) {
        char ch = *ptr;
        if (is_delimiter(ch) || ch == '\'' || ch == '"' || isspace((unsigned char) ch)) {
            return 1;
        }
    }
    return 0;
}
