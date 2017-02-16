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

#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."


#include "test.h"
#include <toku_time.h>

static const char *fname = TOKU_TEST_FILENAME;

static const enum toku_compression_method compression_method = TOKU_DEFAULT_COMPRESSION_METHOD;

static TOKUTXN const null_txn = 0;

static void test3 (int nodesize, int basementnodesize, int count) {
    FT_HANDLE t;
    int r;
    struct timeval t0,t1;
    int i;
    CACHETABLE ct;
    
    toku_cachetable_create(&ct, 0, ZERO_LSN, nullptr);
    gettimeofday(&t0, 0);
    unlink(fname);
    r = toku_open_ft_handle(fname, 1, &t, nodesize, basementnodesize, compression_method, ct, null_txn, toku_builtin_compare_fun);
    assert(r==0);
    for (i=0; i<count; i++) {
	char key[100],val[100];
	DBT k,v;
	snprintf(key,100,"hello%d",i);
	snprintf(val,100,"there%d",i);
	toku_ft_insert(t, toku_fill_dbt(&k, key, 1+strlen(key)), toku_fill_dbt(&v, val, 1+strlen(val)), null_txn);
    }
    r = toku_verify_ft(t); assert(r==0);
    r = toku_close_ft_handle_nolsn(t, 0);        assert(r==0);
    toku_cachetable_close(&ct);
    
    gettimeofday(&t1, 0);
    {
	double diff = toku_tdiff(&t1, &t0);
	if (verbose) printf("serial insertions: blocksize=%d %d insertions in %.3f seconds, %.2f insertions/second\n", nodesize, count, diff, count/diff);
    }
}

static void ft_blackbox_test (void) {
    if (verbose) printf("test3 slow\n");

    test3(2048, 512, 1<<15);
    if (verbose) printf("test3 fast\n");

    //if (verbose) toku_pma_show_stats();

    test3(1<<15, 1<<12, 1024);
    if (verbose) printf("test3 fast\n");

    test3(1<<18, 1<<15, 1<<20);


//    test3(1<<19, 1<<16, 1<<20);

//    test3(1<<20, 1<<17, 1<<20);

//    test3(1<<20, 1<<17, 1<<21);

//    test3(1<<20, 1<<17, 1<<22);

}

int
test_main (int argc , const char *argv[]) {
    default_parse_args(argc, argv);

    ft_blackbox_test();
    
    if (verbose) printf("test ok\n");
    return 0;
}
