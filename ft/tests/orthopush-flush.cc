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

#include "test.h"


#include "ule.h"

static TOKUTXN const null_txn = 0;
static const char *fname = TOKU_TEST_FILENAME;
static txn_gc_info non_mvcc_gc_info(nullptr, TXNID_NONE, TXNID_NONE, false);
static toku::comparator dummy_cmp;

// generate size random bytes into dest
static void
rand_bytes(void *dest, int size)
{
    long *l;
    for (CAST_FROM_VOIDP(l, dest); (unsigned int) size >= (sizeof *l); ++l, size -= (sizeof *l)) {
        *l = random();
    }
    for (char *c = (char *) l; size > 0; ++c, --size) {
        *c = random() & 0xff;
    }
}

// generate size random bytes into dest, with a lot less entropy (every
// group of 4 bytes is the same)
static void
rand_bytes_limited(void *dest, int size)
{
    long *l;
    for (CAST_FROM_VOIDP(l, dest); (size_t) size >= (sizeof *l); ++l, size -= (sizeof *l)) {
        char c = random() & 0xff;
        for (char *p = (char *) l; (size_t) (p - (char *) l) < (sizeof *l); ++p) {
            *p = c;
        }
    }
    char c = random() & 0xff;
    for (char *p = (char *) l; size > 0; ++p, --size) {
        *p = c;
    }
}

// generate a random message with xids and a key starting with pfx, insert
// it in bnc, and save it in output params save and is_fresh_out
static void
insert_random_message(NONLEAF_CHILDINFO bnc, ft_msg **save, bool *is_fresh_out, XIDS xids, int pfx)
{
    int keylen = (random() % 128) + 16;
    int vallen = (random() % 128) + 16;
    void *key = toku_xmalloc(keylen + (sizeof pfx));
    void *val = toku_xmalloc(vallen);
    *(int *) key = pfx;
    rand_bytes((char *) key + (sizeof pfx), keylen);
    rand_bytes(val, vallen);
    MSN msn = next_dummymsn();
    bool is_fresh = (random() & 0x100) == 0;

    DBT keydbt, valdbt;
    toku_fill_dbt(&keydbt, key, keylen + (sizeof pfx));
    toku_fill_dbt(&valdbt, val, vallen);
    *save = new ft_msg(&keydbt, &valdbt, FT_INSERT, msn, xids);
    *is_fresh_out = is_fresh;

    toku_bnc_insert_msg(bnc, key, keylen + (sizeof pfx), val, vallen,
                        FT_INSERT, msn, xids, is_fresh,
                        dummy_cmp);
}

// generate a random message with xids and a key starting with pfx, insert
// it into blb, and save it in output param save
static void
insert_random_message_to_bn(
    FT_HANDLE t,
    BASEMENTNODE blb,
    void** keyp,
    uint32_t* keylenp,
    LEAFENTRY *save,
    XIDS xids,
    int pfx
    )
{
    int keylen = (random() % 16) + 16;
    int vallen = (random() % 128) + 16;
    uint32_t *pfxp;
    char key[(sizeof *pfxp) + keylen];
    char val[vallen];
    pfxp = (uint32_t *) &key[0];
    *pfxp = pfx;
    char *randkeyp = &key[sizeof *pfxp];
    rand_bytes_limited(randkeyp, keylen);
    rand_bytes(val, vallen);
    MSN msn = next_dummymsn();

    DBT keydbt_s, *keydbt, valdbt_s, *valdbt;
    keydbt = &keydbt_s;
    valdbt = &valdbt_s;
    toku_fill_dbt(keydbt, key, (sizeof *pfxp) + keylen);
    toku_fill_dbt(valdbt, val, vallen);
    *keylenp = keydbt->size;
    *keyp = toku_xmemdup(keydbt->data, keydbt->size);
    ft_msg msg(keydbt, valdbt, FT_INSERT, msn, xids);
    int64_t numbytes;
    toku_le_apply_msg(msg, NULL, NULL, 0, keydbt->size, &non_mvcc_gc_info, save, &numbytes);
    toku_ft_bn_apply_msg(t->ft->cmp, t->ft->update_fun, blb, msg, &non_mvcc_gc_info, NULL, NULL);
    if (msn.msn > blb->max_msn_applied.msn) {
        blb->max_msn_applied = msn;
    }
}

// generate a random message with xids and a key starting with pfx, insert
// it into blb1 and also into blb2, and save it in output param save
//
// used for making two leaf nodes the same in order to compare the result
// of 'maybe_apply' and a normal buffer flush
static void
insert_same_message_to_bns(
    FT_HANDLE t,
    BASEMENTNODE blb1,
    BASEMENTNODE blb2,
    void** keyp,
    uint32_t* keylenp,
    LEAFENTRY *save,
    XIDS xids,
    int pfx
    )
{
    int keylen = (random() % 16) + 16;
    int vallen = (random() % 128) + 16;
    uint32_t *pfxp;
    char key[(sizeof *pfxp) + keylen];
    char val[vallen];
    pfxp = (uint32_t *) &key[0];
    *pfxp = pfx;
    char *randkeyp = &key[sizeof *pfxp];
    rand_bytes_limited(randkeyp, keylen);
    rand_bytes(val, vallen);
    MSN msn = next_dummymsn();

    DBT keydbt_s, *keydbt, valdbt_s, *valdbt;
    keydbt = &keydbt_s;
    valdbt = &valdbt_s;
    toku_fill_dbt(keydbt, key, (sizeof *pfxp) + keylen);
    toku_fill_dbt(valdbt, val, vallen);
    *keylenp = keydbt->size;
    *keyp = toku_xmemdup(keydbt->data, keydbt->size);
    ft_msg msg(keydbt, valdbt, FT_INSERT, msn, xids);
    int64_t numbytes;
    toku_le_apply_msg(msg, NULL, NULL, 0, keydbt->size, &non_mvcc_gc_info, save, &numbytes);
    toku_ft_bn_apply_msg(t->ft->cmp, t->ft->update_fun, blb1, msg, &non_mvcc_gc_info, NULL, NULL);
    if (msn.msn > blb1->max_msn_applied.msn) {
        blb1->max_msn_applied = msn;
    }
    toku_ft_bn_apply_msg(t->ft->cmp, t->ft->update_fun, blb2, msg, &non_mvcc_gc_info, NULL, NULL);
    if (msn.msn > blb2->max_msn_applied.msn) {
        blb2->max_msn_applied = msn;
    }
}

struct orthopush_flush_update_fun_extra {
    DBT new_val;
    int *num_applications;
};

