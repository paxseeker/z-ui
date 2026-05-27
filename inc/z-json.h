#ifndef Z_JSON_H
#define Z_JSON_H

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>

#define Z_JSON_DEFAULT_INDENT 4

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    Z_JSON_NULL,
    Z_JSON_STRING,
    Z_JSON_NUMBER,
    Z_JSON_BOOL,
    Z_JSON_OBJECT,
    Z_JSON_ARRAY
} Z_JSON_TYPE;

typedef struct ZJson {
    Z_JSON_TYPE type;
    union {
        const char *str_val;
        double num_val;
        bool bool_val;
        struct {
            struct ZJson **items;
            size_t count;
            size_t capacity;
        } array;
        struct {
            const char **keys;
            struct ZJson **values;
            size_t count;
            size_t capacity;
        } object;
    } value;
} ZJson;

ZJson *z_json_parse_str(const char *str);
ZJson *z_json_parse_file(const char *filename);
bool z_json_write_file(ZJson *json, const char *filename);
bool z_json_write_file_pretty(ZJson *json, const char *filename, int indent);

ZJson *z_json_create_null(void);
ZJson *z_json_create_string(const char *str);
ZJson *z_json_create_number(double num);
ZJson *z_json_create_bool(bool val);
ZJson *z_json_create_object(void);
ZJson *z_json_create_array(void);

bool z_json_object_set(ZJson *obj, const char *key, ZJson *value);
bool z_json_object_remove(ZJson *obj, const char *key);
bool z_json_array_append(ZJson *arr, ZJson *value);
bool z_json_array_remove(ZJson *arr, size_t index);

ZJson *z_json_object_get(ZJson *obj, const char *key);
ZJson *z_json_array_get(ZJson *arr, size_t index);
size_t z_json_array_size(ZJson *arr);
ZJson *z_json_get_path(ZJson *json, const char *path);
char *z_json_stringify(ZJson *json, size_t *out_len);
char *z_json_stringify_pretty(ZJson *json, size_t *out_len, int indent);
void z_json_free(ZJson *json);

typedef ZJson z_json_t;

z_json_t* z_json_parse(const char *json_str);
z_json_t* z_json_object(void);
z_json_t* z_json_array(void);
double z_json_get_number(z_json_t *json, const char *key, double default_value);
bool z_json_get_bool(z_json_t *json, const char *key, bool default_value);
const char* z_json_get_string(z_json_t *json, const char *key, const char *default_value);
bool z_json_has_key(z_json_t *json, const char *key);
char* z_json_serialize(z_json_t *json);
void z_json_set_number(z_json_t *json, const char *key, double value);
void z_json_set_bool(z_json_t *json, const char *key, bool value);
void z_json_set_string(z_json_t *json, const char *key, const char *value);
void z_json_set_object(z_json_t *json, const char *key, z_json_t *object);

#ifdef __cplusplus
}
#endif

#endif /* Z_JSON_H */
#ifdef Z_JSON_IMPLEMENTATION

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

typedef struct {
    const char *str;
    size_t pos;
    size_t len;
} z_json_parser;

static void z_json_skip_whitespace(z_json_parser *p) {
    while (p->pos < p->len && (p->str[p->pos] == ' ' || p->str[p->pos] == '\t' ||
           p->str[p->pos] == '\n' || p->str[p->pos] == '\r')) {
        p->pos++;
    }
}

static ZJson *z_json_parse_value(z_json_parser *p);

