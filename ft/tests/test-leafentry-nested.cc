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
#include <toku_portability.h>
#include <string.h>

#include "test.h"
#include "fttypes.h"

#include "ule.h"
#include "ule-internal.h"

enum {MAX_SIZE = 256};
static XIDS nested_xids[MAX_TRANSACTION_RECORDS];

static void
verify_ule_equal(ULE a, ULE b) {
    assert(a->num_cuxrs > 0);
    assert(a->num_puxrs < MAX_TRANSACTION_RECORDS);
    assert(a->num_cuxrs == b->num_cuxrs);
    assert(a->num_puxrs == b->num_puxrs);
    uint32_t i;
    for (i = 0; i < (a->num_cuxrs + a->num_puxrs); i++) {
        assert(a->uxrs[i].type == b->uxrs[i].type);
        assert(a->uxrs[i].xid  == b->uxrs[i].xid);
        if (a->uxrs[i].type == XR_INSERT) {
            assert(a->uxrs[i].vallen  == b->uxrs[i].vallen);
            assert(memcmp(a->uxrs[i].valp, b->uxrs[i].valp, a->uxrs[i].vallen) == 0);
        }
    }
}

static void
verify_le_equal(LEAFENTRY a, LEAFENTRY b) {
    if (a==NULL) assert(b==NULL);
    else {
        assert(b!=NULL);

        size_t size = leafentry_memsize(a);
        assert(size==leafentry_memsize(b));

        assert(memcmp(a, b, size) == 0);

        ULE_S ule_a;
        ULE_S ule_b;

        le_unpack(&ule_a, a);
        le_unpack(&ule_b, b);
        verify_ule_equal(&ule_a, &ule_b);
        ule_cleanup(&ule_a);
        ule_cleanup(&ule_b);
    }
}

static void
fillrandom(uint8_t buf[MAX_SIZE], uint32_t length) {
    assert(length < MAX_SIZE);
    uint32_t i;
    for (i = 0; i < length; i++) {
        buf[i] = random() & 0xFF;
    } 
}

static void
test_le_offset_is(LEAFENTRY le, void *field, size_t expected_offset) {
    size_t le_address    = (size_t) le;
    size_t field_address = (size_t) field;
    assert(field_address >= le_address);
    size_t actual_offset = field_address - le_address;
    assert(actual_offset == expected_offset);
}

//Fixed offsets in a packed leafentry.
enum {
    LE_OFFSET_NUM      = 0,
    LE_OFFSET_VARIABLE = 1+LE_OFFSET_NUM
};

static void
test_le_fixed_offsets (void) {
    LEAFENTRY XMALLOC(le);
    test_le_offset_is(le, &le->type,                       LE_OFFSET_NUM);
    toku_free(le);
}

//Fixed offsets in a leafentry with no uncommitted transaction records.
//(Note, there is no type required.) 
enum {
    LE_COMMITTED_OFFSET_VALLEN = LE_OFFSET_VARIABLE,
    LE_COMMITTED_OFFSET_VAL    = 4 + LE_COMMITTED_OFFSET_VALLEN
};

static void
test_le_committed_offsets (void) {
    LEAFENTRY XMALLOC(le);
    test_le_offset_is(le, &le->u.clean.vallen, LE_COMMITTED_OFFSET_VALLEN);
    test_le_offset_is(le, &le->u.clean.val, LE_COMMITTED_OFFSET_VAL);
    toku_free(le);
}

//Fixed offsets in a leafentry with uncommitted transaction records.
enum {
    LE_MVCC_OFFSET_NUM_CUXRS   =  LE_OFFSET_VARIABLE, //Type of innermost record
    LE_MVCC_OFFSET_NUM_PUXRS    = 4+LE_MVCC_OFFSET_NUM_CUXRS, //XID of outermost noncommitted record
    LE_MVCC_OFFSET_XRS    = 1+LE_MVCC_OFFSET_NUM_PUXRS
};

static void
test_le_provisional_offsets (void) {
    LEAFENTRY XMALLOC(le);
    test_le_offset_is(le, &le->u.mvcc.num_cxrs,            LE_MVCC_OFFSET_NUM_CUXRS);
    test_le_offset_is(le, &le->u.mvcc.num_pxrs, LE_MVCC_OFFSET_NUM_PUXRS);
    test_le_offset_is(le, &le->u.mvcc.xrs,               LE_MVCC_OFFSET_XRS);
    toku_free(le);
}

//We use a packed struct to represent a leafentry.
//We want to make sure the compiler correctly represents the offsets.
//This test verifies all offsets in a packed leafentry correspond to the required memory format.
static void
test_le_offsets (void) {
    test_le_fixed_offsets();
    test_le_committed_offsets();
    test_le_provisional_offsets();
}

