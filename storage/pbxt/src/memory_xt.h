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
 * 2005-01-04	Paul McCullagh
 *
 * H&G2JCtL
 */
#ifndef __xt_memory_h__
#define __xt_memory_h__

#include <string.h>

#include "xt_defs.h"

struct XTThread;

#ifdef DEBUG
#define DEBUG_MEMORY
#endif

#ifdef DEBUG_MEMORY

#define XT_MM_STACK_TRACE	200
#define XT_MM_TRACE_DEPTH	4
#define XT_MM_TRACE_INC		((char *) 1)
#define XT_MM_TRACE_DEC		((char *) 2)
#define XT_MM_TRACE_SW_INC	((char *) 1)
#define XT_MM_TRACE_SW_DEC	((char *) 2)
#define XT_MM_TRACE_ERROR	((char *) 3)

typedef struct XTMMTraceRef {
	int						mm_pos;
	u_int					mm_id;
	u_int					mm_line[XT_MM_STACK_TRACE];
	c_char					*mm_trace[XT_MM_STACK_TRACE];
} XTMMTraceRefRec, *XTMMTraceRefPtr;

#define XT_MM_TRACE_INIT(x)	(x)->mm_pos = 0

extern char *mm_watch_point;

#define XT_MEMMOVE(b, d, s, l)	xt_mm_memmove(b, d, s, l)
#define XT_MEMCPY(b, d, s, l)	xt_mm_memcpy(b, d, s, l)
#define XT_MEMSET(b, d, v, l)	xt_mm_memset(b, d, v, l)

#define xt_malloc(t, s)			xt_mm_malloc(t, s, __LINE__, __FILE__)
#define xt_calloc(t, s)			xt_mm_calloc(t, s, __LINE__, __FILE__)
#define xt_realloc(t, p, s)		xt_mm_realloc(t, p, s, __LINE__, __FILE__)
#define xt_free					xt_mm_free
#define xt_pfree				xt_mm_pfree

#define xt_malloc_ns(s)			xt_mm_malloc(NULL, s, __LINE__, __FILE__)
#define xt_calloc_ns(s)			xt_mm_calloc(NULL, s, __LINE__, __FILE__)
#define xt_realloc_ns(p, s)		xt_mm_sys_realloc(NULL, p, s, __LINE__, __FILE__)
#define xt_free_ns(p)			xt_mm_free(NULL, p)

void	xt_mm_memmove(void *block, void *dest, void *source, size_t size);
void	xt_mm_memcpy(void *block, void *dest, void *source, size_t size);
void	xt_mm_memset(void *block, void *dest, int value, size_t size);

void	*xt_mm_malloc(struct XTThread *self, size_t size, u_int line, const char *file);
void	*xt_mm_calloc(struct XTThread *self, size_t size, u_int line, const char *file);
xtBool	xt_mm_realloc(struct XTThread *self, void **ptr, size_t size, u_int line, const char *file);
void	xt_mm_free(struct XTThread *self, void *ptr);
void	xt_mm_pfree(struct XTThread *self, void **ptr);
size_t	xt_mm_malloc_size(struct XTThread *self, void *ptr);
void	xt_mm_check_ptr(struct XTThread *self, void *ptr);
xtBool	xt_mm_sys_realloc(struct XTThread *self, void **ptr, size_t newsize, u_int line, const char *file);

#ifndef XT_SCAN_CORE_DEFINED
#define XT_SCAN_CORE_DEFINED
xtBool	xt_mm_scan_core(void);
#endif

void	mm_trace_inc(struct XTThread *self, XTMMTraceRefPtr tr);
void	mm_trace_dec(struct XTThread *self, XTMMTraceRefPtr tr);
void	mm_trace_init(struct XTThread *self, XTMMTraceRefPtr tr);
void	mm_trace_print(XTMMTraceRefPtr tr);

#else

#define XT_MEMMOVE(b, d, s, l)	memmove(d, s, l)
#define XT_MEMCPY(b, d, s, l)	memcpy(d, s, l)
#define XT_MEMSET(b, d, v, l)	memset(d, v, l)

void	*xt_malloc(struct XTThread *self, size_t size);
void	*xt_calloc(struct XTThread *self, size_t size);
xtBool	xt_realloc(struct XTThread *self, void **ptr, size_t size);
void	xt_free(struct XTThread *self, void *ptr);
void	xt_pfree(struct XTThread *self, void **ptr);

void	*xt_malloc_ns(size_t size);
void	*xt_calloc_ns(size_t size);
xtBool	xt_realloc_ns(void **ptr, size_t size);
void	xt_free_ns(void *ptr);

#define xt_pfree(t, p)			xt_pfree(t, (void **) p)

#endif

#ifdef DEBUG_MEMORY
#define xt_dup_string(t, s)		xt_mm_dup_string(t, s, __LINE__, __FILE__)

char	*xt_mm_dup_string(struct XTThread *self, const char *path, u_int line, const char *file);
#else
char	*xt_dup_string(struct XTThread *self, const char *path);
#endif

char	*xt_long_to_str(struct XTThread *self, long v);
char	*xt_dup_nstr(struct XTThread *self, const char *str, int start, size_t len);

xtBool	xt_init_memory(void);
void	xt_exit_memory(void);

#endif
