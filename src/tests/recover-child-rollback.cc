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

static void
stress_table(DB_ENV *env, DB **dbp, struct cli_args *cli_args) {
    //
    // the threads that we want:
    //   - one (or more) thread(s) constantly updating random values, wrapped in a persistent parent transaction.

    if (verbose) printf("starting creation of pthreads\n");
    const int num_threads = cli_args->num_update_threads;
    struct arg myargs[num_threads];
    for (int i = 0; i < num_threads; i++) {
        arg_init(&myargs[i], dbp, env, cli_args);
    }

    struct update_op_args uoe = get_update_op_args(cli_args, NULL);
    // make the guy that updates the db
    for (int i = 0; i < cli_args->num_update_threads; ++i) {
        myargs[i].operation_extra = &uoe;
        myargs[i].operation = update_op;
        myargs[i].do_prepare = true;
        myargs[i].wrap_in_parent = true;
    }

    run_workers(myargs, num_threads, cli_args->num_seconds, true, cli_args);
}

int
test_main(int argc, char *const argv[]) {
    struct cli_args args = get_default_args();
    args.num_seconds = 5;
    //args.txn_size = 64;   // 100 * 256 is more than enough to spill (4096) byte rollback nodes for parent and child.
    //args.val_size = 512;  // Large values to overflow a rollback log node fast.
    //args.env_args.node_size = 4*1024*1024;  // Large nodes to prevent spending much time
    //args.env_args.basement_node_size = 128*1024;  // Large nodes to prevent spending much time
    args.env_args.checkpointing_period = 1;
    parse_stress_test_args(argc, argv, &args);
    if (args.do_test_and_crash) {
        stress_test_main(&args);
    }
    if (args.do_recover) {
        stress_recover(&args);
    }
    return 0;
}