static void
test_ule_packs_to_nothing (ULE ule) {
    LEAFENTRY le;
    int r = le_pack(ule, NULL, 0, NULL, 0, 0, &le);
    assert(r==0);
    assert(le==NULL);
}

//A leafentry must contain at least one 'insert' (all deletes means the leafentry
//should not exist).
//Verify that 'le_pack' of any set of all deletes ends up not creating a leafentry.
static void
test_le_empty_packs_to_nothing (void) {
    ULE_S ule;
    ule.uxrs = ule.uxrs_static;

    //Set up defaults.
    int committed;
    for (committed = 1; committed < MAX_TRANSACTION_RECORDS; committed++) {
        int32_t num_xrs;

        for (num_xrs = committed; num_xrs < MAX_TRANSACTION_RECORDS; num_xrs++) {
            ule.num_cuxrs = committed;
            ule.num_puxrs = num_xrs - committed;
            if (num_xrs == 1) {
                ule.uxrs[num_xrs-1].xid    = TXNID_NONE;
            }
            else {
                ule.uxrs[num_xrs-1].xid    = ule.uxrs[num_xrs-2].xid + (random() % 32 + 1); //Abitrary number, xids must be strictly increasing
            }
            ule.uxrs[num_xrs-1].type = XR_DELETE;
            test_ule_packs_to_nothing(&ule);
            if (num_xrs > 2 && num_xrs > committed && num_xrs % 4) {
                //Set some of them to placeholders instead of deletes
                ule.uxrs[num_xrs-2].type = XR_PLACEHOLDER;
            }
            test_ule_packs_to_nothing(&ule);
        }
    }

}

static void
le_verify_accessors(LEAFENTRY le, ULE ule, size_t pre_calculated_memsize) {
    assert(le);
    assert(ule->num_cuxrs > 0);
    assert(ule->num_puxrs <= MAX_TRANSACTION_RECORDS);
    assert(ule->uxrs[ule->num_cuxrs + ule->num_puxrs-1].type != XR_PLACEHOLDER);
    //Extract expected values from ULE
    size_t memsize  = le_memsize_from_ule(ule);
    size_t num_uxrs = ule->num_cuxrs + ule->num_puxrs;

    void *latest_val        = ule->uxrs[num_uxrs -1].type == XR_DELETE ? NULL : ule->uxrs[num_uxrs -1].valp;
    uint32_t latest_vallen = ule->uxrs[num_uxrs -1].type == XR_DELETE ? 0    : ule->uxrs[num_uxrs -1].vallen;
    {
        int i;
        for (i = num_uxrs - 1; i >= 0; i--) {
            if (ule->uxrs[i].type == XR_INSERT) {
                goto found_insert;
            }
        }
        assert(false);
    }
found_insert:;
    TXNID outermost_uncommitted_xid = ule->num_puxrs == 0 ? TXNID_NONE : ule->uxrs[ule->num_cuxrs].xid;
    int   is_provdel = ule->uxrs[num_uxrs-1].type == XR_DELETE;

    assert(le!=NULL);
    //Verify all accessors
    assert(memsize  == pre_calculated_memsize);
    assert(memsize  == leafentry_memsize(le));
    {
        uint32_t test_vallen;
        void*     test_valp = le_latest_val_and_len(le, &test_vallen);
        if (latest_val != NULL) assert(test_valp != latest_val);
        assert(test_vallen == latest_vallen);
        assert(memcmp(test_valp, latest_val, test_vallen) == 0);
        assert(le_latest_val(le)    == test_valp);
        assert(le_latest_vallen(le) == test_vallen);
    }
    {
        assert(le_outermost_uncommitted_xid(le) == outermost_uncommitted_xid);
    }
    {
        assert((le_latest_is_del(le)==0) == (is_provdel==0));
    }
}



static void
test_le_pack_committed (void) {
    ULE_S ule;
    ule.uxrs = ule.uxrs_static;

    uint8_t val[MAX_SIZE];
    uint32_t valsize;
    for (valsize = 0; valsize < MAX_SIZE; valsize += (random() % MAX_SIZE) + 1) {
        fillrandom(val, valsize);

        ule.num_cuxrs       = 1;
        ule.num_puxrs       = 0;
        ule.uxrs[0].type   = XR_INSERT;
        ule.uxrs[0].xid    = 0;
        ule.uxrs[0].valp   = val;
        ule.uxrs[0].vallen = valsize;

        size_t memsize;
        LEAFENTRY le;
        int r = le_pack(&ule, nullptr, 0, nullptr, 0, 0, &le);
        assert(r==0);
        assert(le!=NULL);
        memsize = le_memsize_from_ule(&ule);
        le_verify_accessors(le, &ule, memsize);
        ULE_S tmp_ule;
        le_unpack(&tmp_ule, le);
        verify_ule_equal(&ule, &tmp_ule);
        LEAFENTRY tmp_le;
        size_t    tmp_memsize;
        r = le_pack(&tmp_ule, nullptr, 0, nullptr, 0, 0, &tmp_le);
        tmp_memsize = le_memsize_from_ule(&tmp_ule);
        assert(r==0);
        assert(tmp_memsize == memsize);
        assert(memcmp(le, tmp_le, memsize) == 0);
        le_verify_accessors(tmp_le, &tmp_ule, tmp_memsize);

        toku_free(tmp_le);
        toku_free(le);
        ule_cleanup(&tmp_ule);
    }
}