static int
orthopush_flush_update_fun(DB * UU(db), const DBT *UU(key), const DBT *UU(old_val), const DBT *extra,
                           void (*set_val)(const DBT *new_val, void *set_extra), void *set_extra) {
    struct orthopush_flush_update_fun_extra *CAST_FROM_VOIDP(e, extra->data);
    (*e->num_applications)++;
    set_val(&e->new_val, set_extra);
    return 0;
}

// generate a random update message with xids and a key starting with pfx,
// insert it into blb, and save it in output param save, and update the
// max msn so far in max_msn
//
// the update message will overwrite the value with something generated
// here, and add one to the int pointed to by applied
static void
insert_random_update_message(NONLEAF_CHILDINFO bnc, ft_msg **save, bool is_fresh, XIDS xids, int pfx, int *applied, MSN *max_msn)
{
    int keylen = (random() % 16) + 16;
    int vallen = (random() % 16) + 16;
    void *key = toku_xmalloc(keylen + (sizeof pfx));
    struct orthopush_flush_update_fun_extra *XMALLOC(update_extra);
    *(int *) key = pfx;
    rand_bytes_limited((char *) key + (sizeof pfx), keylen);
    toku_fill_dbt(&update_extra->new_val, toku_xmalloc(vallen), vallen);
    rand_bytes(update_extra->new_val.data, vallen);
    update_extra->num_applications = applied;
    MSN msn = next_dummymsn();

    DBT keydbt, valdbt;
    toku_fill_dbt(&keydbt, key, keylen + (sizeof pfx));
    toku_fill_dbt(&valdbt, update_extra, sizeof *update_extra);
    *save = new ft_msg(&keydbt, &valdbt, FT_UPDATE, msn, xids);

    toku_bnc_insert_msg(bnc, key, keylen + (sizeof pfx),
                        update_extra, sizeof *update_extra,
                        FT_UPDATE, msn, xids, is_fresh,
                        dummy_cmp);
    if (msn.msn > max_msn->msn) {
        *max_msn = msn;
    }
}

// flush from one internal node to another, where both only have one
// buffer
static void
flush_to_internal(FT_HANDLE t) {
    int r;

    ft_msg **MALLOC_N(4096,parent_messages);  // 128k / 32 = 4096
    ft_msg **MALLOC_N(4096,child_messages);
    bool *MALLOC_N(4096,parent_messages_is_fresh);
    bool *MALLOC_N(4096,child_messages_is_fresh);
    memset(parent_messages_is_fresh, 0, 4096*(sizeof parent_messages_is_fresh[0]));
    memset(child_messages_is_fresh, 0, 4096*(sizeof child_messages_is_fresh[0]));

    XIDS xids_0 = toku_xids_get_root_xids();
    XIDS xids_123, xids_234;
    r = toku_xids_create_child(xids_0, &xids_123, (TXNID)123);
    CKERR(r);
    r = toku_xids_create_child(xids_0, &xids_234, (TXNID)234);
    CKERR(r);

    NONLEAF_CHILDINFO child_bnc = toku_create_empty_nl();
    int i;
    for (i = 0; toku_bnc_memory_used(child_bnc) < 128*1024; ++i) {
        insert_random_message(child_bnc, &child_messages[i], &child_messages_is_fresh[i], xids_123, 0);
    }
    int num_child_messages = i;

    NONLEAF_CHILDINFO parent_bnc = toku_create_empty_nl();
    for (i = 0; toku_bnc_memory_used(parent_bnc) < 128*1024; ++i) {
        insert_random_message(parent_bnc, &parent_messages[i], &parent_messages_is_fresh[i], xids_234, 0);
    }
    int num_parent_messages = i;

    FTNODE XMALLOC(child);
    BLOCKNUM blocknum = { 42 };
    toku_initialize_empty_ftnode(child, blocknum, 1, 1, FT_LAYOUT_VERSION, 0);
    destroy_nonleaf_childinfo(BNC(child, 0));
    set_BNC(child, 0, child_bnc);
    BP_STATE(child, 0) = PT_AVAIL;

    toku_bnc_flush_to_child(t->ft, parent_bnc, child, TXNID_NONE);

    int parent_messages_present[num_parent_messages];
    int child_messages_present[num_child_messages];
    memset(parent_messages_present, 0, sizeof parent_messages_present);
    memset(child_messages_present, 0, sizeof child_messages_present);

    struct checkit_fn {
        int num_parent_messages;
        ft_msg **parent_messages;
        int *parent_messages_present;
        bool *parent_messages_is_fresh;
        int num_child_messages;
        ft_msg **child_messages;
        int *child_messages_present;
        bool *child_messages_is_fresh;
        checkit_fn(int np, ft_msg **pm, int *npp, bool *pmf, int nc, ft_msg **cm, int *ncp, bool *cmf) :
            num_parent_messages(np), parent_messages(pm), parent_messages_present(npp), parent_messages_is_fresh(pmf),
            num_child_messages(nc), child_messages(cm), child_messages_present(ncp), child_messages_is_fresh(cmf) {
        }
        int operator()(const ft_msg &msg, bool is_fresh) {
            DBT keydbt;
            DBT valdbt;
            toku_fill_dbt(&keydbt, msg.kdbt()->data, msg.kdbt()->size);
            toku_fill_dbt(&valdbt, msg.vdbt()->data, msg.vdbt()->size);
            int found = 0;
            MSN msn = msg.msn();
            enum ft_msg_type type = msg.type();
            XIDS xids = msg.xids();
            for (int k = 0; k < num_parent_messages; ++k) {
                if (dummy_cmp(&keydbt, parent_messages[k]->kdbt()) == 0 &&
                        msn.msn == parent_messages[k]->msn().msn) {
                    assert(parent_messages_present[k] == 0);
                    assert(found == 0);
                    assert(dummy_cmp(&valdbt, parent_messages[k]->vdbt()) == 0);
                    assert(type == parent_messages[k]->type());
                    assert(toku_xids_get_innermost_xid(xids) == toku_xids_get_innermost_xid(parent_messages[k]->xids()));
                    assert(parent_messages_is_fresh[k] == is_fresh);
                    parent_messages_present[k]++;
                    found++;
                }
            }
            for (int k = 0; k < num_child_messages; ++k) {
                if (dummy_cmp(&keydbt, child_messages[k]->kdbt()) == 0 &&
                        msn.msn == child_messages[k]->msn().msn) {
                    assert(child_messages_present[k] == 0);
                    assert(found == 0);
                    assert(dummy_cmp(&valdbt, child_messages[k]->vdbt()) == 0);
                    assert(type == child_messages[k]->type());
                    assert(toku_xids_get_innermost_xid(xids) == toku_xids_get_innermost_xid(child_messages[k]->xids()));
                    assert(child_messages_is_fresh[k] == is_fresh);
                    child_messages_present[k]++;
                    found++;
                }
            }
            assert(found == 1);
            return 0;
        }
    } checkit(num_parent_messages, parent_messages, parent_messages_present, parent_messages_is_fresh,
              num_child_messages, child_messages, child_messages_present, child_messages_is_fresh);
    child_bnc->msg_buffer.iterate(checkit);

    for (i = 0; i < num_parent_messages; ++i) {
        assert(parent_messages_present[i] == 1);
    }
    for (i = 0; i < num_child_messages; ++i) {
        assert(child_messages_present[i] == 1);
    }

    toku_xids_destroy(&xids_0);
    toku_xids_destroy(&xids_123);
    toku_xids_destroy(&xids_234);

    for (i = 0; i < num_parent_messages; ++i) {
        toku_free(parent_messages[i]->kdbt()->data);
        toku_free(parent_messages[i]->vdbt()->data);
        delete parent_messages[i];
    }
    for (i = 0; i < num_child_messages; ++i) {
        toku_free(child_messages[i]->kdbt()->data);
        toku_free(child_messages[i]->vdbt()->data);
        delete child_messages[i];
    }
    destroy_nonleaf_childinfo(parent_bnc);
    toku_ftnode_free(&child);
    toku_free(parent_messages);
    toku_free(child_messages);
    toku_free(parent_messages_is_fresh);
    toku_free(child_messages_is_fresh);
}

