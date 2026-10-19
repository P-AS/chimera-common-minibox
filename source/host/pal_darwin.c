/* Platform abstraction layer, macOS. pal_linux.c's interface, kept on Mach.
 *
 * Two things are different enough to need a file of their own.
 *
 * A block is ONE set of pages seen twice - at the guest's fixed address,
 * protection-managed, and at the host's always-writable mirror. Linux makes
 * that a memfd mapped twice. macOS will not run code from a view of shared
 * memory that has ever been writable anywhere (mprotect to RX answers EACCES,
 * measured on macOS 27), and guest code is exactly that. Anonymous memory
 * aliased with mach_vm_remap (copy = FALSE) has no such rule: both views see
 * every write and either may be RX while the other is RW. So the "handle" is
 * a first view nobody touches, which owns the pages, and every view mapped
 * from it is a remap of it.
 *
 * And nothing is ever writable and executable at once: mmap and mprotect
 * refuse PROT_WRITE|PROT_EXEC outside MAP_JIT memory, and MAP_JIT memory
 * cannot be placed at a fixed address or remapped. Code the host writes at run
 * time (the thunks) is written through a second, writable view
 * (mb_pal_map_code); guest pages the guest asks to be RWX are memblock's
 * business (see mb_page_native_prot). */
#if defined(__APPLE__)
#include "minibox_internal.h"
#include <errno.h>
#include <sys/mman.h>
#include <mach/mach.h>
#include <mach/mach_vm.h>

static int prot_to_native(mb_prot prot) {
	switch (prot) {
		case MB_PROT_NONE:    return PROT_NONE;
		case MB_PROT_R:       return PROT_READ;
		case MB_PROT_RW:      return PROT_READ | PROT_WRITE;
		case MB_PROT_RX:      return PROT_READ | PROT_EXEC;
		case MB_PROT_RWX:     return PROT_READ | PROT_WRITE | PROT_EXEC;   /* refused by the OS */
		case MB_PROT_RWSTACK: return PROT_READ | PROT_WRITE;
	}
	return PROT_NONE;
}

/* A view of [src, src + size) at `at` (0: anywhere), sharing its pages. A
 * fixed address that is not free fails, as MAP_FIXED_NOREPLACE does. */
static int remap(uintptr_t src, uintptr_t size, uintptr_t at, uintptr_t *out) {
	mach_vm_address_t dst = at;
	vm_prot_t cur, max;
	const int flags = at != 0 ? VM_FLAGS_FIXED : VM_FLAGS_ANYWHERE;
	if (mach_vm_remap(mach_task_self(), &dst, size, 0, flags, mach_task_self(),
	                  src, FALSE, &cur, &max, VM_INHERIT_NONE) != KERN_SUCCESS)
		return -1;
	*out = (uintptr_t)dst;
	return 0;
}

int mb_pal_open_handle(uintptr_t size, mb_handle *out) {
	void *p = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANON, -1, 0);
	if (p == MAP_FAILED) return -1;
	out->h = (uintptr_t)p;
	out->size = size;
	out->lazy = false;
	return 0;
}

void mb_pal_close_handle(mb_handle h) { if (h.h) munmap((void *)h.h, h.size); }

int mb_pal_map_handle(mb_handle h, mb_range in, mb_range *out) {
	uintptr_t at;
	if (remap(h.h, in.size, in.start, &at) != 0) return -1;
	if (mprotect((void *)at, in.size, PROT_NONE) != 0) { munmap((void *)at, in.size); return -1; }
	out->start = at;
	out->size = in.size;
	return 0;
}

void mb_pal_unmap_handle(mb_range addr) { munmap((void *)addr.start, addr.size); }

int mb_pal_map_anon(mb_range in, mb_prot prot, mb_range *out) {
	/* a fixed address is a hint mmap may decline; declined is failed, as
	 * Linux's MAP_FIXED_NOREPLACE fails */
	void *p = mmap((void *)in.start, in.size, prot_to_native(prot), MAP_PRIVATE | MAP_ANON, -1, 0);
	if (p == MAP_FAILED) return -1;
	if (in.start != 0 && (uintptr_t)p != in.start) { munmap(p, in.size); errno = EEXIST; return -1; }
	out->start = (uintptr_t)p;
	out->size = in.size;
	return 0;
}

void mb_pal_unmap_anon(mb_range addr) { munmap((void *)addr.start, addr.size); }

int mb_pal_protect(mb_range addr, mb_prot prot) {
	return mprotect((void *)addr.start, addr.size, prot_to_native(prot));
}

int mb_pal_commit(mb_range addr, mb_prot prot) {
	return mprotect((void *)addr.start, addr.size, prot_to_native(prot));
}

int mb_pal_map_code(uintptr_t size, mb_range *exec, mb_range *write) {
	mb_range in = { 0, size };
	if (mb_pal_map_anon(in, MB_PROT_RW, write) != 0) return -1;
	uintptr_t at;
	if (remap(write->start, size, 0, &at) != 0) { mb_pal_unmap_anon(*write); return -1; }
	if (mprotect((void *)at, size, PROT_READ | PROT_EXEC) != 0) {
		munmap((void *)at, size);
		mb_pal_unmap_anon(*write);
		return -1;
	}
	exec->start = at;
	exec->size = size;
	return 0;
}

void mb_pal_unmap_code(mb_range exec, mb_range write) {
	munmap((void *)exec.start, exec.size);
	munmap((void *)write.start, write.size);
}

#endif