static char *z_json_parse_string(z_json_parser *p) {
    if (p->pos >= p->len || p->str[p->pos] != '"') return NULL;
    p->pos++;

    size_t start = p->pos;
    size_t len = 0;
    while (p->pos < p->len && p->str[p->pos] != '"') {
        if (p->str[p->pos] == '\\') {
            p->pos++;
            if (p->pos >= p->len) return NULL;
        }
        p->pos++;
        len++;
    }
    if (p->pos >= p->len) return NULL;
    p->pos++;

    char *result = (char *)malloc(len + 1);
    if (!result) return NULL;

    size_t rpos = 0;
    size_t pos = start;
    while (pos < p->pos - 1) {
        if (p->str[pos] == '\\') {
            pos++;
            switch (p->str[pos]) {
                case '"': result[rpos++] = '"'; break;
                case '\\': result[rpos++] = '\\'; break;
                case '/': result[rpos++] = '/'; break;
                case 'b': result[rpos++] = '\b'; break;
                case 'f': result[rpos++] = '\f'; break;
                case 'n': result[rpos++] = '\n'; break;
                case 'r': result[rpos++] = '\r'; break;
                case 't': result[rpos++] = '\t'; break;
                case 'u': {
                    if (pos + 4 >= p->pos - 1) return NULL;
                    int codepoint = 0;
                    for (int i = 0; i < 4; i++) {
                        char c = p->str[pos + 1 + i];
                        codepoint <<= 4;
                        if (c >= '0' && c <= '9') codepoint |= c - '0';
                        else if (c >= 'a' && c <= 'f') codepoint |= c - 'a' + 10;
                        else if (c >= 'A' && c <= 'F') codepoint |= c - 'A' + 10;
                    }
                    if (codepoint < 0x80) {
                        result[rpos++] = (char)codepoint;
                    } else if (codepoint < 0x800) {
                        result[rpos++] = (char)(0xC0 | (codepoint >> 6));
                        result[rpos++] = (char)(0x80 | (codepoint & 0x3F));
                    } else {
                        result[rpos++] = (char)(0xE0 | (codepoint >> 12));
                        result[rpos++] = (char)(0x80 | ((codepoint >> 6) & 0x3F));
                        result[rpos++] = (char)(0x80 | (codepoint & 0x3F));
                    }
                    pos += 4;
                    break;
                }
            }
        } else {
            result[rpos++] = p->str[pos];
        }
        pos++;
    }
    result[rpos] = '\0';
    return result;
}

static ZJson *z_json_parse_number(z_json_parser *p) {
    size_t start = p->pos;
    if (p->pos < p->len && p->str[p->pos] == '-') p->pos++;
    while (p->pos < p->len && p->str[p->pos] >= '0' && p->str[p->pos] <= '9') p->pos++;
    if (p->pos < p->len && p->str[p->pos] == '.') {
        p->pos++;
        while (p->pos < p->len && p->str[p->pos] >= '0' && p->str[p->pos] <= '9') p->pos++;
    }
    if (p->pos < p->len && (p->str[p->pos] == 'e' || p->str[p->pos] == 'E')) {
        p->pos++;
        if (p->pos < p->len && (p->str[p->pos] == '+' || p->str[p->pos] == '-')) p->pos++;
        while (p->pos < p->len && p->str[p->pos] >= '0' && p->str[p->pos] <= '9') p->pos++;
    }

    char num_str[64];
    size_t len = p->pos - start;
    if (len >= sizeof(num_str)) len = sizeof(num_str) - 1;
    memcpy(num_str, p->str + start, len);
    num_str[len] = '\0';

    ZJson *json = (ZJson *)malloc(sizeof(ZJson));
    if (!json) return NULL;
    json->type = Z_JSON_NUMBER;
    json->value.num_val = strtod(num_str, NULL);
    return json;
}

