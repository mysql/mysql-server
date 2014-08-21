/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
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
#ident "$Id: perf_txn_single_thread.cc 51911 2013-01-10 18:21:29Z zardosht $"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>

#include <toku_pthread.h>
#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <db.h>

#include "threaded_stress_test_helpers.h"

// The intent of this test is to measure how fast a single thread can 
// commit and create transactions when there exist N transactions.

DB_TXN** txns;
int num_txns;

static int commit_and_create_txn(
    DB_TXN* UU(txn), 
    ARG arg, 
    void* UU(operation_extra), 
    void* UU(stats_extra)
    ) 
{
    int rand_txn_id = random() % num_txns;
    int r = txns[rand_txn_id]->commit(txns[rand_txn_id], 0);
    CKERR(r);
    r = arg->env->txn_begin(arg->env, 0, &txns[rand_txn_id], arg->txn_flags | DB_TXN_READ_ONLY); 
    CKERR(r);
    return 0;
}

static void
stress_table(DB_ENV* env, DB** dbp, struct cli_args *cli_args) {
    if (verbose) printf("starting running of stress\n");

    num_txns = cli_args->txn_size;
    XCALLOC_N(num_txns, txns);
    for (int i = 0; i < num_txns; i++) {
        int r = env->txn_begin(env, 0, &txns[i], DB_TXN_SNAPSHOT); 
        CKERR(r);
    }
    
    struct arg myarg;
    arg_init(&myarg, dbp, env, cli_args);
    myarg.operation = commit_and_create_txn;
    
    run_workers(&myarg, 1, cli_args->num_seconds, false, cli_args);

    for (int i = 0; i < num_txns; i++) {
        int chk_r = txns[i]->commit(txns[i], 0);
        CKERR(chk_r);
    }
    toku_free(txns);
    num_txns = 0;
}

int
test_main(int argc, char *const argv[]) {
    num_txns = 0;
    txns = NULL;
    struct cli_args args = get_default_args_for_perf();
    parse_stress_test_args(argc, argv, &args);
    args.single_txn = true;
    // this test is all about transactions, make the DB small
    args.num_elements = 1;
    args.num_DBs= 1;    
    perf_test_main(&args);
    return 0;
}
