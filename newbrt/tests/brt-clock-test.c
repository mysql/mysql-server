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

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

static int
string_key_cmp(DB *UU(e), const DBT *a, const DBT *b)
{
    char *s = a->data, *t = b->data;
    return strcmp(s, t);
}

static int omt_cmp(OMTVALUE p, void *q)
{
    LEAFENTRY a = p, b = q;
    void *ak, *bk;
    u_int32_t al, bl;
    ak = le_key_and_len(a, &al);
    bk = le_key_and_len(b, &bl);
    int l = MIN(al, bl);
    int c = memcmp(ak, bk, l);
    if (c < 0) { return -1; }
    if (c > 0) { return +1; }
    int d = al - bl;
    if (d < 0) { return -1; }
    if (d > 0) { return +1; }
    else       { return  0; }
}

static LEAFENTRY
le_fastmalloc(char *key, int keylen, char *val, int vallen)
{
    LEAFENTRY r = toku_malloc(sizeof(r->type) + sizeof(r->keylen) + sizeof(r->u.clean.vallen) +
                              keylen + vallen);
    resource_assert(r);
    r->type = LE_CLEAN;
    r->keylen = keylen;
    r->u.clean.vallen = vallen;
    memcpy(&r->u.clean.key_val[0], key, keylen);
    memcpy(&r->u.clean.key_val[keylen], val, vallen);
    return r;
}

static LEAFENTRY
le_malloc(char *key, char *val)
{
    int keylen = strlen(key) + 1;
    int vallen = strlen(val) + 1;
    return le_fastmalloc(key, keylen, val, vallen);
}


static void
test1(int fd, struct brt_header *brt_h, BRTNODE *dn) {
    int r;
    struct brtnode_fetch_extra bfe_all;
    brt_h->compare_fun = string_key_cmp;
    fill_bfe_for_full_read(&bfe_all, brt_h);
    r = toku_deserialize_brtnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, dn, &bfe_all);
    BOOL is_leaf = ((*dn)->height == 0);
    assert(r==0);
    for (int i = 0; i < (*dn)->n_children; i++) {
        assert(BP_STATE(*dn,i) == PT_AVAIL);
    }
    // should sweep and NOT get rid of anything
    PAIR_ATTR attr;
    memset(&attr,0,sizeof(attr));
    toku_brtnode_pe_callback(*dn, attr, &attr, NULL);
    for (int i = 0; i < (*dn)->n_children; i++) {
        assert(BP_STATE(*dn,i) == PT_AVAIL);
    }
    // should sweep and get compress all
    toku_brtnode_pe_callback(*dn, attr, &attr, NULL);
    for (int i = 0; i < (*dn)->n_children; i++) {
        if (!is_leaf) {
            assert(BP_STATE(*dn,i) == PT_COMPRESSED);
        }
        else {
            assert(BP_STATE(*dn,i) == PT_ON_DISK);
        }
    }
    PAIR_ATTR size;
    BOOL req = toku_brtnode_pf_req_callback(*dn, &bfe_all);
    assert(req);
    toku_brtnode_pf_callback(*dn, &bfe_all, fd, &size);
    toku_brtnode_pe_callback(*dn, attr, &attr, NULL);
    for (int i = 0; i < (*dn)->n_children; i++) {
        assert(BP_STATE(*dn,i) == PT_AVAIL);
    }
    // should sweep and get compress all
    toku_brtnode_pe_callback(*dn, attr, &attr, NULL);
    for (int i = 0; i < (*dn)->n_children; i++) {
        if (!is_leaf) {
            assert(BP_STATE(*dn,i) == PT_COMPRESSED);
        }
        else {
            assert(BP_STATE(*dn,i) == PT_ON_DISK);
        }
    }    

    req = toku_brtnode_pf_req_callback(*dn, &bfe_all);
    assert(req);
    toku_brtnode_pf_callback(*dn, &bfe_all, fd, &size);
    toku_brtnode_pe_callback(*dn, attr, &attr, NULL);
    for (int i = 0; i < (*dn)->n_children; i++) {
        assert(BP_STATE(*dn,i) == PT_AVAIL);
    }
    (*dn)->dirty = 1;
    toku_brtnode_pe_callback(*dn, attr, &attr, NULL);
    toku_brtnode_pe_callback(*dn, attr, &attr, NULL);
    toku_brtnode_pe_callback(*dn, attr, &attr, NULL);
    toku_brtnode_pe_callback(*dn, attr, &attr, NULL);
    for (int i = 0; i < (*dn)->n_children; i++) {
        assert(BP_STATE(*dn,i) == PT_AVAIL);
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
    
    brt_h->compare_fun = string_key_cmp;
    fill_bfe_for_subset_read(
        &bfe_subset,
        brt_h,
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
        TRUE,
        FALSE
        );

    r = toku_deserialize_brtnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, dn, &bfe_subset);
    BOOL is_leaf = ((*dn)->height == 0);
    // at this point, although both partitions are available, only the 
    // second basement node should have had its clock
    // touched
    assert(BP_STATE(*dn, 0) == PT_AVAIL);
    assert(BP_STATE(*dn, 1) == PT_AVAIL);
    assert(BP_SHOULD_EVICT(*dn, 0));
    assert(!BP_SHOULD_EVICT(*dn, 1));
    PAIR_ATTR attr;
    memset(&attr,0,sizeof(attr));
    toku_brtnode_pe_callback(*dn, attr, &attr, NULL);
    assert(BP_STATE(*dn, 0) == (is_leaf) ? PT_ON_DISK : PT_COMPRESSED);
    assert(BP_STATE(*dn, 1) == PT_AVAIL);
    assert(BP_SHOULD_EVICT(*dn, 1));
    toku_brtnode_pe_callback(*dn, attr, &attr, NULL);
    assert(BP_STATE(*dn, 1) == (is_leaf) ? PT_ON_DISK : PT_COMPRESSED);

    BOOL req = toku_brtnode_pf_req_callback(*dn, &bfe_subset);
    assert(req);
    toku_brtnode_pf_callback(*dn, &bfe_subset, fd, &attr);
    assert(BP_STATE(*dn, 0) == PT_AVAIL);
    assert(BP_STATE(*dn, 1) == PT_AVAIL);
    assert(BP_SHOULD_EVICT(*dn, 0));
    assert(!BP_SHOULD_EVICT(*dn, 1));


    toku_brtnode_free(dn);
}

