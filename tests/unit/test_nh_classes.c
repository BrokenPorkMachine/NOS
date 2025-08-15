#include <assert.h>
#include <stdio.h>
#include "../../kernel/VM/nitroheap/classes.h"

int main(void) {
    // Small size with minimal alignment
    int cls = nh_class_from_size(1, 1);
    assert(cls >= 0);
    assert(nh_size_classes[cls].size == 8);
    assert(nh_size_classes[cls].align == 8);

    // Alignment larger than size class alignment should move to next class
    int cls_aligned = nh_class_from_size(30, 64);
    assert(cls_aligned >= 0);
    assert(nh_size_classes[cls_aligned].size == 64);
    assert(nh_size_classes[cls_aligned].align == 64);

    // Larger request near upper bound
    int cls_large = nh_class_from_size(5000, 16);
    assert(cls_large >= 0);
    assert(nh_size_classes[cls_large].size == 5120);
    assert(nh_size_classes[cls_large].align == 4096);

    // Request exceeding all classes should return -1
    int cls_huge = nh_class_from_size(90000, 32);
    assert(cls_huge >= 0);
    assert(nh_size_classes[cls_huge].size == 98304);
    assert(nh_size_classes[cls_huge].align == 65536);

    // Request exceeding all classes should return -1
    assert(nh_class_from_size(200000, 8) == -1);

    // class_align utility
    assert(nh_class_align(cls) == nh_size_classes[cls].align);
    assert(nh_class_align(-1) == 0);
    assert(nh_class_align(nh_size_class_count) == 0);

    printf("nh_size_class tests passed\n");
    return 0;
}

