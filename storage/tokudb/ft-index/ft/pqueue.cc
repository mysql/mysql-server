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

#ident "Copyright (c) 2010-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_portability.h>
#include "toku_os.h"
#include "ft-internal.h"
#include "ftloader-internal.h"
#include "pqueue.h"

#define pqueue_left(i)   ((i) << 1)
#define pqueue_right(i)  (((i) << 1) + 1)
#define pqueue_parent(i) ((i) >> 1)

int pqueue_init(pqueue_t **result, size_t n, int which_db, DB *db, ft_compare_func compare, struct error_callback_s *err_callback)
{
    pqueue_t *MALLOC(q);
    if (!q) {
        return get_error_errno();
    }

    /* Need to allocate n+1 elements since element 0 isn't used. */
    MALLOC_N(n + 1, q->d);
    if (!q->d) {
        int r = get_error_errno();
        toku_free(q);
        return r;
    }
    q->size = 1;
    q->avail = q->step = (n+1);  /* see comment above about n+1 */

    q->which_db = which_db;
    q->db = db;
    q->compare = compare;
    q->dup_error = 0;

    q->error_callback = err_callback;

    *result = q;
    return 0;
}

void pqueue_free(pqueue_t *q)
{
    toku_free(q->d);
    toku_free(q);
}


size_t pqueue_size(pqueue_t *q)
{
    /* queue element 0 exists but doesn't count since it isn't used. */
    return (q->size - 1);
}

static int pqueue_compare(pqueue_t *q, DBT *next_key, DBT *next_val, DBT *curr_key)
{
    int r = q->compare(q->db, next_key, curr_key);
    if ( r == 0 ) { // duplicate key : next_key == curr_key
        q->dup_error = 1; 
        if (q->error_callback)
            ft_loader_set_error_and_callback(q->error_callback, DB_KEYEXIST, q->db, q->which_db, next_key, next_val);
    }
    return ( r > -1 );
}

static void pqueue_bubble_up(pqueue_t *q, size_t i)
{
    size_t parent_node;
    pqueue_node_t *moving_node = q->d[i];
    DBT *moving_key = moving_node->key;

    for (parent_node = pqueue_parent(i);
         ((i > 1) && pqueue_compare(q, q->d[parent_node]->key, q->d[parent_node]->val, moving_key));
         i = parent_node, parent_node = pqueue_parent(i))
    {
        q->d[i] = q->d[parent_node];
    }

    q->d[i] = moving_node;
}


static size_t pqueue_maxchild(pqueue_t *q, size_t i)
{
    size_t child_node = pqueue_left(i);

    if (child_node >= q->size)
        return 0;

    if ((child_node+1) < q->size &&
        pqueue_compare(q, q->d[child_node]->key, q->d[child_node]->val, q->d[child_node+1]->key))
        child_node++; /* use right child instead of left */

    return child_node;
}


static void pqueue_percolate_down(pqueue_t *q, size_t i)
{
    size_t child_node;
    pqueue_node_t *moving_node = q->d[i];
    DBT *moving_key = moving_node->key;
    DBT *moving_val = moving_node->val;

    while ((child_node = pqueue_maxchild(q, i)) &&
           pqueue_compare(q, moving_key, moving_val, q->d[child_node]->key))
    {
        q->d[i] = q->d[child_node];
        i = child_node;
    }

    q->d[i] = moving_node;
}


int pqueue_insert(pqueue_t *q, pqueue_node_t *d)
{
    size_t i;

    if (!q) return 1;
    if (q->size >= q->avail) return 1;

    /* insert item */
    i = q->size++;
    q->d[i] = d;
    pqueue_bubble_up(q, i);

    if ( q->dup_error ) return DB_KEYEXIST;
    return 0;
}

int pqueue_pop(pqueue_t *q, pqueue_node_t **d)
{
    if (!q || q->size == 1) {
        *d = NULL;
        return 0;
    }

    *d = q->d[1];
    q->d[1] = q->d[--q->size];
    pqueue_percolate_down(q, 1);

    if ( q->dup_error ) return DB_KEYEXIST;
    return 0;
}
