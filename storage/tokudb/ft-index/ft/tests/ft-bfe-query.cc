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

#ident "Copyright (c) 2011-2013 Tokutek Inc.  All rights reserved."

#include "test.h"

static  int
int64_key_cmp (DB *db UU(), const DBT *a, const DBT *b) {
    int64_t x = *(int64_t *) a->data;
    int64_t y = *(int64_t *) b->data;

    if (x<y) return -1;
    if (x>y) return 1;
    return 0;
}

static void
test_prefetch_read(int fd, FT_HANDLE UU(brt), FT brt_h) {
    int r;
    brt_h->compare_fun = int64_key_cmp;    
    FT_CURSOR XMALLOC(cursor);
    FTNODE dn = NULL;
    PAIR_ATTR attr;
    
    // first test that prefetching everything should work
    memset(&cursor->range_lock_left_key, 0 , sizeof(DBT));
    memset(&cursor->range_lock_right_key, 0 , sizeof(DBT));
    cursor->left_is_neg_infty = true;
    cursor->right_is_pos_infty = true;
    cursor->disable_prefetching = false;
    
    struct ftnode_fetch_extra bfe;

    // quick test to see that we have the right behavior when we set
    // disable_prefetching to true
    cursor->disable_prefetching = true;
    fill_bfe_for_prefetch(&bfe, brt_h, cursor);
    FTNODE_DISK_DATA ndd = NULL;
    r = toku_deserialize_ftnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, &dn, &ndd, &bfe);
    assert(r==0);
    assert(dn->n_children == 3);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_ON_DISK);
    assert(BP_STATE(dn,2) == PT_ON_DISK);
    r = toku_ftnode_pf_callback(dn, ndd, &bfe, fd, &attr);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_ON_DISK);
    assert(BP_STATE(dn,2) == PT_ON_DISK);
    destroy_bfe_for_prefetch(&bfe);
    toku_ftnode_free(&dn);
    toku_free(ndd);

    // now enable prefetching again
    cursor->disable_prefetching = false;
    
    fill_bfe_for_prefetch(&bfe, brt_h, cursor);
    r = toku_deserialize_ftnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, &dn, &ndd, &bfe);
    assert(r==0);
    assert(dn->n_children == 3);
    assert(BP_STATE(dn,0) == PT_AVAIL);
    assert(BP_STATE(dn,1) == PT_AVAIL);
    assert(BP_STATE(dn,2) == PT_AVAIL);
    toku_ftnode_pe_callback(dn, make_pair_attr(0xffffffff), &attr, brt_h);
    assert(BP_STATE(dn,0) == PT_COMPRESSED);
    assert(BP_STATE(dn,1) == PT_COMPRESSED);
    assert(BP_STATE(dn,2) == PT_COMPRESSED);
    r = toku_ftnode_pf_callback(dn, ndd, &bfe, fd, &attr);
    assert(BP_STATE(dn,0) == PT_AVAIL);
    assert(BP_STATE(dn,1) == PT_AVAIL);
    assert(BP_STATE(dn,2) == PT_AVAIL);
    destroy_bfe_for_prefetch(&bfe);
    toku_ftnode_free(&dn);
    toku_free(ndd);

    uint64_t left_key = 150;
    toku_fill_dbt(&cursor->range_lock_left_key, &left_key, sizeof(uint64_t));
    cursor->left_is_neg_infty = false;
    fill_bfe_for_prefetch(&bfe, brt_h, cursor);
    r = toku_deserialize_ftnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, &dn, &ndd, &bfe);
    assert(r==0);
    assert(dn->n_children == 3);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_AVAIL);
    assert(BP_STATE(dn,2) == PT_AVAIL);
    toku_ftnode_pe_callback(dn, make_pair_attr(0xffffffff), &attr, brt_h);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_COMPRESSED);
    assert(BP_STATE(dn,2) == PT_COMPRESSED);
    r = toku_ftnode_pf_callback(dn, ndd, &bfe, fd, &attr);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_AVAIL);
    assert(BP_STATE(dn,2) == PT_AVAIL);
    destroy_bfe_for_prefetch(&bfe);
    toku_ftnode_free(&dn);
    toku_free(ndd);

    uint64_t right_key = 151;
    toku_fill_dbt(&cursor->range_lock_right_key, &right_key, sizeof(uint64_t));
    cursor->right_is_pos_infty = false;
    fill_bfe_for_prefetch(&bfe, brt_h, cursor);
    r = toku_deserialize_ftnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, &dn, &ndd, &bfe);
    assert(r==0);
    assert(dn->n_children == 3);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_AVAIL);
    assert(BP_STATE(dn,2) == PT_ON_DISK);
    toku_ftnode_pe_callback(dn, make_pair_attr(0xffffffff), &attr, brt_h);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_COMPRESSED);
    assert(BP_STATE(dn,2) == PT_ON_DISK);
    r = toku_ftnode_pf_callback(dn, ndd, &bfe, fd, &attr);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_AVAIL);
    assert(BP_STATE(dn,2) == PT_ON_DISK);
    destroy_bfe_for_prefetch(&bfe);
    toku_ftnode_free(&dn);
    toku_free(ndd);

    left_key = 100000;
    right_key = 100000;
    fill_bfe_for_prefetch(&bfe, brt_h, cursor);
    r = toku_deserialize_ftnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, &dn, &ndd, &bfe);
    assert(r==0);
    assert(dn->n_children == 3);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_ON_DISK);
    assert(BP_STATE(dn,2) == PT_AVAIL);
    toku_ftnode_pe_callback(dn, make_pair_attr(0xffffffff), &attr, brt_h);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_ON_DISK);
    assert(BP_STATE(dn,2) == PT_COMPRESSED);
    r = toku_ftnode_pf_callback(dn, ndd, &bfe, fd, &attr);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_ON_DISK);
    assert(BP_STATE(dn,2) == PT_AVAIL);
    destroy_bfe_for_prefetch(&bfe);
    toku_free(ndd);
    toku_ftnode_free(&dn);

    left_key = 100;
    right_key = 100;
    fill_bfe_for_prefetch(&bfe, brt_h, cursor);
    r = toku_deserialize_ftnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, &dn, &ndd, &bfe);
    assert(r==0);
    assert(dn->n_children == 3);
    assert(BP_STATE(dn,0) == PT_AVAIL);
    assert(BP_STATE(dn,1) == PT_ON_DISK);
    assert(BP_STATE(dn,2) == PT_ON_DISK);
    toku_ftnode_pe_callback(dn, make_pair_attr(0xffffffff), &attr, brt_h);
    assert(BP_STATE(dn,0) == PT_COMPRESSED);
    assert(BP_STATE(dn,1) == PT_ON_DISK);
    assert(BP_STATE(dn,2) == PT_ON_DISK);
    r = toku_ftnode_pf_callback(dn, ndd, &bfe, fd, &attr);
    assert(BP_STATE(dn,0) == PT_AVAIL);
    assert(BP_STATE(dn,1) == PT_ON_DISK);
    assert(BP_STATE(dn,2) == PT_ON_DISK);
    destroy_bfe_for_prefetch(&bfe);
    toku_ftnode_free(&dn);
    toku_free(ndd);

    toku_free(cursor);
}

