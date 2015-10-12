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

#ifndef __Q_MEM_H__
#define __Q_MEM_H__

#include <stdint.h>
#include <pthread.h>
#include "list.h"

/*
 * memory managements for the QuarkIM program. This is a very simple version of
 * something like the slab allocator. We should not allocate memory using
 * "malloc" in the whole program. Instead, we should fetch them all from
 * mem_pools.
 */

#define Q_MEM_POOL_NAME_LEN (16)
#define Q_MEM_ALIGN         (8)

/* maximum size per buffer */
#define Q_MEM_POOL_MAX_BUF_SIZE (4096)
/* max buffer count per pool */
#define Q_MEM_POOL_MAX_BUF_COUNT (100)

/*
 * memory pool structure:
 * |----------------------------------------------------------|
 * | mem_pool_hdr | mem_hdr0 | data0 | ... | mem_hdrN | dataN |
 * |----------------------------------------------------------|
 */

struct q_mem_pool {
	char mp_name[Q_MEM_POOL_NAME_LEN];
	/* usable memory size for each mem_buf inside the pool */
	size_t mp_buf_size;
	/* how many bufs do we have */
	size_t mp_total;
	/* how many free */
	size_t mp_free;

	/* the list of all free blocks in the pool */
	struct list_head mp_free_bufs;
	/* blocks that are in use */
	struct list_head mp_used_bufs;

	/* structure locks and cv */
	pthread_mutex_t mp_lock;
	/* wait on cond if no more free buf */
	pthread_cond_t mp_cond;
};
typedef struct q_mem_pool q_mem_pool_t;

struct q_mem_buf {
	/* owner of the buf */
	q_mem_pool_t *m_pool;
	/* linkage to owner pool list */
	struct list_head m_pool_link;
	uint64_t m_barrier;
	/* user data stored here. */
	char m_ptr[0];
};
typedef struct q_mem_buf q_mem_buf_t;

#define Q_MEM_BUF_DATA(mem_buf) ((void *)(mem_buf)->m_ptr)

/* create mem pool */
q_mem_pool_t *q_mem_pool_create(const char *name, size_t buf_size,
				size_t count);

/* TBD: destroy one pool */
int q_mem_pool_destroy(q_mem_pool_t *pool);

/* fetching memory from pool. When there are no enough buf, will block and
 * wait. */
q_mem_buf_t *q_mem_get(q_mem_pool_t *pool);

/* returning buf back to pool */
void q_mem_put(q_mem_buf_t *buf);

#endif