static void
test_le_pack_uncommitted (uint8_t committed_type, uint8_t prov_type, int num_placeholders) {
    ULE_S ule;
    ule.uxrs = ule.uxrs_static;
    assert(num_placeholders >= 0);

    uint8_t cval[MAX_SIZE];
    uint8_t pval[MAX_SIZE];
    uint32_t cvalsize;
    uint32_t pvalsize;
    for (cvalsize = 0; cvalsize < MAX_SIZE; cvalsize += (random() % MAX_SIZE) + 1) {
        pvalsize = (cvalsize + random()) % MAX_SIZE;
        if (committed_type == XR_INSERT)
            fillrandom(cval, cvalsize);
        if (prov_type == XR_INSERT)
            fillrandom(pval, pvalsize);
        ule.uxrs[0].type   = committed_type;
        ule.uxrs[0].xid    = TXNID_NONE;
        ule.uxrs[0].vallen = cvalsize;
        ule.uxrs[0].valp   = cval;
        ule.num_cuxrs       = 1;
        ule.num_puxrs       = 1 + num_placeholders;

        uint32_t idx;
        for (idx = 1; idx <= (uint32_t)num_placeholders; idx++) {
            ule.uxrs[idx].type = XR_PLACEHOLDER;
            ule.uxrs[idx].xid  = ule.uxrs[idx-1].xid + (random() % 32 + 1); //Abitrary number, xids must be strictly increasing
        }
        ule.uxrs[idx].xid  = ule.uxrs[idx-1].xid + (random() % 32 + 1); //Abitrary number, xids must be strictly increasing
        ule.uxrs[idx].type   = prov_type;
        ule.uxrs[idx].vallen = pvalsize;
        ule.uxrs[idx].valp   = pval;

        size_t memsize;
        LEAFENTRY le;
        int r = le_pack(&ule, nullptr, 0, nullptr, 0, 0, &le);
        assert(r==0);
        assert(le!=NULL);
        memsize = le_memsize_from_ule(&ule);
        le_verify_accessors(le, &ule, memsize);
        ULE_S tmp_ule;
        le_unpack(&tmp_ule, le);
        verify_ule_equal(&ule, &tmp_ule);
        LEAFENTRY tmp_le;
        size_t    tmp_memsize;
        r = le_pack(&tmp_ule, nullptr, 0, nullptr, 0, 0, &tmp_le);
        tmp_memsize = le_memsize_from_ule(&tmp_ule);
        assert(r==0);
        assert(tmp_memsize == memsize);
        assert(memcmp(le, tmp_le, memsize) == 0);
        le_verify_accessors(tmp_le, &tmp_ule, tmp_memsize);

        toku_free(tmp_le);
        toku_free(le);
        ule_cleanup(&tmp_ule);
    }
}

static void
test_le_pack_provpair (int num_placeholders) {
    test_le_pack_uncommitted(XR_DELETE, XR_INSERT, num_placeholders);
}

static void
test_le_pack_provdel (int num_placeholders) {
    test_le_pack_uncommitted(XR_INSERT, XR_DELETE, num_placeholders);
}

static void
test_le_pack_both (int num_placeholders) {
    test_le_pack_uncommitted(XR_INSERT, XR_INSERT, num_placeholders);
}

//Test of PACK
//  Committed leafentry
//      delete -> nothing (le_empty_packs_to_nothing)
//      insert
//          make key/val have diff lengths/content
//  Uncommitted
//      committed delete
//          followed by placeholder*, delete (le_empty_packs_to_nothing)
//          followed by placeholder*, insert
//      committed insert
//          followed by placeholder*, delete
//          followed by placeholder*, insert
//          
//  placeholder* is 0,1, or 2 placeholders
static void
test_le_pack (void) {
    test_le_empty_packs_to_nothing();
    test_le_pack_committed();
    int i;
    for (i = 0; i < 3; i++) {
        test_le_pack_provpair(i);
        test_le_pack_provdel(i);
        test_le_pack_both(i);
    }
}