static void
test_subset_read(int fd, FT_HANDLE UU(brt), FT brt_h) {
    int r;
    brt_h->compare_fun = int64_key_cmp;    
    FT_CURSOR XMALLOC(cursor);
    FTNODE dn = NULL;
    FTNODE_DISK_DATA ndd = NULL;
    PAIR_ATTR attr;
    
    // first test that prefetching everything should work
    memset(&cursor->range_lock_left_key, 0 , sizeof(DBT));
    memset(&cursor->range_lock_right_key, 0 , sizeof(DBT));
    cursor->left_is_neg_infty = true;
    cursor->right_is_pos_infty = true;
    
    struct ftnode_fetch_extra bfe;

    uint64_t left_key = 150;
    uint64_t right_key = 151;
    DBT left, right;
    toku_fill_dbt(&left, &left_key, sizeof(left_key));
    toku_fill_dbt(&right, &right_key, sizeof(right_key));
    fill_bfe_for_subset_read(
        &bfe,
        brt_h,
        NULL, 
        &left,
        &right,
        false,
        false,
        false,
        false
        );
    
    // fake the childnum to read
    // set disable_prefetching ON
    bfe.child_to_read = 2;
    bfe.disable_prefetching = true;
    r = toku_deserialize_ftnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, &dn, &ndd, &bfe);
    assert(r==0);
    assert(dn->n_children == 3);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_ON_DISK);
    assert(BP_STATE(dn,2) == PT_AVAIL);
    // need to call this twice because we had a subset read before, that touched the clock
    toku_ftnode_pe_callback(dn, make_pair_attr(0xffffffff), &attr, brt_h);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_ON_DISK);
    assert(BP_STATE(dn,2) == PT_AVAIL);
    toku_ftnode_pe_callback(dn, make_pair_attr(0xffffffff), &attr, brt_h);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_ON_DISK);
    assert(BP_STATE(dn,2) == PT_COMPRESSED);
    r = toku_ftnode_pf_callback(dn, ndd, &bfe, fd, &attr);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_ON_DISK);
    assert(BP_STATE(dn,2) == PT_AVAIL);
    toku_ftnode_free(&dn);
    toku_free(ndd);

    // fake the childnum to read
    bfe.child_to_read = 2;
    bfe.disable_prefetching = false;
    r = toku_deserialize_ftnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, &dn, &ndd, &bfe);
    assert(r==0);
    assert(dn->n_children == 3);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_AVAIL);
    assert(BP_STATE(dn,2) == PT_AVAIL);
    // need to call this twice because we had a subset read before, that touched the clock
    toku_ftnode_pe_callback(dn, make_pair_attr(0xffffffff), &attr, brt_h);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_COMPRESSED);
    assert(BP_STATE(dn,2) == PT_AVAIL);
    toku_ftnode_pe_callback(dn, make_pair_attr(0xffffffff), &attr, brt_h);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_COMPRESSED);
    assert(BP_STATE(dn,2) == PT_COMPRESSED);
    r = toku_ftnode_pf_callback(dn, ndd, &bfe, fd, &attr);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_AVAIL);
    assert(BP_STATE(dn,2) == PT_AVAIL);
    toku_ftnode_free(&dn);
    toku_free(ndd);

    // fake the childnum to read
    bfe.child_to_read = 0;
    r = toku_deserialize_ftnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, &dn, &ndd, &bfe);
    assert(r==0);
    assert(dn->n_children == 3);
    assert(BP_STATE(dn,0) == PT_AVAIL);
    assert(BP_STATE(dn,1) == PT_AVAIL);
    assert(BP_STATE(dn,2) == PT_ON_DISK);
    // need to call this twice because we had a subset read before, that touched the clock
    toku_ftnode_pe_callback(dn, make_pair_attr(0xffffffff), &attr, brt_h);
    assert(BP_STATE(dn,0) == PT_AVAIL);
    assert(BP_STATE(dn,1) == PT_COMPRESSED);
    assert(BP_STATE(dn,2) == PT_ON_DISK);
    toku_ftnode_pe_callback(dn, make_pair_attr(0xffffffff), &attr, brt_h);
    assert(BP_STATE(dn,0) == PT_COMPRESSED);
    assert(BP_STATE(dn,1) == PT_COMPRESSED);
    assert(BP_STATE(dn,2) == PT_ON_DISK);
    r = toku_ftnode_pf_callback(dn, ndd, &bfe, fd, &attr);
    assert(BP_STATE(dn,0) == PT_AVAIL);
    assert(BP_STATE(dn,1) == PT_AVAIL);
    assert(BP_STATE(dn,2) == PT_ON_DISK);
    toku_ftnode_free(&dn);
    toku_free(ndd);

    toku_free(cursor);
}