static ZJson *z_json_parse_object(z_json_parser *p) {
    if (p->pos >= p->len || p->str[p->pos] != '{') return NULL;
    p->pos++;
    z_json_skip_whitespace(p);

    ZJson *json = (ZJson *)malloc(sizeof(ZJson));
    if (!json) return NULL;
    json->type = Z_JSON_OBJECT;
    json->value.object.keys = NULL;
    json->value.object.values = NULL;
    json->value.object.count = 0;
    json->value.object.capacity = 0;

    if (p->pos < p->len && p->str[p->pos] == '}') {
        p->pos++;
        return json;
    }

    while (p->pos < p->len) {
        z_json_skip_whitespace(p);
        char *key = z_json_parse_string(p);
        if (!key) { z_json_free(json); return NULL; }

        z_json_skip_whitespace(p);
        if (p->pos >= p->len || p->str[p->pos] != ':') { free(key); z_json_free(json); return NULL; }
        p->pos++;

        ZJson *val = z_json_parse_value(p);
        if (!val) { free(key); z_json_free(json); return NULL; }

        if (json->value.object.count >= json->value.object.capacity) {
            size_t new_cap = json->value.object.capacity == 0 ? 8 : json->value.object.capacity * 2;
            const char **new_keys = (const char **)realloc(json->value.object.keys, new_cap * sizeof(const char *));
            ZJson **new_values = (ZJson **)realloc(json->value.object.values, new_cap * sizeof(ZJson *));
            if (!new_keys || !new_values) { free(key); z_json_free(json); return NULL; }
            json->value.object.keys = new_keys;
            json->value.object.values = new_values;
            json->value.object.capacity = new_cap;
        }
        json->value.object.keys[json->value.object.count] = key;
        json->value.object.values[json->value.object.count] = val;
        json->value.object.count++;

        z_json_skip_whitespace(p);
        if (p->pos < p->len && p->str[p->pos] == ',') {
            p->pos++;
        } else {
            break;
        }
    }

    z_json_skip_whitespace(p);
    if (p->pos < p->len && p->str[p->pos] == '}') p->pos++;
    return json;
}

static ZJson *z_json_parse_array(z_json_parser *p) {
    if (p->pos >= p->len || p->str[p->pos] != '[') return NULL;
    p->pos++;
    z_json_skip_whitespace(p);

    ZJson *json = (ZJson *)malloc(sizeof(ZJson));
    if (!json) return NULL;
    json->type = Z_JSON_ARRAY;
    json->value.array.items = NULL;
    json->value.array.count = 0;
    json->value.array.capacity = 0;

    if (p->pos < p->len && p->str[p->pos] == ']') {
        p->pos++;
        return json;
    }

    while (p->pos < p->len) {
        z_json_skip_whitespace(p);
        ZJson *val = z_json_parse_value(p);
        if (!val) { z_json_free(json); return NULL; }

        if (json->value.array.count >= json->value.array.capacity) {
            size_t new_cap = json->value.array.capacity == 0 ? 8 : json->value.array.capacity * 2;
            ZJson **new_items = (ZJson **)realloc(json->value.array.items, new_cap * sizeof(ZJson *));
            if (!new_items) { z_json_free(json); return NULL; }
            json->value.array.items = new_items;
            json->value.array.capacity = new_cap;
        }
        json->value.array.items[json->value.array.count++] = val;

        z_json_skip_whitespace(p);
        if (p->pos < p->len && p->str[p->pos] == ',') {
            p->pos++;
        } else {
            break;
        }
    }

    z_json_skip_whitespace(p);
    if (p->pos < p->len && p->str[p->pos] == ']') p->pos++;
    return json;
}

static ZJson *z_json_parse_value(z_json_parser *p) {
    z_json_skip_whitespace(p);
    if (p->pos >= p->len) return NULL;

    char c = p->str[p->pos];
    if (c == '"') {
        char *str = z_json_parse_string(p);
        if (!str) return NULL;
        ZJson *json = (ZJson *)malloc(sizeof(ZJson));
        if (!json) { free(str); return NULL; }
        json->type = Z_JSON_STRING;
        json->value.str_val = str;
        return json;
    } else if (c == '{') {
        return z_json_parse_object(p);
    } else if (c == '[') {
        return z_json_parse_array(p);
    } else if (c == '-' || (c >= '0' && c <= '9')) {
        return z_json_parse_number(p);
    } else if (p->pos + 4 <= p->len && strncmp(p->str + p->pos, "true", 4) == 0) {
        ZJson *json = (ZJson *)malloc(sizeof(ZJson));
        if (!json) return NULL;
        json->type = Z_JSON_BOOL;
        json->value.bool_val = true;
        p->pos += 4;
        return json;
    } else if (p->pos + 5 <= p->len && strncmp(p->str + p->pos, "false", 5) == 0) {
        ZJson *json = (ZJson *)malloc(sizeof(ZJson));
        if (!json) return NULL;
        json->type = Z_JSON_BOOL;
        json->value.bool_val = false;
        p->pos += 5;
        return json;
    } else if (p->pos + 4 <= p->len && strncmp(p->str + p->pos, "null", 4) == 0) {
        ZJson *json = (ZJson *)malloc(sizeof(ZJson));
        if (!json) return NULL;
        json->type = Z_JSON_NULL;
        p->pos += 4;
        return json;
    }
    return NULL;
}

