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
#ident "$Id$"
#include "test.h"

#include <stdio.h>
#include <stdlib.h>

#include <toku_pthread.h>
#include <unistd.h>
#include <memory.h>
#include <sys/stat.h>
#include <db.h>

#include "threaded_stress_test_helpers.h"

//
// This test is a form of stress that does operations on a single dictionary:
// We create a dictionary bigger than the cachetable (around 4x greater).
// Then, we spawn a bunch of pthreads that do the following:
//  - scan dictionary forward with bulk fetch
//  - scan dictionary forward slowly
//  - scan dictionary backward with bulk fetch
//  - scan dictionary backward slowly
//  - Grow the dictionary with insertions
//  - do random point queries into the dictionary
// With the small cachetable, this should produce quite a bit of churn in reading in and evicting nodes.
// If the test runs to completion without crashing, we consider it a success. It also tests that snapshots
// work correctly by verifying that table scans sum their vals to 0.
//
// This does NOT test:
//  - splits and merges
//  - multiple DBs
//
// Variables that are interesting to tweak and run:
//  - small cachetable
//  - number of elements
//

static int checkpoint_var(DB_TXN *txn, ARG arg, void* operation_extra, void *stats_extra) {
    int db_index = random() % arg->cli->num_DBs;
    int r = 0;
    int val_size = *(int *)operation_extra;
    DB* db = arg->dbp[db_index];
    char data[val_size];
    memset(data, 0, sizeof(data));
    int i;
    for (i = 0; i < 10; i++) {
        // do point queries
        ptquery_and_maybe_check_op(db, txn, arg, false);
    }
    increment_counter(stats_extra, PTQUERIES, i);
    for (i = 0; i < 20; i++) {
        // do a random insertion
        int rand_key = random() % arg->cli->num_elements;
        DBT key, val;
        r = db->put(
            db, 
            txn, 
            dbt_init(&key, &rand_key, sizeof(rand_key)), 
            dbt_init(&val, data, sizeof(data)), 
            0);
        if (r != 0) {
            goto cleanup;
        }
    }
cleanup:
    increment_counter(stats_extra, PUTS, i);
    return r;
}


static void
stress_table(DB_ENV* env, DB** dbp, struct cli_args *cli_args) {
    //
    // the threads that we want:
    //   - some threads constantly updating random values
    //   - one thread doing table scan with bulk fetch
    //   - one thread doing table scan without bulk fetch
    //   - some threads doing random point queries
    //

    if (verbose) printf("starting creation of pthreads\n");
    int val_size = cli_args->val_size;
    const int num_threads = cli_args->num_ptquery_threads;
    struct arg myargs[num_threads];
    for (int i = 0; i < num_threads; i++) {
        arg_init(&myargs[i], dbp, env, cli_args);
    }
    for (int i = 0; i < num_threads; i++) {
        myargs[i].operation = checkpoint_var;
        myargs[i].operation_extra = &val_size;
    }
    run_workers(myargs, num_threads, cli_args->num_seconds, false, cli_args);
}

int
test_main(int argc, char *const argv[]) {
    struct cli_args args = get_default_args_for_perf();
    args.env_args.checkpointing_period = 30;
    args.num_DBs = 4;
    args.num_ptquery_threads = 4;
    args.crash_on_operation_failure = false;
    parse_stress_test_args(argc, argv, &args);
    stress_test_main(&args);
    return 0;
}
