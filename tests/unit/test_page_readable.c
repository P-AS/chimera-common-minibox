/* mb_page_readable: the fault report's stack readers depend on it.
 *
 * A guard page must read false, or the death backtrace faults inside the
 * fault handler and loses the report it is part of. Readable memory (stack,
 * code) must read true, or reports go silent. Unmapped addresses read false.
 */
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#ifndef _WIN32
#include <sys/mman.h>
#endif
#include "minibox_internal.h"

int main(void) {
	uintptr_t stack_var = 0;
	assert(mb_page_readable((uintptr_t)&stack_var));
	assert(mb_page_readable((uintptr_t)&main));
#ifndef _WIN32
	const size_t ps = 4096;
	void *p = mmap(NULL, ps, PROT_READ | PROT_WRITE,
	               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
	assert(p != MAP_FAILED);
	*(volatile char *)p = 1;
	assert(mb_page_readable((uintptr_t)p));
	assert(mprotect(p, ps, PROT_NONE) == 0);
	assert(!mb_page_readable((uintptr_t)p));
	assert(!mb_page_readable((uintptr_t)0x1000));   /* unmapped low page */
	munmap(p, ps);
#endif
	printf("page_readable OK\n");
	return 0;
}