static void
test_le_apply(ULE ule_initial, FT_MSG msg, ULE ule_expected) {
    int r;
    LEAFENTRY le_initial;
    LEAFENTRY le_expected;
    LEAFENTRY le_result;

    r = le_pack(ule_initial, nullptr, 0, nullptr, 0, 0, &le_initial);
    CKERR(r);

    size_t result_memsize = 0;
    int64_t ignoreme;
    txn_gc_info gc_info(nullptr, TXNID_NONE, TXNID_NONE, true);
    toku_le_apply_msg(msg,
                      le_initial,
                      nullptr,
                      0,
                      &gc_info,
                      &le_result,
                      &ignoreme);
    if (le_result) {
        result_memsize = leafentry_memsize(le_result);
        le_verify_accessors(le_result, ule_expected, result_memsize);
    }

    size_t expected_memsize = 0;
    r = le_pack(ule_expected, nullptr, 0, nullptr, 0, 0, &le_expected);
    CKERR(r);
    if (le_expected) {
        expected_memsize = leafentry_memsize(le_expected);
    }


    verify_le_equal(le_result, le_expected);
    if (le_result && le_expected) {
        assert(result_memsize  == expected_memsize);
    }
    if (le_initial)  toku_free(le_initial);
    if (le_result)   toku_free(le_result);
    if (le_expected) toku_free(le_expected);
}

static const ULE_S ule_committed_delete = {
    .num_puxrs = 0,
    .num_cuxrs = 1,
    .uxrs_static = {{
        .type   = XR_DELETE,
        .vallen = 0,
        .valp   = NULL,
        .xid    = 0
    }},
    .uxrs = (UXR_S *)ule_committed_delete.uxrs_static
};

static FT_MSG_S
msg_init(enum ft_msg_type type, XIDS xids,
         DBT *key, DBT *val) {
    FT_MSG_S msg;
    msg.type = type;
    msg.xids = xids;
    msg.u.id.key = key;
    msg.u.id.val = val;
    return msg;
}

static uint32_t
next_nesting_level(uint32_t current) {
    uint32_t rval = current + 1;

    if (current > 3 && current < MAX_TRANSACTION_RECORDS - 1) {
        rval = current + random() % 100;
        if (rval >= MAX_TRANSACTION_RECORDS)
            rval = MAX_TRANSACTION_RECORDS - 1;
    }
    return rval;
}

static void
generate_committed_for(ULE ule, DBT *val) {
    ule->num_cuxrs = 1;
    ule->num_puxrs = 0;
    ule->uxrs = ule->uxrs_static;
    ule->uxrs[0].type   = XR_INSERT;
    ule->uxrs[0].vallen = val->size;
    ule->uxrs[0].valp   = val->data;
    ule->uxrs[0].xid    = 0;
}

static void
generate_provpair_for(ULE ule, FT_MSG msg) {
    uint32_t level;
    XIDS xids = msg->xids;
    ule->uxrs = ule->uxrs_static;

    ule->num_cuxrs = 1;
    ule->num_puxrs = xids_get_num_xids(xids);
    uint32_t num_uxrs = ule->num_cuxrs + ule->num_puxrs;
    ule->uxrs[0].type   = XR_DELETE;
    ule->uxrs[0].vallen = 0;
    ule->uxrs[0].valp   = NULL;
    ule->uxrs[0].xid    = TXNID_NONE;
    for (level = 1; level < num_uxrs - 1; level++) {
        ule->uxrs[level].type   = XR_PLACEHOLDER;
        ule->uxrs[level].vallen = 0;
        ule->uxrs[level].valp   = NULL;
        ule->uxrs[level].xid    = xids_get_xid(xids, level-1);
    }
    ule->uxrs[num_uxrs - 1].type   = XR_INSERT;
    ule->uxrs[num_uxrs - 1].vallen = msg->u.id.val->size;
    ule->uxrs[num_uxrs - 1].valp   = msg->u.id.val->data;
    ule->uxrs[num_uxrs - 1].xid    = xids_get_innermost_xid(xids);
}

