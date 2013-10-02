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

#include <toku_portability.h>
#include <memory.h>
#include <string.h>
#include <db.h>

#include <util/mempool.h>
#include "dmt-wrapper.h"

namespace toku {
template<>
class dmt_functor<DMTVALUE> {
    public:
        size_t get_dmtdatain_t_size(void) const {
            return sizeof(DMTVALUE);
        }
        void write_dmtdata_t_to(DMTVALUE *const dest) const {
            *dest = value;
        }

        dmt_functor(DMTVALUE _value)
            : value(_value) {}
        dmt_functor(const uint32_t size UU(), DMTVALUE *const src)
            : value(*src) {
            paranoid_invariant(size == sizeof(DMTVALUE));
        }
    private:
        const DMTVALUE value;
};
}

int
toku_dmt_create_steal_sorted_array(DMT *dmtp, DMTVALUE **valuesp, uint32_t numvalues, uint32_t capacity) {
    //TODO: implement using create_steal_sorted_array when it exists
    (void)capacity;
    toku_dmt_create_from_sorted_array(dmtp, *valuesp, numvalues);
    toku_free(*valuesp);
    *valuesp = nullptr;


//    DMT XMALLOC(dmt);
    //dmt->create_steal_sorted_array(valuesp, numvalues, capacity);
 //   *dmtp = dmt;
    return 0;
}

//TODO: Put all dmt API functions here.
int toku_dmt_create (DMT *dmtp) {
    DMT XMALLOC(dmt);
    dmt->create();
    *dmtp = dmt;
    return 0;
}

void toku_dmt_destroy(DMT *dmtp) {
    DMT dmt=*dmtp;
    dmt->destroy();
    toku_free(dmt);
    *dmtp=NULL;
}

uint32_t toku_dmt_size(DMT V) {
    return V->size();
}

int toku_dmt_create_from_sorted_array(DMT *dmtp, DMTVALUE *values, uint32_t numvalues) {
    //TODO: implement using create_from_sorted_array when it exists

    DMT XMALLOC(dmt);
    dmt->create();
    for (uint32_t i = 0; i < numvalues; i++) {
        toku_dmt_insert_at(dmt, values[i], i);
    }
    //dmt->create_from_sorted_array(values, numvalues);
    *dmtp=dmt;
    return 0;
}

int toku_dmt_insert_at(DMT dmt, DMTVALUE value, uint32_t index) {
    toku::dmt_functor<DMTVALUE> functor(value);
    return dmt->insert_at(functor, index);
}

int toku_dmt_set_at (DMT dmt, DMTVALUE value, uint32_t index) {
    int r = dmt->delete_at(index);
    if (r!=0) return r;
    return toku_dmt_insert_at(dmt, value, index);
}

int toku_dmt_delete_at(DMT dmt, uint32_t index) {
    return dmt->delete_at(index);
}

int toku_dmt_fetch(DMT dmt, uint32_t i, DMTVALUE *v) {
    uint32_t size;
    return dmt->fetch(i, &size, v);
}

struct functor {
    int (*f)(DMTVALUE, uint32_t, void *);
    void *v;
};
static_assert(std::is_pod<functor>::value, "not POD");

int call_functor(const uint32_t size, const DMTVALUE &v, uint32_t idx, functor *const ftor);
int call_functor(const uint32_t size, const DMTVALUE &v, uint32_t idx, functor *const ftor) {
    invariant(size == sizeof(DMTVALUE));
    return ftor->f(const_cast<DMTVALUE>(v), idx, ftor->v);
}

int toku_dmt_iterate(DMT dmt, int (*f)(DMTVALUE, uint32_t, void*), void*v) {
    struct functor ftor = { .f = f, .v = v };
    return dmt->iterate<functor, call_functor>(&ftor);
}

int toku_dmt_iterate_on_range(DMT dmt, uint32_t left, uint32_t right, int (*f)(DMTVALUE, uint32_t, void*), void*v) {
    struct functor ftor = { .f = f, .v = v };
    return dmt->iterate_on_range<functor, call_functor>(left, right, &ftor);
}

