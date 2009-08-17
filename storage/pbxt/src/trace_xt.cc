/* Copyright (c) 2005 PrimeBase Technologies GmbH
 *
 * PrimeBase XT
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * 2005-02-07	Paul McCullagh
 *
 * H&G2JCtL
 */

#include "xt_config.h"

#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <stdlib.h>
#include <time.h>

#include "trace_xt.h"
#include "pthread_xt.h"
#include "thread_xt.h"

#ifdef DEBUG
//#define PRINT_TRACE
//#define RESET_AFTER_DUMP
#endif

static xtBool			trace_initialized = FALSE;
static xt_mutex_type	trace_mutex;
static size_t			trace_log_size;
static size_t			trace_log_offset;
static size_t			trace_log_end;
static char				*trace_log_buffer;
static u_long			trace_stat_count;
static FILE				*trace_dump_file;
static xtBool			trace_flush_dump = FALSE;

#define DEFAULT_TRACE_LOG_SIZE		(40*1024*1204)
#define MAX_PRINT_LEN				2000

xtPublic xtBool xt_init_trace(void)
{
	int err;

	err = xt_p_mutex_init_with_autoname(&trace_mutex, NULL);
	if (err) {
		xt_log_errno(XT_NS_CONTEXT, err);
		trace_initialized = FALSE;
		return FALSE;
	}
	trace_initialized = TRUE;
	trace_log_buffer = (char *) malloc(DEFAULT_TRACE_LOG_SIZE+1);
	if (!trace_log_buffer) {
		xt_log_errno(XT_NS_CONTEXT, ENOMEM);
		xt_exit_trace();
		return FALSE;
	}
	trace_log_size = DEFAULT_TRACE_LOG_SIZE;
	trace_log_offset = 0;
	trace_log_end = 0;
	trace_stat_count = 0;

#ifdef XT_TRACK_CONNECTIONS
	for (int i=0; i<XT_TRACK_MAX_CONNS; i++)
		xt_track_conn_info[i].cu_t_id = i;
#endif

	return TRUE;
}

xtPublic void xt_exit_trace(void)
{
	if (trace_initialized) {
#ifdef DEBUG
		xt_dump_trace();
#endif
		xt_free_mutex(&trace_mutex);
		trace_initialized = FALSE;
		if (trace_log_buffer)
			free(trace_log_buffer);
		trace_log_buffer = NULL;
		trace_log_size = 0;
		trace_log_offset = 0;
		trace_log_end = 0;
		trace_stat_count = 0;
	}
	if (trace_dump_file) {
		fclose(trace_dump_file);
		trace_dump_file = NULL;
	}
}

xtPublic void xt_print_trace(void)
{
	if (trace_log_offset) {
		xt_lock_mutex_ns(&trace_mutex);
		if (trace_log_end > trace_log_offset+1) {
			trace_log_buffer[trace_log_end] = 0;
			printf("%s", trace_log_buffer + trace_log_offset + 1);
		}
		trace_log_buffer[trace_log_offset] = 0;
		printf("%s", trace_log_buffer);
		trace_log_offset = 0;
		trace_log_end = 0;
		xt_unlock_mutex_ns(&trace_mutex);
	}
}

xtPublic void xt_dump_trace(void)
{
	FILE *fp;

	if (trace_log_offset) {
		fp = fopen("pbxt.log", "w");

		xt_lock_mutex_ns(&trace_mutex);
		if (fp) {
			if (trace_log_end > trace_log_offset+1) {
				trace_log_buffer[trace_log_end] = 0;
				fprintf(fp, "%s", trace_log_buffer + trace_log_offset + 1);
			}
			trace_log_buffer[trace_log_offset] = 0;
			fprintf(fp, "%s", trace_log_buffer);
			fclose(fp);
		}

#ifdef RESET_AFTER_DUMP
		trace_log_offset = 0;
		trace_log_end = 0;
		trace_stat_count = 0;
#endif
		xt_unlock_mutex_ns(&trace_mutex);
	}

	if (trace_dump_file) {
		xt_lock_mutex_ns(&trace_mutex);
		if (trace_dump_file) {
			fflush(trace_dump_file);
			fclose(trace_dump_file);
			trace_dump_file = NULL;
		}
		xt_unlock_mutex_ns(&trace_mutex);
	}
}