ZJson *z_json_parse_str(const char *str) {
    z_json_parser p;
    p.str = str;
    p.pos = 0;
    p.len = strlen(str);
    return z_json_parse_value(&p);
}

ZJson *z_json_parse_file(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return NULL;

    fseek(f, 0, SEEK_END);
    long len = ftell(f);
    fseek(f, 0, SEEK_SET);

    char *str = (char *)malloc(len + 1);
    if (!str) { fclose(f); return NULL; }
    fread(str, 1, len, f);
    str[len] = '\0';
    fclose(f);

    ZJson *result = z_json_parse_str(str);
    free(str);
    return result;
}

ZJson *z_json_object_get(ZJson *obj, const char *key) {
    if (!obj || obj->type != Z_JSON_OBJECT) return NULL;
    for (size_t i = 0; i < obj->value.object.count; i++) {
        if (strcmp(obj->value.object.keys[i], key) == 0) {
            return obj->value.object.values[i];
        }
    }
    return NULL;
}

ZJson *z_json_array_get(ZJson *arr, size_t index) {
    if (!arr || arr->type != Z_JSON_ARRAY) return NULL;
    if (index >= arr->value.array.count) return NULL;
    return arr->value.array.items[index];
}

ZJson *z_json_get_path(ZJson *json, const char *path) {
    if (!json || !path) return NULL;

    char *path_copy = strdup(path);
    if (!path_copy) return NULL;

    ZJson *current = json;
    char *p = path_copy;
    char *token;

    while ((token = strtok(p, ".")) != NULL) {
        p = NULL;

        char *bracket = strchr(token, '[');
        if (bracket) {
            *bracket = '\0';
            char *end = strchr(bracket + 1, ']');
            if (end) *end = '\0';

            if (strlen(token) > 0) {
                current = z_json_object_get(current, token);
                if (!current) { free(path_copy); return NULL; }
            }

            if (bracket[1] != '\0') {
                int index = atoi(bracket + 1);
                current = z_json_array_get(current, (size_t)index);
                if (!current) { free(path_copy); return NULL; }
            }
        } else {
            current = z_json_object_get(current, token);
            if (!current) { free(path_copy); return NULL; }
        }
    }

    free(path_copy);
    return current;
}

size_t z_json_array_size(ZJson *arr) {
    if (!arr || arr->type != Z_JSON_ARRAY) return 0;
    return arr->value.array.count;
}

