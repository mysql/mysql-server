/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef TEST_FT_TXNS_H
#define TEST_FT_TXNS_H

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

static inline void
test_setup(const char *envdir, TOKULOGGER *loggerp, CACHETABLE *ctp) {
    *loggerp = NULL;
    *ctp = NULL;
    int r;
    toku_os_recursive_delete(envdir);
    r = toku_os_mkdir(envdir, S_IRWXU);
    CKERR(r);

    r = toku_logger_create(loggerp);
    CKERR(r);
    TOKULOGGER logger = *loggerp;

    r = toku_logger_open(envdir, logger);
    CKERR(r);

    toku_cachetable_create(ctp, 0, ZERO_LSN, logger);
    CACHETABLE ct = *ctp;
    toku_cachetable_set_env_dir(ct, envdir);

    toku_logger_set_cachetable(logger, ct);

    r = toku_logger_open_rollback(logger, ct, true);
    CKERR(r);

    CHECKPOINTER cp = toku_cachetable_get_checkpointer(*ctp);
    r = toku_checkpoint(cp, logger, NULL, NULL, NULL, NULL, STARTUP_CHECKPOINT);
    CKERR(r);
}

static inline void
xid_lsn_keep_cachetable_callback (DB_ENV *env, CACHETABLE cachetable) {
    CACHETABLE *CAST_FROM_VOIDP(ctp, (void *) env);
    *ctp = cachetable;
}

static inline void test_setup_and_recover(const char *envdir, TOKULOGGER *loggerp, CACHETABLE *ctp) {
    int r;
    TOKULOGGER logger = NULL;
    CACHETABLE ct = NULL;
    r = toku_logger_create(&logger);
    CKERR(r);

    DB_ENV *CAST_FROM_VOIDP(ctv, (void *) &ct);  // Use intermediate to avoid compiler warning.
    r = tokudb_recover(ctv,
                       NULL_prepared_txn_callback,
                       xid_lsn_keep_cachetable_callback,
                       logger,
                       envdir, envdir, 0, 0, 0, NULL, 0);
    CKERR(r);
    if (!toku_logger_is_open(logger)) {
        //Did not need recovery.
        invariant(ct==NULL);
        r = toku_logger_open(envdir, logger);
        CKERR(r);
        toku_cachetable_create(&ct, 0, ZERO_LSN, logger);
        toku_logger_set_cachetable(logger, ct);
    }
    *ctp = ct;
    *loggerp = logger;
}

static inline void clean_shutdown(TOKULOGGER *loggerp, CACHETABLE *ctp) {
    int r;
    CHECKPOINTER cp = toku_cachetable_get_checkpointer(*ctp);
    r = toku_checkpoint(cp, *loggerp, NULL, NULL, NULL, NULL, SHUTDOWN_CHECKPOINT);
    CKERR(r);

    toku_logger_close_rollback(*loggerp);

    r = toku_checkpoint(cp, *loggerp, NULL, NULL, NULL, NULL, SHUTDOWN_CHECKPOINT);
    CKERR(r);

    toku_logger_shutdown(*loggerp);

    toku_cachetable_close(ctp);

    r = toku_logger_close(loggerp);
    CKERR(r);
}

static inline void shutdown_after_recovery(TOKULOGGER *loggerp, CACHETABLE *ctp) {
    toku_logger_close_rollback(*loggerp);
    toku_cachetable_close(ctp);
    int r = toku_logger_close(loggerp);
    CKERR(r);
}

#endif /* TEST_FT_TXNS_H */