// flush from one internal node to another, where the child has 8 buffers
static void
flush_to_internal_multiple(FT_HANDLE t) {
    int r;

    ft_msg **MALLOC_N(4096,parent_messages);  // 128k / 32 = 4096
    ft_msg **MALLOC_N(4096,child_messages);
    bool *MALLOC_N(4096,parent_messages_is_fresh);
    bool *MALLOC_N(4096,child_messages_is_fresh);
    memset(parent_messages_is_fresh, 0, 4096*(sizeof parent_messages_is_fresh[0]));
    memset(child_messages_is_fresh, 0, 4096*(sizeof child_messages_is_fresh[0]));

    XIDS xids_0 = toku_xids_get_root_xids();
    XIDS xids_123, xids_234;
    r = toku_xids_create_child(xids_0, &xids_123, (TXNID)123);
    CKERR(r);
    r = toku_xids_create_child(xids_0, &xids_234, (TXNID)234);
    CKERR(r);

    NONLEAF_CHILDINFO child_bncs[8];
    ft_msg *childkeys[7];
    int i;
    for (i = 0; i < 8; ++i) {
        child_bncs[i] = toku_create_empty_nl();
        if (i < 7) {
            childkeys[i] = NULL;
        }
    }
    int total_size = 0;
    for (i = 0; total_size < 128*1024; ++i) {
        total_size -= toku_bnc_memory_used(child_bncs[i%8]);
        insert_random_message(child_bncs[i%8], &child_messages[i], &child_messages_is_fresh[i], xids_123, i%8);
        total_size += toku_bnc_memory_used(child_bncs[i%8]);
        if (i % 8 < 7) {
            if (childkeys[i%8] == NULL || dummy_cmp(child_messages[i]->kdbt(), childkeys[i%8]->kdbt()) > 0) {
                childkeys[i%8] = child_messages[i];
            }
        }
    }
    int num_child_messages = i;

    NONLEAF_CHILDINFO parent_bnc = toku_create_empty_nl();
    for (i = 0; toku_bnc_memory_used(parent_bnc) < 128*1024; ++i) {
        insert_random_message(parent_bnc, &parent_messages[i], &parent_messages_is_fresh[i], xids_234, 0);
    }
    int num_parent_messages = i;

    FTNODE XMALLOC(child);
    BLOCKNUM blocknum = { 42 };
    toku_initialize_empty_ftnode(child, blocknum, 1, 8, FT_LAYOUT_VERSION, 0);
    for (i = 0; i < 8; ++i) {
        destroy_nonleaf_childinfo(BNC(child, i));
        set_BNC(child, i, child_bncs[i]);
        BP_STATE(child, i) = PT_AVAIL;
        if (i < 7) {
            child->pivotkeys.insert_at(childkeys[i]->kdbt(), i);
        }
    }

    toku_bnc_flush_to_child(t->ft, parent_bnc, child, TXNID_NONE);

    int total_messages = 0;
    for (i = 0; i < 8; ++i) {
        total_messages += toku_bnc_n_entries(BNC(child, i));
    }
    assert(total_messages == num_parent_messages + num_child_messages);
    int parent_messages_present[num_parent_messages];
    int child_messages_present[num_child_messages];
    memset(parent_messages_present, 0, sizeof parent_messages_present);
    memset(child_messages_present, 0, sizeof child_messages_present);

    for (int j = 0; j < 8; ++j) {
        struct checkit_fn {
            int num_parent_messages;
            ft_msg **parent_messages;
            int *parent_messages_present;
            bool *parent_messages_is_fresh;
            int num_child_messages;
            ft_msg **child_messages;
            int *child_messages_present;
            bool *child_messages_is_fresh;
            checkit_fn(int np, ft_msg **pm, int *npp, bool *pmf, int nc, ft_msg **cm, int *ncp, bool *cmf) :
                num_parent_messages(np), parent_messages(pm), parent_messages_present(npp), parent_messages_is_fresh(pmf),
                num_child_messages(nc), child_messages(cm), child_messages_present(ncp), child_messages_is_fresh(cmf) {
            }
            int operator()(const ft_msg &msg, bool is_fresh) {
                DBT keydbt;
                DBT valdbt;
                toku_fill_dbt(&keydbt, msg.kdbt()->data, msg.kdbt()->size);
                toku_fill_dbt(&valdbt, msg.vdbt()->data, msg.vdbt()->size);
                int found = 0;
                MSN msn = msg.msn();
                enum ft_msg_type type = msg.type();
                XIDS xids = msg.xids();
                for (int _i = 0; _i < num_parent_messages; ++_i) {
                    if (dummy_cmp(&keydbt, parent_messages[_i]->kdbt()) == 0 &&
                            msn.msn == parent_messages[_i]->msn().msn) {
                        assert(parent_messages_present[_i] == 0);
                        assert(found == 0);
                        assert(dummy_cmp(&valdbt, parent_messages[_i]->vdbt()) == 0);
                        assert(type == parent_messages[_i]->type());
                        assert(toku_xids_get_innermost_xid(xids) == toku_xids_get_innermost_xid(parent_messages[_i]->xids()));
                        assert(parent_messages_is_fresh[_i] == is_fresh);
                        parent_messages_present[_i]++;
                        found++;
                    }
                }
                for (int _i = 0; _i < num_child_messages; ++_i) {
                    if (dummy_cmp(&keydbt, child_messages[_i]->kdbt()) == 0 &&
                            msn.msn == child_messages[_i]->msn().msn) {
                        assert(child_messages_present[_i] == 0);
                        assert(found == 0);
                        assert(dummy_cmp(&valdbt, child_messages[_i]->vdbt()) == 0);
                        assert(type == child_messages[_i]->type());
                        assert(toku_xids_get_innermost_xid(xids) == toku_xids_get_innermost_xid(child_messages[_i]->xids()));
                        assert(child_messages_is_fresh[_i] == is_fresh);
                        child_messages_present[_i]++;
                        found++;
                    }
                }
                assert(found == 1);
                return 0;
            }
        } checkit(num_parent_messages, parent_messages, parent_messages_present, parent_messages_is_fresh,
                  num_child_messages, child_messages, child_messages_present, child_messages_is_fresh);
        child_bncs[j]->msg_buffer.iterate(checkit);
    }

    for (i = 0; i < num_parent_messages; ++i) {
        assert(parent_messages_present[i] == 1);
    }
    for (i = 0; i < num_child_messages; ++i) {
        assert(child_messages_present[i] == 1);
    }

    toku_xids_destroy(&xids_0);
    toku_xids_destroy(&xids_123);
    toku_xids_destroy(&xids_234);

    for (i = 0; i < num_parent_messages; ++i) {
        toku_free(parent_messages[i]->kdbt()->data);
        toku_free(parent_messages[i]->vdbt()->data);
        delete parent_messages[i];
    }
    for (i = 0; i < num_child_messages; ++i) {
        toku_free(child_messages[i]->kdbt()->data);
        toku_free(child_messages[i]->vdbt()->data);
        delete child_messages[i];
    }
    destroy_nonleaf_childinfo(parent_bnc);
    toku_ftnode_free(&child);
    toku_free(parent_messages);
    toku_free(child_messages);
    toku_free(parent_messages_is_fresh);
    toku_free(child_messages_is_fresh);
}

