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
#include "toku_list.h"


#include "test.h"
#include <memory.h>
#include <stdio.h>
#include <stdlib.h>

struct testlist {
    struct toku_list next;
    int tag;
};

static void testlist_init (struct testlist *tl, int tag) {
    tl->tag = tag;
}

static void test_push_pop (int n) {
    int i;
    struct toku_list head;

    toku_list_init(&head);
    for (i=0; i<n; i++) {
        struct testlist *tl = (struct testlist *) toku_malloc(sizeof *tl);
        assert(tl);
        testlist_init(tl, i);
        toku_list_push(&head, &tl->next);
        assert(!toku_list_empty(&head));
    }
    for (i=n-1; i>=0; i--) {
        struct toku_list *list;
        struct testlist *tl;

        list = toku_list_head(&head);
        tl  = toku_list_struct(list, struct testlist, next);
        assert(tl->tag == 0);
        list = toku_list_tail(&head);
        tl = toku_list_struct(list, struct testlist, next);
        assert(tl->tag == i);
        list = toku_list_pop(&head);
        tl = toku_list_struct(list, struct testlist, next);
        assert(tl->tag == i);
        toku_free(tl);
    }
    assert(toku_list_empty(&head));
}

static void test_push_pop_head (int n) {
    int i;
    struct toku_list head;

    toku_list_init(&head);
    for (i=0; i<n; i++) {
        struct testlist *tl = (struct testlist *) toku_malloc(sizeof *tl);
        assert(tl);
        testlist_init(tl, i);
        toku_list_push(&head, &tl->next);
        assert(!toku_list_empty(&head));
    }
    for (i=0; i<n; i++) {
        struct toku_list *list;
        struct testlist *tl;

        list = toku_list_head(&head);
        tl  = toku_list_struct(list, struct testlist, next);
        assert(tl->tag == i);
        list = toku_list_tail(&head);
        tl = toku_list_struct(list, struct testlist, next);
        assert(tl->tag == n-1);

        list = toku_list_pop_head(&head);
        tl = toku_list_struct(list, struct testlist, next);
        assert(tl->tag == i);
        toku_free(tl);
    }
    assert(toku_list_empty(&head));
}

static void test_push_head_pop (int n) {
    int i;
    struct toku_list head;

    toku_list_init(&head);
    for (i=0; i<n; i++) {
        struct testlist *tl = (struct testlist *) toku_malloc(sizeof *tl);
        assert(tl);
        testlist_init(tl, i);
        toku_list_push_head(&head, &tl->next);
        assert(!toku_list_empty(&head));
    }
    for (i=0; i<n; i++) {
        struct toku_list *list;
        struct testlist *tl;

        list = toku_list_head(&head);
        tl  = toku_list_struct(list, struct testlist, next);
        assert(tl->tag == n-1);
        list = toku_list_tail(&head);
        tl = toku_list_struct(list, struct testlist, next);
        assert(tl->tag == i);

        list = toku_list_pop(&head);
        tl = toku_list_struct(list, struct testlist, next);
        assert(tl->tag == i);
        toku_free(tl);
    }
    assert(toku_list_empty(&head));
}

#if 0
// cant move an empty list
static void test_move_empty (void) {
    struct toku_list h1, h2;

    toku_list_init(&h1);
    toku_list_init(&h2);
    toku_list_move(&h1, &h2);
    assert(toku_list_empty(&h2));
    assert(toku_list_empty(&h1));
}
#endif

static void test_move (int n) {
    struct toku_list h1, h2;
    int i;

    toku_list_init(&h1);
    toku_list_init(&h2);
    for (i=0; i<n; i++) {
        struct testlist *tl = (struct testlist *) toku_malloc(sizeof *tl);
        assert(tl);
        testlist_init(tl, i);
        toku_list_push(&h2, &tl->next);
    }
    toku_list_move(&h1, &h2);
    assert(!toku_list_empty(&h1));
    assert(toku_list_empty(&h2));
    i = 0;
    while (!toku_list_empty(&h1)) {
        struct toku_list *list = toku_list_pop_head(&h1);
        struct testlist *tl = toku_list_struct(list, struct testlist, next);
        assert(tl->tag == i);
        toku_free(tl);
        i += 1;
    }
    assert(i == n);
}

int
test_main(int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    test_push_pop(0);
    test_push_pop(8);
    test_push_pop_head(0);
    test_push_pop_head(8);
    test_push_head_pop(8);
    test_move(1);
    test_move(8);
    //    test_move_empty();

    return 0;
}

