/*
 * Copyright (c) 2015, Peter Xu <xzpeter@gmail.com>
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * * Redistributions of source code must retain the above copyright notice, this
 * list of conditions and the following disclaimer.
 *
 * * Redistributions in binary form must reproduce the above copyright notice,
 * this list of conditions and the following disclaimer in the documentation
 * and/or other materials provided with the distribution.
 *
 * * Neither the name of QuarkIM nor the names of its
 * contributors may be used to endorse or promote products derived from
 * this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
 * OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#include <pthread.h>
#include <stdio.h>
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include "q_log.h"
#include "q_mem.h"

static void *q_malloc(size_t size)
{
	return calloc(1, size);
}

static void *q_free(void *ptr)
{
	if (ptr)
		free(ptr);
}

static void q_mem_pool_header_init(q_mem_pool_t *pool, const char *name,
				   size_t usize, size_t count)
{
	assert(pool);
	strncpy(pool->mp_name, name, Q_MEM_POOL_NAME_LEN - 1);
	pool->mp_name[Q_MEM_POOL_NAME_LEN-1] = 0x00;
	pool->mp_buf_size = usize;
	pool->mp_total = count;
	pool->mp_free = count;
	INIT_LIST_HEAD(&pool->mp_free_bufs);
	INIT_LIST_HEAD(&pool->mp_used_bufs);
	pthread_mutex_init(&pool->mp_lock, NULL);
	pthread_cond_init(&pool->mp_cond, NULL);
}

q_mem_pool_t *q_mem_pool_create(const char *name, size_t buf_size,
				size_t count)
{
	int i = 0;
	q_mem_pool_t *pool = NULL;
	q_mem_buf_t *mbuf = NULL;
	size_t total_size = 0;
	size_t unit_size = 0;

	q_log(Q_LOG_DEBUG, "creating mem pool '%s'", name);

	if (buf_size > Q_MEM_POOL_MAX_BUF_SIZE) {
		q_log(Q_LOG_ERROR, "buf size overflow (%lu > %lu)",
		      buf_size, Q_MEM_POOL_MAX_BUF_SIZE);
		return NULL;
	}

	if (buf_size % Q_MEM_ALIGN) {
		q_log(Q_LOG_ERROR, "buf size %lu not aligned %lu",
		      buf_size, Q_MEM_ALIGN);
		return NULL;
	}

	if (count <= 0 || count > Q_MEM_POOL_MAX_BUF_COUNT) {
		q_log(Q_LOG_ERROR, "buf count overflow (%lu > %lu)",
		      count, Q_MEM_POOL_MAX_BUF_COUNT);
		return NULL;
	}

	unit_size = buf_size + sizeof(q_mem_buf_t);
	total_size = unit_size * count + sizeof(q_mem_pool_t);

	q_log(Q_LOG_DEBUG, "allocating space %lu for pool '%s'",
	      total_size, name);
	
	pool = q_malloc(total_size);
	if (!pool) {
		q_log(Q_LOG_ERROR, "failed alloc space for pool '%s'",
		      name);
		return NULL;
	}
	
	q_mem_pool_header_init(pool, name, buf_size, count);

	mbuf = (q_mem_buf_t *) (pool + 1);

	for (i = 0; i < count; i++) {
		mbuf->m_pool = pool;
		INIT_LIST_HEAD(&mbuf->m_pool_link);
		list_add(&mbuf->m_pool_link, &pool->mp_free_bufs);
		mbuf->m_barrier = 0xdeadbeef;
		mbuf = (q_mem_buf_t *) ((char *)mbuf + unit_size);
	}
	assert((char *)mbuf - total_size == (char *)pool);

	return pool;
}

q_mem_buf_t *q_mem_get(q_mem_pool_t *pool)
{
	q_mem_buf_t *buf = NULL;

	assert(pool);
	pthread_mutex_lock(&pool->mp_lock);

	while (pool->mp_free == 0) {
		/* there is no buffer left... just wait.. */
		q_log(Q_LOG_WARN, "no buf for pool '%s', will wait...",
		      pool->mp_name);
		pthread_cond_wait(&pool->mp_cond, &pool->mp_lock);
	}

	assert(!list_empty(&pool->mp_free_bufs));

	/* take the head buf from active list */
	buf = list_first_entry(&pool->mp_free_bufs, q_mem_buf_t,
			       m_pool_link);
	/* moving to used buf list */
	list_move(&buf->m_pool_link, &pool->mp_used_bufs);
	pool->mp_free--;

	q_log(Q_LOG_DEBUG, "get one buf for pool '%s', %lu left",
	      pool->mp_name, pool->mp_free);

	pthread_mutex_unlock(&pool->mp_lock);

	return buf;
}

void q_mem_put(q_mem_buf_t *buf)
{
	q_mem_pool_t *pool = NULL;
	assert(buf);
	pool = buf->m_pool;

	pthread_mutex_lock(&pool->mp_lock);

	/* do memory clean up */
	memset(Q_MEM_BUF_DATA(buf), 0x00, pool->mp_buf_size);
	/* move to free list head (rather than tail, to keep cache warm) */
	list_move(&buf->m_pool_link, &pool->mp_free_bufs);
	pool->mp_free++;

	if (pool->mp_free == 1) {
		q_log(Q_LOG_WARN, "pool '%s' not empty now, notify others",
		      pool->mp_name);
		pthread_cond_signal(&pool->mp_cond);
	}

	q_log(Q_LOG_DEBUG, "released one buf for pool '%s', %lu left",
	      pool->mp_name, pool->mp_free);

	pthread_mutex_unlock(&pool->mp_lock);
}

#ifdef TEST_MEM

void *alloc_buffer_worker(void *data)
{
	int i = 0;
	q_mem_pool_t *pool = (q_mem_pool_t *)data;
	q_mem_buf_t *buf[10];

	puts("worker taking up all buf");

	for (i = 0; i < 10; i++) {
		buf[i] = q_mem_get(pool);
	}

	puts("sleep for 5 sec before release buff...");
	sleep(10);

	for (i = 0; i < 10; i++) {
		q_mem_put(buf[i]);
	}

	puts("quitting thread");
	return NULL;
}
	
int main(void)
{
	q_mem_pool_t *pool = q_mem_pool_create("test", 4096, 10);
	q_mem_buf_t *buf = NULL;
	pthread_t thread;
	int i = 0;

	pthread_create(&thread, NULL, alloc_buffer_worker, (void *)pool);

	puts("main sleep 1 sec to start alloc");
	sleep(1);

	for (i = 0; i < 10; i++) {
		buf = q_mem_get(pool);
		q_mem_put(buf);
	}
}
#endif
