#pragma once
#include <stddef.h>

typedef struct {
    size_t size;
    size_t align;
} nh_size_class_t;

extern const nh_size_class_t nh_size_classes[];
extern const size_t nh_size_class_count;

int nh_class_from_size(size_t sz, size_t align);
size_t nh_class_align(int cls);