static void
test3_leaf(int fd, struct brt_header *brt_h, BRTNODE *dn) {
    int r;
    struct brtnode_fetch_extra bfe_min;
    DBT left, right;
    DB dummy_db;
    memset(&dummy_db, 0, sizeof(dummy_db));
    memset(&left, 0, sizeof(left));
    memset(&right, 0, sizeof(right));
    
    brt_h->compare_fun = string_key_cmp;
    fill_bfe_for_min_read(
        &bfe_min,
        brt_h
        );

    r = toku_deserialize_brtnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, dn, &bfe_min);
    //
    // make sure we have a leaf
    //
    assert((*dn)->height == 0);
    for (int i = 0; i < (*dn)->n_children; i++) {
        assert(BP_STATE(*dn, i) == PT_ON_DISK);
    }
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

static void
test_serialize_leaf(void) {
    //    struct brt source_brt;
    const int nodesize = 1024;
    struct brtnode sn, *dn;

    int fd = open(__FILE__ ".brt", O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    int r;

    sn.max_msn_applied_to_node_on_disk.msn = 0;
    sn.nodesize = nodesize;
    sn.flags = 0x11223344;
    sn.thisnodename.b = 20;
    sn.layout_version = BRT_LAYOUT_VERSION;
    sn.layout_version_original = BRT_LAYOUT_VERSION;
    sn.height = 0;
    sn.optimized_for_upgrade = 1234;
    sn.n_children = 2;
    sn.dirty = 1;
    LEAFENTRY elts[3];
    elts[0] = le_malloc("a", "aval");
    elts[1] = le_malloc("b", "bval");
    elts[2] = le_malloc("x", "xval");
    MALLOC_N(sn.n_children, sn.bp);
    MALLOC_N(1, sn.childkeys);
    sn.childkeys[0] = kv_pair_malloc("b", 2, 0, 0);
    sn.totalchildkeylens = 2;
    BP_STATE(&sn,0) = PT_AVAIL;
    BP_STATE(&sn,1) = PT_AVAIL;
    set_BLB(&sn, 0, toku_create_empty_bn());
    set_BLB(&sn, 1, toku_create_empty_bn());
    r = toku_omt_insert(BLB_BUFFER(&sn, 0), elts[0], omt_cmp, elts[0], NULL); assert(r==0);
    r = toku_omt_insert(BLB_BUFFER(&sn, 0), elts[1], omt_cmp, elts[1], NULL); assert(r==0);
    r = toku_omt_insert(BLB_BUFFER(&sn, 1), elts[2], omt_cmp, elts[2], NULL); assert(r==0);
    BLB_NBYTESINBUF(&sn, 0) = 2*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(BLB_BUFFER(&sn, 0));
    BLB_NBYTESINBUF(&sn, 1) = 1*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(BLB_BUFFER(&sn, 1));

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
    test3_leaf(fd, brt_h,&dn);

    for (int i = 0; i < sn.n_children-1; ++i) {
        kv_pair_free(sn.childkeys[i]);
    }
    for (int i = 0; i < 3; ++i) {
        toku_free(elts[i]);
    }
    for (int i = 0; i < sn.n_children; i++) {
        struct mempool * mp = &(BLB_BUFFER_MEMPOOL(&sn, i));
	toku_mempool_destroy(mp);
        destroy_basement_node(BLB(&sn, i));
    }
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
    test_serialize_leaf();

    return 0;
}