xtPublic void xt_trace(const char *fmt, ...)
{
	va_list	ap;
	size_t	len;

	va_start(ap, fmt);
	xt_lock_mutex_ns(&trace_mutex);

	if (trace_log_offset + MAX_PRINT_LEN > trace_log_size) {
		/* Start at the beginning of the buffer again: */
		trace_log_end = trace_log_offset;
		trace_log_offset = 0;
	}

	len = (size_t) vsnprintf(trace_log_buffer + trace_log_offset, trace_log_size - trace_log_offset, fmt, ap);
	trace_log_offset += len;

	xt_unlock_mutex_ns(&trace_mutex);
	va_end(ap);

#ifdef PRINT_TRACE
	xt_print_trace();
#endif
}

xtPublic void xt_ttracef(XTThreadPtr self, char *fmt, ...)
{
	va_list	ap;
	size_t	len;

	va_start(ap, fmt);
	xt_lock_mutex_ns(&trace_mutex);

	if (trace_log_offset + MAX_PRINT_LEN > trace_log_size) {
		trace_log_end = trace_log_offset;
		trace_log_offset = 0;
	}

	trace_stat_count++;
	len = (size_t) sprintf(trace_log_buffer + trace_log_offset, "%lu %s: ", trace_stat_count, self->t_name);
	trace_log_offset += len;
	len = (size_t) vsnprintf(trace_log_buffer + trace_log_offset, trace_log_size - trace_log_offset, fmt, ap);
	trace_log_offset += len;

	xt_unlock_mutex_ns(&trace_mutex);
	va_end(ap);

#ifdef PRINT_TRACE
	xt_print_trace();
#endif
}

xtPublic void xt_ttraceq(XTThreadPtr self, char *query)
{
	size_t	qlen = strlen(query), tlen;
	char	*ptr, *qptr;

	if (!self)
		self = xt_get_self();

	xt_lock_mutex_ns(&trace_mutex);

	if (trace_log_offset + qlen + 100 >= trace_log_size) {
		/* Start at the beginning of the buffer again: */
		trace_log_end = trace_log_offset;
		trace_log_offset = 0;
	}

	trace_stat_count++;
	tlen = (size_t) sprintf(trace_log_buffer + trace_log_offset, "%lu %s: ", trace_stat_count, self->t_name);
	trace_log_offset += tlen;

	ptr = trace_log_buffer + trace_log_offset;
	qlen = 0;
	qptr = query;
	while (*qptr) {
		if (*qptr == '\n' || *qptr == '\r')
			*ptr = ' ';
		else
			*ptr = *qptr;
		if (*qptr == '\n' || *qptr == '\r' || *qptr == ' ') {
			qptr++;
			while (*qptr == '\n' || *qptr == '\r' || *qptr == ' ')
				qptr++;				
		}
		else
			qptr++;
		ptr++;
		qlen++;
	}

	trace_log_offset += qlen;
	*(trace_log_buffer + trace_log_offset) = '\n';
	*(trace_log_buffer + trace_log_offset + 1) = '\0';
	trace_log_offset++;
	
	xt_unlock_mutex_ns(&trace_mutex);

#ifdef PRINT_TRACE
	xt_print_trace();
#endif
}

/*
 * Returns the time in microseconds.
 * (1/1000000 of a second)
 */
xtPublic xtWord8 xt_trace_clock(void)
{
	static xtWord8	trace_start_clock = 0;
	xtWord8			now;

#ifdef XT_WIN
	now = ((xtWord8) GetTickCount()) * (xtWord8) 1000;
#else
	struct timeval	tv;

	gettimeofday(&tv, NULL);
	now = (xtWord8) tv.tv_sec * (xtWord8) 1000000 + tv.tv_usec;
#endif
	if (trace_start_clock)
		return now - trace_start_clock;
	trace_start_clock = now;
	return 0;
}

