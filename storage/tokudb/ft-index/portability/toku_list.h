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

#pragma once

#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

// This toku_list is intended to be embedded in other data structures.
struct toku_list {
    struct toku_list *next, *prev;
};

static inline int toku_list_num_elements_est(struct toku_list *head) {
    if (head->next == head) return 0;
    if (head->next == head->prev) return 1;
    return 2;
}


static inline void toku_list_init(struct toku_list *head) {
    head->next = head->prev = head;
}

static inline int toku_list_empty(struct toku_list *head) {
    return head->next == head;
}

static inline struct toku_list *toku_list_head(struct toku_list *head) {
    return head->next;
}

static inline struct toku_list *toku_list_tail(struct toku_list *head) {
    return head->prev;
}

static inline void toku_list_insert_between(struct toku_list *a, struct toku_list *toku_list, struct toku_list *b) {

    toku_list->next = a->next;
    toku_list->prev = b->prev;
    a->next = b->prev = toku_list;
}

static inline void toku_list_push(struct toku_list *head, struct toku_list *toku_list) {
    toku_list_insert_between(head->prev, toku_list, head);
}

static inline void toku_list_push_head(struct toku_list *head, struct toku_list *toku_list) {
    toku_list_insert_between(head, toku_list, head->next);
}

static inline void toku_list_remove(struct toku_list *toku_list) {
    struct toku_list *prev = toku_list->prev;
    struct toku_list *next = toku_list->next;
    next->prev = prev;
    prev->next = next;
    toku_list_init(toku_list); // Set the toku_list element to be empty
}

static inline struct toku_list *toku_list_pop(struct toku_list *head) {
    struct toku_list *toku_list = head->prev;
    toku_list_remove(toku_list);
    return toku_list;
}

static inline struct toku_list *toku_list_pop_head(struct toku_list *head) {
    struct toku_list *toku_list = head->next;
    toku_list_remove(toku_list);
    return toku_list;
}

static inline void toku_list_move(struct toku_list *newhead, struct toku_list *oldhead) {
    struct toku_list *first = oldhead->next;
    struct toku_list *last = oldhead->prev;
    // assert(!toku_list_empty(oldhead));
    newhead->next = first;
    newhead->prev = last;
    last->next = first->prev = newhead;
    toku_list_init(oldhead);
}

// Note: Need the extra level of parens in these macros so that
//   toku_list_struct(h, foo, b)->zot
// will work right.  Otherwise the type cast will try to include ->zot, and it will be all messed up.
#if ((defined(__GNUC__) && __GNUC__ >= 4) || defined(__builtin_offsetof) ) && !defined(__clang__) 
#define toku_list_struct(p, t, f) ((t*)((char*)(p) - __builtin_offsetof(t, f)))
#else
#define toku_list_struct(p, t, f) ((t*)((char*)(p) - ((char*)&((t*)0)->f)))
#endif
