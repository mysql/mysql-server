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

// Test that recovery works correctly on a recovery log in a log directory.

#include "test.h"
#include <libgen.h>

static void run_recovery(const char *testdir) {
    int r;

    int log_version;
    char shutdown[32+1];
    r = sscanf(testdir, "upgrade-recovery-logs-%d-%32s", &log_version, shutdown);
    assert(r == 2);

    char **logfiles = nullptr;
    int n_logfiles = 0;
    r = toku_logger_find_logfiles(testdir, &logfiles, &n_logfiles);
    CKERR(r);
    assert(n_logfiles > 0);

    FILE *f = fopen(logfiles[n_logfiles-1], "r");
    assert(f);
    uint32_t real_log_version;
    r = toku_read_logmagic(f, &real_log_version);
    CKERR(r);
    assert((uint32_t)log_version == (uint32_t)real_log_version);
    r = fclose(f);
    CKERR(r);

    toku_logger_free_logfiles(logfiles, n_logfiles);

    // test needs recovery
    r = tokuft_needs_recovery(testdir, false);
    if (strcmp(shutdown, "clean") == 0) {
        CKERR(r); // clean does not need recovery
    } else if (strncmp(shutdown, "dirty", 5) == 0) {
        CKERR2(r, 1); // dirty needs recovery
    } else {
        CKERR(EINVAL);
    }

    // test maybe upgrade log
    LSN lsn_of_clean_shutdown;
    bool upgrade_in_progress;
    r = toku_maybe_upgrade_log(testdir, testdir, &lsn_of_clean_shutdown, &upgrade_in_progress);
    if (strcmp(shutdown, "dirty") == 0 && log_version <= 24) {
        CKERR2(r, TOKUDB_UPGRADE_FAILURE); // we dont support dirty upgrade from versions <= 24
        return;
    } else {
        CKERR(r);
    }

    if (!verbose) {
        // redirect stderr
        int devnul = open(DEV_NULL_FILE, O_WRONLY);
        assert(devnul >= 0);
        int rr = toku_dup2(devnul, fileno(stderr));
        assert(rr == fileno(stderr));
        rr = close(devnul);
        assert(rr == 0);
    }

    // run recovery
    if (r == 0) {
        r = tokuft_recover(NULL,
                           NULL_prepared_txn_callback,
                           NULL_keep_cachetable_callback,
                           NULL_logger, testdir, testdir, 0, 0, 0, NULL, 0);
        CKERR(r);
    }
}

int test_main(int argc, const char *argv[]) {
    int i = 0;
    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "-v") == 0) {
            verbose++;
            continue;
        }
        if (strcmp(argv[i], "-q") == 0) {
            if (verbose > 0)
                verbose--;
            continue;
        }
        break;
    }
    if (i < argc) {
        const char *full_test_dir = argv[i];
        const char *test_dir = basename((char *)full_test_dir);
        if (strcmp(full_test_dir, test_dir) != 0) {
            int r;
            char cmd[32 + strlen(full_test_dir) + strlen(test_dir)];
            sprintf(cmd, "rm -rf %s", test_dir);
            r = system(cmd);
            CKERR(r);
            sprintf(cmd, "cp -r %s %s", full_test_dir, test_dir);
            r = system(cmd);
            CKERR(r);
        }
        run_recovery(test_dir);
    }
    return 0;
}