//Test all the different things that can happen to a
//non-existent leafentry (logical equivalent of a committed delete).
static void
test_le_empty_apply(void) {
    ULE_S ule_initial        = ule_committed_delete;
    FT_MSG_S msg;

    DBT key;
    DBT val;
    uint8_t keybuf[MAX_SIZE];
    uint8_t valbuf[MAX_SIZE];
    uint32_t keysize;
    uint32_t valsize;
    uint32_t  nesting_level;
    for (keysize = 0; keysize < MAX_SIZE; keysize += (random() % MAX_SIZE) + 1) {
        for (valsize = 0; valsize < MAX_SIZE; valsize += (random() % MAX_SIZE) + 1) {
            for (nesting_level = 0;
                 nesting_level < MAX_TRANSACTION_RECORDS;
                 nesting_level = next_nesting_level(nesting_level)) {
                XIDS msg_xids = nested_xids[nesting_level];
                fillrandom(keybuf, keysize);
                fillrandom(valbuf, valsize);
                toku_fill_dbt(&key, keybuf, keysize);
                toku_fill_dbt(&val, valbuf, valsize);

                //COMMIT/ABORT is illegal with TXNID 0
                if (nesting_level > 0) {
                    //Abort/commit of an empty le is an empty le
                    ULE_S ule_expected = ule_committed_delete;

                    msg = msg_init(FT_COMMIT_ANY, msg_xids,  &key, &val);
                    test_le_apply(&ule_initial, &msg, &ule_expected);
                    msg = msg_init(FT_COMMIT_BROADCAST_TXN, msg_xids,  &key, &val);
                    test_le_apply(&ule_initial, &msg, &ule_expected);

                    msg = msg_init(FT_ABORT_ANY, msg_xids, &key, &val);
                    test_le_apply(&ule_initial, &msg, &ule_expected);
                    msg = msg_init(FT_ABORT_BROADCAST_TXN, msg_xids, &key, &val);
                    test_le_apply(&ule_initial, &msg, &ule_expected);
                }
                {
                    //delete of an empty le is an empty le
                    ULE_S ule_expected = ule_committed_delete;

                    msg = msg_init(FT_DELETE_ANY, msg_xids, &key, &val);
                    test_le_apply(&ule_initial, &msg, &ule_expected);
                }
                {
                    msg = msg_init(FT_INSERT, msg_xids, &key, &val);
                    ULE_S ule_expected;
                    generate_provpair_for(&ule_expected, &msg);
                    test_le_apply(&ule_initial, &msg, &ule_expected);
                }
                {
                    msg = msg_init(FT_INSERT_NO_OVERWRITE, msg_xids, &key, &val);
                    ULE_S ule_expected;
                    generate_provpair_for(&ule_expected, &msg);
                    test_le_apply(&ule_initial, &msg, &ule_expected);
                }
            }
        }
    }
}

static void
generate_provdel_for(ULE ule, FT_MSG msg) {
    uint32_t level;
    XIDS xids = msg->xids;

    ule->num_cuxrs = 1;
    ule->num_puxrs = xids_get_num_xids(xids);
    uint32_t num_uxrs = ule->num_cuxrs + ule->num_puxrs;
    ule->uxrs[0].type   = XR_INSERT;
    ule->uxrs[0].vallen = msg->u.id.val->size;
    ule->uxrs[0].valp   = msg->u.id.val->data;
    ule->uxrs[0].xid    = TXNID_NONE;
    for (level = ule->num_cuxrs; level < ule->num_cuxrs + ule->num_puxrs - 1; level++) {
        ule->uxrs[level].type   = XR_PLACEHOLDER;
        ule->uxrs[level].vallen = 0;
        ule->uxrs[level].valp   = NULL;
        ule->uxrs[level].xid    = xids_get_xid(xids, level-1);
    }
    ule->uxrs[num_uxrs - 1].type   = XR_DELETE;
    ule->uxrs[num_uxrs - 1].vallen = 0;
    ule->uxrs[num_uxrs - 1].valp   = NULL;
    ule->uxrs[num_uxrs - 1].xid    = xids_get_innermost_xid(xids);
}

static void
generate_both_for(ULE ule, DBT *oldval, FT_MSG msg) {
    uint32_t level;
    XIDS xids = msg->xids;

    ule->num_cuxrs = 1;
    ule->num_puxrs = xids_get_num_xids(xids);
    uint32_t num_uxrs = ule->num_cuxrs + ule->num_puxrs;
    ule->uxrs[0].type   = XR_INSERT;
    ule->uxrs[0].vallen = oldval->size;
    ule->uxrs[0].valp   = oldval->data;
    ule->uxrs[0].xid    = TXNID_NONE;
    for (level = ule->num_cuxrs; level < ule->num_cuxrs + ule->num_puxrs - 1; level++) {
        ule->uxrs[level].type   = XR_PLACEHOLDER;
        ule->uxrs[level].vallen = 0;
        ule->uxrs[level].valp   = NULL;
        ule->uxrs[level].xid    = xids_get_xid(xids, level-1);
    }
    ule->uxrs[num_uxrs - 1].type   = XR_INSERT;
    ule->uxrs[num_uxrs - 1].vallen = msg->u.id.val->size;
    ule->uxrs[num_uxrs - 1].valp   = msg->u.id.val->data;
    ule->uxrs[num_uxrs - 1].xid    = xids_get_innermost_xid(xids);
}

