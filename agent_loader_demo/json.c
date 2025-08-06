#include "json.h"
#include <ctype.h>
#include <stdio.h>
#include <string.h>

static const char *skip_ws(const char *p) {
    while (*p && isspace((unsigned char)*p)) p++;
    return p;
}

int json_get_string(const char *json, const char *key,
                    char *out, size_t out_size) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p) return -1;
    p += strlen(pattern);
    p = strchr(p, ':');
    if (!p) return -1;
    p++;
    p = skip_ws(p);
    if (*p != '"') return -1;
    p++;
    const char *end = strchr(p, '"');
    if (!end) return -1;
    size_t len = (size_t)(end - p);
    if (len + 1 > out_size) return -1;
    memcpy(out, p, len);
    out[len] = '\0';
    return 0;
}

int json_get_int(const char *json, const char *key, int *out_value) {
    char pattern[64];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char *p = strstr(json, pattern);
    if (!p) return -1;
    p += strlen(pattern);
    p = strchr(p, ':');
    if (!p) return -1;
    p++;
    p = skip_ws(p);
    int sign = 1;
    if (*p == '-') { sign = -1; p++; }
    int val = 0;
    if (!isdigit((unsigned char)*p)) return -1;
    while (isdigit((unsigned char)*p)) {
        val = val * 10 + (*p - '0');
        p++;
    }
    *out_value = val * sign;
    return 0;
}
