/* Drives the pthreaded guest through the host: proves green threads, futex-
 * backed mutexes/condvars, join, and a savestate taken with threads present. */
#include "minibox.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct { FILE *f; } fr_t;
static intptr_t file_read(uintptr_t ud, uint8_t *d, uintptr_t s){return (intptr_t)fread(d,1,s,((fr_t*)ud)->f);}
typedef struct { uint8_t *b; size_t len, cap, pos; } mb_t;
static int32_t mem_write(uintptr_t ud,const uint8_t*d,uintptr_t n){mb_t*m=(mb_t*)ud;if(m->len+n>m->cap){m->cap=(m->len+n)*2+64;m->b=realloc(m->b,m->cap);}memcpy(m->b+m->len,d,n);m->len+=n;return 0;}
static intptr_t mem_read(uintptr_t ud,uint8_t*d,uintptr_t n){mb_t*m=(mb_t*)ud;uintptr_t a=m->len-m->pos;if(n>a)n=a;memcpy(d,m->b+m->pos,n);m->pos+=n;return (intptr_t)n;}

static int fails = 0;
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "  FAIL %s:%d %s\n", __FILE__, __LINE__, #c); fails++; } } while (0)

typedef int (*init_fn)(void);
typedef uint64_t (*run_fn)(void);
typedef uint64_t (*run1_fn)(uint64_t);
static uintptr_t proc(mb_host *h, const char *n){mb_return r;wbx_get_proc_addr(h,n,&r);if(r.error_message[0]){fprintf(stderr,"proc %s:%s\n",n,r.error_message);exit(2);}return r.data;}

int main(int argc, char **argv) {
	const char *path = argc > 1 ? argv[1] : "guest_threads.wbx";
	FILE *f = fopen(path, "rb");
	if (!f) { fprintf(stderr, "cannot open %s\n", path); return 1; }
	mb_memory_layout_template layout = { 64u<<20, 16u<<20, 16u<<20, 16u<<20, 64u<<20 };
	fr_t fr = { f };
	mb_return r;
	wbx_create_host(&layout, "guest_threads.wbx", file_read, (uintptr_t)&fr, &r);
	fclose(f);
	if (r.error_message[0]) { fprintf(stderr, "create: %s\n", r.error_message); return 1; }
	mb_host *h = (mb_host *)r.data;

	wbx_activate_host(h, &r);
	CHECK(((init_fn)proc(h, "Init"))() == 1);
	run_fn RunThreads = (run_fn)proc(h, "RunThreads");
	run1_fn RunCondvar = (run1_fn)proc(h, "RunCondvar");
	run_fn RunTls = (run_fn)proc(h, "RunTls");
	wbx_deactivate_host(h, &r); wbx_seal(h, &r);
	if (r.error_message[0]) { fprintf(stderr, "seal: %s\n", r.error_message); return 1; }
	wbx_activate_host(h, &r);

	/* 4 threads x 1000 increments under a futex mutex -> exactly 4000 */
	uint64_t total = RunThreads();
	printf("RunThreads -> %llu (expect 4000)\n", (unsigned long long)total);
	CHECK(total == 4000);

	/* condvar producer/consumer -> value*2+1 */
	uint64_t cv = RunCondvar(21);
	printf("RunCondvar(21) -> %llu (expect 43)\n", (unsigned long long)cv);
	CHECK(cv == 43);
	{
		const uint64_t tls = RunTls();
		printf("RunTls -> %#llx (expect 0: thread locals zeroed, initialised, apart from the stack)\n", (unsigned long long)tls);
		CHECK(tls == 0);
	}

	/* determinism: identical results on repeat */
	CHECK(RunThreads() == 4000);
	CHECK(RunCondvar(100) == 201);

	/* savestate round-trip between thread runs (threads quiescent at tid 1) */
	mb_t st = {0};
	wbx_deactivate_host(h, &r); wbx_save_state(h, mem_write, (uintptr_t)&st, &r); wbx_activate_host(h, &r);
	CHECK(!r.error_message[0]);
	uint64_t after_save = RunThreads();
	st.pos = 0;
	wbx_deactivate_host(h, &r); wbx_load_state(h, mem_read, (uintptr_t)&st, &r); wbx_activate_host(h, &r);
	CHECK(!r.error_message[0]);
	CHECK(RunThreads() == after_save);

	wbx_deactivate_host(h, &r); wbx_destroy_host(h, &r);
	free(st.b);
	if (fails == 0) printf("run_threads: all checks passed\n");
	else printf("run_threads: %d checks FAILED\n", fails);
	return fails ? 1 : 0;
}
