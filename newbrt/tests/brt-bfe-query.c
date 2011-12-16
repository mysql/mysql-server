/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2011 Tokutek Inc.  All rights reserved."

#include "test.h"
#include "includes.h"

static  int
int64_key_cmp (DB *db UU(), const DBT *a, const DBT *b) {
    int64_t x = *(int64_t *) a->data;
    int64_t y = *(int64_t *) b->data;

    if (x<y) return -1;
    if (x>y) return 1;
    return 0;
}

static void
test_prefetch_read(int fd, BRT UU(brt), struct brt_header *brt_h) {
    int r;
    brt_h->compare_fun = int64_key_cmp;    
    BRT_CURSOR cursor = toku_malloc(sizeof *cursor);
    BRTNODE dn = NULL;
    PAIR_ATTR attr;
    
    // first test that prefetching everything should work
    memset(&cursor->range_lock_left_key, 0 , sizeof(DBT));
    memset(&cursor->range_lock_right_key, 0 , sizeof(DBT));
    cursor->left_is_neg_infty = TRUE;
    cursor->right_is_pos_infty = TRUE;
    cursor->disable_prefetching = FALSE;
    
    struct brtnode_fetch_extra bfe;

    // quick test to see that we have the right behavior when we set
    // disable_prefetching to TRUE
    cursor->disable_prefetching = TRUE;
    fill_bfe_for_prefetch(&bfe, brt_h, cursor);
    r = toku_deserialize_brtnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, &dn, &bfe);
    assert(r==0);
    assert(dn->n_children == 3);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_ON_DISK);
    assert(BP_STATE(dn,2) == PT_ON_DISK);
    r = toku_brtnode_pf_callback(dn, &bfe, fd, &attr);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_ON_DISK);
    assert(BP_STATE(dn,2) == PT_ON_DISK);
    destroy_bfe_for_prefetch(&bfe);
    toku_brtnode_free(&dn);

    // now enable prefetching again
    cursor->disable_prefetching = FALSE;
    
    fill_bfe_for_prefetch(&bfe, brt_h, cursor);
    r = toku_deserialize_brtnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, &dn, &bfe);
    assert(r==0);
    assert(dn->n_children == 3);
    assert(BP_STATE(dn,0) == PT_AVAIL);
    assert(BP_STATE(dn,1) == PT_AVAIL);
    assert(BP_STATE(dn,2) == PT_AVAIL);
    toku_brtnode_pe_callback(dn, make_pair_attr(0xffffffff), &attr, NULL);
    assert(BP_STATE(dn,0) == PT_COMPRESSED);
    assert(BP_STATE(dn,1) == PT_COMPRESSED);
    assert(BP_STATE(dn,2) == PT_COMPRESSED);
    r = toku_brtnode_pf_callback(dn, &bfe, fd, &attr);
    assert(BP_STATE(dn,0) == PT_AVAIL);
    assert(BP_STATE(dn,1) == PT_AVAIL);
    assert(BP_STATE(dn,2) == PT_AVAIL);
    destroy_bfe_for_prefetch(&bfe);
    toku_brtnode_free(&dn);

    u_int64_t left_key = 150;
    toku_fill_dbt(&cursor->range_lock_left_key, &left_key, sizeof(u_int64_t));
    cursor->left_is_neg_infty = FALSE;
    fill_bfe_for_prefetch(&bfe, brt_h, cursor);
    r = toku_deserialize_brtnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, &dn, &bfe);
    assert(r==0);
    assert(dn->n_children == 3);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_AVAIL);
    assert(BP_STATE(dn,2) == PT_AVAIL);
    toku_brtnode_pe_callback(dn, make_pair_attr(0xffffffff), &attr, NULL);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_COMPRESSED);
    assert(BP_STATE(dn,2) == PT_COMPRESSED);
    r = toku_brtnode_pf_callback(dn, &bfe, fd, &attr);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_AVAIL);
    assert(BP_STATE(dn,2) == PT_AVAIL);
    destroy_bfe_for_prefetch(&bfe);
    toku_brtnode_free(&dn);

    u_int64_t right_key = 151;
    toku_fill_dbt(&cursor->range_lock_right_key, &right_key, sizeof(u_int64_t));
    cursor->right_is_pos_infty = FALSE;
    fill_bfe_for_prefetch(&bfe, brt_h, cursor);
    r = toku_deserialize_brtnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, &dn, &bfe);
    assert(r==0);
    assert(dn->n_children == 3);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_AVAIL);
    assert(BP_STATE(dn,2) == PT_ON_DISK);
    toku_brtnode_pe_callback(dn, make_pair_attr(0xffffffff), &attr, NULL);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_COMPRESSED);
    assert(BP_STATE(dn,2) == PT_ON_DISK);
    r = toku_brtnode_pf_callback(dn, &bfe, fd, &attr);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_AVAIL);
    assert(BP_STATE(dn,2) == PT_ON_DISK);
    destroy_bfe_for_prefetch(&bfe);
    toku_brtnode_free(&dn);

    left_key = 100000;
    right_key = 100000;
    fill_bfe_for_prefetch(&bfe, brt_h, cursor);
    r = toku_deserialize_brtnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, &dn, &bfe);
    assert(r==0);
    assert(dn->n_children == 3);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_ON_DISK);
    assert(BP_STATE(dn,2) == PT_AVAIL);
    toku_brtnode_pe_callback(dn, make_pair_attr(0xffffffff), &attr, NULL);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_ON_DISK);
    assert(BP_STATE(dn,2) == PT_COMPRESSED);
    r = toku_brtnode_pf_callback(dn, &bfe, fd, &attr);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_ON_DISK);
    assert(BP_STATE(dn,2) == PT_AVAIL);
    destroy_bfe_for_prefetch(&bfe);
    toku_brtnode_free(&dn);

    left_key = 100;
    right_key = 100;
    fill_bfe_for_prefetch(&bfe, brt_h, cursor);
    r = toku_deserialize_brtnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, &dn, &bfe);
    assert(r==0);
    assert(dn->n_children == 3);
    assert(BP_STATE(dn,0) == PT_AVAIL);
    assert(BP_STATE(dn,1) == PT_ON_DISK);
    assert(BP_STATE(dn,2) == PT_ON_DISK);
    toku_brtnode_pe_callback(dn, make_pair_attr(0xffffffff), &attr, NULL);
    assert(BP_STATE(dn,0) == PT_COMPRESSED);
    assert(BP_STATE(dn,1) == PT_ON_DISK);
    assert(BP_STATE(dn,2) == PT_ON_DISK);
    r = toku_brtnode_pf_callback(dn, &bfe, fd, &attr);
    assert(BP_STATE(dn,0) == PT_AVAIL);
    assert(BP_STATE(dn,1) == PT_ON_DISK);
    assert(BP_STATE(dn,2) == PT_ON_DISK);
    destroy_bfe_for_prefetch(&bfe);
    toku_brtnode_free(&dn);

    toku_free(cursor);
}

