/* A guest that exercises the green-thread path: it spawns N pthreads that each
 * increment a shared counter under a mutex a fixed number of times, joins them,
 * and reports the total. Correct cooperative scheduling + futex-backed
 * pthread_mutex + join must yield exactly N*ITERS. */
#include <emulibc.h>
#include <pthread.h>
#include <stdint.h>
#include <stdio.h>

#define NTHREADS 4
#define ITERS 1000

static pthread_mutex_t g_lock = PTHREAD_MUTEX_INITIALIZER;
static volatile uint64_t g_counter;

static void *worker(void *arg) {
	(void)arg;
	for (int i = 0; i < ITERS; i++) {
		pthread_mutex_lock(&g_lock);
		g_counter++;
		pthread_mutex_unlock(&g_lock);
	}
	return 0;
}

ECL_EXPORT uint64_t RunThreads(void) {
	pthread_t th[NTHREADS];
	g_counter = 0;
	for (int i = 0; i < NTHREADS; i++)
		if (pthread_create(&th[i], 0, worker, 0) != 0) { fprintf(stderr, "pthread_create failed\n"); return 0; }
	for (int i = 0; i < NTHREADS; i++)
		pthread_join(th[i], 0);
	return g_counter;   /* must equal NTHREADS*ITERS */
}

/* a producer/consumer round using a condition variable, exercising futex
 * wait/wake and requeue via pthread_cond */
static pthread_mutex_t g_cvlock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_cv = PTHREAD_COND_INITIALIZER;
static volatile int g_ready;
static volatile uint64_t g_produced;

static void *consumer(void *arg) {
	(void)arg;
	pthread_mutex_lock(&g_cvlock);
	while (!g_ready) pthread_cond_wait(&g_cv, &g_cvlock);
	uint64_t v = g_produced;
	pthread_mutex_unlock(&g_cvlock);
	return (void *)(uintptr_t)v;
}

ECL_EXPORT uint64_t RunCondvar(uint64_t value) {
	pthread_t c;
	g_ready = 0; g_produced = 0;
	pthread_create(&c, 0, consumer, 0);
	pthread_mutex_lock(&g_cvlock);
	g_produced = value * 2 + 1;
	g_ready = 1;
	pthread_cond_signal(&g_cv);
	pthread_mutex_unlock(&g_cvlock);
	void *r = 0;
	pthread_join(c, &r);
	return (uint64_t)(uintptr_t)r;   /* must equal value*2+1 */
}

ECL_EXPORT int Init(void) { return 1; }
int main(void) { return 0; }

/* Thread-local storage, on a new thread and on the main one: a zeroed local
 * reads zero, an initialised one reads its initialiser, and neither shares
 * memory with the thread's stack. Until musl was handed the program's PT_TLS
 * they sat below the TLS block it reserved - on the top of a new thread's
 * stack, on static data for the main thread. */
static _Thread_local uint32_t t_zero[64];
static _Thread_local uint32_t t_init = 0x5eed1234u;

static __attribute__((noinline)) uint32_t tls_use_stack(int depth) {
	volatile uint8_t pad[512];
	for (int i = 0; i < 512; i++) pad[i] = (uint8_t)(0xa5 ^ depth);
	return depth ? tls_use_stack(depth - 1) + pad[7] : pad[3];
}

static uint64_t tls_check(int fresh) {
	uint64_t bad = 0;
	for (int i = 0; fresh && i < 64; i++) if (t_zero[i] != 0) bad |= 1;
	if (t_init != 0x5eed1234u) bad |= 2;
	for (int i = 0; i < 64; i++) t_zero[i] = 0x11111111u * (uint32_t)(i & 15);
	tls_use_stack(8);
	for (int i = 0; i < 64; i++) if (t_zero[i] != 0x11111111u * (uint32_t)(i & 15)) bad |= 4;
	return bad;
}

static void *tls_worker(void *arg) {
	(void)arg;
	return (void *)(uintptr_t)tls_check(1);
}

/* 0 when right; bits 0..2 the main thread's faults, 8..10 a new thread's */
ECL_EXPORT uint64_t RunTls(void) {
	static int main_seen;
	const uint64_t on_main = tls_check(!main_seen);
	main_seen = 1;
	pthread_t t;
	void *on_new = 0;
	if (pthread_create(&t, 0, tls_worker, 0) != 0) return 0xffff;
	pthread_join(t, &on_new);
	return on_main | (uint64_t)(uintptr_t)on_new << 8;
}