// flush from one internal node to a leaf node, which has 8 basement
// nodes
//
// if make_leaf_up_to_date is true, then apply the messages that are stale
// in the parent to the leaf before doing the flush, otherwise assume the
// leaf was just read off disk
//
// if use_flush is true, use a buffer flush, otherwise, use maybe_apply
static void
flush_to_leaf(FT_HANDLE t, bool make_leaf_up_to_date, bool use_flush) {
    int r;

    ft_msg **MALLOC_N(4096,parent_messages);  // 128k / 32 = 4096
    LEAFENTRY* child_messages = NULL;
    XMALLOC_N(4096,child_messages);
    void** key_pointers = NULL;
    XMALLOC_N(4096, key_pointers);
    uint32_t* keylens = NULL;
    XMALLOC_N(4096, keylens);
    bool *MALLOC_N(4096,parent_messages_is_fresh);
    memset(parent_messages_is_fresh, 0, 4096*(sizeof parent_messages_is_fresh[0]));
    int *MALLOC_N(4096,parent_messages_applied);
    memset(parent_messages_applied, 0, 4096*(sizeof parent_messages_applied[0]));

    XIDS xids_0 = toku_xids_get_root_xids();
    XIDS xids_123, xids_234;
    r = toku_xids_create_child(xids_0, &xids_123, (TXNID)123);
    CKERR(r);
    r = toku_xids_create_child(xids_0, &xids_234, (TXNID)234);
    CKERR(r);
    
    BASEMENTNODE child_blbs[8];
    DBT childkeys[7];
    int i;
    for (i = 0; i < 8; ++i) {
        child_blbs[i] = toku_create_empty_bn();
        if (i < 7) {
            toku_init_dbt(&childkeys[i]);
        }
    }

    FTNODE child = NULL;
    XMALLOC(child);
    BLOCKNUM blocknum = { 42 };
    toku_initialize_empty_ftnode(child, blocknum, 0, 8, FT_LAYOUT_VERSION, 0);
    for (i = 0; i < 8; ++i) {
        destroy_basement_node(BLB(child, i));
        set_BLB(child, i, child_blbs[i]);
        BP_STATE(child, i) = PT_AVAIL;
    }

    int total_size = 0;
    for (i = 0; total_size < 128*1024; ++i) {
        total_size -= child_blbs[i%8]->data_buffer.get_memory_size();
        insert_random_message_to_bn(t, child_blbs[i%8], &key_pointers[i], &keylens[i], &child_messages[i], xids_123, i%8);
        total_size += child_blbs[i%8]->data_buffer.get_memory_size();
        if (i % 8 < 7) {
            DBT keydbt;
            if (childkeys[i%8].size == 0 || dummy_cmp(toku_fill_dbt(&keydbt, key_pointers[i], keylens[i]), &childkeys[i%8]) > 0) {
                toku_fill_dbt(&childkeys[i%8], key_pointers[i], keylens[i]);
            }
        }
    }
    int num_child_messages = i;

    for (i = 0; i < num_child_messages; ++i) {
        DBT keydbt;
        if (i % 8 < 7) {
            assert(dummy_cmp(toku_fill_dbt(&keydbt, key_pointers[i], keylens[i]), &childkeys[i%8]) <= 0);
        }
    }

    {
        int num_stale = random() % 2000;
        memset(&parent_messages_is_fresh[num_stale], true, (4096 - num_stale) * (sizeof parent_messages_is_fresh[0]));
    }
    NONLEAF_CHILDINFO parent_bnc = toku_create_empty_nl();
    MSN max_parent_msn = MIN_MSN;
    for (i = 0; toku_bnc_memory_used(parent_bnc) < 128*1024; ++i) {
        insert_random_update_message(parent_bnc, &parent_messages[i], parent_messages_is_fresh[i], xids_234, i%8, &parent_messages_applied[i], &max_parent_msn);
    }
    int num_parent_messages = i;

    for (i = 0; i < 7; ++i) {
        child->pivotkeys.insert_at(&childkeys[i], i);
    }

    if (make_leaf_up_to_date) {
        for (i = 0; i < num_parent_messages; ++i) {
            if (!parent_messages_is_fresh[i]) {
                toku_ft_leaf_apply_msg(t->ft->cmp, t->ft->update_fun, child, -1, *parent_messages[i], &non_mvcc_gc_info, NULL, NULL);
            }
        }
        for (i = 0; i < 8; ++i) {
            BLB(child, i)->stale_ancestor_messages_applied = true;
        }
    } else {
        for (i = 0; i < 8; ++i) {
            BLB(child, i)->stale_ancestor_messages_applied = false;
        }
    }

    for (i = 0; i < num_parent_messages; ++i) {
        if (make_leaf_up_to_date && !parent_messages_is_fresh[i]) {
            assert(parent_messages_applied[i] == 1);
        } else {
            assert(parent_messages_applied[i] == 0);
        }
    }

    if (use_flush) {
        toku_bnc_flush_to_child(t->ft, parent_bnc, child, TXNID_NONE);
        destroy_nonleaf_childinfo(parent_bnc);
    } else {
        FTNODE XMALLOC(parentnode);
        BLOCKNUM parentblocknum = { 17 };
        toku_initialize_empty_ftnode(parentnode, parentblocknum, 1, 1, FT_LAYOUT_VERSION, 0);
        destroy_nonleaf_childinfo(BNC(parentnode, 0));
        set_BNC(parentnode, 0, parent_bnc);
        BP_STATE(parentnode, 0) = PT_AVAIL;
        parentnode->max_msn_applied_to_node_on_disk = max_parent_msn;
        struct ancestors ancestors = { .node = parentnode, .childnum = 0, .next = NULL };
        bool msgs_applied;
        toku_apply_ancestors_messages_to_node(t, child, &ancestors, pivot_bounds::infinite_bounds(), &msgs_applied, -1);

        struct checkit_fn {
            int operator()(const ft_msg &UU(msg), bool is_fresh) {
                 assert(!is_fresh);
                return 0;
            }
        } checkit;
        parent_bnc->msg_buffer.iterate(checkit);
        invariant(parent_bnc->fresh_message_tree.size() + parent_bnc->stale_message_tree.size()
                  == (uint32_t) num_parent_messages);

        toku_ftnode_free(&parentnode);
    }

    int total_messages = 0;
    for (i = 0; i < 8; ++i) {
        total_messages += BLB_DATA(child, i)->num_klpairs();
    }
    assert(total_messages <= num_parent_messages + num_child_messages);

    for (i = 0; i < num_parent_messages; ++i) {
        assert(parent_messages_applied[i] == 1);
    }

    int parent_messages_present[num_parent_messages];
    int child_messages_present[num_child_messages];
    memset(parent_messages_present, 0, sizeof parent_messages_present);
    memset(child_messages_present, 0, sizeof child_messages_present);
    for (int j = 0; j < 8; ++j) {
        uint32_t len = BLB_DATA(child, j)->num_klpairs();
        for (uint32_t idx = 0; idx < len; ++idx) {
            LEAFENTRY le;
            DBT keydbt, valdbt;
            {
                uint32_t keylen, vallen;
                void *keyp = NULL;
                void *valp = NULL;
                r = BLB_DATA(child, j)->fetch_klpair(idx, &le, &keylen, &keyp);
                assert_zero(r);
                valp = le_latest_val_and_len(le, &vallen);
                toku_fill_dbt(&keydbt, keyp, keylen);
                toku_fill_dbt(&valdbt, valp, vallen);
            }
            int found = 0;
            for (i = num_parent_messages - 1; i >= 0; --i) {
                if (dummy_cmp(&keydbt, parent_messages[i]->kdbt()) == 0) {
                    if (found == 0) {
                        struct orthopush_flush_update_fun_extra *CAST_FROM_VOIDP(e, parent_messages[i]->vdbt()->data);
                        assert(dummy_cmp(&valdbt, &e->new_val) == 0);
                        found++;
                    }
                    assert(parent_messages_present[i] == 0);
                    parent_messages_present[i]++;
                }
            }
            for (i = j + (~7 & (num_child_messages - 1)); i >= 0; i -= 8) {
                if (i >= num_child_messages) { continue; }
                DBT childkeydbt, childvaldbt;
                {
                    uint32_t vallen;
                    void *valp = le_latest_val_and_len(child_messages[i], &vallen);
                    toku_fill_dbt(&childkeydbt, key_pointers[i], keylens[i]);
                    toku_fill_dbt(&childvaldbt, valp, vallen);
                }
                if (dummy_cmp(&keydbt, &childkeydbt) == 0) {
                    if (found == 0) {
                        assert(dummy_cmp(&valdbt, &childvaldbt) == 0);
                        found++;
                    }
                    assert(child_messages_present[i] == 0);
                    child_messages_present[i]++;
                }
            }
        }
    }

    for (i = 0; i < num_parent_messages; ++i) {
        assert(parent_messages_present[i] == 1);
    }
    for (i = 0; i < num_child_messages; ++i) {
        assert(child_messages_present[i] == 1);
    }

    toku_xids_destroy(&xids_0);
    toku_xids_destroy(&xids_123);
    toku_xids_destroy(&xids_234);

    for (i = 0; i < num_parent_messages; ++i) {
        toku_free(parent_messages[i]->kdbt()->data);
        struct orthopush_flush_update_fun_extra *CAST_FROM_VOIDP(extra, parent_messages[i]->vdbt()->data);
        toku_free(extra->new_val.data);
        toku_free(parent_messages[i]->vdbt()->data);
        delete parent_messages[i];
    }
    for (i = 0; i < num_child_messages; ++i) {
        toku_free(child_messages[i]);
        toku_free(key_pointers[i]);
    }
    toku_ftnode_free(&child);
    toku_free(parent_messages);
    toku_free(key_pointers);
    toku_free(keylens);
    toku_free(child_messages);
    toku_free(parent_messages_is_fresh);
    toku_free(parent_messages_applied);
}

