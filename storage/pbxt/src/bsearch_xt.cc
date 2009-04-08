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
 * 2004-01-03	Paul McCullagh
 *
 * H&G2JCtL
 */

#include "xt_config.h"

#include <stdio.h>

#include "bsearch_xt.h"
#include "pthread_xt.h"
#include "thread_xt.h"

/**
 * Binary search a array of 'count' items, with byte size 'size'. This
 * function returns a pointer to the element and the 'index'
 * of the element if found.
 *
 * If not found the index of the insert point of the item
 * is returned (0 <= index <= count).
 *
 * The comparison routine 'compar' may throw an exception.
 * In this case the error details will be stored in 'thread'.
 */
void *xt_bsearch(XTThreadPtr thread, const void *key, register const void *base, size_t count, size_t size, size_t *idx, const void *thunk, XTCompareFunc compar)
{
	register size_t		i;
	register size_t		guess;
	register int		r;

	i = 0;
	while (i < count) {
		guess = (i + count - 1) >> 1;
		r = (compar)(thread, thunk, key, ((char *) base) + guess * size);
		if (r == 0) {
			*idx = guess;
			return ((char *) base) + guess * size;
		}
		if (r < 0)
			count = guess;
		else
			i = guess + 1;
	}

	*idx = i;
	return NULL;
}