static void z_json_stringify_value(ZJson *json, char **buf, size_t *pos, size_t *cap) {
    if (!json) return;

    if (*pos + 64 >= *cap) {
        *cap = *cap == 0 ? 256 : *cap * 2;
        *buf = (char *)realloc(*buf, *cap);
    }

    switch (json->type) {
        case Z_JSON_NULL:
            strcat(*buf, "null");
            *pos += 4;
            break;
        case Z_JSON_BOOL:
            if (json->value.bool_val) { strcat(*buf, "true"); *pos += 4; }
            else { strcat(*buf, "false"); *pos += 5; }
            break;
        case Z_JSON_NUMBER: {
            char num_str[64];
            snprintf(num_str, sizeof(num_str), "%.15g", json->value.num_val);
            strcat(*buf, num_str);
            *pos += strlen(num_str);
            break;
        }
        case Z_JSON_STRING: {
            strcat(*buf, "\"");
            *pos += 1;
            const char *s = json->value.str_val;
            while (*s) {
                if (*s == '"' || *s == '\\' || *s == '\n' || *s == '\r' || *s == '\t') {
                    if (*pos + 2 >= *cap) {
                        *cap = *cap * 2;
                        *buf = (char *)realloc(*buf, *cap);
                    }
                    char escape[2] = {'\\', *s == '"' ? '"' : *s == '\\' ? '\\' : *s == '\n' ? 'n' : *s == '\r' ? 'r' : 't'};
                    strncat(*buf, escape, 2);
                    *pos += 2;
                } else {
                    (*buf)[(*pos)++] = *s;
                    (*buf)[*pos] = '\0';
                }
                s++;
            }
            strcat(*buf, "\"");
            *pos += 1;
            break;
        }
        case Z_JSON_ARRAY: {
            strcat(*buf, "[");
            *pos += 1;
            for (size_t i = 0; i < json->value.array.count; i++) {
                if (i > 0) { strcat(*buf, ","); *pos += 1; }
                z_json_stringify_value(json->value.array.items[i], buf, pos, cap);
            }
            strcat(*buf, "]");
            *pos += 1;
            break;
        }
        case Z_JSON_OBJECT: {
            strcat(*buf, "{");
            *pos += 1;
            for (size_t i = 0; i < json->value.object.count; i++) {
                if (i > 0) { strcat(*buf, ","); *pos += 1; }
                strcat(*buf, "\"");
                *pos += 1;
                const char *k = json->value.object.keys[i];
                while (*k) (*buf)[(*pos)++] = *k++;
                (*buf)[*pos] = '\0';
                strcat(*buf, "\":");
                *pos += 2;
                z_json_stringify_value(json->value.object.values[i], buf, pos, cap);
            }
            strcat(*buf, "}");
            *pos += 1;
            break;
        }
    }
}

char *z_json_stringify(ZJson *json, size_t *out_len) {
    if (!json) { if (out_len) *out_len = 0; return NULL; }
    size_t pos = 0, cap = 256;
    char *buf = (char *)malloc(cap);
    if (!buf) return NULL;
    buf[0] = '\0';

    z_json_stringify_value(json, &buf, &pos, &cap);
    if (out_len) *out_len = pos;
    return buf;
}

static void z_json_add_indent(char **buf, size_t *pos, size_t *cap, int level, const char *indent_str) {
    if (*pos + 1 >= *cap) {
        *cap = *cap * 2;
        *buf = (char *)realloc(*buf, *cap);
    }
    (*buf)[(*pos)++] = '\n';
    (*buf)[*pos] = '\0';

    for (int i = 0; i < level; i++) {
        size_t indent_len = strlen(indent_str);
        if (*pos + indent_len >= *cap) {
            *cap = *cap * 2;
            *buf = (char *)realloc(*buf, *cap);
        }
        strncat(*buf, indent_str, indent_len);
        *pos += indent_len;
    }
}

