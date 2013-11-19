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

#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "test.h"
#include "bndata.h"



#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

static size_t
calc_le_size(int keylen, int vallen) {
    return LE_CLEAN_MEMSIZE(vallen) + keylen + sizeof(uint32_t);
}

static void
le_add_to_bn(bn_data* bn, uint32_t idx, const  char *key, int keysize, const char *val, int valsize)
{
    LEAFENTRY r = NULL;
    uint32_t size_needed = LE_CLEAN_MEMSIZE(valsize);
    bn->get_space_for_insert(
        idx, 
        key,
        keysize,
        size_needed,
        &r
        );
    resource_assert(r);
    r->type = LE_CLEAN;
    r->u.clean.vallen = valsize;
    memcpy(r->u.clean.val, val, valsize);
}

static KLPAIR
le_fastmalloc(struct mempool * mp, const char *key, int keylen, const char *val, int vallen)
{
    KLPAIR kl;
    size_t le_size = calc_le_size(keylen, vallen);
    CAST_FROM_VOIDP(kl, toku_mempool_malloc(mp, le_size, 1));
    resource_assert(kl);
    kl->keylen = keylen;
    memcpy(kl->key_le, key, keylen);
    LEAFENTRY le = get_le_from_klpair(kl);
    le->type = LE_CLEAN;
    le->u.clean.vallen = vallen;
    memcpy(le->u.clean.val, val, vallen);
    return kl;
}

static KLPAIR
le_malloc(struct mempool * mp, const char *key, const char *val)
{
    int keylen = strlen(key) + 1;
    int vallen = strlen(val) + 1;
    return le_fastmalloc(mp, key, keylen, val, vallen);
}

struct check_leafentries_struct {
    int nelts;
    LEAFENTRY *elts;
    int i;
    int (*cmp)(OMTVALUE, void *);
};

enum ftnode_verify_type {
    read_all=1,
    read_compressed,
    read_none
};

static int
string_key_cmp(DB *UU(e), const DBT *a, const DBT *b)
{
    char *CAST_FROM_VOIDP(s, a->data);
    char *CAST_FROM_VOIDP(t, b->data);
    return strcmp(s, t);
}

static void
setup_dn(enum ftnode_verify_type bft, int fd, FT brt_h, FTNODE *dn, FTNODE_DISK_DATA* ndd) {
    int r;
    brt_h->compare_fun = string_key_cmp;
    if (bft == read_all) {
        struct ftnode_fetch_extra bfe;
        fill_bfe_for_full_read(&bfe, brt_h);
        r = toku_deserialize_ftnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, dn, ndd, &bfe);
        assert(r==0);
    }
    else if (bft == read_compressed || bft == read_none) {
        struct ftnode_fetch_extra bfe;
        fill_bfe_for_min_read(&bfe, brt_h);
        r = toku_deserialize_ftnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, dn, ndd, &bfe);
        assert(r==0);
        // assert all bp's are compressed or on disk.
        for (int i = 0; i < (*dn)->n_children; i++) {
            assert(BP_STATE(*dn,i) == PT_COMPRESSED || BP_STATE(*dn, i) == PT_ON_DISK);
        }
        // if read_none, get rid of the compressed bp's
        if (bft == read_none) {
            if ((*dn)->height == 0) {
                PAIR_ATTR attr;
                toku_ftnode_pe_callback(*dn, make_pair_attr(0xffffffff), &attr, brt_h);
                // assert all bp's are on disk
                for (int i = 0; i < (*dn)->n_children; i++) {
                    if ((*dn)->height == 0) {
                        assert(BP_STATE(*dn,i) == PT_ON_DISK);
                        assert(is_BNULL(*dn, i));
                    }
                    else {
                        assert(BP_STATE(*dn,i) == PT_COMPRESSED);
                    }
                }
            }
            else {
                // first decompress everything, and make sure
                // that it is available
                // then run partial eviction to get it compressed
                PAIR_ATTR attr;
                fill_bfe_for_full_read(&bfe, brt_h);
                assert(toku_ftnode_pf_req_callback(*dn, &bfe));
                r = toku_ftnode_pf_callback(*dn, *ndd, &bfe, fd, &attr);
                assert(r==0);
                // assert all bp's are available
                for (int i = 0; i < (*dn)->n_children; i++) {
                    assert(BP_STATE(*dn,i) == PT_AVAIL);
                }
                toku_ftnode_pe_callback(*dn, make_pair_attr(0xffffffff), &attr, brt_h);
                for (int i = 0; i < (*dn)->n_children; i++) {
                    // assert all bp's are still available, because we touched the clock
                    assert(BP_STATE(*dn,i) == PT_AVAIL);
                    // now assert all should be evicted
                    assert(BP_SHOULD_EVICT(*dn, i));
                }
                toku_ftnode_pe_callback(*dn, make_pair_attr(0xffffffff), &attr, brt_h);
                for (int i = 0; i < (*dn)->n_children; i++) {
                    assert(BP_STATE(*dn,i) == PT_COMPRESSED);
                }
            }
        }
        // now decompress them
        fill_bfe_for_full_read(&bfe, brt_h);
        assert(toku_ftnode_pf_req_callback(*dn, &bfe));
        PAIR_ATTR attr;
        r = toku_ftnode_pf_callback(*dn, *ndd, &bfe, fd, &attr);
        assert(r==0);
        // assert all bp's are available
        for (int i = 0; i < (*dn)->n_children; i++) {
            assert(BP_STATE(*dn,i) == PT_AVAIL);
        }
        // continue on with test
    }
    else {
        // if we get here, this is a test bug, NOT a bug in development code
        assert(false);
    }
}

static void write_sn_to_disk(int fd, FT_HANDLE brt, FTNODE sn, FTNODE_DISK_DATA* src_ndd, bool do_clone) {
    int r;
    if (do_clone) {
        void* cloned_node_v = NULL;
        PAIR_ATTR attr;
        long clone_size;
        toku_ftnode_clone_callback(sn, &cloned_node_v, &clone_size, &attr, false, brt->ft);
        FTNODE CAST_FROM_VOIDP(cloned_node, cloned_node_v);
        r = toku_serialize_ftnode_to(fd, make_blocknum(20), cloned_node, src_ndd, false, brt->ft, false);
        assert(r==0);        
        toku_ftnode_free(&cloned_node);
    }
    else {
        r = toku_serialize_ftnode_to(fd, make_blocknum(20), sn, src_ndd, true, brt->ft, false);
        assert(r==0);
    }
}

