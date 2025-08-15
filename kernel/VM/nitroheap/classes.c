#include "classes.h"

const nh_size_class_t nh_size_classes[] = {
    {8,8}, {16,16}, {24,16}, {32,16}, {40,16}, {48,16}, {56,16}, {64,64},
    {80,64}, {96,64}, {112,64}, {128,128},
    {160,128}, {192,128}, {224,128}, {256,256},
    {320,256}, {384,256}, {448,256}, {512,512},
    {640,512}, {768,512}, {896,512}, {1024,1024},
    {1280,1024}, {1536,1024}, {1792,1024}, {2048,2048},
    {2560,2048}, {3072,2048}, {3584,2048}, {4096,4096},
    {5120,4096}, {6144,4096}, {7168,4096}, {8192,8192},
    {12288,8192}, {16384,16384}, {24576,16384}, {32768,32768},
    {49152,32768}, {65536,65536}, {98304,65536}, {131072,131072}
};

const size_t nh_size_class_count = sizeof(nh_size_classes)/sizeof(nh_size_classes[0]);

int nh_class_from_size(size_t sz, size_t align) {
    if (align == 0) align = 1;

    // Binary search for the first size class whose size >= sz
    size_t lo = 0, hi = nh_size_class_count;
    while (lo < hi) {
        size_t mid = (lo + hi) / 2;
        if (nh_size_classes[mid].size < sz)
            lo = mid + 1;
        else
            hi = mid;
    }

    // Walk forward until the alignment requirement is also satisfied
    for (size_t i = lo; i < nh_size_class_count; ++i) {
        if (nh_size_classes[i].align >= align)
            return (int)i;
    }

    return -1;
}

size_t nh_class_align(int cls) {
    if (cls < 0 || (size_t)cls >= nh_size_class_count)
        return 0;
    return nh_size_classes[cls].align;
}
