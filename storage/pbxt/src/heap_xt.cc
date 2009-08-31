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

#include "xt_config.h"

#include "pthread_xt.h"
#include "heap_xt.h"
#include "thread_xt.h"

#ifdef xt_heap_new
#undef xt_heap_new
#endif

#ifdef DEBUG_MEMORY
xtPublic XTHeapPtr xt_mm_heap_new(XTThreadPtr self, size_t size, XTFinalizeFunc finalize, u_int line, c_char *file, xtBool track)
#else
xtPublic XTHeapPtr xt_heap_new(XTThreadPtr self, size_t size, XTFinalizeFunc finalize)
#endif
{
	volatile XTHeapPtr	hp;
	
#ifdef DEBUG_MEMORY
	hp = (XTHeapPtr) xt_mm_calloc(self, size, line, file);
	hp->h_track = track;
	if (track)
		printf("HEAP: +1  1 %s:%d\n", file, (int) line);
#else
	hp = (XTHeapPtr) xt_calloc(self, size);
#endif
	if (!hp)
		return NULL;

	try_(a) {
		xt_spinlock_init_with_autoname(self, &hp->h_lock);
	}
	catch_(a) {
		xt_free(self, hp);
		throw_();
	}
	cont_(a);

	hp->h_ref_count = 1;
	hp->h_finalize = finalize;
	hp->h_onrelease = NULL;
	return hp;
}

xtPublic void xt_check_heap(XTThreadPtr XT_NDEBUG_UNUSED(self), XTHeapPtr XT_NDEBUG_UNUSED(hp))
{
#ifdef DEBUG_MEMORY
	xt_mm_malloc_size(self, hp);
#endif
}

#ifdef DEBUG_MEMORY
xtPublic void xt_mm_heap_reference(XTThreadPtr self, XTHeapPtr hp, u_int line, c_char *file)
#else
xtPublic void xt_heap_reference(XTThreadPtr, XTHeapPtr hp)
#endif
{
	xt_spinlock_lock(&hp->h_lock);
#ifdef DEBUG_MEMORY
	if (hp->h_track)
		printf("HEAP: +1 %d->%d %s:%d\n", (int) hp->h_ref_count, (int) hp->h_ref_count+1, file, (int) line);
#endif
	hp->h_ref_count++;
	xt_spinlock_unlock(&hp->h_lock);
}

xtPublic void xt_heap_release(XTThreadPtr self, XTHeapPtr hp)
{	
	if (!hp)
		return;
#ifdef DEBUG_MEMORY
	xt_spinlock_lock(&hp->h_lock);
	ASSERT(hp->h_ref_count != 0);
	xt_spinlock_unlock(&hp->h_lock);
#endif
	xt_spinlock_lock(&hp->h_lock);
	if (hp->h_onrelease)
		(*hp->h_onrelease)(self, hp);
	if (hp->h_ref_count > 0) {
#ifdef DEBUG_MEMORY
	if (hp->h_track)
		printf("HEAP: -1 %d->%d\n", (int) hp->h_ref_count, (int) hp->h_ref_count-1);
#endif
		hp->h_ref_count--;
		if (hp->h_ref_count == 0) {
			if (hp->h_finalize)
				(*hp->h_finalize)(self, hp);
			xt_spinlock_unlock(&hp->h_lock);
			xt_free(self, hp);
			return;
		}
	}
	xt_spinlock_unlock(&hp->h_lock);
}

xtPublic void xt_heap_set_release_callback(XTThreadPtr XT_UNUSED(self), XTHeapPtr hp, XTFinalizeFunc onrelease)
{
	hp->h_onrelease = onrelease;
}

xtPublic u_int xt_heap_get_ref_count(struct XTThread *XT_UNUSED(self), XTHeapPtr hp)
{
	return hp->h_ref_count;
}


