/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id: brt-serialize-test.c 36450 2011-11-02 20:10:18Z bperlman $"
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "test.h"

#include "includes.h"

enum brtnode_verify_type {
    read_all=1,
    read_compressed,
    read_none
};

static int
string_key_cmp(DB *UU(e), const DBT *a, const DBT *b)
{
    char *s = a->data, *t = b->data;
    return strcmp(s, t);
}

static void
test1(int fd, struct brt_header *brt_h, BRTNODE *dn) {
    int r;
    struct brtnode_fetch_extra bfe_all;
    fill_bfe_for_full_read(&bfe_all, brt_h, NULL, string_key_cmp);
    r = toku_deserialize_brtnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, dn, &bfe_all);
    assert(r==0);
    for (int i = 0; i < (*dn)->n_children; i++) {
        assert(BP_STATE(*dn,i) == PT_AVAIL);
    }
    // should sweep and NOT get rid of anything
    long bytes_freed;
    toku_brtnode_pe_callback(*dn, 0xffffffff, &bytes_freed, NULL);
    for (int i = 0; i < (*dn)->n_children; i++) {
        assert(BP_STATE(*dn,i) == PT_AVAIL);
    }
    // should sweep and get compress all
    toku_brtnode_pe_callback(*dn, 0xffffffff, &bytes_freed, NULL);
    for (int i = 0; i < (*dn)->n_children; i++) {
        assert(BP_STATE(*dn,i) == PT_COMPRESSED);
    }
    long size;
    toku_brtnode_pf_callback(*dn, &bfe_all, fd, &size);
    toku_brtnode_pe_callback(*dn, 0xffffffff, &bytes_freed, NULL);
    for (int i = 0; i < (*dn)->n_children; i++) {
        assert(BP_STATE(*dn,i) == PT_AVAIL);
    }
    // should sweep and get compress all
    toku_brtnode_pe_callback(*dn, 0xffffffff, &bytes_freed, NULL);
    for (int i = 0; i < (*dn)->n_children; i++) {
        assert(BP_STATE(*dn,i) == PT_COMPRESSED);
    }

    
    toku_brtnode_free(dn);
}


static int search_cmp(struct brt_search* UU(so), DBT* UU(key)) {
    return 0;
}

static void
test2(int fd, struct brt_header *brt_h, BRTNODE *dn) {
    int r;
    struct brtnode_fetch_extra bfe_subset;
    DBT left, right;
    DB dummy_db;
    memset(&dummy_db, 0, sizeof(dummy_db));
    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));
    brt_search_t search_t;
    
    fill_bfe_for_subset_read(
        &bfe_subset,
        brt_h,
        &dummy_db,
        string_key_cmp,
        brt_search_init(
            &search_t, 
            search_cmp, 
            BRT_SEARCH_LEFT, 
            NULL, 
            NULL
            ),
        &left,
        &right,
        TRUE,
        TRUE
        );

    
    r = toku_deserialize_brtnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, dn, &bfe_subset);
    printf("states %d %d %d %d\n", BP_STATE(*dn, 0), BP_SHOULD_EVICT(*dn, 0), BP_SHOULD_EVICT(*dn, 1), BP_STATE(*dn, 1));

    toku_brtnode_free(dn);

}



static void
test_serialize_nonleaf(void) {
    //    struct brt source_brt;
    const int nodesize = 1024;
    struct brtnode sn, *dn;

    int fd = open(__FILE__ ".brt", O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    int r;

    //    source_brt.fd=fd;
    sn.max_msn_applied_to_node_on_disk.msn = 0;
    char *hello_string;
    sn.nodesize = nodesize;
    sn.flags = 0x11223344;
    sn.thisnodename.b = 20;
    sn.layout_version = BRT_LAYOUT_VERSION;
    sn.layout_version_original = BRT_LAYOUT_VERSION;
    sn.height = 1;
    sn.optimized_for_upgrade = 1234;
    sn.n_children = 2;
    sn.dirty = 1;
    hello_string = toku_strdup("hello");
    MALLOC_N(2, sn.bp);
    MALLOC_N(1, sn.childkeys);
    sn.childkeys[0] = kv_pair_malloc(hello_string, 6, 0, 0);
    sn.totalchildkeylens = 6;
    BP_BLOCKNUM(&sn, 0).b = 30;
    BP_BLOCKNUM(&sn, 1).b = 35;
    BP_SUBTREE_EST(&sn,0).ndata = random() + (((long long)random())<<32);
    BP_SUBTREE_EST(&sn,1).ndata = random() + (((long long)random())<<32);
    BP_SUBTREE_EST(&sn,0).nkeys = random() + (((long long)random())<<32);
    BP_SUBTREE_EST(&sn,1).nkeys = random() + (((long long)random())<<32);
    BP_SUBTREE_EST(&sn,0).dsize = random() + (((long long)random())<<32);
    BP_SUBTREE_EST(&sn,1).dsize = random() + (((long long)random())<<32);
    BP_SUBTREE_EST(&sn,0).exact =  (BOOL)(random()%2 != 0);
    BP_SUBTREE_EST(&sn,1).exact =  (BOOL)(random()%2 != 0);
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

    r = toku_bnc_insert_msg(BNC(&sn, 0), "a", 2, "aval", 5, BRT_NONE, next_dummymsn(), xids_0, true, NULL, string_key_cmp); assert_zero(r);
    r = toku_bnc_insert_msg(BNC(&sn, 0), "b", 2, "bval", 5, BRT_NONE, next_dummymsn(), xids_123, false, NULL, string_key_cmp); assert_zero(r);
    r = toku_bnc_insert_msg(BNC(&sn, 1), "x", 2, "xval", 5, BRT_NONE, next_dummymsn(), xids_234, true, NULL, string_key_cmp); assert_zero(r);
    BNC(&sn, 0)->n_bytes_in_buffer = 2*(BRT_CMD_OVERHEAD+KEY_VALUE_OVERHEAD+2+5) + xids_get_serialize_size(xids_0) + xids_get_serialize_size(xids_123);
    BNC(&sn, 1)->n_bytes_in_buffer = 1*(BRT_CMD_OVERHEAD+KEY_VALUE_OVERHEAD+2+5) + xids_get_serialize_size(xids_234);
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

    test1(fd, brt_h, &dn);
    test2(fd, brt_h, &dn);


    kv_pair_free(sn.childkeys[0]);
    toku_free(hello_string);
    destroy_nonleaf_childinfo(BNC(&sn, 0));
    destroy_nonleaf_childinfo(BNC(&sn, 1));
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
    test_serialize_nonleaf();

    return 0;
}
