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



#include "test.h"

static void
test_create (void) {
    message_buffer msg_buffer;
    msg_buffer.create();
    msg_buffer.destroy();
}

static char *buildkey(size_t len) {
    char *XMALLOC_N(len, k);
    memset(k, 0, len);
    return k;
}

static char *buildval(size_t len) {
    char *XMALLOC_N(len, v);
    memset(v, ~len, len);
    return v;
}

static void
test_enqueue(int n) {
    MSN startmsn = ZERO_MSN;

    message_buffer msg_buffer;
    msg_buffer.create();

    for (int i=0; i<n; i++) {
        int thekeylen = i + 1;
        int thevallen = i + 2;
        char *thekey = buildkey(thekeylen);
        char *theval = buildval(thevallen);
        XIDS xids;
        if (i == 0) {
            xids = toku_xids_get_root_xids();
        } else {
            int r = toku_xids_create_child(toku_xids_get_root_xids(), &xids, (TXNID)i);
            assert_zero(r);
        }
        MSN msn = next_dummymsn();
        if (startmsn.msn == ZERO_MSN.msn)
            startmsn = msn;
        enum ft_msg_type type = (enum ft_msg_type) i;
        DBT k, v;
        ft_msg msg(toku_fill_dbt(&k, thekey, thekeylen), toku_fill_dbt(&v, theval, thevallen), type, msn, xids);
        msg_buffer.enqueue(msg, true, nullptr);
        toku_xids_destroy(&xids);
        toku_free(thekey);
        toku_free(theval);
    }

    struct checkit_fn {
        MSN startmsn;
        int verbose;
        int i;
        checkit_fn(MSN smsn, bool v)
            : startmsn(smsn), verbose(v), i(0) {
        }
        int operator()(const ft_msg &msg, bool UU(is_fresh)) {
            int thekeylen = i + 1;
            int thevallen = i + 2;
            char *thekey = buildkey(thekeylen);
            char *theval = buildval(thevallen);

            MSN msn = msg.msn();
            enum ft_msg_type type = msg.type();
            if (verbose) printf("checkit %d %d %" PRIu64 "\n", i, type, msn.msn);
            assert(msn.msn == startmsn.msn + i);
            assert((int) msg.kdbt()->size == thekeylen); assert(memcmp(msg.kdbt()->data, thekey, msg.kdbt()->size) == 0);
            assert((int) msg.vdbt()->size == thevallen); assert(memcmp(msg.vdbt()->data, theval, msg.vdbt()->size) == 0);
            assert(i % 256 == (int)type);
            assert((TXNID)i == toku_xids_get_innermost_xid(msg.xids()));
            i += 1;
            toku_free(thekey);
            toku_free(theval);
            return 0;
        }
    } checkit(startmsn, verbose);
    msg_buffer.iterate(checkit);
    assert(checkit.i == n);

    msg_buffer.destroy();
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    initialize_dummymsn();
    test_create();
    test_enqueue(4);
    test_enqueue(512);
    
    return 0;
}
