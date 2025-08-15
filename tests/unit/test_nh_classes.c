#include <assert.h>
#include <stdio.h>
#include "../../kernel/VM/nitroheap/classes.h"

int main(void) {
    // Small size with minimal alignment
    int cls = size_class_for(1, 1);
    assert(cls >= 0);
    assert(nh_size_classes[cls].size == 8);
    assert(nh_size_classes[cls].align == 8);

    // Alignment larger than size class alignment should move to next class
    int cls_aligned = size_class_for(30, 64);
    assert(cls_aligned >= 0);
    assert(nh_size_classes[cls_aligned].size == 64);
    assert(nh_size_classes[cls_aligned].align == 64);

    // Larger request near upper bound
    int cls_large = size_class_for(5000, 16);
    assert(cls_large >= 0);
    assert(nh_size_classes[cls_large].size == 5120);
    assert(nh_size_classes[cls_large].align == 4096);

    // Request exceeding all classes should return -1
    assert(size_class_for(20000, 8) == -1);

    // class_align utility
    assert(class_align(cls) == nh_size_classes[cls].align);
    assert(class_align(-1) == 0);
    assert(class_align(nh_size_class_count) == 0);

    printf("nh_size_class tests passed\n");
    return 0;
}

