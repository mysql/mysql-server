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
 * H&G2JCtL
 */
#ifndef __xt_hashtab_h__
#define __xt_hashtab_h__

#include "xt_defs.h"

struct XTThread;

#define xtHashValue			u_int

typedef xtBool (*XTHTCompareFunc)(void *key, void *data);
typedef xtHashValue (*XTHTHashFunc)(xtBool is_key, void *key_data);
typedef void (*XTHTFreeFunc)(struct XTThread *self, void *item);

typedef struct XTHashItem {
	struct XTHashItem		*hi_next;
	xtHashValue				hi_hash;
	void					*hi_data;
} XTHashItemRec, *XTHashItemPtr;

typedef struct XTHashTab {
	XTHTCompareFunc			ht_comp_func;
	XTHTHashFunc			ht_hash_func;
	XTHTFreeFunc			ht_free_func;
	xt_mutex_type			*ht_lock;
	xt_cond_type			*ht_cond;

	xtHashValue				ht_tab_size;
	XTHashItemPtr			ht_items[XT_VAR_LENGTH];
} XTHashTabRec, *XTHashTabPtr;

typedef struct XTHashEnum {
	u_int					he_i;
	XTHashItemPtr			he_item;
	XTHashTabPtr			he_ht;
} XTHashEnumRec, *XTHashEnumPtr;

XTHashTabPtr	xt_new_hashtable(struct XTThread *self, XTHTCompareFunc comp_func, XTHTHashFunc hash_func, XTHTFreeFunc free_func, xtBool with_lock, xtBool with_cond);
void			xt_free_hashtable(struct XTThread *self, XTHashTabPtr ht);

void			xt_ht_put(struct XTThread *self, XTHashTabPtr ht, void *data);
void			*xt_ht_get(struct XTThread *self, XTHashTabPtr ht, void *key);
xtBool			xt_ht_del(struct XTThread *self, XTHashTabPtr ht, void *key);

xtHashValue		xt_ht_hash(char *s);
xtHashValue		xt_ht_casehash(char *s);

xtBool			xt_ht_lock(struct XTThread *self, XTHashTabPtr ht);
void			xt_ht_unlock(struct XTThread *self, XTHashTabPtr ht);
void			xt_ht_wait(struct XTThread *self, XTHashTabPtr ht);
void			xt_ht_timed_wait(struct XTThread *self, XTHashTabPtr ht, u_long milli_sec);
void			xt_ht_signal(struct XTThread *self, XTHashTabPtr ht);

void			xt_ht_enum(struct XTThread *self, XTHashTabPtr ht, XTHashEnumPtr en);
void			*xt_ht_next(struct XTThread *self, XTHashEnumPtr en);

#endif
