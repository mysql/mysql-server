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

#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "test.h"

#include "ft/cursor.h"

enum ftnode_verify_type {
    read_all=1,
    read_compressed,
    read_none
};

#ifndef MIN
#define MIN(x, y) (((x) < (y)) ? (x) : (y))
#endif

static int
string_key_cmp(DB *UU(e), const DBT *a, const DBT *b)
{
    char *CAST_FROM_VOIDP(s, a->data);
    char *CAST_FROM_VOIDP(t, b->data);
    return strcmp(s, t);
}

static void
le_add_to_bn(bn_data* bn, uint32_t idx, const  char *key, int keylen, const char *val, int vallen)
{
    LEAFENTRY r = NULL;
    uint32_t size_needed = LE_CLEAN_MEMSIZE(vallen);
    void *maybe_free = nullptr;
    bn->get_space_for_insert(
        idx, 
        key,
        keylen,
        size_needed,
        &r,
        &maybe_free
        );
    if (maybe_free) {
        toku_free(maybe_free);
    }
    resource_assert(r);
    r->type = LE_CLEAN;
    r->u.clean.vallen = vallen;
    memcpy(r->u.clean.val, val, vallen);
}


static void
le_malloc(bn_data* bn, uint32_t idx, const char *key, const char *val)
{
    int keylen = strlen(key) + 1;
    int vallen = strlen(val) + 1;
    le_add_to_bn(bn, idx, key, keylen, val, vallen);
}


static void
test1(int fd, FT ft_h, FTNODE *dn) {
    int r;
    ftnode_fetch_extra bfe_all;
    bfe_all.create_for_full_read(ft_h);
    FTNODE_DISK_DATA ndd = NULL;
    r = toku_deserialize_ftnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, dn, &ndd, &bfe_all);
    bool is_leaf = ((*dn)->height == 0);
    assert(r==0);
    for (int i = 0; i < (*dn)->n_children; i++) {
        assert(BP_STATE(*dn,i) == PT_AVAIL);
    }
    // should sweep and NOT get rid of anything
    PAIR_ATTR attr;
    memset(&attr,0,sizeof(attr));
    toku_ftnode_pe_callback(*dn, attr, ft_h, def_pe_finalize_impl, nullptr);
    for (int i = 0; i < (*dn)->n_children; i++) {
        assert(BP_STATE(*dn,i) == PT_AVAIL);
    }
    // should sweep and get compress all
    toku_ftnode_pe_callback(*dn, attr, ft_h, def_pe_finalize_impl, nullptr);
    for (int i = 0; i < (*dn)->n_children; i++) {
        if (!is_leaf) {
            assert(BP_STATE(*dn,i) == PT_COMPRESSED);
        }
        else {
            assert(BP_STATE(*dn,i) == PT_ON_DISK);
        }
    }
    PAIR_ATTR size;
    bool req = toku_ftnode_pf_req_callback(*dn, &bfe_all);
    assert(req);
    toku_ftnode_pf_callback(*dn, ndd, &bfe_all, fd, &size);
    toku_ftnode_pe_callback(*dn, attr, ft_h, def_pe_finalize_impl, nullptr);
    for (int i = 0; i < (*dn)->n_children; i++) {
        assert(BP_STATE(*dn,i) == PT_AVAIL);
    }
    // should sweep and get compress all
    toku_ftnode_pe_callback(*dn, attr, ft_h, def_pe_finalize_impl, nullptr);
    for (int i = 0; i < (*dn)->n_children; i++) {
        if (!is_leaf) {
            assert(BP_STATE(*dn,i) == PT_COMPRESSED);
        }
        else {
            assert(BP_STATE(*dn,i) == PT_ON_DISK);
        }
    }    

    req = toku_ftnode_pf_req_callback(*dn, &bfe_all);
    assert(req);
    toku_ftnode_pf_callback(*dn, ndd, &bfe_all, fd, &size);
    toku_ftnode_pe_callback(*dn, attr, ft_h, def_pe_finalize_impl, nullptr);
    for (int i = 0; i < (*dn)->n_children; i++) {
        assert(BP_STATE(*dn,i) == PT_AVAIL);
    }
    (*dn)->dirty = 1;
    toku_ftnode_pe_callback(*dn, attr, ft_h, def_pe_finalize_impl, nullptr);
    toku_ftnode_pe_callback(*dn, attr, ft_h, def_pe_finalize_impl, nullptr);
    toku_ftnode_pe_callback(*dn, attr, ft_h, def_pe_finalize_impl, nullptr);
    toku_ftnode_pe_callback(*dn, attr, ft_h, def_pe_finalize_impl, nullptr);
    for (int i = 0; i < (*dn)->n_children; i++) {
        assert(BP_STATE(*dn,i) == PT_AVAIL);
    }
    toku_free(ndd);
    toku_ftnode_free(dn);
}


