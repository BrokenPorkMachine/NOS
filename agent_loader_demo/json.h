#ifndef MINIMAL_JSON_H
#define MINIMAL_JSON_H

#include <stddef.h>

/* Minimal JSON helper capable of extracting string and integer values
 * from flat key/value objects. */

int json_get_string(const char *json, const char *key,
                    char *out, size_t out_size);
int json_get_int(const char *json, const char *key, int *out_value);

#endif /* MINIMAL_JSON_H */