static void z_json_stringify_value_pretty(ZJson *json, char **buf, size_t *pos, size_t *cap, int indent_level, const char *indent_str) {
    if (!json) return;

    switch (json->type) {
        case Z_JSON_NULL:
            if (*pos + 4 >= *cap) { *cap = *cap * 2; *buf = (char *)realloc(*buf, *cap); }
            memcpy(*buf + *pos, "null", 4);
            (*buf)[*pos + 4] = '\0';
            *pos += 4;
            break;
        case Z_JSON_BOOL:
            if (json->value.bool_val) {
                if (*pos + 4 >= *cap) { *cap = *cap * 2; *buf = (char *)realloc(*buf, *cap); }
                memcpy(*buf + *pos, "true", 4);
                (*buf)[*pos + 4] = '\0';
                *pos += 4;
            } else {
                if (*pos + 5 >= *cap) { *cap = *cap * 2; *buf = (char *)realloc(*buf, *cap); }
                memcpy(*buf + *pos, "false", 5);
                (*buf)[*pos + 5] = '\0';
                *pos += 5;
            }
            break;
        case Z_JSON_NUMBER: {
            char num_str[64];
            snprintf(num_str, sizeof(num_str), "%.15g", json->value.num_val);
            size_t num_len = strlen(num_str);
            if (*pos + num_len >= *cap) { *cap = *cap * 2; *buf = (char *)realloc(*buf, *cap); }
            strncat(*buf, num_str, num_len);
            *pos += num_len;
            break;
        }
        case Z_JSON_STRING: {
            if (*pos + 1 >= *cap) { *cap = *cap * 2; *buf = (char *)realloc(*buf, *cap); }
            (*buf)[(*pos)++] = '"';
            (*buf)[*pos] = '\0';

            const char *s = json->value.str_val;
            while (*s) {
                if (*s == '"' || *s == '\\' || *s == '\n' || *s == '\r' || *s == '\t') {
                    if (*pos + 2 >= *cap) { *cap = *cap * 2; *buf = (char *)realloc(*buf, *cap); }
                    (*buf)[(*pos)++] = '\\';
                    (*buf)[(*pos)++] = *s == '"' ? '"' : *s == '\\' ? '\\' : *s == '\n' ? 'n' : *s == '\r' ? 'r' : 't';
                    (*buf)[*pos] = '\0';
                } else {
                    if (*pos + 1 >= *cap) { *cap = *cap * 2; *buf = (char *)realloc(*buf, *cap); }
                    (*buf)[(*pos)++] = *s;
                    (*buf)[*pos] = '\0';
                }
                s++;
            }

            if (*pos + 1 >= *cap) { *cap = *cap * 2; *buf = (char *)realloc(*buf, *cap); }
            (*buf)[(*pos)++] = '"';
            (*buf)[*pos] = '\0';
            break;
        }
        case Z_JSON_ARRAY: {
            if (*pos + 1 >= *cap) { *cap = *cap * 2; *buf = (char *)realloc(*buf, *cap); }
            (*buf)[(*pos)++] = '[';
            (*buf)[*pos] = '\0';

            for (size_t i = 0; i < json->value.array.count; i++) {
                z_json_add_indent(buf, pos, cap, indent_level + 1, indent_str);
                z_json_stringify_value_pretty(json->value.array.items[i], buf, pos, cap, indent_level + 1, indent_str);
                if (i < json->value.array.count - 1) {
                    if (*pos + 1 >= *cap) { *cap = *cap * 2; *buf = (char *)realloc(*buf, *cap); }
                    (*buf)[(*pos)++] = ',';
                    (*buf)[*pos] = '\0';
                }
            }

            if (json->value.array.count > 0) {
                z_json_add_indent(buf, pos, cap, indent_level, indent_str);
            }

            if (*pos + 1 >= *cap) { *cap = *cap * 2; *buf = (char *)realloc(*buf, *cap); }
            (*buf)[(*pos)++] = ']';
            (*buf)[*pos] = '\0';
            break;
        }
        case Z_JSON_OBJECT: {
            if (*pos + 1 >= *cap) { *cap = *cap * 2; *buf = (char *)realloc(*buf, *cap); }
            (*buf)[(*pos)++] = '{';
            (*buf)[*pos] = '\0';

            for (size_t i = 0; i < json->value.object.count; i++) {
                z_json_add_indent(buf, pos, cap, indent_level + 1, indent_str);

                if (*pos + 1 >= *cap) { *cap = *cap * 2; *buf = (char *)realloc(*buf, *cap); }
                (*buf)[(*pos)++] = '"';
                (*buf)[*pos] = '\0';

                const char *k = json->value.object.keys[i];
                while (*k) {
                    if (*pos + 1 >= *cap) { *cap = *cap * 2; *buf = (char *)realloc(*buf, *cap); }
                    (*buf)[(*pos)++] = *k;
                    (*buf)[*pos] = '\0';
                    k++;
                }

                if (*pos + 3 >= *cap) { *cap = *cap * 2; *buf = (char *)realloc(*buf, *cap); }
                (*buf)[(*pos)++] = '"';
                (*buf)[(*pos)++] = ':';
                (*buf)[(*pos)++] = ' ';
                (*buf)[*pos] = '\0';

                z_json_stringify_value_pretty(json->value.object.values[i], buf, pos, cap, indent_level + 1, indent_str);

                if (i < json->value.object.count - 1) {
                    if (*pos + 1 >= *cap) { *cap = *cap * 2; *buf = (char *)realloc(*buf, *cap); }
                    (*buf)[(*pos)++] = ',';
                    (*buf)[*pos] = '\0';
                }
            }

            if (json->value.object.count > 0) {
                z_json_add_indent(buf, pos, cap, indent_level, indent_str);
            }

            if (*pos + 1 >= *cap) { *cap = *cap * 2; *buf = (char *)realloc(*buf, *cap); }
            (*buf)[(*pos)++] = '}';
            (*buf)[*pos] = '\0';
            break;
        }
    }
}