xtPublic char *xt_trace_clock_str(char *ptr)
{
	static char	buffer[50];
	xtWord8		now = xt_trace_clock();

	if (!ptr)
		ptr = buffer;

	sprintf(ptr, "%d.%06d", (int) (now / (xtWord8) 1000000), (int) (now % (xtWord8) 1000000));
	return ptr;
}

xtPublic char *xt_trace_clock_diff(char *ptr)
{
	static xtWord8	trace_last_clock = 0;
	static char		buffer[50];
	xtWord8			now = xt_trace_clock();

	if (!ptr)
		ptr = buffer;

	sprintf(ptr, "%d.%06d (%d)", (int) (now / (xtWord8) 1000000), (int) (now % (xtWord8) 1000000), (int) (now - trace_last_clock));
	trace_last_clock = now;
	return ptr;
}

xtPublic char *xt_trace_clock_diff(char *ptr, xtWord8 start_time)
{
	xtWord8 now = xt_trace_clock();

	sprintf(ptr, "%d.%06d (%d)", (int) (now / (xtWord8) 1000000), (int) (now % (xtWord8) 1000000), (int) (now - start_time));
	return ptr;
}


xtPublic void xt_set_fflush(xtBool on)
{
	trace_flush_dump = on;
}

xtPublic void xt_ftracef(char *fmt, ...)
{
	va_list	ap;

	va_start(ap, fmt);
	xt_lock_mutex_ns(&trace_mutex);

	if (!trace_dump_file) {
		char buffer[100];

		for (int i=1; ;i++) {
			sprintf(buffer, "pbxt-dump-%d.log", i);
			if (!xt_fs_exists(buffer)) {
				trace_dump_file = fopen(buffer, "w");
				break;
			}
		}
	}

	vfprintf(trace_dump_file, fmt, ap);
	if (trace_flush_dump)
		fflush(trace_dump_file);

	xt_unlock_mutex_ns(&trace_mutex);
	va_end(ap);
}

/*
 * -----------------------------------------------------------------------
 * CONNECTION TRACKING
 */

#ifdef XT_TRACK_CONNECTIONS
XTConnInfoRec	xt_track_conn_info[XT_TRACK_MAX_CONNS];

static int trace_comp_conn_info(const void *a, const void *b)
{
	XTConnInfoPtr	ci_a = (XTConnInfoPtr) a, ci_b = (XTConnInfoPtr) b;

	if (ci_a->ci_curr_xact_id > ci_b->ci_curr_xact_id)
		return 1;
	if (ci_a->ci_curr_xact_id < ci_b->ci_curr_xact_id)
		return -1;
	return 0;
}

xtPublic void xt_dump_conn_tracking(void)
{
	XTConnInfoRec	conn_info[XT_TRACK_MAX_CONNS];
	XTConnInfoPtr	ptr;

	memcpy(conn_info, xt_track_conn_info, sizeof(xt_track_conn_info));
	qsort(conn_info, XT_TRACK_MAX_CONNS, sizeof(XTConnInfoRec), trace_comp_conn_info);

	ptr = conn_info;
	for (int i=0; i<XT_TRACK_MAX_CONNS; i++) {
		if (ptr->ci_curr_xact_id || ptr->ci_prev_xact_id) {
			printf("%3d curr=%d prev=%d prev-time=%ld\n", (int) ptr->cu_t_id, (int) ptr->ci_curr_xact_id, (int) ptr->ci_prev_xact_id, (long) ptr->ci_prev_xact_time);
			if (i+1<XT_TRACK_MAX_CONNS) {
				printf("    diff=%d\n", (int) (ptr+1)->ci_curr_xact_id - (int) ptr->ci_curr_xact_id);
			}
		}
		ptr++;
	}
}

#endif


