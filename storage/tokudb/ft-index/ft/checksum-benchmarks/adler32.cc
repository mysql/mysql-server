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
#include <toku_assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/time.h>
#include <zlib.h>
#include <openssl/md2.h>
#include <openssl/md4.h>
#include <openssl/md5.h>

const unsigned int prime = 2000000011;

unsigned int karprabin (unsigned char *datac, int N) {
    assert(N%4==0);
    unsigned int *data=(unsigned int*)datac;
    N=N/4;
    int i;
    unsigned int result=0;
    for (i=0; i<N; i++) {
	result=(result*prime)+data[i];
    }
    return result;
}

// According to
//  P. L'Ecuyer, "Tables of Linear Congruential Generators of
//  Different Sizes and Good Lattice Structure", Mathematics of
//  Computation 68:225, 249--260 (1999).
// m=2^{32}-5  a=1588635695 is good.

const unsigned int mkr = 4294967291U;
const unsigned int akr = 1588635695U;


// But this is slower
unsigned int karprabinP (unsigned char *datac, int N) {
    assert(N%4==0);
    unsigned int *data=(unsigned int*)datac;
    N=N/4;
    int i;
    unsigned long long result=0;
    for (i=0; i<N; i++) {
	result=((result*akr)+data[i])%mkr;
    }
    return result;
}

float tdiff (struct timeval *start, struct timeval *end) {
    return (end->tv_sec-start->tv_sec) +1e-6*(end->tv_usec - start->tv_usec);
}

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    struct timeval start, end;
    const int N=2<<20;
    unsigned char *data=malloc(N);
    int i;
    assert(data);
    for (i=0; i<N; i++) data[i]=random();

    // adler32
    {
	uLong a32 = adler32(0L, Z_NULL, 0);
	for (i=0; i<3; i++) {
	    gettimeofday(&start, 0);
	    a32 = adler32(a32, data, N);
	    gettimeofday(&end,   0);
	    float tm = tdiff(&start, &end);
	    printf("adler32=%lu, time=%9.6fs %9.6fns/b\n", a32, tm, 1e9*tm/N);
	}
    }

    // crc32
    {
	uLong c32 = crc32(0L, Z_NULL, 0);
	for (i=0; i<3; i++) {
	    gettimeofday(&start, 0);
	    c32 = crc32(c32, data, N);
	    gettimeofday(&end,   0);
	    float tm = tdiff(&start, &end);
	    printf("crc32=%lu, time=%9.6fs %9.6fns/b\n", c32, tm, 1e9*tm/N);
	}
    }

    // MD2
    {
	unsigned char buf[MD2_DIGEST_LENGTH];
	int j;
	for (i=0; i<3; i++) {
	    gettimeofday(&start, 0);
	    MD2(data, N, buf);
	    gettimeofday(&end,   0);
	    float tm = tdiff(&start, &end);
	    printf("md2=");
	    for (j=0; j<MD2_DIGEST_LENGTH; j++) {
		printf("%02x", buf[j]);
	    }
	    printf(" time=%9.6fs %9.6fns/b\n", tm, 1e9*tm/N);
	}
    }

    // MD4
    {
	unsigned char buf[MD4_DIGEST_LENGTH];
	int j;
	for (i=0; i<3; i++) {
	    gettimeofday(&start, 0);
	    MD4(data, N, buf);
	    gettimeofday(&end,   0);
	    float tm = tdiff(&start, &end);
	    printf("md4=");
	    for (j=0; j<MD4_DIGEST_LENGTH; j++) {
		printf("%02x", buf[j]);
	    }
	    printf(" time=%9.6fs %9.6fns/b\n", tm, 1e9*tm/N);
	}
    }

    // MD5
    {
	unsigned char buf[MD5_DIGEST_LENGTH];
	int j;
	for (i=0; i<3; i++) {
	    gettimeofday(&start, 0);
	    MD5(data, N, buf);
	    gettimeofday(&end,   0);
	    float tm = tdiff(&start, &end);
	    printf("md5=");
	    for (j=0; j<MD5_DIGEST_LENGTH; j++) {
		printf("%02x", buf[j]);
	    }
	    printf(" time=%9.6fs %9.6fns/b\n", tm, 1e9*tm/N);
	}
    }

    // karp rabin
    {
	for (i=0; i<3; i++) {
	    gettimeofday(&start, 0);
	    unsigned int kr = karprabin(data, N);
	    gettimeofday(&end,   0);
	    float tm = tdiff(&start, &end);
	    printf("kr=%ud time=%9.6fs %9.6fns/b\n", kr, tm, 1e9*tm/N);
	}
    }
    free(data);
    return 0;
}
