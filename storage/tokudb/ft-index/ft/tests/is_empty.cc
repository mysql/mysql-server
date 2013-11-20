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

#ident "Copyright (c) 2010-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "test.h"

#include "toku_os.h"
#include "checkpoint.h"


#define FILENAME "test0.ft"

static void test_it (int N) {
    FT_HANDLE brt;
    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU);                                                                    CKERR(r);

    TOKULOGGER logger;
    r = toku_logger_create(&logger);                                                                        CKERR(r);
    r = toku_logger_open(TOKU_TEST_FILENAME, logger);                                                                  CKERR(r);


    CACHETABLE ct;
    toku_cachetable_create(&ct, 0, ZERO_LSN, logger);
    toku_cachetable_set_env_dir(ct, TOKU_TEST_FILENAME);

    toku_logger_set_cachetable(logger, ct);

    r = toku_logger_open_rollback(logger, ct, true);                                                        CKERR(r);

    TOKUTXN txn;
    r = toku_txn_begin_txn((DB_TXN*)NULL, (TOKUTXN)0, &txn, logger, TXN_SNAPSHOT_ROOT, false);                     CKERR(r);

    r = toku_open_ft_handle(FILENAME, 1, &brt, 1024, 256, TOKU_DEFAULT_COMPRESSION_METHOD, ct, txn, toku_builtin_compare_fun);                    CKERR(r);

    r = toku_txn_commit_txn(txn, false, NULL, NULL);                                 CKERR(r);
    toku_txn_close_txn(txn);
    CHECKPOINTER cp = toku_cachetable_get_checkpointer(ct);
    r = toku_checkpoint(cp, logger, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);                             CKERR(r);
    r = toku_close_ft_handle_nolsn(brt, NULL);                                                                          CKERR(r);

    unsigned int rands[N];
    for (int i=0; i<N; i++) {
	r = toku_txn_begin_txn((DB_TXN*)NULL, (TOKUTXN)0, &txn, logger, TXN_SNAPSHOT_ROOT, false);                 CKERR(r);
	r = toku_open_ft_handle(FILENAME, 0, &brt, 1024, 256, TOKU_DEFAULT_COMPRESSION_METHOD, ct, txn, toku_builtin_compare_fun);                CKERR(r);
	r = toku_txn_commit_txn(txn, false, NULL, NULL);                             CKERR(r);
	toku_txn_close_txn(txn);

	r = toku_txn_begin_txn((DB_TXN*)NULL, (TOKUTXN)0, &txn, logger, TXN_SNAPSHOT_ROOT, false);                 CKERR(r);
	char key[100],val[300];
	DBT k, v;
	rands[i] = random();
	snprintf(key, sizeof(key), "key%x.%x", rands[i], i);
	memset(val, 'v', sizeof(val));
	val[sizeof(val)-1]=0;
	toku_ft_insert(brt, toku_fill_dbt(&k, key, 1+strlen(key)), toku_fill_dbt(&v, val, 1+strlen(val)), txn);
	r = toku_txn_commit_txn(txn, false, NULL, NULL);                                 CKERR(r);
	toku_txn_close_txn(txn);


	r = toku_checkpoint(cp, logger, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);                             CKERR(r);
	r = toku_close_ft_handle_nolsn(brt, NULL);                                                                          CKERR(r);

	if (verbose) printf("i=%d\n", i);
    }
    for (int i=0; i<N; i++) {
	r = toku_txn_begin_txn((DB_TXN*)NULL, (TOKUTXN)0, &txn, logger, TXN_SNAPSHOT_ROOT, false);                     CKERR(r);
	r = toku_open_ft_handle(FILENAME, 0, &brt, 1024, 256, TOKU_DEFAULT_COMPRESSION_METHOD, ct, txn, toku_builtin_compare_fun);                CKERR(r);
	r = toku_txn_commit_txn(txn, false, NULL, NULL);                                 CKERR(r);
	toku_txn_close_txn(txn);

	r = toku_txn_begin_txn((DB_TXN*)NULL, (TOKUTXN)0, &txn, logger, TXN_SNAPSHOT_ROOT, false);                     CKERR(r);
	char key[100];
	DBT k;
	snprintf(key, sizeof(key), "key%x.%x", rands[i], i);
	toku_ft_delete(brt, toku_fill_dbt(&k, key, 1+strlen(key)), txn);

	if (0) {
	bool is_empty;
        is_empty = toku_ft_is_empty_fast(brt);
	assert(!is_empty);
	}
	
	r = toku_txn_commit_txn(txn, false, NULL, NULL);                                 CKERR(r);
	toku_txn_close_txn(txn);

	r = toku_checkpoint(cp, logger, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);                             CKERR(r);
	r = toku_close_ft_handle_nolsn(brt, NULL);                                                                          CKERR(r);

	if (verbose) printf("d=%d\n", i);
    }
    r = toku_txn_begin_txn((DB_TXN*)NULL, (TOKUTXN)0, &txn, logger, TXN_SNAPSHOT_ROOT, false);                        CKERR(r);
    r = toku_open_ft_handle(FILENAME, 0, &brt, 1024, 256, TOKU_DEFAULT_COMPRESSION_METHOD, ct, txn, toku_builtin_compare_fun);                       CKERR(r);
    r = toku_txn_commit_txn(txn, false, NULL, NULL);                                     CKERR(r);
    toku_txn_close_txn(txn);

    if (0) {
    bool is_empty;
    is_empty = toku_ft_is_empty_fast(brt);
    assert(is_empty);
    }

    r = toku_checkpoint(cp, logger, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);                                CKERR(r);
    r = toku_close_ft_handle_nolsn(brt, NULL);                                                                             CKERR(r);

    r = toku_checkpoint(cp, logger, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);                                CKERR(r);
    toku_logger_close_rollback(logger);
    r = toku_checkpoint(cp, logger, NULL, NULL, NULL, NULL, CLIENT_CHECKPOINT);                                CKERR(r);
    toku_cachetable_close(&ct);
    r = toku_logger_close(&logger);                                                        assert(r==0);

}
    

int test_main (int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    for (int i=1; i<=64; i++) {
	test_it(i);
    }
    return 0;
}
