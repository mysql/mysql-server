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
 * 2005-02-04	Paul McCullagh
 *
 * H&G2JCtL
 */
#ifndef __xt_sortedlist_h__
#define __xt_sortedlist_h__

#include "pthread_xt.h"
#include "bsearch_xt.h"

struct XTThread;

typedef struct XTSortedList {
	u_int			sl_item_size;
	u_int			sl_grow_size;
	XTCompareFunc	sl_comp_func;
	void			*sl_thunk;
	XTFreeFunc		sl_free_func;
	xt_mutex_type	*sl_lock;
	struct XTThread *sl_locker;
	u_int			sl_lock_count;
	xt_cond_type	*sl_cond;

	u_int			sl_current_size;
	u_int			sl_usage_count;
	char			*sl_data;
} XTSortedListRec, *XTSortedListPtr;

typedef struct XTSortedListInfo {
	XTSortedListPtr	li_sl;
	void			*li_key;
} XTSortedListInfoRec, *XTSortedListInfoPtr;

XTSortedListPtr		xt_new_sortedlist(struct XTThread *self, u_int item_size, u_int initial_size, u_int grow_size, XTCompareFunc comp_func, void *thunk, XTFreeFunc free_func, xtBool with_lock, xtBool with_cond);
void				xt_init_sortedlist(struct XTThread *self, XTSortedListPtr sl, u_int item_size, u_int initial_size, u_int grow_size, XTCompareFunc comp_func, void *thunk, XTFreeFunc free_func, xtBool with_lock, xtBool with_cond);
void				xt_free_sortedlist(struct XTThread *self, XTSortedListPtr ld);
void				xt_empty_sortedlist(struct XTThread *self, XTSortedListPtr sl);
XTSortedListPtr		xt_new_sortedlist_ns(u_int item_size, u_int grow_size, XTCompareFunc comp_func, void *thunk, XTFreeFunc free_func);

xtBool				xt_sl_insert(struct XTThread *self, XTSortedListPtr sl, void *key, void *data);
void				*xt_sl_find(struct XTThread *self, XTSortedListPtr sl, void *key);
xtBool				xt_sl_delete(struct XTThread *self, XTSortedListPtr sl, void *key);
void				xt_sl_delete_item_at(struct XTThread *self, XTSortedListPtr sl, size_t i);
void				xt_sl_remove_from_front(struct XTThread *self, XTSortedListPtr sl, size_t items);
void				xt_sl_delete_from_info(struct XTThread *self, XTSortedListInfoPtr li);
size_t				xt_sl_get_size(XTSortedListPtr sl);
void				xt_sl_set_size(XTSortedListPtr sl, size_t new_size);
void				*xt_sl_item_at(XTSortedListPtr sl, size_t i);
void				*xt_sl_last_item(XTSortedListPtr sl);
void				*xt_sl_first_item(XTSortedListPtr sl);

xtBool				xt_sl_lock(struct XTThread *self, XTSortedListPtr sl);
void				xt_sl_unlock(struct XTThread *self, XTSortedListPtr sl);
void				xt_sl_lock_ns(XTSortedListPtr sl, struct XTThread *thread);
void				xt_sl_unlock_ns(XTSortedListPtr sl);

void				xt_sl_wait(struct XTThread *self, XTSortedListPtr sl);
xtBool				xt_sl_signal(struct XTThread *self, XTSortedListPtr sl);
void				xt_sl_broadcast(struct XTThread *self, XTSortedListPtr sl);

#endif
