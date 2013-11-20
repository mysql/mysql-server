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

#include "fifo.h"
#include "xids.h"
#include "ybt.h"
#include <memory.h>
#include <toku_assert.h>

struct fifo {
    int n_items_in_fifo;
    char *memory;       // An array of bytes into which fifo_entries are embedded.
    int   memory_size;  // How big is fifo_memory
    int   memory_used;  // How many bytes are in use?
};

const int fifo_initial_size = 4096;
static void fifo_init(struct fifo *fifo) {
    fifo->n_items_in_fifo = 0;
    fifo->memory       = 0;
    fifo->memory_size  = 0;
    fifo->memory_used  = 0;
}

__attribute__((const,nonnull))
static int fifo_entry_size(struct fifo_entry *entry) {
    return sizeof (struct fifo_entry) + entry->keylen + entry->vallen
                  + xids_get_size(&entry->xids_s)
                  - sizeof(XIDS_S); //Prevent double counting from fifo_entry+xids_get_size
}

__attribute__((const,nonnull))
size_t toku_ft_msg_memsize_in_fifo(FT_MSG cmd) {
    // This must stay in sync with fifo_entry_size because that's what we
    // really trust.  But sometimes we only have an in-memory FT_MSG, not
    // a serialized fifo_entry so we have to fake it.
    return sizeof (struct fifo_entry) + cmd->u.id.key->size + cmd->u.id.val->size
        + xids_get_size(cmd->xids)
        - sizeof(XIDS_S);
}

int toku_fifo_create(FIFO *ptr) {
    struct fifo *XMALLOC(fifo);
    if (fifo == 0) return ENOMEM;
    fifo_init(fifo);
    *ptr = fifo;
    return 0;
}

void toku_fifo_free(FIFO *ptr) {
    FIFO fifo = *ptr;
    if (fifo->memory) toku_free(fifo->memory);
    fifo->memory=0;
    toku_free(fifo);
    *ptr = 0;
}

int toku_fifo_n_entries(FIFO fifo) {
    return fifo->n_items_in_fifo;
}

static int next_power_of_two (int n) {
    int r = 4096;
    while (r < n) {
        r*=2;
        assert(r>0);
    }
    return r;
}

int toku_fifo_enq(FIFO fifo, const void *key, unsigned int keylen, const void *data, unsigned int datalen, enum ft_msg_type type, MSN msn, XIDS xids, bool is_fresh, int32_t *dest) {
    int need_space_here = sizeof(struct fifo_entry)
                          + keylen + datalen
                          + xids_get_size(xids)
                          - sizeof(XIDS_S); //Prevent double counting
    int need_space_total = fifo->memory_used+need_space_here;
    if (fifo->memory == NULL) {
        fifo->memory_size = next_power_of_two(need_space_total);
        XMALLOC_N(fifo->memory_size, fifo->memory);
    }
    if (need_space_total > fifo->memory_size) {
        // Out of memory at the end.
        int next_2 = next_power_of_two(need_space_total);
        // resize the fifo
        XREALLOC_N(next_2, fifo->memory);
        fifo->memory_size = next_2;
    }
    struct fifo_entry *entry = (struct fifo_entry *)(fifo->memory + fifo->memory_used);
    fifo_entry_set_msg_type(entry, type);
    entry->msn = msn;
    xids_cpy(&entry->xids_s, xids);
    entry->is_fresh = is_fresh;
    entry->keylen = keylen;
    unsigned char *e_key = xids_get_end_of_array(&entry->xids_s);
    memcpy(e_key, key, keylen);
    entry->vallen = datalen;
    memcpy(e_key + keylen, data, datalen);
    if (dest) {
        *dest = fifo->memory_used;
    }
    fifo->n_items_in_fifo++;
    fifo->memory_used += need_space_here;
    return 0;
}

int toku_fifo_iterate_internal_start(FIFO UU(fifo)) { return 0; }
int toku_fifo_iterate_internal_has_more(FIFO fifo, int off) { return off < fifo->memory_used; }
int toku_fifo_iterate_internal_next(FIFO fifo, int off) {
    struct fifo_entry *e = (struct fifo_entry *)(fifo->memory + off);
    return off + fifo_entry_size(e);
}
struct fifo_entry * toku_fifo_iterate_internal_get_entry(FIFO fifo, int off) {
    return (struct fifo_entry *)(fifo->memory + off);
}
size_t toku_fifo_internal_entry_memsize(struct fifo_entry *e) {
    return fifo_entry_size(e);
}

void toku_fifo_iterate (FIFO fifo, void(*f)(bytevec key,ITEMLEN keylen,bytevec data,ITEMLEN datalen, enum ft_msg_type type, MSN msn, XIDS xids, bool is_fresh, void*), void *arg) {
    FIFO_ITERATE(fifo,
                 key, keylen, data, datalen, type, msn, xids, is_fresh,
                 f(key,keylen,data,datalen,type,msn,xids,is_fresh, arg));
}

unsigned int toku_fifo_buffer_size_in_use (FIFO fifo) {
    return fifo->memory_used;
}

unsigned long toku_fifo_memory_size_in_use(FIFO fifo) {
    return sizeof(*fifo)+fifo->memory_used;
}

unsigned long toku_fifo_memory_footprint(FIFO fifo) {
    size_t size_used = toku_memory_footprint(fifo->memory, fifo->memory_used);
    long rval = sizeof(*fifo) + size_used; 
    return rval;
}

DBT *fill_dbt_for_fifo_entry(DBT *dbt, const struct fifo_entry *entry) {
    return toku_fill_dbt(dbt, xids_get_end_of_array((XIDS) &entry->xids_s), entry->keylen);
}

struct fifo_entry *toku_fifo_get_entry(FIFO fifo, int off) {
    return toku_fifo_iterate_internal_get_entry(fifo, off);
}

void toku_fifo_clone(FIFO orig_fifo, FIFO* cloned_fifo) {
    struct fifo *XMALLOC(new_fifo);
    assert(new_fifo);
    new_fifo->n_items_in_fifo = orig_fifo->n_items_in_fifo;
    new_fifo->memory_used = orig_fifo->memory_used;
    new_fifo->memory_size = new_fifo->memory_used;
    XMALLOC_N(new_fifo->memory_size, new_fifo->memory);
    memcpy(
        new_fifo->memory, 
        orig_fifo->memory, 
        new_fifo->memory_size
        );
    *cloned_fifo = new_fifo;
}

bool toku_are_fifos_same(FIFO fifo1, FIFO fifo2) {
    return (
        fifo1->memory_used == fifo2->memory_used &&
        memcmp(fifo1->memory, fifo2->memory, fifo1->memory_used) == 0
        );
}
