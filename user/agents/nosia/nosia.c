#include "../../../nosm/drivers/IO/tty.h"

__attribute__((weak)) void tty_init(void) {}
__attribute__((weak)) void tty_clear(void) {}
__attribute__((weak)) void tty_write(const char *s) { (void)s; }

void nosia_main(void) {
    tty_init();
    tty_clear();
    tty_write("NOSIA: NitrOS Installer\n");
    tty_write("Installation routine not yet implemented.\n");
    for(;;) {}
}
