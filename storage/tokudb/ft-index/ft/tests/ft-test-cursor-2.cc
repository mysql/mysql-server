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

#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."


#include "test.h"

static const char *fname = TOKU_TEST_FILENAME;

static TOKUTXN const null_txn = 0;

static int
save_data (ITEMLEN UU(keylen), bytevec UU(key), ITEMLEN vallen, bytevec val, void *v, bool lock_only) {
    if (lock_only) return 0;
    assert(key!=NULL);
    void **CAST_FROM_VOIDP(vp, v);
    *vp = toku_memdup(val, vallen);
    return 0;
}


// Verify that different cursors return different data items when a DBT is initialized to all zeros (no flags)
// Note: The BRT test used to implement DBTs with per-cursor allocated space, but there isn't any such thing any more
// so this test is a little bit obsolete.
static void test_multiple_ft_cursor_dbts(int n) {
    if (verbose) printf("test_multiple_ft_cursors:%d\n", n);

    int r;
    CACHETABLE ct;
    FT_HANDLE ft;
    FT_CURSOR cursors[n];

    unlink(fname);

    toku_cachetable_create(&ct, 0, ZERO_LSN, NULL_LOGGER);

    r = toku_open_ft_handle(fname, 1, &ft, 1<<12, 1<<9, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun);
    assert(r==0);

    int i;
    for (i=0; i<n; i++) {
	DBT kbt,vbt;
	char key[10],val[10];
	snprintf(key, sizeof key, "k%04d", i);
	snprintf(val, sizeof val, "v%04d", i);
	toku_ft_insert(ft,
                       toku_fill_dbt(&kbt, key, 1+strlen(key)),
                       toku_fill_dbt(&vbt, val, 1+strlen(val)),
                       0);
    }

    for (i=0; i<n; i++) {
        r = toku_ft_cursor(ft, &cursors[i], NULL, false, false);
        assert(r == 0);
    }

    void *ptrs[n];
    for (i=0; i<n; i++) {
	DBT kbt;
	char key[10];
	snprintf(key, sizeof key, "k%04d", i);
	r = toku_ft_cursor_get(cursors[i],
				toku_fill_dbt(&kbt, key, 1+strlen(key)),
				save_data,
				&ptrs[i],
				DB_SET);
	assert(r == 0);
    }

    for (i=0; i<n; i++) {
	int j;
	for (j=i+1; j<n; j++) {
	    assert(strcmp((char*)ptrs[i],(char*)ptrs[j])!=0);
	}
    }

    for (i=0; i<n; i++) {
        toku_ft_cursor_close(cursors[i]);
        assert(r == 0);
	toku_free(ptrs[i]);
    }

    r = toku_close_ft_handle_nolsn(ft, 0);
    assert(r==0);

    toku_cachetable_close(&ct);
}

static void test_ft_cursor(void) {
    test_multiple_ft_cursor_dbts(1);
    test_multiple_ft_cursor_dbts(2);
    test_multiple_ft_cursor_dbts(3);
}


int
test_main (int argc , const char *argv[]) {
    default_parse_args(argc, argv);
    test_ft_cursor();
    if (verbose) printf("test ok\n");
    return 0;
}