//Test all the different things that can happen to a
//committed leafentry (logical equivalent of a committed insert).
static void
test_le_committed_apply(void) {
    ULE_S ule_initial;
    ule_initial.uxrs = ule_initial.uxrs_static;
    FT_MSG_S msg;

    DBT key;
    DBT val;
    uint8_t valbuf[MAX_SIZE];
    uint32_t valsize;
    uint32_t  nesting_level;
    for (valsize = 0; valsize < MAX_SIZE; valsize += (random() % MAX_SIZE) + 1) {
        for (nesting_level = 0;
             nesting_level < MAX_TRANSACTION_RECORDS;
             nesting_level = next_nesting_level(nesting_level)) {
            XIDS msg_xids = nested_xids[nesting_level];
            fillrandom(valbuf, valsize);
            toku_fill_dbt(&val, valbuf, valsize);

            //Generate initial ule
            generate_committed_for(&ule_initial, &val);


            //COMMIT/ABORT is illegal with TXNID 0
            if (nesting_level > 0) {
                //Commit/abort will not change a committed le
                ULE_S ule_expected = ule_initial;
                msg = msg_init(FT_COMMIT_ANY, msg_xids,  &key, &val);
                test_le_apply(&ule_initial, &msg, &ule_expected);
                msg = msg_init(FT_COMMIT_BROADCAST_TXN, msg_xids,  &key, &val);
                test_le_apply(&ule_initial, &msg, &ule_expected);

                msg = msg_init(FT_ABORT_ANY, msg_xids, &key, &val);
                test_le_apply(&ule_initial, &msg, &ule_expected);
                msg = msg_init(FT_ABORT_BROADCAST_TXN, msg_xids, &key, &val);
                test_le_apply(&ule_initial, &msg, &ule_expected);
            }

            {
                msg = msg_init(FT_DELETE_ANY, msg_xids, &key, &val);
                ULE_S ule_expected;
                ule_expected.uxrs = ule_expected.uxrs_static;
                generate_provdel_for(&ule_expected, &msg);
                test_le_apply(&ule_initial, &msg, &ule_expected);
            }

            {
                uint8_t valbuf2[MAX_SIZE];
                uint32_t valsize2 = random() % MAX_SIZE;
                fillrandom(valbuf2, valsize2);
                DBT val2;
                toku_fill_dbt(&val2, valbuf2, valsize2);
                msg = msg_init(FT_INSERT, msg_xids, &key, &val2);
                ULE_S ule_expected;
                ule_expected.uxrs = ule_expected.uxrs_static;
                generate_both_for(&ule_expected, &val, &msg);
                test_le_apply(&ule_initial, &msg, &ule_expected);
            }
            {
                //INSERT_NO_OVERWRITE will not change a committed insert
                ULE_S ule_expected = ule_initial;
                uint8_t valbuf2[MAX_SIZE];
                uint32_t valsize2 = random() % MAX_SIZE;
                fillrandom(valbuf2, valsize2);
                DBT val2;
                toku_fill_dbt(&val2, valbuf2, valsize2);
                msg = msg_init(FT_INSERT_NO_OVERWRITE, msg_xids, &key, &val2);
                test_le_apply(&ule_initial, &msg, &ule_expected);
            }
        }
    }
}

static void
test_le_apply_messages(void) {
    test_le_empty_apply();
    test_le_committed_apply();
}

static bool ule_worth_running_garbage_collection(ULE ule, TXNID oldest_referenced_xid_known) {
    LEAFENTRY le;
    int r = le_pack(ule, nullptr, 0, nullptr, 0, 0, &le); CKERR(r);
    invariant_notnull(le);
    txn_gc_info gc_info(nullptr, oldest_referenced_xid_known, oldest_referenced_xid_known, true);
    bool worth_running = toku_le_worth_running_garbage_collection(le, &gc_info);
    toku_free(le);
    return worth_running;
}