struct heftor {
    int (*h)(DMTVALUE, void *v);
    void *v;
};
static_assert(std::is_pod<heftor>::value, "not POD");

int call_heftor(const uint32_t size, const DMTVALUE &v, const heftor &htor);
int call_heftor(const uint32_t size, const DMTVALUE &v, const heftor &htor) {
    invariant(size == sizeof(DMTVALUE));
    return htor.h(const_cast<DMTVALUE>(v), htor.v);
}

int toku_dmt_insert(DMT dmt, DMTVALUE value, int(*h)(DMTVALUE, void*v), void *v, uint32_t *index) {
    struct heftor htor = { .h = h, .v = v };
    toku::dmt_functor<DMTVALUE> functor(value);
    return dmt->insert<heftor, call_heftor>(functor, htor, index);
}

int toku_dmt_find_zero(DMT V, int (*h)(DMTVALUE, void*extra), void*extra, DMTVALUE *value, uint32_t *index) {
    struct heftor htor = { .h = h, .v = extra };
    uint32_t ignore;
    return V->find_zero<heftor, call_heftor>(htor, &ignore, value, index);
}

int toku_dmt_find(DMT V, int (*h)(DMTVALUE, void*extra), void*extra, int direction, DMTVALUE *value, uint32_t *index) {
    struct heftor htor = { .h = h, .v = extra };
    uint32_t ignore;
    return V->find<heftor, call_heftor>(htor, direction, &ignore, value, index);
}

int toku_dmt_split_at(DMT dmt, DMT *newdmtp, uint32_t index) {
    //TODO: use real split_at when it exists
    if (index > dmt->size()) { return EINVAL; }
    DMT XMALLOC(newdmt);
    newdmt->create();
    int r;

    for (uint32_t i = index; i < dmt->size(); i++) {
        DMTVALUE v;
        r = toku_dmt_fetch(dmt, i, &v);
        invariant_zero(r);
        r = toku_dmt_insert_at(newdmt, v, i-index);
        invariant_zero(r);
    }
    if (dmt->size() > 0) {
        for (uint32_t i = dmt->size(); i > index; i--) {
            r = toku_dmt_delete_at(dmt, i-1);
            invariant_zero(r);
        }
    }
    r = 0;

#if 0
    int r = dmt->split_at(newdmt, index);
#endif
    if (r != 0) {
        toku_free(newdmt);
    } else {
        *newdmtp = newdmt;
    }
    return r;
}

int toku_dmt_merge(DMT leftdmt, DMT rightdmt, DMT *newdmtp) {
    //TODO: use real merge when it exists
    DMT XMALLOC(newdmt);
    newdmt->create();
    int r;
    for (uint32_t i = 0; i < leftdmt->size(); i++) {
        DMTVALUE v;
        r = toku_dmt_fetch(leftdmt, i, &v);
        invariant_zero(r);
        r = toku_dmt_insert_at(newdmt, v, i);
        invariant_zero(r);
    }
    uint32_t offset = leftdmt->size();
    for (uint32_t i = 0; i < rightdmt->size(); i++) {
        DMTVALUE v;
        r = toku_dmt_fetch(rightdmt, i, &v);
        invariant_zero(r);
        r = toku_dmt_insert_at(newdmt, v, i+offset);
        invariant_zero(r);
    }
    leftdmt->destroy();
    rightdmt->destroy();

//    newdmt->merge(leftdmt, rightdmt);

    toku_free(leftdmt);
    toku_free(rightdmt);
    *newdmtp = newdmt;
    return 0;
}

int toku_dmt_clone_noptr(DMT *dest, DMT src) {
    DMT XMALLOC(dmt);
    dmt->clone(*src);
    *dest = dmt;
    return 0;
}

void toku_dmt_clear(DMT dmt) {
    dmt->clear();
}

size_t toku_dmt_memory_size (DMT dmt) {
    return dmt->memory_size();
}

