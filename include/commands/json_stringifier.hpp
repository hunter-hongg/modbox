#ifndef JSON_STRINGIFIER_HPP
#define JSON_STRINGIFIER_HPP

#include <cstdio>
#include <cstdint>

static inline void json_escape_string(FILE* out, const char* s) {
    (void)fputc('"', out);
    for (const char* p = s; *p; ++p) {
        switch (*p) {
            case '"':  (void)fputs("\\\"", out); break;
            case '\\': (void)fputs("\\\\", out); break;
            case '\b': (void)fputs("\\b", out); break;
            case '\f': (void)fputs("\\f", out); break;
            case '\n': (void)fputs("\\n", out); break;
            case '\r': (void)fputs("\\r", out); break;
            case '\t': (void)fputs("\\t", out); break;
            default:
                if (static_cast<unsigned char>(*p) < 0x20) {
                    (void)fprintf(out, "\\u%04x", static_cast<unsigned char>(*p));
                } else {
                    (void)fputc(*p, out);
                }
                break;
        }
    }
    (void)fputc('"', out);
}

static inline void json_emit_int(FILE* out, const char* key, int val, bool last) {
    if (key) (void)fprintf(out, "%s\"%s\": ", last ? "" : ", ", key);
    (void)fprintf(out, "%d", val);
}

static inline void json_emit_uint64(FILE* out, const char* key, uint64_t val, bool last) {
    if (key) (void)fprintf(out, "%s\"%s\": ", last ? "" : ", ", key);
    (void)fprintf(out, "%llu", (unsigned long long)val);
}

static inline void json_emit_long(FILE* out, const char* key, long val, bool last) {
    if (key) (void)fprintf(out, "%s\"%s\": ", last ? "" : ", ", key);
    (void)fprintf(out, "%ld", val);
}

static inline void json_emit_bool(FILE* out, const char* key, bool val, bool last) {
    if (key) (void)fprintf(out, "%s\"%s\": ", last ? "" : ", ", key);
    (void)fprintf(out, "%s", val ? "true" : "false");
}

static inline void json_emit_null(FILE* out, const char* key, bool last) {
    if (key) (void)fprintf(out, "%s\"%s\": ", last ? "" : ", ", key);
    (void)fprintf(out, "null");
}

static inline void json_emit_str(FILE* out, const char* key, const char* val, bool last) {
    if (key) (void)fprintf(out, "%s\"%s\": ", last ? "" : ", ", key);
    json_escape_string(out, val);
}

static inline void json_emit_str_optional(FILE* out, const char* key, const char* val, bool has_val, bool last) {
    if (!has_val) return;
    (void)fprintf(out, "%s\"%s\": ", last ? "" : ", ", key);
    json_escape_string(out, val);
}

#endif /* JSON_STRINGIFIER_HPP */