static int search_cmp(const struct ft_search& UU(so), const DBT* UU(key)) {
    return 0;
}

static void
test2(int fd, FT ft_h, FTNODE *dn) {
    DBT left, right;
    DB dummy_db;
    memset(&dummy_db, 0, sizeof(dummy_db));
    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));
    ft_search search;
    
    ftnode_fetch_extra bfe_subset;
    bfe_subset.create_for_subset_read(
        ft_h,
        ft_search_init(&search, search_cmp, FT_SEARCH_LEFT, nullptr, nullptr, nullptr),
        &left,
        &right,
        true,
        true,
        false,
        false
        );

    FTNODE_DISK_DATA ndd = NULL;
    int r = toku_deserialize_ftnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, dn, &ndd, &bfe_subset);
    assert(r==0);
    bool is_leaf = ((*dn)->height == 0);
    // at this point, although both partitions are available, only the 
    // second basement node should have had its clock
    // touched
    assert(BP_STATE(*dn, 0) == PT_AVAIL);
    assert(BP_STATE(*dn, 1) == PT_AVAIL);
    assert(BP_SHOULD_EVICT(*dn, 0));
    assert(!BP_SHOULD_EVICT(*dn, 1));
    PAIR_ATTR attr;
    memset(&attr,0,sizeof(attr));
    toku_ftnode_pe_callback(*dn, attr, ft_h, def_pe_finalize_impl, nullptr);
    assert(BP_STATE(*dn, 0) == (is_leaf) ? PT_ON_DISK : PT_COMPRESSED);
    assert(BP_STATE(*dn, 1) == PT_AVAIL);
    assert(BP_SHOULD_EVICT(*dn, 1));
    toku_ftnode_pe_callback(*dn, attr, ft_h, def_pe_finalize_impl, nullptr);
    assert(BP_STATE(*dn, 1) == (is_leaf) ? PT_ON_DISK : PT_COMPRESSED);

    bool req = toku_ftnode_pf_req_callback(*dn, &bfe_subset);
    assert(req);
    toku_ftnode_pf_callback(*dn, ndd, &bfe_subset, fd, &attr);
    assert(BP_STATE(*dn, 0) == PT_AVAIL);
    assert(BP_STATE(*dn, 1) == PT_AVAIL);
    assert(BP_SHOULD_EVICT(*dn, 0));
    assert(!BP_SHOULD_EVICT(*dn, 1));

    toku_free(ndd);
    toku_ftnode_free(dn);
}

static void
test3_leaf(int fd, FT ft_h, FTNODE *dn) {
    DBT left, right;
    DB dummy_db;
    memset(&dummy_db, 0, sizeof(dummy_db));
    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));
    
    ftnode_fetch_extra bfe_min;
    bfe_min.create_for_min_read(ft_h);

    FTNODE_DISK_DATA ndd = NULL;
    int r = toku_deserialize_ftnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, dn, &ndd, &bfe_min);
    assert(r==0);
    //
    // make sure we have a leaf
    //
    assert((*dn)->height == 0);
    for (int i = 0; i < (*dn)->n_children; i++) {
        assert(BP_STATE(*dn, i) == PT_ON_DISK);
    }
    toku_ftnode_free(dn);
    toku_free(ndd);
}