char *z_json_stringify_pretty(ZJson *json, size_t *out_len, int indent) {
    if (!json) { if (out_len) *out_len = 0; return NULL; }
    if (indent <= 0) return z_json_stringify(json, out_len);

    char *indent_str = (char *)malloc(indent + 1);
    if (!indent_str) return NULL;
    for (int i = 0; i < indent; i++) indent_str[i] = ' ';
    indent_str[indent] = '\0';

    size_t pos = 0, cap = 256;
    char *buf = (char *)malloc(cap);
    if (!buf) { free(indent_str); return NULL; }
    buf[0] = '\0';

    z_json_stringify_value_pretty(json, &buf, &pos, &cap, 0, indent_str);

    free(indent_str);
    if (out_len) *out_len = pos;
    return buf;
}

ZJson *z_json_create_null(void) {
    ZJson *json = (ZJson *)malloc(sizeof(ZJson));
    if (!json) return NULL;
    json->type = Z_JSON_NULL;
    return json;
}

ZJson *z_json_create_string(const char *str) {
    ZJson *json = (ZJson *)malloc(sizeof(ZJson));
    if (!json) return NULL;
    json->type = Z_JSON_STRING;
    json->value.str_val = (str && str[0]) ? strdup(str) : strdup("");
    return json;
}

ZJson *z_json_create_number(double num) {
    ZJson *json = (ZJson *)malloc(sizeof(ZJson));
    if (!json) return NULL;
    json->type = Z_JSON_NUMBER;
    json->value.num_val = num;
    return json;
}

ZJson *z_json_create_bool(bool val) {
    ZJson *json = (ZJson *)malloc(sizeof(ZJson));
    if (!json) return NULL;
    json->type = Z_JSON_BOOL;
    json->value.bool_val = val;
    return json;
}

ZJson *z_json_create_object(void) {
    ZJson *json = (ZJson *)malloc(sizeof(ZJson));
    if (!json) return NULL;
    json->type = Z_JSON_OBJECT;
    json->value.object.keys = NULL;
    json->value.object.values = NULL;
    json->value.object.count = 0;
    json->value.object.capacity = 0;
    return json;
}

ZJson *z_json_create_array(void) {
    ZJson *json = (ZJson *)malloc(sizeof(ZJson));
    if (!json) return NULL;
    json->type = Z_JSON_ARRAY;
    json->value.array.items = NULL;
    json->value.array.count = 0;
    json->value.array.capacity = 0;
    return json;
}

static int z_json_object_find_index(ZJson *obj, const char *key) {
    for (size_t i = 0; i < obj->value.object.count; i++) {
        if (strcmp(obj->value.object.keys[i], key) == 0) {
            return (int)i;
        }
    }
    return -1;
}

