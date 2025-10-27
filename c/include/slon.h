#ifndef SLON_H
#define SLON_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SLON_NULL,
    SLON_BOOL,
    SLON_NUMBER,
    SLON_STRING,
    SLON_ARRAY,
    SLON_OBJECT,
    SLON_DATETIME
} slon_type;

typedef struct {
    int year;
    int month;
    int day;
    int hour;
    int minute;
    int second;
    int millisecond;
} slon_datetime;

typedef struct slon_value slon_value;

typedef struct {
    size_t length;
    slon_value **items;
} slon_array;

typedef struct {
    char *key;
    slon_value *value;
} slon_pair;

typedef struct {
    size_t length;
    slon_pair *entries;
} slon_object;

struct slon_value {
    slon_type type;
    union {
        int boolean;
        double number;
        char *string;
        slon_array array;
        slon_object object;
        slon_datetime datetime;
    } as;
};

typedef struct {
    int position;
    const char *message;
} slon_error;

int slon_parse(const char *text, slon_value **out_value, slon_error *error);

void slon_value_free(slon_value *value);

char *slon_stringify(const slon_value *value);

void slon_stringify_free(char *text);

#ifdef __cplusplus
}
#endif

#endif /* SLON_H */