static void test_le_garbage_collection_birdie(void) {
    DBT key;
    DBT val;
    ULE_S ule;
    uint8_t keybuf[MAX_SIZE];
    uint32_t keysize=8;
    uint8_t valbuf[MAX_SIZE];
    uint32_t valsize=8;
    bool do_garbage_collect;

    memset(&key, 0, sizeof(key));
    memset(&val, 0, sizeof(val));
    fillrandom(keybuf, keysize);
    fillrandom(valbuf, valsize);
    memset(&ule, 0, sizeof(ule));
    ule.uxrs = ule.uxrs_static;

    //
    // Test garbage collection "worth-doing" heurstic
    //

    // Garbage collection should not be worth doing on a clean leafentry.
    ule.num_cuxrs = 1;
    ule.num_puxrs = 0;
    ule.uxrs[0].xid = TXNID_NONE;
    ule.uxrs[0].type = XR_INSERT;
    do_garbage_collect = ule_worth_running_garbage_collection(&ule, 200);
    invariant(!do_garbage_collect);
    
    // It is worth doing when there is more than one committed entry
    ule.num_cuxrs = 2;
    ule.num_puxrs = 1;
    ule.uxrs[1].xid = 500;
    do_garbage_collect = ule_worth_running_garbage_collection(&ule, 200);
    invariant(do_garbage_collect);
    
    // It is not worth doing when there is one of each, when the
    // provisional entry is newer than the oldest known referenced xid
    ule.num_cuxrs = 1;
    ule.num_puxrs = 1;
    ule.uxrs[1].xid = 1500;
    do_garbage_collect = ule_worth_running_garbage_collection(&ule, 200);
    invariant(!do_garbage_collect);
    ule.uxrs[1].xid = 200;
    do_garbage_collect = ule_worth_running_garbage_collection(&ule, 200);
    invariant(!do_garbage_collect);

    // It is not worth doing when there is only one committed entry,
    // multiple provisional entries, but the outermost entry is newer.
    ule.num_cuxrs = 1;
    ule.num_puxrs = 3;
    ule.uxrs[1].xid = 201;
    ule.uxrs[2].xid = 206;
    ule.uxrs[3].xid = 215;
    do_garbage_collect = ule_worth_running_garbage_collection(&ule, 200);
    invariant(!do_garbage_collect);

    // It is worth doing when the above scenario has an outermost entry
    // older than the oldest known, even if its children seem newer.
    // this children must have commit because the parent is not live.
    ule.num_cuxrs = 1;
    ule.num_puxrs = 3;
    ule.uxrs[1].xid = 190;
    ule.uxrs[2].xid = 206;
    ule.uxrs[3].xid = 215;
    do_garbage_collect = ule_worth_running_garbage_collection(&ule, 200);
    invariant(do_garbage_collect);

    // It is worth doing when there is more than one committed entry,
    // even if a provisional entry exists that is newer than the
    // oldest known refrenced xid
    ule.num_cuxrs = 2;
    ule.num_puxrs = 1;
    ule.uxrs[1].xid = 499;
    ule.uxrs[2].xid = 500;
    do_garbage_collect = ule_worth_running_garbage_collection(&ule, 200);
    invariant(do_garbage_collect);

    // It is worth doing when there is one of each, and the provisional
    // entry is older than the oldest known referenced xid
    ule.num_cuxrs = 1;
    ule.num_puxrs = 1;
    ule.uxrs[1].xid = 199;
    do_garbage_collect = ule_worth_running_garbage_collection(&ule, 200);
    invariant(do_garbage_collect);

    // It is definately worth doing when the above case is true
    // and there is more than one provisional entry.
    ule.num_cuxrs = 1;
    ule.num_puxrs = 2;
    ule.uxrs[1].xid = 150;
    ule.uxrs[2].xid = 175;
    do_garbage_collect = ule_worth_running_garbage_collection(&ule, 200);
    invariant(do_garbage_collect);
}