static void
test_serialize_nonleaf(void) {
    //    struct ft_handle source_ft;
    struct ftnode sn, *dn;

    int fd = open(TOKU_TEST_FILENAME, O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    int r;

    //    source_ft.fd=fd;
    sn.max_msn_applied_to_node_on_disk.msn = 0;
    sn.flags = 0x11223344;
    sn.blocknum.b = 20;
    sn.layout_version = FT_LAYOUT_VERSION;
    sn.layout_version_original = FT_LAYOUT_VERSION;
    sn.height = 1;
    sn.n_children = 2;
    sn.dirty = 1;
    sn.oldest_referenced_xid_known = TXNID_NONE;
    MALLOC_N(2, sn.bp);
    DBT pivotkey;
    sn.pivotkeys.create_from_dbts(toku_fill_dbt(&pivotkey, "hello", 6), 1);
    BP_BLOCKNUM(&sn, 0).b = 30;
    BP_BLOCKNUM(&sn, 1).b = 35;
    BP_STATE(&sn,0) = PT_AVAIL;
    BP_STATE(&sn,1) = PT_AVAIL;
    set_BNC(&sn, 0, toku_create_empty_nl());
    set_BNC(&sn, 1, toku_create_empty_nl());
    //Create XIDS
    XIDS xids_0 = toku_xids_get_root_xids();
    XIDS xids_123;
    XIDS xids_234;
    r = toku_xids_create_child(xids_0, &xids_123, (TXNID)123);
    CKERR(r);
    r = toku_xids_create_child(xids_123, &xids_234, (TXNID)234);
    CKERR(r);

    toku::comparator cmp;
    cmp.create(string_key_cmp, nullptr);

    toku_bnc_insert_msg(BNC(&sn, 0), "a", 2, "aval", 5, FT_NONE, next_dummymsn(), xids_0, true, cmp);
    toku_bnc_insert_msg(BNC(&sn, 0), "b", 2, "bval", 5, FT_NONE, next_dummymsn(), xids_123, false, cmp);
    toku_bnc_insert_msg(BNC(&sn, 1), "x", 2, "xval", 5, FT_NONE, next_dummymsn(), xids_234, true, cmp);

    //Cleanup:
    toku_xids_destroy(&xids_0);
    toku_xids_destroy(&xids_123);
    toku_xids_destroy(&xids_234);
    cmp.destroy();

    FT_HANDLE XMALLOC(ft);
    FT XCALLOC(ft_h);
    toku_ft_init(ft_h,
                 make_blocknum(0),
                 ZERO_LSN,
                 TXNID_NONE,
                 4*1024*1024,
                 128*1024,
                 TOKU_DEFAULT_COMPRESSION_METHOD,
                 16);
    ft_h->cmp.create(string_key_cmp, nullptr);
    ft->ft = ft_h;
    
    ft_h->blocktable.create();
    { int r_truncate = ftruncate(fd, 0); CKERR(r_truncate); }
    //Want to use block #20
    BLOCKNUM b = make_blocknum(0);
    while (b.b < 20) {
        ft_h->blocktable.allocate_blocknum(&b, ft_h);
    }
    assert(b.b == 20);

    {
        DISKOFF offset;
        DISKOFF size;
        ft_h->blocktable.realloc_on_disk(b, 100, &offset, ft_h, fd, false, 0);
        assert(offset==(DISKOFF)block_allocator::BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);

        ft_h->blocktable.translate_blocknum_to_offset_size(b, &offset, &size);
        assert(offset == (DISKOFF)block_allocator::BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
        assert(size   == 100);
    }
    FTNODE_DISK_DATA ndd = NULL;
    r = toku_serialize_ftnode_to(fd, make_blocknum(20), &sn, &ndd, true, ft->ft, false);
    assert(r==0);

    test1(fd, ft_h, &dn);
    test2(fd, ft_h, &dn);

    toku_destroy_ftnode_internals(&sn);
    toku_free(ndd);

    ft_h->blocktable.block_free(block_allocator::BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
    ft_h->blocktable.destroy();
    toku_free(ft_h->h);
    ft_h->cmp.destroy();
    toku_free(ft_h);
    toku_free(ft);

    r = close(fd); assert(r != -1);
}

static void
test_serialize_leaf(void) {
    //    struct ft_handle source_ft;
    struct ftnode sn, *dn;

    int fd = open(TOKU_TEST_FILENAME, O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    int r;

    sn.max_msn_applied_to_node_on_disk.msn = 0;
    sn.flags = 0x11223344;
    sn.blocknum.b = 20;
    sn.layout_version = FT_LAYOUT_VERSION;
    sn.layout_version_original = FT_LAYOUT_VERSION;
    sn.height = 0;
    sn.n_children = 2;
    sn.dirty = 1;
    sn.oldest_referenced_xid_known = TXNID_NONE;
    MALLOC_N(sn.n_children, sn.bp);
    DBT pivotkey;
    sn.pivotkeys.create_from_dbts(toku_fill_dbt(&pivotkey, "b", 2), 1);
    BP_STATE(&sn,0) = PT_AVAIL;
    BP_STATE(&sn,1) = PT_AVAIL;
    set_BLB(&sn, 0, toku_create_empty_bn());
    set_BLB(&sn, 1, toku_create_empty_bn());
    le_malloc(BLB_DATA(&sn, 0), 0, "a", "aval");
    le_malloc(BLB_DATA(&sn, 0), 1, "b", "bval");
    le_malloc(BLB_DATA(&sn, 1), 0, "x", "xval");

    FT_HANDLE XMALLOC(ft);
    FT XCALLOC(ft_h);
    toku_ft_init(ft_h,
                 make_blocknum(0),
                 ZERO_LSN,
                 TXNID_NONE,
                 4*1024*1024,
                 128*1024,
                 TOKU_DEFAULT_COMPRESSION_METHOD,
                 16);
    ft->ft = ft_h;
    
    ft_h->blocktable.create();
    { int r_truncate = ftruncate(fd, 0); CKERR(r_truncate); }
    //Want to use block #20
    BLOCKNUM b = make_blocknum(0);
    while (b.b < 20) {
        ft_h->blocktable.allocate_blocknum(&b, ft_h);
    }
    assert(b.b == 20);

    {
        DISKOFF offset;
        DISKOFF size;
        ft_h->blocktable.realloc_on_disk(b, 100, &offset, ft_h, fd, false, 0);
        assert(offset==(DISKOFF)block_allocator::BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);

        ft_h->blocktable.translate_blocknum_to_offset_size(b, &offset, &size);
        assert(offset == (DISKOFF)block_allocator::BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
        assert(size   == 100);
    }
    FTNODE_DISK_DATA ndd = NULL;
    r = toku_serialize_ftnode_to(fd, make_blocknum(20), &sn, &ndd, true, ft->ft, false);
    assert(r==0);

    test1(fd, ft_h, &dn);
    test3_leaf(fd, ft_h,&dn);

    toku_destroy_ftnode_internals(&sn);

    ft_h->blocktable.block_free(block_allocator::BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
    ft_h->blocktable.destroy();
    toku_free(ft_h->h);
    toku_free(ft_h);
    toku_free(ft);
    toku_free(ndd);
    r = close(fd); assert(r != -1);
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    initialize_dummymsn();
    test_serialize_nonleaf();
    test_serialize_leaf();

    return 0;
}
