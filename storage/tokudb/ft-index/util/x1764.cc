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

  TokuFT, Tokutek Fractal Tree Indexing Library.
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

#include <toku_stdlib.h>
#include <portability/toku_portability.h>

#include "x1764.h"

#define PRINT 0

uint32_t toku_x1764_memory_simple (const void *buf, int len)
{
    const uint64_t *CAST_FROM_VOIDP(lbuf, buf);
    uint64_t c=0;
    while (len>=8) {
	c = c*17 + *lbuf;
	if (PRINT) printf("%d: c=%016" PRIx64 " sum=%016" PRIx64 "\n", __LINE__, *lbuf, c);
	lbuf++;
	len-=8;
    }
    if (len>0) {
	const uint8_t *cbuf=(uint8_t*)lbuf;
	int i;
	uint64_t input=0;
	for (i=0; i<len; i++) {
	    input |= ((uint64_t)(cbuf[i]))<<(8*i);
	}
	c = c*17 + input;
    }
    return ~((c&0xFFFFFFFF) ^ (c>>32));
}

uint32_t toku_x1764_memory (const void *vbuf, int len)
{
    const uint8_t *CAST_FROM_VOIDP(buf, vbuf);
    int len_4_words = 4*sizeof(uint64_t);
    uint64_t suma=0, sumb=0, sumc=0, sumd=0;
    while (len >= len_4_words) {
	suma = suma*(17LL*17LL*17LL*17LL) + *(uint64_t*)(buf +0*sizeof(uint64_t));
	sumb = sumb*(17LL*17LL*17LL*17LL) + *(uint64_t*)(buf +1*sizeof(uint64_t));
	sumc = sumc*(17LL*17LL*17LL*17LL) + *(uint64_t*)(buf +2*sizeof(uint64_t));
	sumd = sumd*(17LL*17LL*17LL*17LL) + *(uint64_t*)(buf +3*sizeof(uint64_t));
	buf += len_4_words;
	len -= len_4_words;
    }
    uint64_t sum = suma*17L*17L*17L + sumb*17L*17L + sumc*17L + sumd;
    assert(len>=0);
    while ((uint64_t)len>=sizeof(uint64_t)) {
	sum = sum*17 + *(uint64_t*)buf;
	buf+=sizeof(uint64_t);
	len-=sizeof(uint64_t);
    }
    if (len>0) {
	uint64_t tailsum = 0;
	for (int i=0; i<len; i++) {
	    tailsum |= ((uint64_t)(buf[i]))<<(8*i);
	}
	sum = sum*17 + tailsum;
    }
    return ~((sum&0xFFFFFFFF) ^ (sum>>32));
}


void toku_x1764_init(struct x1764 *l) {
    l->sum=0;
    l->input=0;
    l->n_input_bytes=0;
}

