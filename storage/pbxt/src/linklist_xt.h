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
 * 2005-02-03	Paul McCullagh
 *
 * H&G2JCtL
 */
#ifndef __xt_linklist_h__
#define __xt_linklist_h__

#include "xt_defs.h"

struct XTThread;

typedef struct XTLinkedItem {
	struct XTLinkedItem		*li_prev;
	struct XTLinkedItem		*li_next;
} XTLinkedItemRec, *XTLinkedItemPtr;

typedef struct XTLinkedList {
	xt_mutex_type			*ll_lock;
	xt_cond_type			*ll_cond;			/* Condition for wait for empty. */
	void					*ll_thunk;
	XTFreeFunc				ll_free_func;
	u_int					ll_item_count;
	XTLinkedItemPtr			ll_items;
} XTLinkedListRec, *XTLinkedListPtr;

XTLinkedListPtr		xt_new_linkedlist(struct XTThread *self, void *thunk, XTFreeFunc free_func, xtBool with_lock);
void				xt_free_linkedlist(struct XTThread *self, XTLinkedListPtr ll);

void				xt_ll_add(struct XTThread *self, XTLinkedListPtr ll, XTLinkedItemPtr li, xtBool lock);
void				xt_ll_remove(struct XTThread *self, XTLinkedListPtr ll, XTLinkedItemPtr li, xtBool lock);
xtBool 				xt_ll_exists(struct XTThread *self, XTLinkedListPtr ll, XTLinkedItemPtr li, xtBool lock);

void				xt_ll_lock(struct XTThread *self, XTLinkedListPtr ll);
void				xt_ll_unlock(struct XTThread *self, XTLinkedListPtr ll);

void				xt_ll_wait_till_empty(struct XTThread *self, XTLinkedListPtr ll);

XTLinkedItemPtr		xt_ll_first_item(struct XTThread *self, XTLinkedListPtr ll);
XTLinkedItemPtr		xt_ll_next_item(struct XTThread *self, XTLinkedItemPtr item);
u_int				xt_ll_get_size(XTLinkedListPtr ll);

typedef struct XTLinkedQItem {
	struct XTLinkedQItem	*qi_next;
} XTLinkedQItemRec, *XTLinkedQItemPtr;

typedef struct XTLinkedQueue {
	size_t					lq_count;
	XTLinkedQItemPtr		lq_front;
	XTLinkedQItemPtr		lq_back;
} XTLinkedQueueRec, *XTLinkedQueuePtr;

void				xt_init_linkedqueue(struct XTThread *self, XTLinkedQueuePtr lq);
void				xt_exit_linkedqueue(struct XTThread *self, XTLinkedQueuePtr lq);

void				xt_lq_add(struct XTThread *self, XTLinkedQueuePtr lq, XTLinkedQItemPtr qi);
XTLinkedQItemPtr	xt_lq_remove(struct XTThread *self, XTLinkedQueuePtr lq);

#endif

