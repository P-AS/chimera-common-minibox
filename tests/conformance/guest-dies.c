/* A guest that dies while it starts (DIE_IN 1: a static constructor, which
 * runs in _start) or while it is sealed (DIE_IN 2: co_clean, which the seal
 * runs): the host must refuse the machine with the reason and live on. Before
 * 2026-10-18 neither call had an escape record, and the death ended the host. */
#include <emulibc.h>
#include <stdint.h>

#if DIE_IN == 1
__attribute__((constructor)) static void dies_starting(void) { *(volatile uint32_t *)(uintptr_t)8 = 1; }
#endif
#if DIE_IN == 2
ECL_EXPORT void co_clean(void) { *(volatile uint32_t *)(uintptr_t)8 = 1; }
#endif

ECL_EXPORT int Init(void) { return 1; }