static void test_le_optimize(void) {
    FT_MSG_S msg;
    DBT key;
    DBT val;
    ULE_S ule_initial;
    ULE_S ule_expected;
    uint8_t keybuf[MAX_SIZE];
    uint32_t keysize=8;
    uint8_t valbuf[MAX_SIZE];
    uint32_t valsize=8;
    ule_initial.uxrs = ule_initial.uxrs_static;
    ule_expected.uxrs = ule_expected.uxrs_static;
    TXNID optimize_txnid = 1000;
    memset(&key, 0, sizeof(key));
    memset(&val, 0, sizeof(val));
    XIDS root_xids = xids_get_root_xids();
    XIDS msg_xids; 
    int r = xids_create_child(root_xids, &msg_xids, optimize_txnid);
    assert(r==0);
    msg = msg_init(FT_OPTIMIZE, msg_xids, &key, &val);

    //
    // create the key
    //
    fillrandom(keybuf, keysize);
    fillrandom(valbuf, valsize);

    //
    // test a clean leafentry has no effect
    //
    ule_initial.num_cuxrs = 1;
    ule_initial.num_puxrs = 0;
    ule_initial.uxrs[0].type = XR_INSERT;
    ule_initial.uxrs[0].xid = TXNID_NONE;
    ule_initial.uxrs[0].vallen = valsize;
    ule_initial.uxrs[0].valp = valbuf;
    
    ule_expected.num_cuxrs = 1;
    ule_expected.num_puxrs = 0;
    ule_expected.uxrs[0].type = XR_INSERT;
    ule_expected.uxrs[0].xid = TXNID_NONE;
    ule_expected.uxrs[0].vallen = valsize;
    ule_expected.uxrs[0].valp = valbuf;

    test_msg_modify_ule(&ule_initial,&msg);
    verify_ule_equal(&ule_initial,&ule_expected);

    //
    // add another committed entry and ensure no effect
    //
    ule_initial.num_cuxrs = 2;
    ule_initial.uxrs[1].type = XR_DELETE;
    ule_initial.uxrs[1].xid = 500;
    ule_initial.uxrs[1].vallen = 0;
    ule_initial.uxrs[1].valp = NULL;

    ule_expected.num_cuxrs = 2;
    ule_expected.uxrs[1].type = XR_DELETE;
    ule_expected.uxrs[1].xid = 500;
    ule_expected.uxrs[1].vallen = 0;
    ule_expected.uxrs[1].valp = NULL;
    
    test_msg_modify_ule(&ule_initial,&msg);
    verify_ule_equal(&ule_initial,&ule_expected);

    //
    // now test when there is one provisional, three cases, after, equal, and before FT_OPTIMIZE's transaction
    //
    ule_initial.num_cuxrs = 1;
    ule_initial.num_puxrs = 1;
    ule_initial.uxrs[1].xid = 1500;

    ule_expected.num_cuxrs = 1;
    ule_expected.num_puxrs = 1;
    ule_expected.uxrs[1].xid = 1500;
    test_msg_modify_ule(&ule_initial,&msg);
    verify_ule_equal(&ule_initial,&ule_expected);

    ule_initial.uxrs[1].xid = 1000;
    ule_expected.uxrs[1].xid = 1000;
    test_msg_modify_ule(&ule_initial,&msg);
    verify_ule_equal(&ule_initial,&ule_expected);

    ule_initial.uxrs[1].xid = 500;
    ule_expected.uxrs[1].xid = 500;
    ule_expected.num_cuxrs = 2;
    ule_expected.num_puxrs = 0;
    test_msg_modify_ule(&ule_initial,&msg);
    verify_ule_equal(&ule_initial,&ule_expected);

    //
    // now test cases with two provisional
    //
    ule_initial.num_cuxrs = 1;
    ule_initial.num_puxrs = 2;
    ule_expected.num_cuxrs = 1;
    ule_expected.num_puxrs = 2;

    ule_initial.uxrs[2].type = XR_INSERT;
    ule_initial.uxrs[2].xid = 1500;
    ule_initial.uxrs[2].vallen = valsize;
    ule_initial.uxrs[2].valp = valbuf;
    ule_initial.uxrs[1].xid = 1200;
    
    ule_expected.uxrs[2].type = XR_INSERT;
    ule_expected.uxrs[2].xid = 1500;
    ule_expected.uxrs[2].vallen = valsize;
    ule_expected.uxrs[2].valp = valbuf;
    ule_expected.uxrs[1].xid = 1200;
    test_msg_modify_ule(&ule_initial,&msg);
    verify_ule_equal(&ule_initial,&ule_expected);

    ule_initial.uxrs[1].xid = 1000;
    ule_expected.uxrs[1].xid = 1000;
    test_msg_modify_ule(&ule_initial,&msg);
    verify_ule_equal(&ule_initial,&ule_expected);

    ule_initial.uxrs[1].xid = 800;
    ule_expected.uxrs[1].xid = 800;
    ule_expected.num_cuxrs = 2;
    ule_expected.num_puxrs = 0;
    ule_expected.uxrs[1].type = ule_initial.uxrs[2].type;
    ule_expected.uxrs[1].valp = ule_initial.uxrs[2].valp;
    ule_expected.uxrs[1].vallen = ule_initial.uxrs[2].vallen;
    test_msg_modify_ule(&ule_initial,&msg);
    verify_ule_equal(&ule_initial,&ule_expected);

    
    xids_destroy(&msg_xids);
    xids_destroy(&root_xids);
}

//TODO: #1125 tests:
//      Will probably have to expose ULE_S definition
//            - Check memsize function is correct
//             - Assert == disksize (almost useless, but go ahead)
//            - Check standard accessors
//             - le_latest_val_and_len
//             - le_latest_val 
//             - le_latest_vallen
//             - le_key_and_len
//             - le_innermost_inserted_val_and_len
//             - le_innermost_inserted_val 
//             - le_innermost_inserted_vallen
//            - Check le_outermost_uncommitted_xid
//            - Check le_latest_is_del
//            - Check unpack+pack memcmps equal
//            - Check exact memory expected (including size) for various leafentry types.
//            - Check apply_msg logic
//             - Known start, known expected.. various types.
//            - Go through test-leafentry10.c
//             - Verify we have tests for all analogous stuff.
//
//  PACK
//  UNPACK
//      verify pack+unpack is no-op
//      verify unpack+pack is no-op
//  accessors
//  Test apply_msg logic
//      i.e. start with LE, apply message
//          in parallel, construct the expected ULE manually, and pack that
//          Compare the two results
//  Test full_promote

static void
init_xids(void) {
    uint32_t i;
    nested_xids[0] = xids_get_root_xids();
    for (i = 1; i < MAX_TRANSACTION_RECORDS; i++) {
        int r = xids_create_child(nested_xids[i-1], &nested_xids[i], i * 37 + random() % 36);
        assert(r==0);
    }
}

static void
destroy_xids(void) {
    uint32_t i;
    for (i = 0; i < MAX_TRANSACTION_RECORDS; i++) {
        xids_destroy(&nested_xids[i]);
    }
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    srandom(7); //Arbitrary seed.
    init_xids();
    test_le_offsets();
    test_le_pack();
    test_le_apply_messages();
    test_le_optimize();
    test_le_garbage_collection_birdie();
    destroy_xids();
    return 0;
}

