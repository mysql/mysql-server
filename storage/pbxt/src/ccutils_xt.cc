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
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 * 2006-05-16	Paul McCullagh
 *
 * H&G2JCtL
 *
 * C++ Utilities
 */

#include "xt_config.h"

#include "pthread_xt.h"
#include "ccutils_xt.h"
#include "bsearch_xt.h"

static int ccu_compare_object(XTThreadPtr XT_UNUSED(self), register const void *XT_UNUSED(thunk), register const void *a, register const void *b)
{
	XTObject *obj_ptr = (XTObject *) b;

	return obj_ptr->compare(a);
}

void XTListImp::append(XTThreadPtr self, XTObject *info, void *key) {
	size_t idx;

	if (li_item_count == 0)
		idx = 0;
	else if (li_item_count == 1) {
		int r;

		if ((r = li_items[0]->compare(key)) == 0)
			idx = 0;
		else if (r < 0)
			idx = 0;
		else
			idx = 1;
	}
	else {
		xt_bsearch(self, key, li_items, li_item_count, sizeof(void *), &idx, NULL, ccu_compare_object);
	}

	if (!xt_realloc(NULL, (void **) &li_items, (li_item_count + 1) * sizeof(void *))) {
		if (li_referenced)
			info->release(self);
		xt_throw_errno(XT_CONTEXT, XT_ENOMEM);
		return;
	}
	memmove(&li_items[idx+1], &li_items[idx], (li_item_count-idx) * sizeof(void *));
	li_items[idx] = info;
	li_item_count++;
}


