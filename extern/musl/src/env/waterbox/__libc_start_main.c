#include <elf.h>
#include <poll.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include "syscall.h"
#include "atomic.h"
#include "libc.h"
#include "pthread_impl.h"

static void dummy(void) {}
weak_alias(dummy, _init);

extern weak hidden void (*const __init_array_start)(void), (*const __init_array_end)(void);

static void dummy1(void *p) {}
weak_alias(dummy1, __init_ssp);

#define AUX_CNT 38

#ifdef __GNUC__
__attribute__((__noinline__))
#endif
void __init_libc(char **envp, char *pn)
{
	size_t i, *auxv, aux[AUX_CNT] = { 0 };
	__environ = envp;
	for (i=0; envp[i]; i++);
	libc.auxv = auxv = (void *)(envp+i+1);
	for (i=0; auxv[i]; i+=2) if (auxv[i]<AUX_CNT) aux[auxv[i]] = auxv[i+1];
	__hwcap = aux[AT_HWCAP];
	if (aux[AT_SYSINFO]) __sysinfo = aux[AT_SYSINFO];
	libc.page_size = aux[AT_PAGESZ];

	if (!pn) pn = (void*)aux[AT_EXECFN];
	if (!pn) pn = "";
	__progname = __progname_full = pn;
	for (i=0; pn[i]; i++) if (pn[i]=='/') __progname = pn+i+1;

	__init_tls(aux);
	__init_ssp((void *)aux[AT_RANDOM]);

	if (aux[AT_UID]==aux[AT_EUID] && aux[AT_GID]==aux[AT_EGID]
		&& !aux[AT_SECURE]) return;

	struct pollfd pfd[3] = { {.fd=0}, {.fd=1}, {.fd=2} };
	int r =
#ifdef SYS_poll
	__syscall(SYS_poll, pfd, 3, 0);
#else
	__syscall(SYS_ppoll, pfd, 3, &(struct timespec){0}, 0, _NSIG/8);
#endif
	if (r<0) a_crash();
	for (i=0; i<3; i++) if (pfd[i].revents&POLLNVAL)
		if (__sys_open("/dev/null", O_RDWR)<0)
			a_crash();
	libc.secure = 1;
}

static void libc_start_init(void)
{
	_init();
	uintptr_t a = (uintptr_t)&__init_array_start;
	for (; a<(uintptr_t)&__init_array_end; a+=sizeof(void(*)()))
		(*(void (**)(void))a)();
}

weak_alias(libc_start_init, __libc_start_init);

/* The program's thread-local storage, for __init_tls. The waterbox loader
 * builds no auxiliary vector, and without one musl found no PT_TLS and sized
 * every thread's TLS block for no thread locals at all: C++ thread_local and
 * Rust's %fs-relative locals then sat below the block - on the top of each new
 * thread's stack, and for the main thread on the static data before
 * builtin_tls - and read back what those wrote. The ELF headers are mapped
 * with the first segment, so __ehdr_start finds them. Only the PT_TLS entry is
 * handed on: nothing else musl reads from program headers (PT_GNU_STACK sets
 * the default thread stack size) changes. */
extern weak hidden const unsigned char __ehdr_start[];
static Elf64_Phdr __tls_phdr;
static size_t __args[] =
{
	0, 0,	/* argv: "waterbox", NULL */
	0, 0,	/* envp: "WATERBOX=1", NULL */
	AT_PHDR, 0, AT_PHNUM, 0, AT_PHENT, sizeof(Elf64_Phdr),
	AT_NULL, 0,
};

void __libc_start_main(void)
{
	__args[0] = (size_t)"waterbox";
	__args[2] = (size_t)"WATERBOX=1";
	if (__ehdr_start) {
		const Elf64_Ehdr *eh = (const Elf64_Ehdr *)__ehdr_start;
		const unsigned char *ph = __ehdr_start + eh->e_phoff;
		for (size_t i = 0; i < eh->e_phnum; i++, ph += eh->e_phentsize) {
			if (((const Elf64_Phdr *)ph)->p_type == PT_TLS) {
				__tls_phdr = *(const Elf64_Phdr *)ph;
				__args[5] = (size_t)&__tls_phdr;
				__args[7] = 1;
			}
		}
	}
	char **argv = (char**)__args;
	int argc = 1;
	char **envp = argv+argc+1;
	__init_libc(envp, argv[0]);
	__libc_start_init();
}
