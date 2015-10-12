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

#include <stdio.h>
#include <stdarg.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <time.h>
#include "q_util.h"
#include "q_log.h"

struct q_log_ctx {
	int ql_init;		/* whether we have init */
	int ql_fd;
	char *ql_file;
	int ql_level;
	pthread_mutex_t ql_mtx;
};

static struct q_log_ctx log_ctx = {
	.ql_init = 0,
	.ql_fd = -1,
	.ql_file = Q_LOG_FILE,
	.ql_level = Q_LOG_LEVEL,
	.ql_mtx = PTHREAD_MUTEX_INITIALIZER,
};

static char *lvl_str[Q_LOG_MAX] = {"D", "I", "W", "E"};

void q_log_init(void)
{
	int fd = 0;
	log_ctx.ql_init = 1;

	fd = open(log_ctx.ql_file, O_WRONLY | O_APPEND | O_CREAT, 0644);
	if (fd == -1) {
		fprintf(stderr, "failed init log\n");
		return;
	}

	log_ctx.ql_fd = fd;
	log_ctx.ql_init = 2;
}

void q_log(int level, const char *fmt, ...)
{
	static char log_buf[1024] = {0};
	int n = 0;
	struct timeval tv;
	struct tm tm_val;
	va_list ap;

	if (level < log_ctx.ql_level || level >= Q_LOG_MAX)
		return;

	pthread_mutex_lock(&log_ctx.ql_mtx);

	/* init fd for the first time */
	if (unlikely(log_ctx.ql_init == 0))
		q_log_init();

	/* if log fd invalid */
	if (log_ctx.ql_fd < 0)
		goto out;

	gettimeofday(&tv, NULL);
	localtime_r(&tv.tv_sec, &tm_val);
	n = snprintf(log_buf, sizeof(log_buf), "%04d-%02d-%02d "
		     "%02d:%02d:%02d.%6lu [%s] ",
		     tm_val.tm_year,
		     tm_val.tm_mon,
		     tm_val.tm_mday,
		     tm_val.tm_hour,
		     tm_val.tm_min,
		     tm_val.tm_sec,
		     tv.tv_usec,
		     lvl_str[level]);
	if (n > 0)
		write(log_ctx.ql_fd, log_buf, n);

	va_start(ap, fmt);
	n = vsnprintf(log_buf, sizeof(log_buf) - 1, fmt, ap);
	va_end(ap);

	if (n > 0)
		write(log_ctx.ql_fd, log_buf, n);

	write(log_ctx.ql_fd, "\n", 1);

out:
	pthread_mutex_unlock(&log_ctx.ql_mtx);
}

#ifdef TEST_LOG
int main(void)
{
	q_log(Q_LOG_INFO, "hello world!");
}
#endif