// flush from one internal node to a leaf node, which has 8 basement
// nodes, but only using maybe_apply, and with actual pivot bounds
//
// if make_leaf_up_to_date is true, then apply the messages that are stale
// in the parent to the leaf before doing the flush, otherwise assume the
// leaf was just read off disk
static void
flush_to_leaf_with_keyrange(FT_HANDLE t, bool make_leaf_up_to_date) {
    int r;

    ft_msg **MALLOC_N(4096,parent_messages);  // 128k / 32 = 4k
    LEAFENTRY* child_messages = NULL;
    XMALLOC_N(4096,child_messages);
    void** key_pointers = NULL;
    XMALLOC_N(4096, key_pointers);
    uint32_t* keylens = NULL;
    XMALLOC_N(4096, keylens);
    bool *MALLOC_N(4096,parent_messages_is_fresh);
    memset(parent_messages_is_fresh, 0, 4096*(sizeof parent_messages_is_fresh[0]));
    int *MALLOC_N(4096,parent_messages_applied);
    memset(parent_messages_applied, 0, 4096*(sizeof parent_messages_applied[0]));

    XIDS xids_0 = toku_xids_get_root_xids();
    XIDS xids_123, xids_234;
    r = toku_xids_create_child(xids_0, &xids_123, (TXNID)123);
    CKERR(r);
    r = toku_xids_create_child(xids_0, &xids_234, (TXNID)234);
    CKERR(r);

    BASEMENTNODE child_blbs[8];
    DBT childkeys[8];
    int i;
    for (i = 0; i < 8; ++i) {
        child_blbs[i] = toku_create_empty_bn();
        toku_init_dbt(&childkeys[i]);
    }

    FTNODE XMALLOC(child);
    BLOCKNUM blocknum = { 42 };
    toku_initialize_empty_ftnode(child, blocknum, 0, 8, FT_LAYOUT_VERSION, 0);
    for (i = 0; i < 8; ++i) {
        destroy_basement_node(BLB(child, i));
        set_BLB(child, i, child_blbs[i]);
        BP_STATE(child, i) = PT_AVAIL;
    }

    int total_size = 0;
    for (i = 0; total_size < 128*1024; ++i) {
        total_size -= child_blbs[i%8]->data_buffer.get_memory_size();
        insert_random_message_to_bn(t, child_blbs[i%8], &key_pointers[i], &keylens[i], &child_messages[i], xids_123, i%8);
        total_size += child_blbs[i%8]->data_buffer.get_memory_size();
        DBT keydbt;
        if (childkeys[i%8].size == 0 || dummy_cmp(toku_fill_dbt(&keydbt, key_pointers[i], keylens[i]), &childkeys[i%8]) > 0) {
            toku_fill_dbt(&childkeys[i%8], key_pointers[i], keylens[i]);
        }
    }
    int num_child_messages = i;

    for (i = 0; i < num_child_messages; ++i) {
        DBT keydbt;
        assert(dummy_cmp(toku_fill_dbt(&keydbt, key_pointers[i], keylens[i]), &childkeys[i%8]) <= 0);
    }

    {
        int num_stale = random() % 2000;
        memset(&parent_messages_is_fresh[num_stale], true, (4096 - num_stale) * (sizeof parent_messages_is_fresh[0]));
    }
    NONLEAF_CHILDINFO parent_bnc = toku_create_empty_nl();
    MSN max_parent_msn = MIN_MSN;
    for (i = 0; toku_bnc_memory_used(parent_bnc) < 128*1024; ++i) {
        insert_random_update_message(parent_bnc, &parent_messages[i], parent_messages_is_fresh[i], xids_234, i%8, &parent_messages_applied[i], &max_parent_msn);
    }
    int num_parent_messages = i;

    for (i = 0; i < 7; ++i) {
        child->pivotkeys.insert_at(&childkeys[i], i);
    }

    if (make_leaf_up_to_date) {
        for (i = 0; i < num_parent_messages; ++i) {
            if (dummy_cmp(parent_messages[i]->kdbt(), &childkeys[7]) <= 0 &&
                !parent_messages_is_fresh[i]) {
                toku_ft_leaf_apply_msg(t->ft->cmp, t->ft->update_fun, child, -1, *parent_messages[i], &non_mvcc_gc_info, NULL, NULL);
            }
        }
        for (i = 0; i < 8; ++i) {
            BLB(child, i)->stale_ancestor_messages_applied = true;
        }
    } else {
        for (i = 0; i < 8; ++i) {
            BLB(child, i)->stale_ancestor_messages_applied = false;
        }
    }

    for (i = 0; i < num_parent_messages; ++i) {
        if (make_leaf_up_to_date &&
            dummy_cmp(parent_messages[i]->kdbt(), &childkeys[7]) <= 0 &&
            !parent_messages_is_fresh[i]) {
            assert(parent_messages_applied[i] == 1);
        } else {
            assert(parent_messages_applied[i] == 0);
        }
    }

    FTNODE XMALLOC(parentnode);
    BLOCKNUM parentblocknum = { 17 };
    toku_initialize_empty_ftnode(parentnode, parentblocknum, 1, 1, FT_LAYOUT_VERSION, 0);
    destroy_nonleaf_childinfo(BNC(parentnode, 0));
    set_BNC(parentnode, 0, parent_bnc);
    BP_STATE(parentnode, 0) = PT_AVAIL;
    parentnode->max_msn_applied_to_node_on_disk = max_parent_msn;
    struct ancestors ancestors = { .node = parentnode, .childnum = 0, .next = NULL };
    DBT lbe, ubi;
    toku_init_dbt(&lbe);
    toku_clone_dbt(&ubi, childkeys[7]);
    const pivot_bounds bounds(lbe, ubi);
    bool msgs_applied;
    toku_apply_ancestors_messages_to_node(t, child, &ancestors, bounds, &msgs_applied, -1);

    struct checkit_fn {
        DBT *childkeys;
        int num_parent_messages;
        ft_msg **parent_messages;
        bool *parent_messages_is_fresh;
        checkit_fn(DBT *ck, int np, ft_msg **pm, bool *pmf) :
            childkeys(ck), num_parent_messages(np), parent_messages(pm), parent_messages_is_fresh(pmf) {
        }
        int operator()(const ft_msg &msg, bool is_fresh) {
            DBT keydbt;
            toku_fill_dbt(&keydbt, msg.kdbt()->data, msg.kdbt()->size);
            MSN msn = msg.msn();
            if (dummy_cmp(&keydbt, &childkeys[7]) > 0) {
                for (int _i = 0; _i < num_parent_messages; ++_i) {
                    if (dummy_cmp(&keydbt, parent_messages[_i]->kdbt()) == 0 &&
                            msn.msn == parent_messages[_i]->msn().msn) {
                        assert(is_fresh == parent_messages_is_fresh[_i]);
                        break;
                    }
                }
            } else {
                assert(!is_fresh);
            }
            return 0;
        }
    } checkit(childkeys, num_parent_messages, parent_messages, parent_messages_is_fresh);
    parent_bnc->msg_buffer.iterate(checkit);

    toku_ftnode_free(&parentnode);

    int total_messages = 0;
    for (i = 0; i < 8; ++i) {
        total_messages += BLB_DATA(child, i)->num_klpairs();
    }
    assert(total_messages <= num_parent_messages + num_child_messages);

    for (i = 0; i < num_parent_messages; ++i) {
        if (dummy_cmp(parent_messages[i]->kdbt(), &childkeys[7]) <= 0) {
            assert(parent_messages_applied[i] == 1);
        } else {
            assert(parent_messages_applied[i] == 0);
        }
    }

    toku_xids_destroy(&xids_0);
    toku_xids_destroy(&xids_123);
    toku_xids_destroy(&xids_234);

    for (i = 0; i < num_parent_messages; ++i) {
        toku_free(parent_messages[i]->kdbt()->data);
        struct orthopush_flush_update_fun_extra *CAST_FROM_VOIDP(extra, parent_messages[i]->vdbt()->data);
        toku_free(extra->new_val.data);
        toku_free(parent_messages[i]->vdbt()->data);
        delete parent_messages[i];
    }
    for (i = 0; i < num_child_messages; ++i) {
        toku_free(child_messages[i]);
        toku_free(key_pointers[i]);
    }
    toku_free(ubi.data);
    toku_ftnode_free(&child);
    toku_free(parent_messages);
    toku_free(key_pointers);
    toku_free(keylens);
    toku_free(child_messages);
    toku_free(parent_messages_is_fresh);
    toku_free(parent_messages_applied);
}

