#pragma once
#include <stdint.h>
#include "nosm.h"

/* Every module builds as a freestanding PIE blob with a Mach-O2 manifest
 * and exports: `nosm_module_ops` (const nosm_module_ops_t)
 * Init gets (mod_id, caps). It must return 0 on success.
 */