static void
test_serialize_leaf_check_msn(enum ftnode_verify_type bft, bool do_clone) {
    //    struct ft_handle source_ft;
    struct ftnode sn, *dn;

    int fd = open(TOKU_TEST_FILENAME, O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    int r;

#define PRESERIALIZE_MSN_ON_DISK ((MSN) { MIN_MSN.msn + 42 })
#define POSTSERIALIZE_MSN_ON_DISK ((MSN) { MIN_MSN.msn + 84 })

    sn.max_msn_applied_to_node_on_disk = PRESERIALIZE_MSN_ON_DISK;
    sn.flags = 0x11223344;
    sn.thisnodename.b = 20;
    sn.layout_version = FT_LAYOUT_VERSION;
    sn.layout_version_original = FT_LAYOUT_VERSION;
    sn.height = 0;
    sn.n_children = 2;
    sn.dirty = 1;
    sn.oldest_referenced_xid_known = TXNID_NONE;
    MALLOC_N(sn.n_children, sn.bp);
    MALLOC_N(1, sn.childkeys);
    toku_memdup_dbt(&sn.childkeys[0], "b", 2);
    sn.totalchildkeylens = 2;
    BP_STATE(&sn,0) = PT_AVAIL;
    BP_STATE(&sn,1) = PT_AVAIL;
    set_BLB(&sn, 0, toku_create_empty_bn());
    set_BLB(&sn, 1, toku_create_empty_bn());
    KLPAIR elts[3];
    le_add_to_bn(BLB_DATA(&sn, 0), 0, "a", 2, "aval", 5);
    le_add_to_bn(BLB_DATA(&sn, 0), 1, "b", 2, "bval", 5);
    le_add_to_bn(BLB_DATA(&sn, 1), 0, "x", 2, "xval", 5);
    BLB_MAX_MSN_APPLIED(&sn, 0) = ((MSN) { MIN_MSN.msn + 73 });
    BLB_MAX_MSN_APPLIED(&sn, 1) = POSTSERIALIZE_MSN_ON_DISK;

    FT_HANDLE XMALLOC(brt);
    FT XCALLOC(brt_h);
    toku_ft_init(brt_h,
                 make_blocknum(0),
                 ZERO_LSN,
                 TXNID_NONE,
                 4*1024*1024,
                 128*1024,
                 TOKU_DEFAULT_COMPRESSION_METHOD);
    brt->ft = brt_h;
    toku_blocktable_create_new(&brt_h->blocktable);
    { int r_truncate = ftruncate(fd, 0); CKERR(r_truncate); }

    //Want to use block #20
    BLOCKNUM b = make_blocknum(0);
    while (b.b < 20) {
        toku_allocate_blocknum(brt_h->blocktable, &b, brt_h);
    }
    assert(b.b == 20);

    {
        DISKOFF offset;
        DISKOFF size;
        toku_blocknum_realloc_on_disk(brt_h->blocktable, b, 100, &offset, brt_h, fd, false);
        assert(offset==BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);

        toku_translate_blocknum_to_offset_size(brt_h->blocktable, b, &offset, &size);
        assert(offset == BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
        assert(size   == 100);
    }
    FTNODE_DISK_DATA src_ndd = NULL;
    FTNODE_DISK_DATA dest_ndd = NULL;

    write_sn_to_disk(fd, brt, &sn, &src_ndd, do_clone);

    setup_dn(bft, fd, brt_h, &dn, &dest_ndd);

    assert(dn->thisnodename.b==20);

    assert(dn->layout_version ==FT_LAYOUT_VERSION);
    assert(dn->layout_version_original ==FT_LAYOUT_VERSION);
    assert(dn->layout_version_read_from_disk ==FT_LAYOUT_VERSION);
    assert(dn->height == 0);
    assert(dn->n_children>=1);
    assert(dn->max_msn_applied_to_node_on_disk.msn == POSTSERIALIZE_MSN_ON_DISK.msn);
    {
        // Man, this is way too ugly.  This entire test suite needs to be refactored.
        // Create a dummy mempool and put the leaves there.  Ugh.
        struct mempool dummy_mp;
        toku_mempool_construct(&dummy_mp, 1024);
        elts[0] = le_malloc(&dummy_mp, "a", "aval");
        elts[1] = le_malloc(&dummy_mp, "b", "bval");
        elts[2] = le_malloc(&dummy_mp, "x", "xval");
        const uint32_t npartitions = dn->n_children;
        assert(dn->totalchildkeylens==(2*(npartitions-1)));
        uint32_t last_i = 0;
        for (uint32_t bn = 0; bn < npartitions; ++bn) {
            assert(BLB_MAX_MSN_APPLIED(dn, bn).msn == POSTSERIALIZE_MSN_ON_DISK.msn);
            assert(dest_ndd[bn].start > 0);
            assert(dest_ndd[bn].size  > 0);
            if (bn > 0) {
                assert(dest_ndd[bn].start >= dest_ndd[bn-1].start + dest_ndd[bn-1].size);
            }
            for (uint32_t i = 0; i < BLB_DATA(dn, bn)->omt_size(); i++) {
                LEAFENTRY curr_le;
                uint32_t curr_keylen;
                void* curr_key;
                BLB_DATA(dn, bn)->fetch_klpair(i, &curr_le, &curr_keylen, &curr_key);
                assert(leafentry_memsize(curr_le) == leafentry_memsize(get_le_from_klpair(elts[last_i])));
                assert(memcmp(curr_le, get_le_from_klpair(elts[last_i]), leafentry_memsize(curr_le)) == 0);
                if (bn < npartitions-1) {
                    assert(strcmp((char*)dn->childkeys[bn].data, (char*)(elts[last_i]->key_le)) <= 0);
                }
                // TODO for later, get a key comparison here as well
                last_i++;
            }

        }
        toku_mempool_destroy(&dummy_mp);
        assert(last_i == 3);
    }
    toku_ftnode_free(&dn);

    for (int i = 0; i < sn.n_children-1; ++i) {
        toku_free(sn.childkeys[i].data);
    }
    for (int i = 0; i < sn.n_children; i++) {
        destroy_basement_node(BLB(&sn, i));
    }
    toku_free(sn.bp);
    toku_free(sn.childkeys);

    toku_block_free(brt_h->blocktable, BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
    toku_blocktable_destroy(&brt_h->blocktable);
    toku_free(brt_h->h);
    toku_free(brt_h);
    toku_free(brt);
    toku_free(src_ndd);
    toku_free(dest_ndd);

    r = close(fd); assert(r != -1);
}

static void
test_serialize_leaf_with_large_pivots(enum ftnode_verify_type bft, bool do_clone) {
    int r;
    struct ftnode sn, *dn;
    const int keylens = 256*1024, vallens = 0;
    const uint32_t nrows = 8;
    // assert(val_size > BN_MAX_SIZE);  // BN_MAX_SIZE isn't visible
    int fd = open(TOKU_TEST_FILENAME, O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    sn.max_msn_applied_to_node_on_disk.msn = 0;
    sn.flags = 0x11223344;
    sn.thisnodename.b = 20;
    sn.layout_version = FT_LAYOUT_VERSION;
    sn.layout_version_original = FT_LAYOUT_VERSION;
    sn.height = 0;
    sn.n_children = nrows;
    sn.dirty = 1;
    sn.oldest_referenced_xid_known = TXNID_NONE;

    MALLOC_N(sn.n_children, sn.bp);
    MALLOC_N(sn.n_children-1, sn.childkeys);
    sn.totalchildkeylens = (sn.n_children-1)*sizeof(int);
    for (int i = 0; i < sn.n_children; ++i) {
        BP_STATE(&sn,i) = PT_AVAIL;
        set_BLB(&sn, i, toku_create_empty_bn());
    }
    for (uint32_t i = 0; i < nrows; ++i) {  // one basement per row
        char key[keylens], val[vallens];
        key[keylens-1] = '\0';
        char c = 'a' + i;
        memset(key, c, keylens-1);
        le_add_to_bn(BLB_DATA(&sn, i), 0, (char *) &key, sizeof(key), (char *) &val, sizeof(val));
        if (i < nrows-1) {
            uint32_t keylen;
            void* curr_key;
            BLB_DATA(&sn, i)->fetch_le_key_and_len(0, &keylen, &curr_key);
            toku_memdup_dbt(&sn.childkeys[i], curr_key, keylen);
        }
    }

    FT_HANDLE XMALLOC(brt);
    FT XCALLOC(brt_h);
    toku_ft_init(brt_h,
                 make_blocknum(0),
                 ZERO_LSN,
                 TXNID_NONE,
                 4*1024*1024,
                 128*1024,
                 TOKU_DEFAULT_COMPRESSION_METHOD);
    brt->ft = brt_h;
    toku_blocktable_create_new(&brt_h->blocktable);
    { int r_truncate = ftruncate(fd, 0); CKERR(r_truncate); }
    //Want to use block #20
    BLOCKNUM b = make_blocknum(0);
    while (b.b < 20) {
        toku_allocate_blocknum(brt_h->blocktable, &b, brt_h);
    }
    assert(b.b == 20);

    {
        DISKOFF offset;
        DISKOFF size;
        toku_blocknum_realloc_on_disk(brt_h->blocktable, b, 100, &offset, brt_h, fd, false);
        assert(offset==BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);

        toku_translate_blocknum_to_offset_size(brt_h->blocktable, b, &offset, &size);
        assert(offset == BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
        assert(size   == 100);
    }
    FTNODE_DISK_DATA src_ndd = NULL;
    FTNODE_DISK_DATA dest_ndd = NULL;

    write_sn_to_disk(fd, brt, &sn, &src_ndd, do_clone);

    setup_dn(bft, fd, brt_h, &dn, &dest_ndd);
    
    assert(dn->thisnodename.b==20);

    assert(dn->layout_version ==FT_LAYOUT_VERSION);
    assert(dn->layout_version_original ==FT_LAYOUT_VERSION);
    {
        // Man, this is way too ugly.  This entire test suite needs to be refactored.
        // Create a dummy mempool and put the leaves there.  Ugh.
        struct mempool dummy_mp;
        size_t le_size = calc_le_size(keylens, vallens);
        size_t mpsize = nrows * le_size;
        toku_mempool_construct(&dummy_mp, mpsize);
        KLPAIR les[nrows];
        {
            char key[keylens], val[vallens];
            key[keylens-1] = '\0';
            for (uint32_t i = 0; i < nrows; ++i) {
                char c = 'a' + i;
                memset(key, c, keylens-1);
                les[i] = le_fastmalloc(&dummy_mp, (char *) &key, sizeof(key), (char *) &val, sizeof(val));
            }
        }
        const uint32_t npartitions = dn->n_children;
        assert(dn->totalchildkeylens==(keylens*(npartitions-1)));
        uint32_t last_i = 0;
        for (uint32_t bn = 0; bn < npartitions; ++bn) {
            assert(dest_ndd[bn].start > 0);
            assert(dest_ndd[bn].size  > 0);
            if (bn > 0) {
                assert(dest_ndd[bn].start >= dest_ndd[bn-1].start + dest_ndd[bn-1].size);
            }
            assert(BLB_DATA(dn, bn)->omt_size() > 0);
            for (uint32_t i = 0; i < BLB_DATA(dn, bn)->omt_size(); i++) {
                LEAFENTRY curr_le;
                uint32_t curr_keylen;
                void* curr_key;
                BLB_DATA(dn, bn)->fetch_klpair(i, &curr_le, &curr_keylen, &curr_key);
                assert(leafentry_memsize(curr_le) == leafentry_memsize(get_le_from_klpair(les[last_i])));
                assert(memcmp(curr_le, get_le_from_klpair(les[last_i]), leafentry_memsize(curr_le)) == 0);
                if (bn < npartitions-1) {
                    assert(strcmp((char*)dn->childkeys[bn].data, (char*)(les[last_i]->key_le)) <= 0);
                }
                // TODO for later, get a key comparison here as well
                last_i++;
            }
        }
        toku_mempool_destroy(&dummy_mp);
        assert(last_i == nrows);
    }

    toku_ftnode_free(&dn);
    for (int i = 0; i < sn.n_children-1; ++i) {
        toku_free(sn.childkeys[i].data);
    }
    toku_free(sn.childkeys);
    for (int i = 0; i < sn.n_children; i++) {
        destroy_basement_node(BLB(&sn, i));
    }
    toku_free(sn.bp);

    toku_block_free(brt_h->blocktable, BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
    toku_blocktable_destroy(&brt_h->blocktable);
    toku_free(brt_h->h);
    toku_free(brt_h);
    toku_free(brt);
    toku_free(src_ndd);
    toku_free(dest_ndd);

    r = close(fd); assert(r != -1);
}

static void
test_serialize_leaf_with_many_rows(enum ftnode_verify_type bft, bool do_clone) {
    int r;
    struct ftnode sn, *dn;
    const int keylens = sizeof(int), vallens = sizeof(int);
    const uint32_t nrows = 196*1024;
    int fd = open(TOKU_TEST_FILENAME, O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    sn.max_msn_applied_to_node_on_disk.msn = 0;
    sn.flags = 0x11223344;
    sn.thisnodename.b = 20;
    sn.layout_version = FT_LAYOUT_VERSION;
    sn.layout_version_original = FT_LAYOUT_VERSION;
    sn.height = 0;
    sn.n_children = 1;
    sn.dirty = 1;
    sn.oldest_referenced_xid_known = TXNID_NONE;

    MALLOC_N(sn.n_children, sn.bp);
    MALLOC_N(sn.n_children-1, sn.childkeys);
    sn.totalchildkeylens = (sn.n_children-1)*sizeof(int);
    for (int i = 0; i < sn.n_children; ++i) {
        BP_STATE(&sn,i) = PT_AVAIL;
        set_BLB(&sn, i, toku_create_empty_bn()); 
    }
    for (uint32_t i = 0; i < nrows; ++i) {
        uint32_t key = i;
        uint32_t val = i;
        le_add_to_bn(BLB_DATA(&sn, 0), i, (char *) &key, sizeof(key), (char *) &val, sizeof(val));
    }

    FT_HANDLE XMALLOC(brt);
    FT XCALLOC(brt_h);
    toku_ft_init(brt_h,
                 make_blocknum(0),
                 ZERO_LSN,
                 TXNID_NONE,
                 4*1024*1024,
                 128*1024,
                 TOKU_DEFAULT_COMPRESSION_METHOD);
    brt->ft = brt_h;
    
    toku_blocktable_create_new(&brt_h->blocktable);
    { int r_truncate = ftruncate(fd, 0); CKERR(r_truncate); }
    //Want to use block #20
    BLOCKNUM b = make_blocknum(0);
    while (b.b < 20) {
        toku_allocate_blocknum(brt_h->blocktable, &b, brt_h);
    }
    assert(b.b == 20);

    {
        DISKOFF offset;
        DISKOFF size;
        toku_blocknum_realloc_on_disk(brt_h->blocktable, b, 100, &offset, brt_h, fd, false);
        assert(offset==BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);

        toku_translate_blocknum_to_offset_size(brt_h->blocktable, b, &offset, &size);
        assert(offset == BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
        assert(size   == 100);
    }

    FTNODE_DISK_DATA src_ndd = NULL;
    FTNODE_DISK_DATA dest_ndd = NULL;
    write_sn_to_disk(fd, brt, &sn, &src_ndd, do_clone);

    setup_dn(bft, fd, brt_h, &dn, &dest_ndd);

    assert(dn->thisnodename.b==20);

    assert(dn->layout_version ==FT_LAYOUT_VERSION);
    assert(dn->layout_version_original ==FT_LAYOUT_VERSION);
    {
        // Man, this is way too ugly.  This entire test suite needs to be refactored.
        // Create a dummy mempool and put the leaves there.  Ugh.
        struct mempool dummy_mp;
        size_t le_size = calc_le_size(keylens, vallens);
        size_t mpsize = nrows * le_size;
        toku_mempool_construct(&dummy_mp, mpsize);
        KLPAIR les[nrows];
        {
            int key = 0, val = 0;
            for (uint32_t i = 0; i < nrows; ++i, key++, val++) {
                les[i] = le_fastmalloc(&dummy_mp, (char *) &key, sizeof(key), (char *) &val, sizeof(val));
            }
        }
        const uint32_t npartitions = dn->n_children;
        assert(dn->totalchildkeylens==(sizeof(int)*(npartitions-1)));
        uint32_t last_i = 0;
        for (uint32_t bn = 0; bn < npartitions; ++bn) {
            assert(dest_ndd[bn].start > 0);
            assert(dest_ndd[bn].size  > 0);
            if (bn > 0) {
                assert(dest_ndd[bn].start >= dest_ndd[bn-1].start + dest_ndd[bn-1].size);
            }
            assert(BLB_DATA(dn, bn)->omt_size() > 0);
            for (uint32_t i = 0; i < BLB_DATA(dn, bn)->omt_size(); i++) {
                LEAFENTRY curr_le;
                uint32_t curr_keylen;
                void* curr_key;
                BLB_DATA(dn, bn)->fetch_klpair(i, &curr_le, &curr_keylen, &curr_key);
                assert(leafentry_memsize(curr_le) == leafentry_memsize(get_le_from_klpair(les[last_i])));
                assert(memcmp(curr_le, get_le_from_klpair(les[last_i]), leafentry_memsize(curr_le)) == 0);
                if (bn < npartitions-1) {
                    uint32_t *CAST_FROM_VOIDP(pivot, dn->childkeys[bn].data);
                    void* tmp = les[last_i]->key_le;
                    uint32_t *CAST_FROM_VOIDP(item, tmp);
                    assert(*pivot >= *item);
                }
                // TODO for later, get a key comparison here as well
                last_i++;
            }
            // don't check soft_copy_is_up_to_date or seqinsert
            assert(BLB_DATA(dn, bn)->get_disk_size() < 128*1024);  // BN_MAX_SIZE, apt to change
        }
        toku_mempool_destroy(&dummy_mp);
        assert(last_i == nrows);
    }

    toku_ftnode_free(&dn);
    for (int i = 0; i < sn.n_children-1; ++i) {
        toku_free(sn.childkeys[i].data);
    }
    for (int i = 0; i < sn.n_children; i++) {
        destroy_basement_node(BLB(&sn, i));
    }
    toku_free(sn.bp);
    toku_free(sn.childkeys);

    toku_block_free(brt_h->blocktable, BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
    toku_blocktable_destroy(&brt_h->blocktable);
    toku_free(brt_h->h);
    toku_free(brt_h);
    toku_free(brt);
    toku_free(src_ndd);
    toku_free(dest_ndd);

    r = close(fd); assert(r != -1);
}


static void
test_serialize_leaf_with_large_rows(enum ftnode_verify_type bft, bool do_clone) {
    int r;
    struct ftnode sn, *dn;
    const uint32_t nrows = 7;
    const size_t key_size = 8;
    const size_t val_size = 512*1024;
    // assert(val_size > BN_MAX_SIZE);  // BN_MAX_SIZE isn't visible
    int fd = open(TOKU_TEST_FILENAME, O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    sn.max_msn_applied_to_node_on_disk.msn = 0;
    sn.flags = 0x11223344;
    sn.thisnodename.b = 20;
    sn.layout_version = FT_LAYOUT_VERSION;
    sn.layout_version_original = FT_LAYOUT_VERSION;
    sn.height = 0;
    sn.n_children = 1;
    sn.dirty = 1;
    sn.oldest_referenced_xid_known = TXNID_NONE;
    
    MALLOC_N(sn.n_children, sn.bp);
    MALLOC_N(sn.n_children-1, sn.childkeys);
    sn.totalchildkeylens = (sn.n_children-1)*8;
    for (int i = 0; i < sn.n_children; ++i) {
        BP_STATE(&sn,i) = PT_AVAIL;
        set_BLB(&sn, i, toku_create_empty_bn());
    }
    for (uint32_t i = 0; i < nrows; ++i) {
        char key[key_size], val[val_size];
        key[key_size-1] = '\0';
        val[val_size-1] = '\0';
        char c = 'a' + i;
        memset(key, c, key_size-1);
        memset(val, c, val_size-1);
        le_add_to_bn(BLB_DATA(&sn, 0), i,key, 8, val, val_size);
    }

    FT_HANDLE XMALLOC(brt);
    FT XCALLOC(brt_h);
    toku_ft_init(brt_h,
                 make_blocknum(0),
                 ZERO_LSN,
                 TXNID_NONE,
                 4*1024*1024,
                 128*1024,
                 TOKU_DEFAULT_COMPRESSION_METHOD);
    brt->ft = brt_h;
    
    toku_blocktable_create_new(&brt_h->blocktable);
    { int r_truncate = ftruncate(fd, 0); CKERR(r_truncate); }
    //Want to use block #20
    BLOCKNUM b = make_blocknum(0);
    while (b.b < 20) {
        toku_allocate_blocknum(brt_h->blocktable, &b, brt_h);
    }
    assert(b.b == 20);

    {
        DISKOFF offset;
        DISKOFF size;
        toku_blocknum_realloc_on_disk(brt_h->blocktable, b, 100, &offset, brt_h, fd, false);
        assert(offset==BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);

        toku_translate_blocknum_to_offset_size(brt_h->blocktable, b, &offset, &size);
        assert(offset == BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
        assert(size   == 100);
    }

    FTNODE_DISK_DATA src_ndd = NULL;
    FTNODE_DISK_DATA dest_ndd = NULL;
    write_sn_to_disk(fd, brt, &sn, &src_ndd, do_clone);

    setup_dn(bft, fd, brt_h, &dn, &dest_ndd);

    assert(dn->thisnodename.b==20);

    assert(dn->layout_version ==FT_LAYOUT_VERSION);
    assert(dn->layout_version_original ==FT_LAYOUT_VERSION);
    {
        // Man, this is way too ugly.  This entire test suite needs to be refactored.
        // Create a dummy mempool and put the leaves there.  Ugh.
        struct mempool dummy_mp;
        size_t le_size = calc_le_size(key_size, val_size);
        size_t mpsize = nrows * le_size;
        toku_mempool_construct(&dummy_mp, mpsize);
        KLPAIR les[nrows];
        {
            char key[key_size], val[val_size];
            key[key_size-1] = '\0';
            val[val_size-1] = '\0';
            for (uint32_t i = 0; i < nrows; ++i) {
                char c = 'a' + i;
                memset(key, c, key_size-1);
                memset(val, c, val_size-1);
                les[i] = le_fastmalloc(&dummy_mp, key, key_size, val, val_size);
            }
        }
        const uint32_t npartitions = dn->n_children;
        assert(npartitions == nrows);
        assert(dn->totalchildkeylens==(key_size*(npartitions-1)));
        uint32_t last_i = 0;
        for (uint32_t bn = 0; bn < npartitions; ++bn) {
            assert(dest_ndd[bn].start > 0);
            assert(dest_ndd[bn].size  > 0);
            if (bn > 0) {
                assert(dest_ndd[bn].start >= dest_ndd[bn-1].start + dest_ndd[bn-1].size);
            }
            assert(BLB_DATA(dn, bn)->omt_size() > 0);
            for (uint32_t i = 0; i < BLB_DATA(dn, bn)->omt_size(); i++) {
                LEAFENTRY curr_le;
                uint32_t curr_keylen;
                void* curr_key;
                BLB_DATA(dn, bn)->fetch_klpair(i, &curr_le, &curr_keylen, &curr_key);
                assert(leafentry_memsize(curr_le) == leafentry_memsize(get_le_from_klpair(les[last_i])));
                assert(memcmp(curr_le, get_le_from_klpair(les[last_i]), leafentry_memsize(curr_le)) == 0);
                if (bn < npartitions-1) {
                    assert(strcmp((char*)dn->childkeys[bn].data, (char*)(les[last_i]->key_le)) <= 0);
                }
                // TODO for later, get a key comparison here as well
                last_i++;
            }
            // don't check soft_copy_is_up_to_date or seqinsert
        }
        toku_mempool_destroy(&dummy_mp);
        assert(last_i == 7);
    }

    toku_ftnode_free(&dn);
    for (int i = 0; i < sn.n_children-1; ++i) {
        toku_free(sn.childkeys[i].data);
    }
    for (int i = 0; i < sn.n_children; i++) {
        destroy_basement_node(BLB(&sn, i));
    }
    toku_free(sn.bp);
    toku_free(sn.childkeys);

    toku_block_free(brt_h->blocktable, BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
    toku_blocktable_destroy(&brt_h->blocktable);
    toku_free(brt_h->h);
    toku_free(brt_h);
    toku_free(brt);
    toku_free(src_ndd);
    toku_free(dest_ndd);

    r = close(fd); assert(r != -1);
}


static void
test_serialize_leaf_with_empty_basement_nodes(enum ftnode_verify_type bft, bool do_clone) {
    struct ftnode sn, *dn;

    int fd = open(TOKU_TEST_FILENAME, O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    int r;

    sn.max_msn_applied_to_node_on_disk.msn = 0;
    sn.flags = 0x11223344;
    sn.thisnodename.b = 20;
    sn.layout_version = FT_LAYOUT_VERSION;
    sn.layout_version_original = FT_LAYOUT_VERSION;
    sn.height = 0;
    sn.n_children = 7;
    sn.dirty = 1;
    sn.oldest_referenced_xid_known = TXNID_NONE;
    MALLOC_N(sn.n_children, sn.bp);
    MALLOC_N(sn.n_children-1, sn.childkeys);
    toku_memdup_dbt(&sn.childkeys[0], "A", 2);
    toku_memdup_dbt(&sn.childkeys[1], "a", 2);
    toku_memdup_dbt(&sn.childkeys[2], "a", 2);
    toku_memdup_dbt(&sn.childkeys[3], "b", 2);
    toku_memdup_dbt(&sn.childkeys[4], "b", 2);
    toku_memdup_dbt(&sn.childkeys[5], "x", 2);
    sn.totalchildkeylens = (sn.n_children-1)*2;
    for (int i = 0; i < sn.n_children; ++i) {
        BP_STATE(&sn,i) = PT_AVAIL;
        set_BLB(&sn, i, toku_create_empty_bn());
        BLB_SEQINSERT(&sn, i) = 0;
    }
    KLPAIR elts[3];
    le_add_to_bn(BLB_DATA(&sn, 1), 0, "a", 2, "aval", 5);
    le_add_to_bn(BLB_DATA(&sn, 3), 0, "b", 2, "bval", 5);
    le_add_to_bn(BLB_DATA(&sn, 5), 0, "x", 2, "xval", 5);

    FT_HANDLE XMALLOC(brt);
    FT XCALLOC(brt_h);
    toku_ft_init(brt_h,
                 make_blocknum(0),
                 ZERO_LSN,
                 TXNID_NONE,
                 4*1024*1024,
                 128*1024,
                 TOKU_DEFAULT_COMPRESSION_METHOD);
    brt->ft = brt_h;
    
    toku_blocktable_create_new(&brt_h->blocktable);
    { int r_truncate = ftruncate(fd, 0); CKERR(r_truncate); }
    //Want to use block #20
    BLOCKNUM b = make_blocknum(0);
    while (b.b < 20) {
        toku_allocate_blocknum(brt_h->blocktable, &b, brt_h);
    }
    assert(b.b == 20);

    {
        DISKOFF offset;
        DISKOFF size;
        toku_blocknum_realloc_on_disk(brt_h->blocktable, b, 100, &offset, brt_h, fd, false);
        assert(offset==BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);

        toku_translate_blocknum_to_offset_size(brt_h->blocktable, b, &offset, &size);
        assert(offset == BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
        assert(size   == 100);
    }
    FTNODE_DISK_DATA src_ndd = NULL;
    FTNODE_DISK_DATA dest_ndd = NULL;
    write_sn_to_disk(fd, brt, &sn, &src_ndd, do_clone);

    setup_dn(bft, fd, brt_h, &dn, &dest_ndd);

    assert(dn->thisnodename.b==20);

    assert(dn->layout_version ==FT_LAYOUT_VERSION);
    assert(dn->layout_version_original ==FT_LAYOUT_VERSION);
    assert(dn->layout_version_read_from_disk ==FT_LAYOUT_VERSION);
    assert(dn->height == 0);
    assert(dn->n_children>0);
    {
        // Man, this is way too ugly.  This entire test suite needs to be refactored.
        // Create a dummy mempool and put the leaves there.  Ugh.
        struct mempool dummy_mp;
        toku_mempool_construct(&dummy_mp, 1024);
        elts[0] = le_malloc(&dummy_mp, "a", "aval");
        elts[1] = le_malloc(&dummy_mp, "b", "bval");
        elts[2] = le_malloc(&dummy_mp, "x", "xval");
        const uint32_t npartitions = dn->n_children;
        assert(dn->totalchildkeylens==(2*(npartitions-1)));
        uint32_t last_i = 0;
        for (uint32_t bn = 0; bn < npartitions; ++bn) {
            assert(dest_ndd[bn].start > 0);
            assert(dest_ndd[bn].size  > 0);
            if (bn > 0) {
                assert(dest_ndd[bn].start >= dest_ndd[bn-1].start + dest_ndd[bn-1].size);
            }
            for (uint32_t i = 0; i < BLB_DATA(dn, bn)->omt_size(); i++) {
                LEAFENTRY curr_le;
                uint32_t curr_keylen;
                void* curr_key;
                BLB_DATA(dn, bn)->fetch_klpair(i, &curr_le, &curr_keylen, &curr_key);
                assert(leafentry_memsize(curr_le) == leafentry_memsize(get_le_from_klpair(elts[last_i])));
                assert(memcmp(curr_le, get_le_from_klpair(elts[last_i]), leafentry_memsize(curr_le)) == 0);
                if (bn < npartitions-1) {
                    assert(strcmp((char*)dn->childkeys[bn].data, (char*)(elts[last_i]->key_le)) <= 0);
                }
                // TODO for later, get a key comparison here as well
                last_i++;
            }

        }
        toku_mempool_destroy(&dummy_mp);
        assert(last_i == 3);
    }
    toku_ftnode_free(&dn);

    for (int i = 0; i < sn.n_children-1; ++i) {
        toku_free(sn.childkeys[i].data);
    }
    for (int i = 0; i < sn.n_children; i++) {
        destroy_basement_node(BLB(&sn, i));
    }
    toku_free(sn.bp);
    toku_free(sn.childkeys);

    toku_block_free(brt_h->blocktable, BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
    toku_blocktable_destroy(&brt_h->blocktable);
    toku_free(brt_h->h);
    toku_free(brt_h);
    toku_free(brt);
    toku_free(src_ndd);
    toku_free(dest_ndd);

    r = close(fd); assert(r != -1);
}

static void
test_serialize_leaf_with_multiple_empty_basement_nodes(enum ftnode_verify_type bft, bool do_clone) {
    struct ftnode sn, *dn;

    int fd = open(TOKU_TEST_FILENAME, O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    int r;

    sn.max_msn_applied_to_node_on_disk.msn = 0;
    sn.flags = 0x11223344;
    sn.thisnodename.b = 20;
    sn.layout_version = FT_LAYOUT_VERSION;
    sn.layout_version_original = FT_LAYOUT_VERSION;
    sn.height = 0;
    sn.n_children = 4;
    sn.dirty = 1;
    sn.oldest_referenced_xid_known = TXNID_NONE;
    MALLOC_N(sn.n_children, sn.bp);
    MALLOC_N(sn.n_children-1, sn.childkeys);
    toku_memdup_dbt(&sn.childkeys[0], "A", 2);
    toku_memdup_dbt(&sn.childkeys[1], "A", 2);
    toku_memdup_dbt(&sn.childkeys[2], "A", 2);
    sn.totalchildkeylens = (sn.n_children-1)*2;
    for (int i = 0; i < sn.n_children; ++i) {
        BP_STATE(&sn,i) = PT_AVAIL;
        set_BLB(&sn, i, toku_create_empty_bn());
    }

    FT_HANDLE XMALLOC(brt);
    FT XCALLOC(brt_h);
    toku_ft_init(brt_h,
                 make_blocknum(0),
                 ZERO_LSN,
                 TXNID_NONE,
                 4*1024*1024,
                 128*1024,
                 TOKU_DEFAULT_COMPRESSION_METHOD);
    brt->ft = brt_h;
    
    toku_blocktable_create_new(&brt_h->blocktable);
    { int r_truncate = ftruncate(fd, 0); CKERR(r_truncate); }
    //Want to use block #20
    BLOCKNUM b = make_blocknum(0);
    while (b.b < 20) {
        toku_allocate_blocknum(brt_h->blocktable, &b, brt_h);
    }
    assert(b.b == 20);

    {
        DISKOFF offset;
        DISKOFF size;
        toku_blocknum_realloc_on_disk(brt_h->blocktable, b, 100, &offset, brt_h, fd, false);
        assert(offset==BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);

        toku_translate_blocknum_to_offset_size(brt_h->blocktable, b, &offset, &size);
        assert(offset == BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
        assert(size   == 100);
    }

    FTNODE_DISK_DATA src_ndd = NULL;
    FTNODE_DISK_DATA dest_ndd = NULL;
    write_sn_to_disk(fd, brt, &sn, &src_ndd, do_clone);

    setup_dn(bft, fd, brt_h, &dn, &dest_ndd);

    assert(dn->thisnodename.b==20);

    assert(dn->layout_version ==FT_LAYOUT_VERSION);
    assert(dn->layout_version_original ==FT_LAYOUT_VERSION);
    assert(dn->layout_version_read_from_disk ==FT_LAYOUT_VERSION);
    assert(dn->height == 0);
    assert(dn->n_children == 1);
    {
        const uint32_t npartitions = dn->n_children;
        assert(dn->totalchildkeylens==(2*(npartitions-1)));
        for (uint32_t i = 0; i < npartitions; ++i) {
            assert(dest_ndd[i].start > 0);
            assert(dest_ndd[i].size  > 0);
            if (i > 0) {
                assert(dest_ndd[i].start >= dest_ndd[i-1].start + dest_ndd[i-1].size);
            }
            assert(BLB_DATA(dn, i)->omt_size() == 0);
        }
    }
    toku_ftnode_free(&dn);

    for (int i = 0; i < sn.n_children-1; ++i) {
        toku_free(sn.childkeys[i].data);
    }
    for (int i = 0; i < sn.n_children; i++) {
        destroy_basement_node(BLB(&sn, i));
    }
    toku_free(sn.bp);
    toku_free(sn.childkeys);

    toku_block_free(brt_h->blocktable, BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
    toku_blocktable_destroy(&brt_h->blocktable);
    toku_free(brt_h->h);
    toku_free(brt_h);
    toku_free(brt);
    toku_free(src_ndd);
    toku_free(dest_ndd);

    r = close(fd); assert(r != -1);
}


static void
test_serialize_nonleaf(enum ftnode_verify_type bft, bool do_clone) {
    //    struct ft_handle source_ft;
    struct ftnode sn, *dn;

    int fd = open(TOKU_TEST_FILENAME, O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    int r;

    //    source_ft.fd=fd;
    sn.max_msn_applied_to_node_on_disk.msn = 0;
    sn.flags = 0x11223344;
    sn.thisnodename.b = 20;
    sn.layout_version = FT_LAYOUT_VERSION;
    sn.layout_version_original = FT_LAYOUT_VERSION;
    sn.height = 1;
    sn.n_children = 2;
    sn.dirty = 1;
    sn.oldest_referenced_xid_known = TXNID_NONE;
    MALLOC_N(2, sn.bp);
    MALLOC_N(1, sn.childkeys);
    toku_memdup_dbt(&sn.childkeys[0], "hello", 6);
    sn.totalchildkeylens = 6;
    BP_BLOCKNUM(&sn, 0).b = 30;
    BP_BLOCKNUM(&sn, 1).b = 35;
    BP_STATE(&sn,0) = PT_AVAIL;
    BP_STATE(&sn,1) = PT_AVAIL;
    set_BNC(&sn, 0, toku_create_empty_nl());
    set_BNC(&sn, 1, toku_create_empty_nl());
    //Create XIDS
    XIDS xids_0 = xids_get_root_xids();
    XIDS xids_123;
    XIDS xids_234;
    r = xids_create_child(xids_0, &xids_123, (TXNID)123);
    CKERR(r);
    r = xids_create_child(xids_123, &xids_234, (TXNID)234);
    CKERR(r);

    toku_bnc_insert_msg(BNC(&sn, 0), "a", 2, "aval", 5, FT_NONE, next_dummymsn(), xids_0, true, NULL, string_key_cmp);
    toku_bnc_insert_msg(BNC(&sn, 0), "b", 2, "bval", 5, FT_NONE, next_dummymsn(), xids_123, false, NULL, string_key_cmp);
    toku_bnc_insert_msg(BNC(&sn, 1), "x", 2, "xval", 5, FT_NONE, next_dummymsn(), xids_234, true, NULL, string_key_cmp);
    //Cleanup:
    xids_destroy(&xids_0);
    xids_destroy(&xids_123);
    xids_destroy(&xids_234);

    FT_HANDLE XMALLOC(brt);
    FT XCALLOC(brt_h);
    toku_ft_init(brt_h,
                 make_blocknum(0),
                 ZERO_LSN,
                 TXNID_NONE,
                 4*1024*1024,
                 128*1024,
                 TOKU_DEFAULT_COMPRESSION_METHOD);
    brt->ft = brt_h;
    
    toku_blocktable_create_new(&brt_h->blocktable);
    { int r_truncate = ftruncate(fd, 0); CKERR(r_truncate); }
    //Want to use block #20
    BLOCKNUM b = make_blocknum(0);
    while (b.b < 20) {
        toku_allocate_blocknum(brt_h->blocktable, &b, brt_h);
    }
    assert(b.b == 20);

    {
        DISKOFF offset;
        DISKOFF size;
        toku_blocknum_realloc_on_disk(brt_h->blocktable, b, 100, &offset, brt_h, fd, false);
        assert(offset==BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);

        toku_translate_blocknum_to_offset_size(brt_h->blocktable, b, &offset, &size);
        assert(offset == BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
        assert(size   == 100);
    }
    FTNODE_DISK_DATA src_ndd = NULL;
    FTNODE_DISK_DATA dest_ndd = NULL;
    write_sn_to_disk(fd, brt, &sn, &src_ndd, do_clone);

    setup_dn(bft, fd, brt_h, &dn, &dest_ndd);

    assert(dn->thisnodename.b==20);

    assert(dn->layout_version ==FT_LAYOUT_VERSION);
    assert(dn->layout_version_original ==FT_LAYOUT_VERSION);
    assert(dn->layout_version_read_from_disk ==FT_LAYOUT_VERSION);
    assert(dn->height == 1);
    assert(dn->n_children==2);
    assert(strcmp((char*)dn->childkeys[0].data, "hello")==0);
    assert(dn->childkeys[0].size==6);
    assert(dn->totalchildkeylens==6);
    assert(BP_BLOCKNUM(dn,0).b==30);
    assert(BP_BLOCKNUM(dn,1).b==35);

    FIFO src_fifo_1 = BNC(&sn, 0)->buffer;
    FIFO src_fifo_2 = BNC(&sn, 1)->buffer;
    FIFO dest_fifo_1 = BNC(dn, 0)->buffer;
    FIFO dest_fifo_2 = BNC(dn, 1)->buffer;

    assert(toku_are_fifos_same(src_fifo_1, dest_fifo_1));
    assert(toku_are_fifos_same(src_fifo_2, dest_fifo_2));

    toku_ftnode_free(&dn);

    toku_free(sn.childkeys[0].data);
    destroy_nonleaf_childinfo(BNC(&sn, 0));
    destroy_nonleaf_childinfo(BNC(&sn, 1));
    toku_free(sn.bp);
    toku_free(sn.childkeys);

    toku_block_free(brt_h->blocktable, BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
    toku_blocktable_destroy(&brt_h->blocktable);
    toku_free(brt_h->h);
    toku_free(brt_h);
    toku_free(brt);
    toku_free(src_ndd);
    toku_free(dest_ndd);

    r = close(fd); assert(r != -1);
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    initialize_dummymsn();

    test_serialize_nonleaf(read_none, false);
    test_serialize_nonleaf(read_all, false);
    test_serialize_nonleaf(read_compressed, false);
    test_serialize_nonleaf(read_none, true);
    test_serialize_nonleaf(read_all, true);
    test_serialize_nonleaf(read_compressed, true);

    test_serialize_leaf_check_msn(read_none, false);
    test_serialize_leaf_check_msn(read_all, false);
    test_serialize_leaf_check_msn(read_compressed, false);
    test_serialize_leaf_check_msn(read_none, true);
    test_serialize_leaf_check_msn(read_all, true);
    test_serialize_leaf_check_msn(read_compressed, true);

    test_serialize_leaf_with_multiple_empty_basement_nodes(read_none, false);
    test_serialize_leaf_with_multiple_empty_basement_nodes(read_all, false);
    test_serialize_leaf_with_multiple_empty_basement_nodes(read_compressed, false);
    test_serialize_leaf_with_multiple_empty_basement_nodes(read_none, true);
    test_serialize_leaf_with_multiple_empty_basement_nodes(read_all, true);
    test_serialize_leaf_with_multiple_empty_basement_nodes(read_compressed, true);

    test_serialize_leaf_with_empty_basement_nodes(read_none, false);
    test_serialize_leaf_with_empty_basement_nodes(read_all, false);
    test_serialize_leaf_with_empty_basement_nodes(read_compressed, false);
    test_serialize_leaf_with_empty_basement_nodes(read_none, true);
    test_serialize_leaf_with_empty_basement_nodes(read_all, true);
    test_serialize_leaf_with_empty_basement_nodes(read_compressed, true);

    test_serialize_leaf_with_large_rows(read_none, false);
    test_serialize_leaf_with_large_rows(read_all, false);
    test_serialize_leaf_with_large_rows(read_compressed, false);
    test_serialize_leaf_with_large_rows(read_none, true);
    test_serialize_leaf_with_large_rows(read_all, true);
    test_serialize_leaf_with_large_rows(read_compressed, true);

    test_serialize_leaf_with_large_pivots(read_none, false);
    test_serialize_leaf_with_large_pivots(read_all, false);
    test_serialize_leaf_with_large_pivots(read_compressed, false);
    test_serialize_leaf_with_large_pivots(read_none, true);
    test_serialize_leaf_with_large_pivots(read_all, true);
    test_serialize_leaf_with_large_pivots(read_compressed, true);

    test_serialize_leaf_with_many_rows(read_none, false);
    test_serialize_leaf_with_many_rows(read_all, false);
    test_serialize_leaf_with_many_rows(read_compressed, false);
    test_serialize_leaf_with_many_rows(read_none, true);
    test_serialize_leaf_with_many_rows(read_all, true);
    test_serialize_leaf_with_many_rows(read_compressed, true);

    return 0;
}
