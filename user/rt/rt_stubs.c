// user/rt/rt_stubs.c
//
// Weak stub definitions for agent entry points so that
// rt0_user.o can link even if the program does not define them.
//
// If your program defines _agent_main() or main(), those definitions
// will override these stubs automatically.

__attribute__((weak)) void _agent_main(void) {}
__attribute__((weak)) void main(void) {}