static void
test_subset_read(int fd, BRT UU(brt), struct brt_header *brt_h) {
    int r;
    brt_h->compare_fun = int64_key_cmp;    
    BRT_CURSOR cursor = toku_malloc(sizeof *cursor);
    BRTNODE dn = NULL;
    PAIR_ATTR attr;
    
    // first test that prefetching everything should work
    memset(&cursor->range_lock_left_key, 0 , sizeof(DBT));
    memset(&cursor->range_lock_right_key, 0 , sizeof(DBT));
    cursor->left_is_neg_infty = TRUE;
    cursor->right_is_pos_infty = TRUE;
    
    struct brtnode_fetch_extra bfe;

    u_int64_t left_key = 150;
    u_int64_t right_key = 151;
    DBT left, right;
    toku_fill_dbt(&left, &left_key, sizeof(left_key));
    toku_fill_dbt(&right, &right_key, sizeof(right_key));
    fill_bfe_for_subset_read(
        &bfe,
        brt_h,
        NULL, 
        &left,
        &right,
        FALSE,
        FALSE,
        FALSE
        );
    
    // fake the childnum to read
    // set disable_prefetching ON
    bfe.child_to_read = 2;
    bfe.disable_prefetching = TRUE;
    r = toku_deserialize_brtnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, &dn, &bfe);
    assert(r==0);
    assert(dn->n_children == 3);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_ON_DISK);
    assert(BP_STATE(dn,2) == PT_AVAIL);
    // need to call this twice because we had a subset read before, that touched the clock
    toku_brtnode_pe_callback(dn, make_pair_attr(0xffffffff), &attr, NULL);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_ON_DISK);
    assert(BP_STATE(dn,2) == PT_AVAIL);
    toku_brtnode_pe_callback(dn, make_pair_attr(0xffffffff), &attr, NULL);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_ON_DISK);
    assert(BP_STATE(dn,2) == PT_COMPRESSED);
    r = toku_brtnode_pf_callback(dn, &bfe, fd, &attr);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_ON_DISK);
    assert(BP_STATE(dn,2) == PT_AVAIL);
    toku_brtnode_free(&dn);

    // fake the childnum to read
    bfe.child_to_read = 2;
    bfe.disable_prefetching = FALSE;
    r = toku_deserialize_brtnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, &dn, &bfe);
    assert(r==0);
    assert(dn->n_children == 3);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_AVAIL);
    assert(BP_STATE(dn,2) == PT_AVAIL);
    // need to call this twice because we had a subset read before, that touched the clock
    toku_brtnode_pe_callback(dn, make_pair_attr(0xffffffff), &attr, NULL);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_COMPRESSED);
    assert(BP_STATE(dn,2) == PT_AVAIL);
    toku_brtnode_pe_callback(dn, make_pair_attr(0xffffffff), &attr, NULL);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_COMPRESSED);
    assert(BP_STATE(dn,2) == PT_COMPRESSED);
    r = toku_brtnode_pf_callback(dn, &bfe, fd, &attr);
    assert(BP_STATE(dn,0) == PT_ON_DISK);
    assert(BP_STATE(dn,1) == PT_AVAIL);
    assert(BP_STATE(dn,2) == PT_AVAIL);
    toku_brtnode_free(&dn);

    // fake the childnum to read
    bfe.child_to_read = 0;
    r = toku_deserialize_brtnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, &dn, &bfe);
    assert(r==0);
    assert(dn->n_children == 3);
    assert(BP_STATE(dn,0) == PT_AVAIL);
    assert(BP_STATE(dn,1) == PT_AVAIL);
    assert(BP_STATE(dn,2) == PT_ON_DISK);
    // need to call this twice because we had a subset read before, that touched the clock
    toku_brtnode_pe_callback(dn, make_pair_attr(0xffffffff), &attr, NULL);
    assert(BP_STATE(dn,0) == PT_AVAIL);
    assert(BP_STATE(dn,1) == PT_COMPRESSED);
    assert(BP_STATE(dn,2) == PT_ON_DISK);
    toku_brtnode_pe_callback(dn, make_pair_attr(0xffffffff), &attr, NULL);
    assert(BP_STATE(dn,0) == PT_COMPRESSED);
    assert(BP_STATE(dn,1) == PT_COMPRESSED);
    assert(BP_STATE(dn,2) == PT_ON_DISK);
    r = toku_brtnode_pf_callback(dn, &bfe, fd, &attr);
    assert(BP_STATE(dn,0) == PT_AVAIL);
    assert(BP_STATE(dn,1) == PT_AVAIL);
    assert(BP_STATE(dn,2) == PT_ON_DISK);
    toku_brtnode_free(&dn);

    toku_free(cursor);
}