// create identical leaf nodes and then buffer flush to one and
// maybe_apply to the other, and compare the results, they should be the
// same.
//
// if make_leaf_up_to_date is true, then apply the messages that are stale
// in the parent to the leaf before doing the flush, otherwise assume the
// leaf was just read off disk
static void
compare_apply_and_flush(FT_HANDLE t, bool make_leaf_up_to_date) {
    int r;

    ft_msg **MALLOC_N(4096,parent_messages);  // 128k / 32 = 4k
    LEAFENTRY* child_messages = NULL;
    XMALLOC_N(4096,child_messages);
    void** key_pointers = NULL;
    XMALLOC_N(4096, key_pointers);
    uint32_t* keylens = NULL;
    XMALLOC_N(4096, keylens);
    bool *MALLOC_N(4096,parent_messages_is_fresh);
    memset(parent_messages_is_fresh, 0, 4096*(sizeof parent_messages_is_fresh[0]));
    int *MALLOC_N(4096,parent_messages_applied);
    memset(parent_messages_applied, 0, 4096*(sizeof parent_messages_applied[0]));

    XIDS xids_0 = toku_xids_get_root_xids();
    XIDS xids_123, xids_234;
    r = toku_xids_create_child(xids_0, &xids_123, (TXNID)123);
    CKERR(r);
    r = toku_xids_create_child(xids_0, &xids_234, (TXNID)234);
    CKERR(r);

    BASEMENTNODE child1_blbs[8], child2_blbs[8];
    DBT child1keys[7], child2keys[7];
    int i;
    for (i = 0; i < 8; ++i) {
        child1_blbs[i] = toku_create_empty_bn();
        child2_blbs[i] = toku_create_empty_bn();
        if (i < 7) {
            toku_init_dbt(&child1keys[i]);
            toku_init_dbt(&child2keys[i]);
        }
    }

    FTNODE XMALLOC(child1), XMALLOC(child2);
    BLOCKNUM blocknum = { 42 };
    toku_initialize_empty_ftnode(child1, blocknum, 0, 8, FT_LAYOUT_VERSION, 0);
    toku_initialize_empty_ftnode(child2, blocknum, 0, 8, FT_LAYOUT_VERSION, 0);
    for (i = 0; i < 8; ++i) {
        destroy_basement_node(BLB(child1, i));
        set_BLB(child1, i, child1_blbs[i]);
        BP_STATE(child1, i) = PT_AVAIL;
        destroy_basement_node(BLB(child2, i));
        set_BLB(child2, i, child2_blbs[i]);
        BP_STATE(child2, i) = PT_AVAIL;
    }

    int total_size = 0;
    for (i = 0; total_size < 128*1024; ++i) {
        total_size -= child1_blbs[i%8]->data_buffer.get_memory_size();
        insert_same_message_to_bns(t, child1_blbs[i%8], child2_blbs[i%8], &key_pointers[i], &keylens[i], &child_messages[i], xids_123, i%8);
        total_size += child1_blbs[i%8]->data_buffer.get_memory_size();
        if (i % 8 < 7) {
            DBT keydbt;
            if (child1keys[i%8].size == 0 || dummy_cmp(toku_fill_dbt(&keydbt, key_pointers[i], keylens[i]), &child1keys[i%8]) > 0) {
                toku_fill_dbt(&child1keys[i%8], key_pointers[i], keylens[i]);
                toku_fill_dbt(&child2keys[i%8], key_pointers[i], keylens[i]);
            }
        }
    }
    int num_child_messages = i;

    for (i = 0; i < num_child_messages; ++i) {
        DBT keydbt;
        if (i % 8 < 7) {
            assert(dummy_cmp(toku_fill_dbt(&keydbt, key_pointers[i], keylens[i]), &child1keys[i%8]) <= 0);
            assert(dummy_cmp(toku_fill_dbt(&keydbt, key_pointers[i], keylens[i]), &child2keys[i%8]) <= 0);
        }
    }

    {
        int num_stale = random() % 2000;
        memset(&parent_messages_is_fresh[num_stale], true, (4096 - num_stale) * (sizeof parent_messages_is_fresh[0]));
    }
    NONLEAF_CHILDINFO parent_bnc = toku_create_empty_nl();
    MSN max_parent_msn = MIN_MSN;
    for (i = 0; toku_bnc_memory_used(parent_bnc) < 128*1024; ++i) {
        insert_random_update_message(parent_bnc, &parent_messages[i], parent_messages_is_fresh[i], xids_234, i%8, &parent_messages_applied[i], &max_parent_msn);
    }
    int num_parent_messages = i;

    for (i = 0; i < 7; ++i) {
        child1->pivotkeys.insert_at(&child1keys[i], i);
        child2->pivotkeys.insert_at(&child2keys[i], i);
    }

    if (make_leaf_up_to_date) {
        for (i = 0; i < num_parent_messages; ++i) {
            if (!parent_messages_is_fresh[i]) {
                toku_ft_leaf_apply_msg(t->ft->cmp, t->ft->update_fun, child1, -1, *parent_messages[i], &non_mvcc_gc_info, NULL, NULL);
                toku_ft_leaf_apply_msg(t->ft->cmp, t->ft->update_fun, child2, -1, *parent_messages[i], &non_mvcc_gc_info, NULL, NULL);
            }
        }
        for (i = 0; i < 8; ++i) {
            BLB(child1, i)->stale_ancestor_messages_applied = true;
            BLB(child2, i)->stale_ancestor_messages_applied = true;
        }
    } else {
        for (i = 0; i < 8; ++i) {
            BLB(child1, i)->stale_ancestor_messages_applied = false;
            BLB(child2, i)->stale_ancestor_messages_applied = false;
        }
    }

    toku_bnc_flush_to_child(t->ft, parent_bnc, child1, TXNID_NONE);

    FTNODE XMALLOC(parentnode);
    BLOCKNUM parentblocknum = { 17 };
    toku_initialize_empty_ftnode(parentnode, parentblocknum, 1, 1, FT_LAYOUT_VERSION, 0);
    destroy_nonleaf_childinfo(BNC(parentnode, 0));
    set_BNC(parentnode, 0, parent_bnc);
    BP_STATE(parentnode, 0) = PT_AVAIL;
    parentnode->max_msn_applied_to_node_on_disk = max_parent_msn;
    struct ancestors ancestors = { .node = parentnode, .childnum = 0, .next = NULL };
    bool msgs_applied;
    toku_apply_ancestors_messages_to_node(t, child2, &ancestors, pivot_bounds::infinite_bounds(), &msgs_applied, -1);

    struct checkit_fn {
        int operator()(const ft_msg &UU(msg), bool is_fresh) {
            assert(!is_fresh);
            return 0;
        }
    } checkit;
    parent_bnc->msg_buffer.iterate(checkit);
    invariant(parent_bnc->fresh_message_tree.size() + parent_bnc->stale_message_tree.size()
              == (uint32_t) num_parent_messages);

    toku_ftnode_free(&parentnode);

    for (int j = 0; j < 8; ++j) {
        bn_data* first = BLB_DATA(child1, j);
        bn_data* second = BLB_DATA(child2, j);
        uint32_t len = first->num_klpairs();
        assert(len == second->num_klpairs());
        for (uint32_t idx = 0; idx < len; ++idx) {
            LEAFENTRY le1, le2;
            DBT key1dbt, val1dbt, key2dbt, val2dbt;
            {
                uint32_t keylen, vallen;
                void *keyp = NULL;
                r = first->fetch_klpair(idx, &le1, &keylen, &keyp);
                assert_zero(r);                
                void *valp = le_latest_val_and_len(le1, &vallen);
                toku_fill_dbt(&key1dbt, keyp, keylen);
                toku_fill_dbt(&val1dbt, valp, vallen);
            }
            {
                uint32_t keylen, vallen;
                void *keyp = NULL;
                r = second->fetch_klpair(idx, &le2, &keylen, &keyp);
                assert_zero(r);                
                void *valp = le_latest_val_and_len(le2, &vallen);
                toku_fill_dbt(&key2dbt, keyp, keylen);
                toku_fill_dbt(&val2dbt, valp, vallen);
            }
            assert(dummy_cmp(&key1dbt, &key2dbt) == 0);
            assert(dummy_cmp(&val1dbt, &val2dbt) == 0);
        }
    }

    toku_xids_destroy(&xids_0);
    toku_xids_destroy(&xids_123);
    toku_xids_destroy(&xids_234);

    for (i = 0; i < num_parent_messages; ++i) {
        toku_free(parent_messages[i]->kdbt()->data);
        struct orthopush_flush_update_fun_extra *CAST_FROM_VOIDP(extra, parent_messages[i]->vdbt()->data);
        toku_free(extra->new_val.data);
        toku_free(parent_messages[i]->vdbt()->data);
        delete parent_messages[i];
    }
    for (i = 0; i < num_child_messages; ++i) {
        toku_free(key_pointers[i]);
        toku_free(child_messages[i]);
    }
    toku_ftnode_free(&child1);
    toku_ftnode_free(&child2);
    toku_free(parent_messages);
    toku_free(key_pointers);
    toku_free(keylens);
    toku_free(child_messages);
    toku_free(parent_messages_is_fresh);
    toku_free(parent_messages_applied);
}

