/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ifndef FIFO_H
#define FIFO_H
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

#include "fttypes.h"
#include "xids-internal.h"
#include "xids.h"


// If the fifo_entry is unpacked, the compiler aligns the xids array and we waste a lot of space
struct __attribute__((__packed__)) fifo_entry {
    unsigned int keylen;
    unsigned int vallen;
    unsigned char type;
    bool          is_fresh;
    MSN           msn;
    XIDS_S        xids_s;
};

// get and set the ft message type for a fifo entry.
// it is internally stored as a single unsigned char.
static inline enum ft_msg_type 
fifo_entry_get_msg_type(const struct fifo_entry * entry)
{
    enum ft_msg_type msg_type;
    msg_type = (enum ft_msg_type) entry->type;
    return msg_type;
}

static inline void
fifo_entry_set_msg_type(struct fifo_entry * entry,
        enum ft_msg_type msg_type)
{
    unsigned char type = (unsigned char) msg_type;
    entry->type = type;
}

typedef struct fifo *FIFO;

int toku_fifo_create(FIFO *);

void toku_fifo_resize(FIFO fifo, size_t new_size);

void toku_fifo_free(FIFO *);

int toku_fifo_n_entries(FIFO);

int toku_fifo_enq (FIFO, const void *key, ITEMLEN keylen, const void *data, ITEMLEN datalen, enum ft_msg_type type, MSN msn, XIDS xids, bool is_fresh, int32_t *dest);

unsigned int toku_fifo_buffer_size_in_use (FIFO fifo);
unsigned long toku_fifo_memory_size_in_use(FIFO fifo);  // return how much memory in the fifo holds useful data

unsigned long toku_fifo_memory_footprint(FIFO fifo);  // return how much memory the fifo occupies

void toku_fifo_iterate(FIFO, void(*f)(bytevec key,ITEMLEN keylen,bytevec data,ITEMLEN datalen, enum ft_msg_type type, MSN msn, XIDS xids, bool is_fresh, void*), void*);

#define FIFO_ITERATE(fifo,keyvar,keylenvar,datavar,datalenvar,typevar,msnvar,xidsvar,is_freshvar,body) ({ \
  for (int fifo_iterate_off = toku_fifo_iterate_internal_start(fifo);                                     \
       toku_fifo_iterate_internal_has_more(fifo, fifo_iterate_off);                                       \
       fifo_iterate_off = toku_fifo_iterate_internal_next(fifo, fifo_iterate_off)) {                      \
      struct fifo_entry *e = toku_fifo_iterate_internal_get_entry(fifo, fifo_iterate_off);                \
      ITEMLEN keylenvar = e->keylen;                                                                      \
      ITEMLEN datalenvar = e->vallen;                                                                     \
      enum ft_msg_type typevar = fifo_entry_get_msg_type(e);                                              \
      MSN     msnvar  = e->msn;                                                                           \
      XIDS    xidsvar = &e->xids_s;                                                                       \
      bytevec keyvar  = xids_get_end_of_array(xidsvar);                                                   \
      bytevec datavar = (const uint8_t*)keyvar + e->keylen;                                               \
      bool is_freshvar = e->is_fresh;                                                                     \
      body;                                                                                               \
  } })

#define FIFO_CURRENT_ENTRY_MEMSIZE toku_fifo_internal_entry_memsize(e)

// Internal functions for the iterator.
int toku_fifo_iterate_internal_start(FIFO fifo);
int toku_fifo_iterate_internal_has_more(FIFO fifo, int off);
int toku_fifo_iterate_internal_next(FIFO fifo, int off);
struct fifo_entry * toku_fifo_iterate_internal_get_entry(FIFO fifo, int off);
size_t toku_fifo_internal_entry_memsize(struct fifo_entry *e) __attribute__((const,nonnull));
size_t toku_ft_msg_memsize_in_fifo(FT_MSG msg) __attribute__((const,nonnull));

DBT *fill_dbt_for_fifo_entry(DBT *dbt, const struct fifo_entry *entry);
struct fifo_entry *toku_fifo_get_entry(FIFO fifo, int off);

void toku_fifo_clone(FIFO orig_fifo, FIFO* cloned_fifo);

bool toku_are_fifos_same(FIFO fifo1, FIFO fifo2);




#endif
