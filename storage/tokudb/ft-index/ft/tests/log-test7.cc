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

// create and close, making sure that everything is deallocated properly.

#define LSIZE 100
#define NUM_LOGGERS 10
TOKULOGGER logger[NUM_LOGGERS];

static void setup_logger(int which) {
    char logname[10];
    snprintf(logname, sizeof(logname), "log%d", which);
    char dnamewhich[TOKU_PATH_MAX+1];
    int r;
    toku_path_join(dnamewhich, 2, TOKU_TEST_FILENAME, logname);
    r = toku_os_mkdir(dnamewhich, S_IRWXU);
    if (r!=0) {
        int er = get_error_errno();
        printf("file %s error (%d) %s\n", dnamewhich, er, strerror(er));
        assert(r==0);
    }
    r = toku_logger_create(&logger[which]);
    assert(r == 0);
    r = toku_logger_set_lg_max(logger[which], LSIZE);
    {
	uint32_t n;
	r = toku_logger_get_lg_max(logger[which], &n);
	assert(n==LSIZE);
    }
    r = toku_logger_open(dnamewhich, logger[which]);
    assert(r == 0);
}

static void play_with_logger(int which) {
    {
	ml_lock(&logger[which]->input_lock);
	int lsize=LSIZE-12-2;
	toku_logger_make_space_in_inbuf(logger[which], lsize);
	snprintf(logger[which]->inbuf.buf+logger[which]->inbuf.n_in_buf, lsize, "a%*d", lsize-1, 0);
	logger[which]->inbuf.n_in_buf += lsize;
	logger[which]->lsn.lsn++;
	logger[which]->inbuf.max_lsn_in_buf = logger[which]->lsn;
	ml_unlock(&logger[which]->input_lock);
    }

    {
	ml_lock(&logger[which]->input_lock);
	toku_logger_make_space_in_inbuf(logger[which], 2);
	memcpy(logger[which]->inbuf.buf+logger[which]->inbuf.n_in_buf, "b1", 2);
	logger[which]->inbuf.n_in_buf += 2;
	logger[which]->lsn.lsn++;
	logger[which]->inbuf.max_lsn_in_buf = logger[which]->lsn;
	ml_unlock(&logger[which]->input_lock);
    }
}

static void tear_down_logger(int which) {
    int r;
    r = toku_logger_close(&logger[which]);
    assert(r == 0);
}

int
test_main (int argc __attribute__((__unused__)),
	  const char *argv[] __attribute__((__unused__))) {
    int i;
    int loop;
    const int numloops = 100;
    for (loop = 0; loop < numloops; loop++) {
        toku_os_recursive_delete(TOKU_TEST_FILENAME);
        int r = toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU);
        assert_zero(r);
        for (i = 0; i < NUM_LOGGERS; i++) setup_logger(i);
        for (i = 0; i < NUM_LOGGERS; i++) play_with_logger(i);
        for (i = 0; i < NUM_LOGGERS; i++) tear_down_logger(i);
    }
    toku_os_recursive_delete(TOKU_TEST_FILENAME);

    return 0;
}
