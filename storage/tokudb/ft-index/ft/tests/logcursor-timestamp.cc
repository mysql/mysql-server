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

#include "logcursor.h"
#include "test.h"

static uint64_t now(void) {
    struct timeval tv;
    int r = gettimeofday(&tv, NULL);
    assert(r == 0);
    return tv.tv_sec * 1000000ULL + tv.tv_usec;
}

// log a couple of timestamp entries and verify the log by walking 
// a cursor through the log entries

int
test_main (int argc, const char *argv[]) {
    default_parse_args(argc, argv);

    int r;
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU);    assert(r==0);
    TOKULOGGER logger;
    LSN lsn = ZERO_LSN;

    // log a couple of timestamp log entries

    r = toku_logger_create(&logger);
    assert(r == 0);

    r = toku_logger_open(TOKU_TEST_FILENAME, logger);
    assert(r == 0);

    BYTESTRING bs0 = { .len = 5, .data = (char *) "hello" };
    toku_log_comment(logger, &lsn, 0, now(), bs0);

    sleep(11);

    BYTESTRING bs1 = { .len = 5, .data = (char *) "world" };
    toku_log_comment(logger, &lsn, 0, now(), bs1);

    r = toku_logger_close(&logger);
    assert(r == 0);

    // verify the log forwards
    TOKULOGCURSOR lc = NULL;
    struct log_entry *le;
    
    r = toku_logcursor_create(&lc, TOKU_TEST_FILENAME);
    assert(r == 0 && lc != NULL);

    r = toku_logcursor_next(lc, &le);
    assert(r == 0 && le->cmd == LT_comment);
    assert(le->u.comment.comment.len == 5 && memcmp(le->u.comment.comment.data, "hello", 5) == 0);
    uint64_t t = le->u.comment.timestamp;
    
    r = toku_logcursor_next(lc, &le);
    assert(r == 0 && le->cmd == LT_comment);
    assert(le->u.comment.comment.len == 5 && memcmp(le->u.comment.comment.data, "world", 5) == 0);
    if (verbose)
        printf("%" PRIu64 "\n", le->u.comment.timestamp - t);
    assert(le->u.comment.timestamp - t >= 10*1000000);

    r = toku_logcursor_next(lc, &le);
    assert(r != 0);

    r = toku_logcursor_destroy(&lc);
    assert(r == 0 && lc == NULL);

    // verify the log backwards
    r = toku_logcursor_create(&lc, TOKU_TEST_FILENAME);
    assert(r == 0 && lc != NULL);

    r = toku_logcursor_prev(lc, &le);
    assert(r == 0 && le->cmd == LT_comment);
    assert(le->u.comment.comment.len == 5 && memcmp(le->u.comment.comment.data, "world", 5) == 0);
    t = le->u.comment.timestamp;
    
    r = toku_logcursor_prev(lc, &le);
    assert(r == 0 && le->cmd == LT_comment);
    assert(le->u.comment.comment.len == 5 && memcmp(le->u.comment.comment.data, "hello", 5) == 0);
    if (verbose)
        printf("%" PRIu64 "\n", t - le->u.comment.timestamp);
    assert(t - le->u.comment.timestamp >= 10*1000000);

    r = toku_logcursor_prev(lc, &le);
    assert(r != 0);

    r = toku_logcursor_destroy(&lc);
    assert(r == 0 && lc == NULL);

    toku_os_recursive_delete(TOKU_TEST_FILENAME);

    return 0;
}
