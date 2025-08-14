#include "nosm_module.h"
#include "printf.h"

static uint32_t g_mod_id;
static uint64_t g_caps;

__attribute__((used)) void _start(void) {}

static int hello_init(const nosm_env_t *env) {
    g_mod_id = env->mod_id;
    g_caps   = env->caps;
    if ( (g_caps & NOSM_CAP_IOPORT) == 0 ) {
        printf("[hello.nmod] missing IOPORT cap\n");
        return -1;
    }
    printf("[hello.nmod] init ok (mod=%u caps=0x%llx)\n",
           g_mod_id, (unsigned long long)g_caps);
    /* do hardware discovery guarded by caps, request IRQs, etc */
    return 0;
}
static void hello_fini(void) {
    printf("[hello.nmod] fini\n");
}
static void hello_suspend(void) { /* quiesce hardware */ }
static void hello_resume(void)  { /* resume */ }

__attribute__((used, section("\"__O2INFO,__manifest\"")))
static const char manifest[] =
"{\n"
"  \"name\":\"hello.nmod\",\n"
"  \"type\":\"nmod\",\n"
"  \"version\":\"1.0\",\n"
"  \"capabilities\":[\"IOPORT\"],\n"
"  \"system\":false,\n"
"  \"signature\":\"TEST_ONLY_ACCEPTED\"\n"
"}\n";

const nosm_module_ops_t nosm_module_ops = {
    .init    = hello_init,
    .fini    = hello_fini,
    .suspend = hello_suspend,
    .resume  = hello_resume,
};

