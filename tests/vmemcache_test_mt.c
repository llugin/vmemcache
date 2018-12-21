/*
 * Copyright 2018, Intel Corporation
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *     * Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *
 *     * Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in
 *       the documentation and/or other materials provided with the
 *       distribution.
 *
 *     * Neither the name of the copyright holder nor the names of its
 *       contributors may be used to endorse or promote products derived
 *       from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
 * LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
 * DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
 * THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 * vmemcache_test_mt.c -- multi-threaded unit test for libvmemcache
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "libvmemcache.h"
#include "vmemcache_tests.h"
#include "os_thread.h"

struct buffers {
	size_t size;
	char *buff;
};

struct context {
	VMEMcache *cache;
	struct buffers *buffs;
	unsigned nbuffs;
	unsigned write_count;
	unsigned thread_number;
	void *(*thread_routine)(void *);
};

static void *
worker_thread_put(void *arg)
{
	struct context *ctx = arg;
	unsigned long long i;
	unsigned long long shift = ctx->thread_number * ctx->write_count;

	for (i = shift; i < (shift + ctx->write_count); i++) {
		if (vmemcache_put(ctx->cache, (char *)&i, sizeof(i),
				ctx->buffs[i % ctx->nbuffs].buff,
				ctx->buffs[i % ctx->nbuffs].size))
			FATAL("ERROR: vmemcache_put: %s", vmemcache_errormsg());
	}

	return NULL;
}

static void
run_threads(unsigned n_threads, os_thread_t *threads, struct context *ctx)
{
	for (unsigned i = 0; i < n_threads; ++i)
		os_thread_create(&threads[i], NULL, ctx[i].thread_routine,
					&ctx[i]);

	for (unsigned i = 0; i < n_threads; ++i)
		os_thread_join(&threads[i], NULL);
}

static void
run_test_puts(unsigned n_threads, os_thread_t *threads, struct context *ctx)
{
	for (unsigned i = 0; i < n_threads; ++i) {
		ctx[i].thread_routine = worker_thread_put;
	}

	run_threads(n_threads, threads, ctx);
}

int
main(int argc, char *argv[])
{
	int ret = -1;

	if (argc < 2) {
		fprintf(stderr, "usage: %s dir-name", argv[0]);
		exit(-1);
	}

	const char *dir = argv[1];

	/* default values of parameters */
	unsigned n_threads = 10;
	unsigned write_count = 100000;
	unsigned nbuffs = 10;
	size_t min_size = 8;
	size_t max_size = 64;

	VMEMcache *cache = vmemcache_new(dir, VMEMCACHE_MIN_POOL,
				VMEMCACHE_MIN_FRAG, VMEMCACHE_REPLACEMENT_LRU);
	if (cache == NULL)
		FATAL("vmemcache_new: %s (%s)", vmemcache_errormsg(), dir);

	struct buffers *buffs = calloc(nbuffs, sizeof(*buffs));
	if (buffs == NULL) {
		FATAL("out of memory");
		goto exit_delete;
	}

	srand((unsigned)time(NULL));

	for (unsigned i = 0; i < nbuffs; ++i) {
		/* generate N random sizes (between A – B bytes) */
		buffs[i].size = min_size +
				(size_t)rand() % (max_size - min_size + 1);

		/* allocate a buffer and fill it for every generated size */
		buffs[i].buff = malloc(buffs[i].size);
		if (buffs[i].buff == NULL) {
			FATAL("out of memory");
			goto exit_free_buffs;
		}

		memset(buffs[i].buff, 0xCC, buffs[i].size);
	}

	os_thread_t *threads = calloc(n_threads, sizeof(*threads));
	if (threads == NULL) {
		FATAL("out of memory");
		goto exit_free_ctx;
	}

	struct context *ctx = calloc(n_threads, sizeof(*ctx));
	if (ctx == NULL) {
		FATAL("out of memory");
		goto exit_free_buffs;
	}

	for (unsigned i = 0; i < n_threads; ++i) {
		ctx[i].cache = cache;
		ctx[i].buffs = buffs;
		ctx[i].nbuffs = nbuffs;
		ctx[i].write_count = write_count / n_threads;
		ctx[i].thread_number = i;
	}

	run_test_puts(n_threads, threads, ctx);

	ret = 0;

	free(threads);

exit_free_ctx:
	free(ctx);

exit_free_buffs:
	for (unsigned i = 0; i < nbuffs; ++i)
		free(buffs[i].buff);
	free(buffs);

exit_delete:
	/* free all the memory */
	while (vmemcache_evict(cache, NULL, 0) == 0)
		;

	vmemcache_delete(cache);

	return ret;
}
