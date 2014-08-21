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

#ident "Copyright (c) 2009-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

/* Purpose of this test is to verify the basic functioning
 * of the engine status functions.
 */


#include "test.h"
#include <db.h>
#include "toku_time.h"

static DB_ENV *env;

#define FLAGS_NOLOG DB_INIT_LOCK|DB_INIT_MPOOL|DB_CREATE|DB_PRIVATE
#define FLAGS_LOG   FLAGS_NOLOG|DB_INIT_TXN|DB_INIT_LOG

static int mode = S_IRWXU+S_IRWXG+S_IRWXO;

static void test_shutdown(void);

static void
test_shutdown(void) {
    int r;
    r=env->close(env, 0); CKERR(r);
    env = NULL;
}

static void
setup (uint32_t flags) {
    int r;
    if (env)
        test_shutdown();
    toku_os_recursive_delete(TOKU_TEST_FILENAME);
    r=toku_os_mkdir(TOKU_TEST_FILENAME, S_IRWXU+S_IRWXG+S_IRWXO);
    CKERR(r);
    r=db_env_create(&env, 0); 
    CKERR(r);
    env->set_errfile(env, stderr);
    r=env->open(env, TOKU_TEST_FILENAME, flags, mode); 
    CKERR(r);
}


static void
print_raw(TOKU_ENGINE_STATUS_ROW row) {
    printf("keyname is %s, type is %d, legend is %s\n",
           row->keyname,
           row->type,
           row->legend);
}    

static void
status_format_time(const time_t *timer, char *buf) {
    ctime_r(timer, buf);
    size_t len = strlen(buf);
    assert(len < 26);
    char end;

    assert(len>=1);
    end = buf[len-1];
    while (end == '\n' || end == '\r') {
        buf[len-1] = '\0';
        len--;
        assert(len>=1);
        end = buf[len-1];
    }
}


int
test_main (int argc, char * const argv[]) {
    uint64_t nrows;
    uint64_t max_rows;
    fs_redzone_state redzone_state;
    uint64_t panic;
    const int panic_string_len = 1024;
    char panic_string[panic_string_len];

    //    char buf[bufsiz] = {'\0'};
    parse_args(argc, argv);
    setup(FLAGS_LOG);
    env->txn_checkpoint(env, 0, 0, 0);

    env->get_engine_status_num_rows(env, &max_rows);
    TOKU_ENGINE_STATUS_ROW_S mystat[max_rows];
    int r = env->get_engine_status (env, mystat, max_rows, &nrows, &redzone_state, &panic, panic_string, panic_string_len, TOKU_ENGINE_STATUS);
    assert(r==0);

    if (verbose) {
        printf("First all the raw fields:\n");
        for (uint64_t i = 0; i < nrows; i++) {
            printf("%s        ", mystat[i].keyname);
            printf("%s        ", mystat[i].columnname ? mystat[i].columnname : "(null)");
            printf("%s       ", mystat[i].legend);
            printf("type=%d  val = ", mystat[i].type);
            switch(mystat[i].type) {
            case FS_STATE:
                printf("fs_state not supported yet, code is %" PRIu64 "\n", mystat[i].value.num);
                break;
            case UINT64:
                printf("%" PRIu64 "\n", mystat[i].value.num);
                break;
            case CHARSTR:
                printf("%s\n", mystat[i].value.str);
                break;
            case UNIXTIME:
                {
                    char tbuf[26];
                    status_format_time((time_t*)&mystat[i].value.num, tbuf);
                    printf("%s\n", tbuf);
                }
                break;
            case TOKUTIME:
                {
                    double t = tokutime_to_seconds(mystat[i].value.num);
                    printf("%.6f\n", t);
                }
                break;
            default:
                printf("UNKNOWN STATUS TYPE:\n");
                print_raw(&mystat[i]);
                break;
            }
        }

        printf("\n\n\n\n\nNow as reported by get_engine_status_text():\n\n");

        int bufsiz = nrows * 128;   // assume 128 characters per row
        char buff[bufsiz];  
        r = env->get_engine_status_text(env, buff, bufsiz);
        printf("%s", buff);

        printf("\n\n\n\n\nFinally, print as reported by test utility print_engine_status()\n");

        print_engine_status(env);

        printf("That's all, folks.\n");
    }
    test_shutdown();
    return 0;
}
