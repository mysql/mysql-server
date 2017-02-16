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

#include "manager_unit_test.h"

namespace toku {

static int create_cb(locktree *lt, void *extra) {
    lt->set_userdata(extra);
    bool *k = (bool *) extra;
    invariant(!(*k));
    (*k) = true;
    return 0;
}

static void destroy_cb(locktree *lt) {
    bool *k = (bool *) lt->get_userdata();
    invariant(*k);
    (*k) = false;
}

static int my_cmp(DB *UU(db), const DBT *UU(a), const DBT *UU(b)) {
    return 0;
}

void manager_unit_test::test_reference_release_lt(void) {
    locktree_manager mgr;
    mgr.create(create_cb, destroy_cb, nullptr, nullptr);
    toku::comparator my_comparator;
    my_comparator.create(my_cmp, nullptr);

    DICTIONARY_ID a = { 0 };
    DICTIONARY_ID b = { 1 };
    DICTIONARY_ID c = { 2 };
    bool aok = false;
    bool bok = false;
    bool cok = false;

    locktree *alt = mgr.get_lt(a, my_comparator, &aok);
    invariant_notnull(alt);
    locktree *blt = mgr.get_lt(b, my_comparator, &bok);
    invariant_notnull(alt);
    locktree *clt = mgr.get_lt(c, my_comparator, &cok);
    invariant_notnull(alt);

    // three distinct locktrees should have been returned
    invariant(alt != blt && alt != clt && blt != clt);

    // on create callbacks should have been called
    invariant(aok);
    invariant(bok);
    invariant(cok);

    // add 3 refs. b should still exist.
    mgr.reference_lt(blt);
    mgr.reference_lt(blt);
    mgr.reference_lt(blt);
    invariant(bok);
    // remove 3 refs. b should still exist.
    mgr.release_lt(blt);
    mgr.release_lt(blt);
    mgr.release_lt(blt);
    invariant(bok);

    // get another handle on a and b, they shoudl be the same
    // as the original alt and blt
    locktree *blt2 = mgr.get_lt(b, my_comparator, &bok);
    invariant(blt2 == blt);
    locktree *alt2 = mgr.get_lt(a, my_comparator, &aok);
    invariant(alt2 == alt);

    // remove one ref from everything. c should die. a and b are ok.
    mgr.release_lt(alt);
    mgr.release_lt(blt);
    mgr.release_lt(clt);
    invariant(aok);
    invariant(bok);
    invariant(!cok);

    // release a and b. both should die.
    mgr.release_lt(blt2);
    mgr.release_lt(alt2);
    invariant(!aok);
    invariant(!bok);
    
    my_comparator.destroy();
    mgr.destroy();
}

} /* namespace toku */

int main(void) {
    toku::manager_unit_test test;
    test.test_reference_release_lt();
    return 0; 
}
