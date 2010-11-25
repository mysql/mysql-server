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
 * 2005-01-10	Paul McCullagh
 *
 * H&G2JCtL
 */
#ifndef __xt_heap_h__
#define __xt_heap_h__

#include "xt_defs.h"
#include "lock_xt.h"
#include "memory_xt.h"

struct XTThread;

/*
 * Heap memory has a reference count, and a lock for shared access.
 * It also has a finalize routine which is called before the memory is
 * freed.
 */
typedef void (*XTFinalizeFunc)(struct XTThread *self, void *heap_ptr);

typedef struct XTHeap {
	XTSpinLockRec			h_lock;					/* Prevent concurrent access to the heap memory: */
	u_int					h_ref_count;			/* So we know when to free (EVERY pointer reference MUST be counted). */
	XTFinalizeFunc			h_finalize;				/* If non-NULL, call before freeing. */
	XTFinalizeFunc			h_onrelease;			/* If non-NULL, call on release. */
#ifdef DEBUG
	xtBool					h_track;
#endif
} XTHeapRec, *XTHeapPtr;

/* Returns with reference count = 1 */
XTHeapPtr	xt_heap_new(struct XTThread *self, size_t size, XTFinalizeFunc finalize);
XTHeapPtr	xt_mm_heap_new(struct XTThread *self, size_t size, XTFinalizeFunc finalize, u_int line, c_char *file, xtBool track);

void		xt_heap_set_release_callback(struct XTThread *self, XTHeapPtr mem, XTFinalizeFunc onrelease);

void		xt_heap_reference(struct XTThread *self, XTHeapPtr mem);
void		xt_mm_heap_reference(struct XTThread *self, XTHeapPtr hp, u_int line, c_char *file);

void		xt_heap_release(struct XTThread *self, XTHeapPtr mem);
u_int		xt_heap_get_ref_count(struct XTThread *self, XTHeapPtr mem);

void		xt_check_heap(struct XTThread *self, XTHeapPtr mem);

#ifdef DEBUG_MEMORY
#define xt_heap_new(t, s, f)		xt_mm_heap_new(t, s, f, __LINE__, __FILE__, FALSE)
#define xt_heap_new_track(t, s, f)	xt_mm_heap_new(t, s, f, __LINE__, __FILE__, TRUE)
#define xt_heap_reference(t, s)		xt_mm_heap_reference(t, s, __LINE__, __FILE__)
#endif

#endif