void toku_x1764_add (struct x1764 *l, const void *vbuf, int len) {
    if (PRINT) printf("%d: n_input_bytes=%d len=%d\n", __LINE__, l->n_input_bytes, len);
    int n_input_bytes = l->n_input_bytes;
    const unsigned char *CAST_FROM_VOIDP(cbuf, vbuf);
    // Special case short inputs
    if (len==1) {
	uint64_t input = l->input | ((uint64_t)(*cbuf))<<(8*n_input_bytes);
	n_input_bytes++;
	if (n_input_bytes==8) {
	    l->sum = l->sum*17 + input;
	    l->n_input_bytes = 0;
	    l->input = 0;
	} else {
	    l->input = input;
	    l->n_input_bytes = n_input_bytes;
	}
	return;
    } else if (len==2) {
	uint64_t input = l->input;
	uint64_t thisv = ((uint64_t)(*(uint16_t*)cbuf));
	if (n_input_bytes==7) {
	    l->sum = l->sum*17 + (input | (thisv<<(8*7)));
	    l->input = thisv>>8;
	    l->n_input_bytes = 1;
	} else if (n_input_bytes==6) {
	    l->sum = l->sum*17 + (input | (thisv<<(8*6)));
	    l->input = 0;
	    l->n_input_bytes = 0;
	} else {
	    l->input = input | (thisv<<(8*n_input_bytes));
	    l->n_input_bytes += 2;
	}
	return;
    }

    uint64_t sum;
    //assert(len>=0);
    if (n_input_bytes) {
	uint64_t input = l->input;
	if (len>=8) {
	    sum = l->sum;
	    while (len>=8) {
		uint64_t thisv = *(uint64_t*)cbuf;
		input |= thisv<<(8*n_input_bytes);
		sum = sum*17 + input;
		if (PRINT) printf("%d: input=%016" PRIx64 " sum=%016" PRIx64 "\n", __LINE__, input, sum);
		input = thisv>>(8*(8-n_input_bytes));
		if (PRINT) printf("%d: input=%016" PRIx64 "\n", __LINE__, input);
		len-=8;
		cbuf+=8;
		// n_input_bytes remains unchanged
		if (PRINT) printf("%d: n_input_bytes=%d len=%d\n", __LINE__, l->n_input_bytes, len);
	    }
	    l->sum = sum;
	}
	if (len>=4) {
	    uint64_t thisv = *(uint32_t*)cbuf;
	    if (n_input_bytes<4) {
		input |= thisv<<(8*n_input_bytes);
		if (PRINT) printf("%d: input=%016" PRIx64 "\n", __LINE__, input);
		n_input_bytes+=4;
	    } else {
		input |= thisv<<(8*n_input_bytes);
		l->sum = l->sum*17 + input;
		if (PRINT) printf("%d: input=%016" PRIx64 " sum=%016" PRIx64 "\n", __LINE__, input, l->sum);
		input = thisv>>(8*(8-n_input_bytes));
		n_input_bytes-=4;
		if (PRINT) printf("%d: input=%016" PRIx64 " n_input_bytes=%d\n", __LINE__, input, n_input_bytes);
	    }
	    len-=4;
	    cbuf+=4;
	    if (PRINT) printf("%d: len=%d\n", __LINE__, len);
	}
	//assert(n_input_bytes<=8);
	while (n_input_bytes<8 && len) {
	    input |= ((uint64_t)(*cbuf))<<(8*n_input_bytes);
	    n_input_bytes++;
	    cbuf++;
	    len--;
	}
	//assert(len>=0);
	if (n_input_bytes<8) {
	    //assert(len==0);
	    l->input = input;
	    l->n_input_bytes = n_input_bytes;
	    if (PRINT) printf("%d: n_input_bytes=%d\n", __LINE__, l->n_input_bytes);
	    return;
	}
	sum = l->sum*17 + input;
    } else {
	//assert(len>=0);
	sum = l->sum;
    }
    //assert(len>=0);
    while (len>=8) {
	sum = sum*17 + *(uint64_t*)cbuf;
	cbuf+=8;
	len -=8;
    }
    l->sum = sum;
    n_input_bytes = 0;
    uint64_t input;
    l->n_input_bytes = len;
    // Surprisingly, the loop is the fastest on bradley's laptop.
    if (1) {
	int i;
	input=0;
	for (i=0; i<len; i++) {
	    input |= ((uint64_t)(cbuf[i]))<<(8*i);
	}
    } else if (0) {
	switch (len) {
	case 7: input = ((uint64_t)(*(uint32_t*)(cbuf))) | (((uint64_t)(*(uint16_t*)(cbuf+4)))<<32) | (((uint64_t)(*(cbuf+4)))<<48); break;
	case 6: input = ((uint64_t)(*(uint32_t*)(cbuf))) | (((uint64_t)(*(uint16_t*)(cbuf+4)))<<32); break;
	case 5: input = ((uint64_t)(*(uint32_t*)(cbuf))) | (((uint64_t)(*(cbuf+4)))<<32); break;
	case 4: input = ((uint64_t)(*(uint32_t*)(cbuf))); break;
	case 3: input = ((uint64_t)(*(uint16_t*)(cbuf))) | (((uint64_t)(*(cbuf+2)))<<16); break;
	case 2: input = ((uint64_t)(*(uint16_t*)(cbuf))); break;
	case 1: input = ((uint64_t)(*cbuf)); break;
	case 0: input = 0;                      break;
	default: abort();
	}
    } else {
	input=0;
	int i=0;
	if (len>=4) { input  = ((uint64_t)(*(uint32_t*)(cbuf)));        cbuf+=4; len-=4; i=4;}
	if (len>=2) { input |= ((uint64_t)(*(uint16_t*)(cbuf)))<<(i*8); cbuf+=2; len-=2; i+=2; }
	if (len>=1) { input |= ((uint64_t)(*(uint8_t *)(cbuf)))<<(i*8); /*cbuf+=1; len-=1; i++;*/ }
    }
    l->input = input;
    if (PRINT) printf("%d: n_input_bytes=%d\n", __LINE__, l->n_input_bytes);
}
uint32_t toku_x1764_finish (struct x1764 *l) {
    if (PRINT) printf("%d: n_input_bytes=%d\n", __LINE__, l->n_input_bytes);
    int len = l->n_input_bytes;
    if (len>0) {
	l->sum = l->sum*17 + l->input;
    }
    return ~((l->sum &0xffffffff) ^ (l->sum>>32));
}
