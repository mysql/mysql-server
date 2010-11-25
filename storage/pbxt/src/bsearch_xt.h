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
#ifndef __xt_bsearch_h__
#define __xt_bsearch_h__

#include "xt_defs.h"

struct XTThread;

void *xt_bsearch(struct XTThread *self, const void *key, register const void *base, size_t count, size_t size, size_t *idx, const void *thunk, XTCompareFunc compar);

#endif