bool z_json_object_set(ZJson *obj, const char *key, ZJson *value) {
    if (!obj || obj->type != Z_JSON_OBJECT || !key || !value) return false;

    int idx = z_json_object_find_index(obj, key);
    if (idx >= 0) {
        z_json_free(obj->value.object.values[idx]);
        obj->value.object.values[idx] = value;
        return true;
    }

    if (obj->value.object.count >= obj->value.object.capacity) {
        size_t new_cap = obj->value.object.capacity == 0 ? 8 : obj->value.object.capacity * 2;
        const char **new_keys = (const char **)realloc(obj->value.object.keys, new_cap * sizeof(const char *));
        ZJson **new_values = (ZJson **)realloc(obj->value.object.values, new_cap * sizeof(ZJson *));
        if (!new_keys || !new_values) return false;
        obj->value.object.keys = new_keys;
        obj->value.object.values = new_values;
        obj->value.object.capacity = new_cap;
    }

    char *key_dup = strdup(key);
    if (!key_dup) return false;

    obj->value.object.keys[obj->value.object.count] = key_dup;
    obj->value.object.values[obj->value.object.count] = value;
    obj->value.object.count++;
    return true;
}

bool z_json_object_remove(ZJson *obj, const char *key) {
    if (!obj || obj->type != Z_JSON_OBJECT || !key) return false;

    int idx = z_json_object_find_index(obj, key);
    if (idx < 0) return false;

    free((void *)obj->value.object.keys[idx]);
    z_json_free(obj->value.object.values[idx]);

    for (size_t i = idx; i < obj->value.object.count - 1; i++) {
        obj->value.object.keys[i] = obj->value.object.keys[i + 1];
        obj->value.object.values[i] = obj->value.object.values[i + 1];
    }
    obj->value.object.count--;
    return true;
}

bool z_json_array_append(ZJson *arr, ZJson *value) {
    if (!arr || arr->type != Z_JSON_ARRAY || !value) return false;

    if (arr->value.array.count >= arr->value.array.capacity) {
        size_t new_cap = arr->value.array.capacity == 0 ? 8 : arr->value.array.capacity * 2;
        ZJson **new_items = (ZJson **)realloc(arr->value.array.items, new_cap * sizeof(ZJson *));
        if (!new_items) return false;
        arr->value.array.items = new_items;
        arr->value.array.capacity = new_cap;
    }

    arr->value.array.items[arr->value.array.count++] = value;
    return true;
}

bool z_json_array_remove(ZJson *arr, size_t index) {
    if (!arr || arr->type != Z_JSON_ARRAY || index >= arr->value.array.count) return false;

    z_json_free(arr->value.array.items[index]);

    for (size_t i = index; i < arr->value.array.count - 1; i++) {
        arr->value.array.items[i] = arr->value.array.items[i + 1];
    }
    arr->value.array.count--;
    return true;
}

bool z_json_write_file(ZJson *json, const char *filename) {
    if (!json || !filename) return false;

    size_t len;
    char *str = z_json_stringify(json, &len);
    if (!str) return false;

    FILE *f = fopen(filename, "wb");
    if (!f) { free(str); return false; }

    size_t written = fwrite(str, 1, len, f);
    fclose(f);
    free(str);

    return written == len;
}

bool z_json_write_file_pretty(ZJson *json, const char *filename, int indent) {
    if (!json || !filename) return false;

    size_t len;
    char *str = z_json_stringify_pretty(json, &len, indent);
    if (!str) return false;

    FILE *f = fopen(filename, "wb");
    if (!f) { free(str); return false; }

    size_t written = fwrite(str, 1, len, f);
    fclose(f);
    free(str);

    return written == len;
}

void z_json_free(ZJson *json) {
    if (!json) return;

    switch (json->type) {
        case Z_JSON_STRING:
            free((void *)json->value.str_val);
            break;
        case Z_JSON_OBJECT:
            for (size_t i = 0; i < json->value.object.count; i++) {
                free((void *)json->value.object.keys[i]);
                z_json_free(json->value.object.values[i]);
            }
            free(json->value.object.keys);
            free(json->value.object.values);
            break;
        case Z_JSON_ARRAY:
            for (size_t i = 0; i < json->value.array.count; i++) {
                z_json_free(json->value.array.items[i]);
            }
            free(json->value.array.items);
            break;
        default: break;
    }
    free(json);
}

#endif /* Z_JSON_IMPLEMENTATION */
