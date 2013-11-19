/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuDB, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "key.h"
#include "fttypes.h"
#include <memory.h>

#if 0
int toku_keycompare (bytevec key1b, ITEMLEN key1len, bytevec key2b, ITEMLEN key2len) {
    const unsigned char *key1 = key1b;
    const unsigned char *key2 = key2b;
    while (key1len > 0 && key2len > 0) {
	unsigned char b1 = key1[0];
	unsigned char b2 = key2[0];
	if (b1<b2) return -1;
	if (b1>b2) return 1;
	key1len--; key1++;
	key2len--; key2++;
    }
    if (key1len<key2len) return -1;
    if (key1len>key2len) return 1;
    return 0;
}

#elif 0
int toku_keycompare (bytevec key1, ITEMLEN key1len, bytevec key2, ITEMLEN key2len) {
    if (key1len==key2len) {
	return memcmp(key1,key2,key1len);
    } else if (key1len<key2len) {
	int r = memcmp(key1,key2,key1len);
	if (r<=0) return -1; /* If the keys are the same up to 1's length, then return -1, since key1 is shorter than key2. */
	else return 1;
    } else {
	return -toku_keycompare(key2,key2len,key1,key1len);
    }
}
#elif 0

int toku_keycompare (bytevec key1, ITEMLEN key1len, bytevec key2, ITEMLEN key2len) {
    if (key1len==key2len) {
	return memcmp(key1,key2,key1len);
    } else if (key1len<key2len) {
	int r = memcmp(key1,key2,key1len);
	if (r<=0) return -1; /* If the keys are the same up to 1's length, then return -1, since key1 is shorter than key2. */
	else return 1;
    } else {
	int r = memcmp(key1,key2,key2len);
	if (r>=0) return 1; /* If the keys are the same up to 2's length, then return 1 since key1 is longer than key2 */
	else return -1;
    }
}
#elif 0
/* This one looks tighter, but it does use memcmp... */
int toku_keycompare (bytevec key1, ITEMLEN key1len, bytevec key2, ITEMLEN key2len) {
    int comparelen = key1len<key2len ? key1len : key2len;
    const unsigned char *k1;
    const unsigned char *k2;
    for (k1=key1, k2=key2;
	 comparelen>0;
	 k1++, k2++, comparelen--) {
	if (*k1 != *k2) {
	    return (int)*k1-(int)*k2;
	}
    }
    if (key1len<key2len) return -1;
    if (key1len>key2len) return 1;
    return 0;
}
#else
/* unroll that one four times */
// when a and b are chars, return a-b is safe here because return type is int.  No over/underflow possible.
int toku_keycompare (bytevec key1, ITEMLEN key1len, bytevec key2, ITEMLEN key2len) {
    int comparelen = key1len<key2len ? key1len : key2len;
    const unsigned char *k1;
    const unsigned char *k2;
    for (CAST_FROM_VOIDP(k1, key1), CAST_FROM_VOIDP(k2, key2);
	 comparelen>4;
	 k1+=4, k2+=4, comparelen-=4) {
	{ int v1=k1[0], v2=k2[0]; if (v1!=v2) return v1-v2; }
	{ int v1=k1[1], v2=k2[1]; if (v1!=v2) return v1-v2; }
	{ int v1=k1[2], v2=k2[2]; if (v1!=v2) return v1-v2; }
	{ int v1=k1[3], v2=k2[3]; if (v1!=v2) return v1-v2; }
    }
    for (;
	 comparelen>0;
	 k1++, k2++, comparelen--) {
	if (*k1 != *k2) {
	    return (int)*k1-(int)*k2;
	}
    }
    if (key1len<key2len) return -1;
    if (key1len>key2len) return 1;
    return 0;
}

#endif

int
toku_builtin_compare_fun (DB *db __attribute__((__unused__)), const DBT *a, const DBT*b) {
    return toku_keycompare(a->data, a->size, b->data, b->size);
}
