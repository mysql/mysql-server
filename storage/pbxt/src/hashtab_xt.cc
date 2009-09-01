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
 * 2005-01-15	Paul McCullagh
 *
 */

#include "xt_config.h"

#include <ctype.h>

#include "pthread_xt.h"
#include "heap_xt.h"
#include "thread_xt.h"
#include "hashtab_xt.h"

XTHashTabPtr xt_new_hashtable(XTThreadPtr self, XTHTCompareFunc comp_func, XTHTHashFunc hash_func, XTHTFreeFunc free_func, xtBool with_lock, xtBool with_cond)
{
	XTHashTabPtr	ht;
	xtHashValue		tab_size = 223;

	ht = (XTHashTabPtr) xt_calloc(self, offsetof(XTHashTabRec, ht_items) + (sizeof(XTHashItemPtr) * tab_size));
	ht->ht_comp_func = comp_func;
	ht->ht_hash_func = hash_func;
	ht->ht_free_func = free_func;
	ht->ht_tab_size = tab_size;

	if (with_lock || with_cond) {
		ht->ht_lock = (xt_mutex_type *) xt_calloc(self, sizeof(xt_mutex_type));
		try_(a) {
			xt_init_mutex_with_autoname(self, ht->ht_lock);
		}
		catch_(a) {
			xt_free(self, ht->ht_lock);
			xt_free(self, ht);
			throw_();
		}
		cont_(a);
	}

	if (with_cond) {
		ht->ht_cond = (xt_cond_type *) xt_calloc(self, sizeof(xt_cond_type));
		try_(b) {
			xt_init_cond(self, ht->ht_cond);
		}
		catch_(b) {
			xt_free(self, ht->ht_cond);
			ht->ht_cond = NULL;
			xt_free_hashtable(self, ht);
			throw_();
		}
		cont_(b);
	}

	return ht;
}

void xt_free_hashtable(XTThreadPtr self, XTHashTabPtr ht)
{
	xtHashValue		i;
	XTHashItemPtr	item, tmp_item;

	if (ht->ht_lock)
		xt_lock_mutex(self, ht->ht_lock);
	for (i=0; i<ht->ht_tab_size; i++) {
		item = ht->ht_items[i];
		while (item) {
			if (ht->ht_free_func)
				(*ht->ht_free_func)(self, item->hi_data);
			tmp_item = item;
			item = item->hi_next;
			xt_free(self, tmp_item);
		}
	}
	if (ht->ht_lock)
		xt_unlock_mutex(self, ht->ht_lock);
	if (ht->ht_lock) {
		xt_free_mutex(ht->ht_lock);
		xt_free(self, ht->ht_lock);
	}
	if (ht->ht_cond) {
		xt_free_cond(ht->ht_cond);
		xt_free(self, ht->ht_cond);
	}
	xt_free(self, ht);
}

xtPublic void xt_ht_put(XTThreadPtr self, XTHashTabPtr ht, void *data)
{
	XTHashItemPtr	item = NULL;
	xtHashValue		h;

	pushr_(ht->ht_free_func, data);
	h = (*ht->ht_hash_func)(FALSE, data);
	item = (XTHashItemPtr) xt_malloc(self, sizeof(XTHashItemRec));
	item->hi_data = data;
	item->hi_hash = h;
	item->hi_next = ht->ht_items[h % ht->ht_tab_size];
	ht->ht_items[h % ht->ht_tab_size] = item;
	popr_();
}

xtPublic void *xt_ht_get(XTThreadPtr XT_UNUSED(self), XTHashTabPtr ht, void *key)
{
	XTHashItemPtr	item;
	xtHashValue		h;
	void			*data = NULL;

	h = (*ht->ht_hash_func)(TRUE, key);

	item = ht->ht_items[h % ht->ht_tab_size];
	while (item) {
		if (item->hi_hash == h && (*ht->ht_comp_func)(key, item->hi_data)) {
			data = item->hi_data;
			break;
		}
		item = item->hi_next;
	}
	
	return data;
}

xtPublic xtBool xt_ht_del(XTThreadPtr self, XTHashTabPtr ht, void *key)
{
	XTHashItemPtr	item, pitem = NULL;
	xtHashValue		h;
	xtBool			found = FALSE;

	h = (*ht->ht_hash_func)(TRUE, key);

	item = ht->ht_items[h % ht->ht_tab_size];
	while (item) {
		if (item->hi_hash == h && (*ht->ht_comp_func)(key, item->hi_data)) {
			void *data;

			found = TRUE;
			data = item->hi_data;
			
			/* Unlink the item: */
			if (pitem)
				pitem->hi_next = item->hi_next;
			else
				ht->ht_items[h % ht->ht_tab_size] = item->hi_next;

			/* Free the item: */
			xt_free(self, item);

			/* Free the data */
			if (ht->ht_free_func)
				(*ht->ht_free_func)(self, data);
			break;
		}
		pitem = item;
		item = item->hi_next;
	}
	
	return found;
}

xtPublic xtHashValue xt_ht_hash(char *s)
{
	register char *p;
	register xtHashValue h = 0, g;
	
	p = s; 
	while (*p) {
		h = (h << 4) + *p;
		/* Assignment intended here! */
		if ((g = h & 0xF0000000)) {
			h = h ^ (g >> 24);
			h = h ^ g;
		}
		p++;
	}
	return h;
}

/*
 * The case-insensitive version of the hash...
 */
xtPublic xtHashValue xt_ht_casehash(char *s)
{
	register char *p;
	register xtHashValue h = 0, g;
	
	p = s; 
	while (*p) {
		h = (h << 4) + tolower(*p);
		/* Assignment intended here! */
		if ((g = h & 0xF0000000)) {
			h = h ^ (g >> 24);
			h = h ^ g;
		}
		p++;
	}
	return h;
}

xtPublic xtBool xt_ht_lock(XTThreadPtr self, XTHashTabPtr ht)
{
	if (ht->ht_lock)
		return xt_lock_mutex(self, ht->ht_lock);
	return TRUE;
}

xtPublic void xt_ht_unlock(XTThreadPtr self, XTHashTabPtr ht)
{
	if (ht->ht_lock)
		xt_unlock_mutex(self, ht->ht_lock);
}

xtPublic void xt_ht_wait(XTThreadPtr self, XTHashTabPtr ht)
{
	xt_wait_cond(self, ht->ht_cond, ht->ht_lock);
}

xtPublic void xt_ht_timed_wait(XTThreadPtr self, XTHashTabPtr ht, u_long milli_sec)
{
	xt_timed_wait_cond(self, ht->ht_cond, ht->ht_lock, milli_sec);
}

xtPublic void xt_ht_signal(XTThreadPtr self, XTHashTabPtr ht)
{
	xt_signal_cond(self, ht->ht_cond);
}

xtPublic void xt_ht_enum(struct XTThread *XT_UNUSED(self), XTHashTabPtr ht, XTHashEnumPtr en)
{
	en->he_i = 0;
	en->he_item = NULL;
	en->he_ht = ht;
}

xtPublic void *xt_ht_next(struct XTThread *XT_UNUSED(self), XTHashEnumPtr en)
{
	if (en->he_item) {
		en->he_item = en->he_item->hi_next;
		if (en->he_item)
			return en->he_item->hi_data;
		en->he_i++;
	}
	while (en->he_i < en->he_ht->ht_tab_size) {
		if ((en->he_item = en->he_ht->ht_items[en->he_i]))
			return en->he_item->hi_data;
		en->he_i++;
	}
	return NULL;
}

