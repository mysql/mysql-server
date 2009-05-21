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

#include "xt_config.h"

#include "pthread_xt.h"
#include "linklist_xt.h"
#include "thread_xt.h"
#include "memory_xt.h"

xtPublic XTLinkedListPtr xt_new_linkedlist(struct XTThread *self, void *thunk, XTFreeFunc free_func, xtBool with_lock)
{
	XTLinkedListPtr	ll;

	ll = (XTLinkedListPtr) xt_calloc(self, sizeof(XTLinkedListRec));
	try_(a) {
		if (with_lock) {
			ll->ll_lock = (xt_mutex_type *) xt_calloc(self, sizeof(xt_mutex_type));
			try_(b) {
				xt_init_mutex_with_autoname(self, ll->ll_lock);
			}
			catch_(b) {
				xt_free(self, ll->ll_lock);
				ll->ll_lock = NULL;
				throw_();
			}
			cont_(b);
			ll->ll_cond = (xt_cond_type *) xt_calloc(self, sizeof(xt_cond_type));
			try_(c) {
				xt_init_cond(self, ll->ll_cond);
			}
			catch_(c) {
				xt_free(self, ll->ll_cond);
				ll->ll_cond = NULL;
				throw_();
			}
			cont_(c);
		}
		ll->ll_thunk = thunk;
		ll->ll_free_func = free_func;
	}
	catch_(a) {
		xt_free_linkedlist(self, ll);
		throw_();
	}
	cont_(a);
	return ll;
}

xtPublic void xt_free_linkedlist(XTThreadPtr self, XTLinkedListPtr ll)
{
	if (ll->ll_lock)
		xt_lock_mutex(self, ll->ll_lock);
	while (ll->ll_items)
		xt_ll_remove(self, ll, ll->ll_items, FALSE);
	if (ll->ll_lock)
		xt_unlock_mutex(self, ll->ll_lock);
	if (ll->ll_lock) {
		xt_free_mutex(ll->ll_lock);
		xt_free(self, ll->ll_lock);
	}
	if (ll->ll_cond) {
		xt_free_cond(ll->ll_cond);
		xt_free(self, ll->ll_cond);
	}
	xt_free(self, ll);
}

xtPublic void xt_ll_add(XTThreadPtr self, XTLinkedListPtr ll, XTLinkedItemPtr li, xtBool lock)
{
	if (lock && ll->ll_lock)
		xt_lock_mutex(self, ll->ll_lock);
	li->li_next = ll->ll_items;
	li->li_prev = NULL;
	if (ll->ll_items)
		ll->ll_items->li_prev = li;
	ll->ll_items = li;
	ll->ll_item_count++;
	if (lock && ll->ll_lock)
		xt_unlock_mutex(self, ll->ll_lock);
}

xtPublic XTLinkedItemPtr xt_ll_first_item(XTThreadPtr XT_UNUSED(self), XTLinkedListPtr ll)
{
	return ll ? ll->ll_items : NULL;
}

xtPublic XTLinkedItemPtr xt_ll_next_item(XTThreadPtr XT_UNUSED(self), XTLinkedItemPtr item)
{
	return item->li_next;
}

xtPublic xtBool xt_ll_exists(XTThreadPtr self, XTLinkedListPtr ll, XTLinkedItemPtr li, xtBool lock)
{
	XTLinkedItemPtr ptr;

	if (lock && ll->ll_lock)
		xt_lock_mutex(self, ll->ll_lock);

	ptr = ll->ll_items;
	
	for (ptr = ll->ll_items; ptr && (ptr != li); ptr = ptr->li_next){}
	
	if (lock && ll->ll_lock)
		xt_unlock_mutex(self, ll->ll_lock);
		
	return (ptr == li);
}

xtPublic void xt_ll_remove(XTThreadPtr self, XTLinkedListPtr ll, XTLinkedItemPtr li, xtBool lock)
{
	if (lock && ll->ll_lock)
		xt_lock_mutex(self, ll->ll_lock);

	/* Move front pointer: */
	if (ll->ll_items == li)
		ll->ll_items = li->li_next;

	/* Remove from list: */
	if (li->li_prev)
		li->li_prev->li_next = li->li_next;
	if (li->li_next)
		li->li_next->li_prev = li->li_prev;

	ll->ll_item_count--;
	if (ll->ll_free_func)
		(*ll->ll_free_func)(self, ll->ll_thunk, li);

	/* Signal one less: */
	if (ll->ll_cond)
		xt_signal_cond(self, ll->ll_cond);

	if (lock && ll->ll_lock)
		xt_unlock_mutex(self, ll->ll_lock);
}

xtPublic void xt_ll_lock(XTThreadPtr self, XTLinkedListPtr ll)
{
	if (ll->ll_lock)
		xt_lock_mutex(self, ll->ll_lock);
}

xtPublic void xt_ll_unlock(XTThreadPtr self, XTLinkedListPtr ll)
{
	if (ll->ll_lock)
		xt_unlock_mutex(self, ll->ll_lock);
}

xtPublic void xt_ll_wait_till_empty(XTThreadPtr self, XTLinkedListPtr ll)
{
	xt_lock_mutex(self, ll->ll_lock);
	pushr_(xt_unlock_mutex, ll->ll_lock);
	for (;;) {
		if (ll->ll_item_count == 0)
			break;
		xt_wait_cond(self, ll->ll_cond, ll->ll_lock);
	}
	freer_(); // xt_unlock_mutex(ll->ll_lock)
}

xtPublic u_int xt_ll_get_size(XTLinkedListPtr ll)
{
	return ll->ll_item_count;
}

xtPublic void xt_init_linkedqueue(XTThreadPtr XT_UNUSED(self), XTLinkedQueuePtr lq)
{
	lq->lq_count = 0;
	lq->lq_front = NULL;
	lq->lq_back = NULL;
}

xtPublic void xt_exit_linkedqueue(XTThreadPtr XT_UNUSED(self), XTLinkedQueuePtr lq)
{
	lq->lq_count = 0;
	lq->lq_front = NULL;
	lq->lq_back = NULL;
}

xtPublic void xt_lq_add(XTThreadPtr XT_UNUSED(self), XTLinkedQueuePtr lq, XTLinkedQItemPtr qi)
{
	lq->lq_count++;
	qi->qi_next = NULL;
	if (!lq->lq_front)
		lq->lq_front = qi;
	if (lq->lq_back)
		lq->lq_back->qi_next = qi;
	lq->lq_back = qi;
}

xtPublic XTLinkedQItemPtr xt_lq_remove(XTThreadPtr XT_UNUSED(self), XTLinkedQueuePtr lq)
{
	XTLinkedQItemPtr qi = NULL;

	if (!lq->lq_front) {
		qi = lq->lq_front;
		lq->lq_front = qi->qi_next;
		if (!lq->lq_front)
			lq->lq_back = NULL;
		qi->qi_next = NULL;
	}
	return qi;
}

