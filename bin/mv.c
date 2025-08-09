#include "../user/libc/libc.h"

// Rename "src" to "dst" using project libc only.
int main(void) {
    return rename("src", "dst");
}

