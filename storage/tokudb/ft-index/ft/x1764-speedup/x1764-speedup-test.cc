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

#ident "Copyright (c) 2011-2013 Tokutek Inc.  All rights reserved."

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <sys/types.h>

uint64_t x1764_simple (const uint64_t *buf, size_t len)
{
    uint64_t sum=0;
    for (size_t i=0; i<len ;i++) {
	sum = sum*17 + buf[i];
    }
    return sum;
}

uint64_t x1764_2x (const uint64_t *buf, size_t len)
{
    assert(len%2==0);
    uint64_t suma=0, sumb=0;
    for (size_t i=0; i<len ;i+=2) {
	suma = suma*(17L*17L) + buf[i];
	sumb = sumb*(17L*17L) + buf[i+1];
    }
    return suma*17+sumb;
}

uint64_t x1764_3x (const uint64_t *buf, size_t len)
{
    assert(len%3==0);
    uint64_t suma=0, sumb=0, sumc=0;
    for (size_t i=0; i<len ;i+=3) {
	suma = suma*(17LL*17LL*17LL) + buf[i];
	sumb = sumb*(17LL*17LL*17LL) + buf[i+1];
	sumc = sumc*(17LL*17LL*17LL) + buf[i+2];
    }
    uint64_t r = suma*17L*17L + sumb*17L + sumc;
    return r;
}

uint64_t x1764_4x (const uint64_t *buf, size_t len)
{
    assert(len%4==0);
    uint64_t suma=0, sumb=0, sumc=0, sumd=0;
    for (size_t i=0; i<len ;i+=4) {
	suma = suma*(17LL*17LL*17LL*17LL) + buf[i];
	sumb = sumb*(17LL*17LL*17LL*17LL) + buf[i+1];
	sumc = sumc*(17LL*17LL*17LL*17LL) + buf[i+2];
	sumd = sumd*(17LL*17LL*17LL*17LL) + buf[i+3];
    }
    return suma*17L*17L*17L + sumb*17L*17L + sumc*17L + sumd;

}

float tdiff (struct timeval *start, struct timeval *end) {
    return (end->tv_sec-start->tv_sec) +1e-6*(end->tv_usec - start->tv_usec);
}

int main (int argc, char *argv[]) {
    int size = 1024*1024*4 + 8*4;
    char *data = malloc(size);
    for (int j=0; j<4; j++) {
	struct timeval start,end,end2,end3,end4;
	for (int i=0; i<size; i++) data[i]=i*i+j;
	gettimeofday(&start, 0);
	uint64_t s = x1764_simple((uint64_t*)data, size/sizeof(uint64_t));
	gettimeofday(&end,   0);
	uint64_t s2 = x1764_2x((uint64_t*)data, size/sizeof(uint64_t));
	gettimeofday(&end2,   0);
	uint64_t s3 = x1764_3x((uint64_t*)data, size/sizeof(uint64_t));
	gettimeofday(&end3,   0);
	uint64_t s4 = x1764_4x((uint64_t*)data, size/sizeof(uint64_t));
	gettimeofday(&end4,   0);
	assert(s==s2);
	assert(s==s3);
	assert(s==s4);
	double b1 = tdiff(&start, &end);
	double b2 = tdiff(&end, &end2);
	double b3 = tdiff(&end2, &end3);
	double b4 = tdiff(&end3, &end4);
	printf("s=%016llx t=%.6fs %.6fs (%4.2fx), %.6fs (%4.2fx), %.6fs (%4.2fx) [%5.2f MB/s]\n",
	       (unsigned long long)s,
	       b1, b2, b1/b2, b3, b1/b3, b4, b1/b4, (size/b4)/(1024*1024));
    }
    return 0;
}