static void
test_prefetching(void) {
    //    struct ft_handle source_ft;
    struct ftnode sn;

    int fd = open(TOKU_TEST_FILENAME, O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    int r;

    //    source_ft.fd=fd;
    sn.max_msn_applied_to_node_on_disk.msn = 0;
    sn.flags = 0x11223344;
    sn.thisnodename.b = 20;
    sn.layout_version = FT_LAYOUT_VERSION;
    sn.layout_version_original = FT_LAYOUT_VERSION;
    sn.height = 1;
    sn.n_children = 3;
    sn.dirty = 1;
    sn.oldest_referenced_xid_known = TXNID_NONE;

    uint64_t key1 = 100;
    uint64_t key2 = 200;
    
    MALLOC_N(sn.n_children, sn.bp);
    MALLOC_N(sn.n_children-1, sn.childkeys);
    toku_memdup_dbt(&sn.childkeys[0], &key1, sizeof(key1));
    toku_memdup_dbt(&sn.childkeys[1], &key2, sizeof(key2));
    sn.totalchildkeylens = sizeof(key1) + sizeof(key2);
    BP_BLOCKNUM(&sn, 0).b = 30;
    BP_BLOCKNUM(&sn, 1).b = 35;
    BP_BLOCKNUM(&sn, 2).b = 40;
    BP_STATE(&sn,0) = PT_AVAIL;
    BP_STATE(&sn,1) = PT_AVAIL;
    BP_STATE(&sn,2) = PT_AVAIL;
    set_BNC(&sn, 0, toku_create_empty_nl());
    set_BNC(&sn, 1, toku_create_empty_nl());
    set_BNC(&sn, 2, toku_create_empty_nl());
    //Create XIDS
    XIDS xids_0 = xids_get_root_xids();
    XIDS xids_123;
    XIDS xids_234;
    r = xids_create_child(xids_0, &xids_123, (TXNID)123);
    CKERR(r);
    r = xids_create_child(xids_123, &xids_234, (TXNID)234);
    CKERR(r);

    // data in the buffers does not matter in this test
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
    FTNODE_DISK_DATA ndd = NULL;
    r = toku_serialize_ftnode_to(fd, make_blocknum(20), &sn, &ndd, true, brt->ft, false);
    assert(r==0);

    test_prefetch_read(fd, brt, brt_h);    
    test_subset_read(fd, brt, brt_h);

    toku_free(sn.childkeys[0].data);
    toku_free(sn.childkeys[1].data);
    destroy_nonleaf_childinfo(BNC(&sn, 0));
    destroy_nonleaf_childinfo(BNC(&sn, 1));
    destroy_nonleaf_childinfo(BNC(&sn, 2));
    toku_free(sn.bp);
    toku_free(sn.childkeys);

    toku_block_free(brt_h->blocktable, BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
    toku_blocktable_destroy(&brt_h->blocktable);
    toku_free(brt_h->h);
    toku_free(brt_h);
    toku_free(brt);
    toku_free(ndd);

    r = close(fd); assert(r != -1);
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    test_prefetching();

    return 0;
}
