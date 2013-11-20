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

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."



#include "test.h"

static void
test_fifo_create (void) {
    int r;
    FIFO f;

    f = 0;
    r = toku_fifo_create(&f); 
    assert(r == 0); assert(f != 0);

    toku_fifo_free(&f);
    assert(f == 0);
}

static void
test_fifo_enq (int n) {
    int r;
    FIFO f;
    MSN startmsn = ZERO_MSN;

    f = 0;
    r = toku_fifo_create(&f); 
    assert(r == 0); assert(f != 0);

    char *thekey = 0; int thekeylen;
    char *theval = 0; int thevallen;

    // this was a function but icc cant handle it    
#define buildkey(len) { \
        thekeylen = len+1; \
        XREALLOC_N(thekeylen, thekey); \
        memset(thekey, len, thekeylen); \
    }

#define buildval(len) { \
        thevallen = len+2; \
        XREALLOC_N(thevallen, theval); \
        memset(theval, ~len, thevallen); \
    }

    for (int i=0; i<n; i++) {
        buildkey(i);
        buildval(i);
        XIDS xids;
        if (i==0)
            xids = xids_get_root_xids();
        else {
            r = xids_create_child(xids_get_root_xids(), &xids, (TXNID)i);
            assert(r==0);
        }
        MSN msn = next_dummymsn();
        if (startmsn.msn == ZERO_MSN.msn)
            startmsn = msn;
        enum ft_msg_type type = (enum ft_msg_type) i;
        r = toku_fifo_enq(f, thekey, thekeylen, theval, thevallen, type, msn, xids, true, NULL); assert(r == 0);
        xids_destroy(&xids);
    }

    int i = 0;
    FIFO_ITERATE(f, key, keylen, val, vallen, type, msn, xids, UU(is_fresh), {
        if (verbose) printf("checkit %d %d %" PRIu64 "\n", i, type, msn.msn);
        assert(msn.msn == startmsn.msn + i);
        buildkey(i);
        buildval(i);
        assert((int) keylen == thekeylen); assert(memcmp(key, thekey, keylen) == 0);
        assert((int) vallen == thevallen); assert(memcmp(val, theval, vallen) == 0);
        assert(i % 256 == (int)type);
	assert((TXNID)i==xids_get_innermost_xid(xids));
        i += 1;
    });
    assert(i == n);

    if (thekey) toku_free(thekey);
    if (theval) toku_free(theval);

    toku_fifo_free(&f);
    assert(f == 0);
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    initialize_dummymsn();
    test_fifo_create();
    test_fifo_enq(4);
    test_fifo_enq(512);
    
    return 0;
}