static void
parse_args(int argc, const char *argv[]) {
    const char *progname=argv[0];
    argc--; argv++;
    while (argc>0) {
        if (strcmp(argv[0],"-v")==0) {
            verbose=1;
        } else if (strcmp(argv[0],"-q")==0) {
            verbose=0;
        } else {
            fprintf(stderr, "Usage:\n %s [-v] [-q]\n", progname);
            exit(1);
        }
        argc--; argv++;
    }
}

static int cmp_fn(DB *db __attribute__((unused)),
                     const DBT *a, const DBT *b) {
    int c;
    if (a->size > b->size) {
        c = memcmp(a->data, b->data, b->size);
    } else if (a->size < b->size) {
        c = memcmp(a->data, b->data, a->size);
    } else {
        return memcmp(a->data, b->data, a->size);
    }
    if (c == 0) {
        c = a->size - b->size;
    }
    return c;
}

int
test_main (int argc, const char *argv[]) {
    parse_args(argc, argv);

    dummy_cmp.create(cmp_fn, nullptr);

    initialize_dummymsn();
    int r;
    CACHETABLE ct;
    toku_cachetable_create(&ct, 0, ZERO_LSN, nullptr);
    unlink(fname);
    FT_HANDLE t;
    r = toku_open_ft_handle(fname, 1, &t, 128*1024, 4096, TOKU_DEFAULT_COMPRESSION_METHOD, ct, null_txn, toku_builtin_compare_fun); assert(r==0);
    toku_ft_set_update(t, orthopush_flush_update_fun);
    // HACK
    t->ft->update_fun = orthopush_flush_update_fun;

    for (int i = 0; i < 10; ++i) {
        flush_to_internal(t);
    }
    for (int i = 0; i < 10; ++i) {
        flush_to_internal_multiple(t);
    }
    for (int i = 0; i < 3; ++i) {
        flush_to_leaf(t, false, false);
        flush_to_leaf(t, false, true);
        flush_to_leaf(t, true, false);
        flush_to_leaf(t, true, true);
    }
    for (int i = 0; i < 10; ++i) {
        flush_to_leaf_with_keyrange(t, false);
        flush_to_leaf_with_keyrange(t, true);
        compare_apply_and_flush(t, false);
        compare_apply_and_flush(t, true);
    }

    r = toku_close_ft_handle_nolsn(t, 0);          assert(r==0);
    toku_cachetable_close(&ct);

    dummy_cmp.destroy();

    return 0;
}