static void
test_prefetching(void) {
    //    struct brt source_brt;
    const int nodesize = 1024;
    struct brtnode sn;

    int fd = open(__FILE__ ".brt", O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    int r;

    //    source_brt.fd=fd;
    sn.max_msn_applied_to_node_on_disk.msn = 0;
    sn.nodesize = nodesize;
    sn.flags = 0x11223344;
    sn.thisnodename.b = 20;
    sn.layout_version = BRT_LAYOUT_VERSION;
    sn.layout_version_original = BRT_LAYOUT_VERSION;
    sn.height = 1;
    sn.optimized_for_upgrade = 1234;
    sn.n_children = 3;
    sn.dirty = 1;

    u_int64_t key1 = 100;
    u_int64_t key2 = 200;
    
    MALLOC_N(sn.n_children, sn.bp);
    MALLOC_N(sn.n_children-1, sn.childkeys);
    sn.childkeys[0] = kv_pair_malloc(&key1, sizeof(key1), 0, 0);
    sn.childkeys[1] = kv_pair_malloc(&key2, sizeof(key2), 0, 0);
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
    BNC(&sn, 0)->n_bytes_in_buffer = 0;
    BNC(&sn, 1)->n_bytes_in_buffer = 0;
    BNC(&sn, 2)->n_bytes_in_buffer = 0;
    //Cleanup:
    xids_destroy(&xids_0);
    xids_destroy(&xids_123);
    xids_destroy(&xids_234);

    struct brt *XMALLOC(brt);
    struct brt_header *XCALLOC(brt_h);
    brt->h = brt_h;
    brt_h->type = BRTHEADER_CURRENT;
    brt_h->panic = 0; brt_h->panic_string = 0;
    brt_h->basementnodesize = 128*1024;
    toku_blocktable_create_new(&brt_h->blocktable);
    //Want to use block #20
    BLOCKNUM b = make_blocknum(0);
    while (b.b < 20) {
        toku_allocate_blocknum(brt_h->blocktable, &b, brt_h);
    }
    assert(b.b == 20);

    {
        DISKOFF offset;
        DISKOFF size;
        toku_blocknum_realloc_on_disk(brt_h->blocktable, b, 100, &offset, brt_h, FALSE);
        assert(offset==BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);

        toku_translate_blocknum_to_offset_size(brt_h->blocktable, b, &offset, &size);
        assert(offset == BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
        assert(size   == 100);
    }

    r = toku_serialize_brtnode_to(fd, make_blocknum(20), &sn, brt->h, 1, 1, FALSE);
    assert(r==0);

    test_prefetch_read(fd, brt, brt_h);    
    test_subset_read(fd, brt, brt_h);

    kv_pair_free(sn.childkeys[0]);
    kv_pair_free(sn.childkeys[1]);
    destroy_nonleaf_childinfo(BNC(&sn, 0));
    destroy_nonleaf_childinfo(BNC(&sn, 1));
    destroy_nonleaf_childinfo(BNC(&sn, 2));
    toku_free(sn.bp);
    toku_free(sn.childkeys);

    toku_block_free(brt_h->blocktable, BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
    toku_blocktable_destroy(&brt_h->blocktable);
    toku_free(brt_h);
    toku_free(brt);

    r = close(fd); assert(r != -1);
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    test_prefetching();

    return 0;
}
