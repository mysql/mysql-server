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
#include <toku_portability.h>
#include "toku_os.h"
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <toku_assert.h>
#include <toku_pthread.h>
#include "util/queue.h"

static int verbose=1;

static int count_0 = 0;
static uint64_t e_max_weight=0, d_max_weight = 0; // max weight seen by enqueue thread and dequeue thread respectively.

static void *start_0 (void *arg) {
    QUEUE q = (QUEUE)arg;
    void *item;
    uint64_t weight;
    long count = 0;
    while (1) {
	uint64_t this_max_weight;
	int r=toku_queue_deq(q, &item, &weight, &this_max_weight);
	if (r==EOF) break;
	assert(r==0);
	if (this_max_weight>d_max_weight) d_max_weight=this_max_weight;
	long v = (long)item;
	//printf("D(%ld)=%ld %ld\n", v, this_max_weight, d_max_weight);
	assert(v==count);
	count_0++;
	count++;
    }
    return NULL;
}

static void enq (QUEUE q, long v, uint64_t weight) {
    uint64_t this_max_weight;
    int r = toku_queue_enq(q, (void*)v, (weight==0)?0:1, &this_max_weight);
    assert(r==0);
    if (this_max_weight>e_max_weight) e_max_weight=this_max_weight;
    //printf("E(%ld)=%ld %ld\n", v, this_max_weight, e_max_weight);
}

static void queue_test_0 (uint64_t weight)
// Test a queue that can hold WEIGHT items.
{
    //printf("\n");
    count_0 = 0;
    e_max_weight = 0;
    d_max_weight = 0;
    QUEUE q;
    int r;
    r = toku_queue_create(&q, weight);                               assert(r==0);
    toku_pthread_t thread;
    r = toku_pthread_create(&thread, NULL, start_0, q); assert(r==0);
    enq(q, 0L, weight);
    enq(q, 1L, weight);
    enq(q, 2L, weight);
    enq(q, 3L, weight);
    sleep(1);
    enq(q, 4L, weight);
    enq(q, 5L, weight);
    r = toku_queue_eof(q);                                      assert(r==0);
    void *result;
    r = toku_pthread_join(thread, &result);	           assert(r==0);
    assert(result==NULL);
    assert(count_0==6);
    r = toku_queue_destroy(q);
    assert(d_max_weight <= weight);
    assert(e_max_weight <= weight);
}


static void parse_args (int argc, const char *argv[]) {
    const char *progname=argv[0];
    argc--; argv++;
    while (argc>0) {
	if (strcmp(argv[0],"-v")==0) {
	    verbose++;
	} else if (strcmp(argv[0],"-q")==0) {
	    verbose--;
	} else {
	    fprintf(stderr, "Usage:\n %s [-v] [-q]\n", progname);
	    exit(1);
	}
	argc--; argv++;
    }
    if (verbose<0) verbose=0;
}

int main (int argc, const char *argv[]) {
    parse_args(argc, argv);
    queue_test_0(0LL);
    queue_test_0(1LL);
    queue_test_0(2LL);
    return 0;
}
