/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "test.h"

#include "includes.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))

static int omt_int_cmp(OMTVALUE p, void *q)
{
    LEAFENTRY a = p, b = q;
    void *ak, *bk;
    u_int32_t al, bl;
    ak = le_key_and_len(a, &al);
    bk = le_key_and_len(b, &bl);
    assert(al == 4 && bl == 4);
    int ai = *(int *) ak;
    int bi = *(int *) bk;
    int c = ai - bi;
    if (c < 0) { return -1; }
    if (c > 0) { return +1; }
    else       { return  0; }
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

struct check_leafentries_struct {
    int nelts;
    LEAFENTRY *elts;
    int i;
    int (*cmp)(OMTVALUE, void *);
};

static int check_leafentries(OMTVALUE v, u_int32_t UU(i), void *extra) {
    struct check_leafentries_struct *e = extra;
    assert(e->i < e->nelts);
    assert(e->cmp(v, e->elts[e->i]) == 0);
    e->i++;
    return 0;
}

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
setup_dn(enum brtnode_verify_type bft, int fd, struct brt_header *brt_h, BRTNODE *dn) {
    int r;
    if (bft == read_all) {
        struct brtnode_fetch_extra bfe;
        fill_bfe_for_full_read(&bfe, brt_h, NULL, string_key_cmp);
        r = toku_deserialize_brtnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, dn, &bfe);
        assert(r==0);
    }
    else if (bft == read_compressed || bft == read_none) {
        struct brtnode_fetch_extra bfe;
        fill_bfe_for_min_read(&bfe, brt_h, NULL, string_key_cmp);
        r = toku_deserialize_brtnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, dn, &bfe);
        assert(r==0);
        // assert all bp's are compressed or on disk.
        for (int i = 0; i < (*dn)->n_children; i++) {
            assert(BP_STATE(*dn,i) == PT_COMPRESSED || BP_STATE(*dn, i) == PT_ON_DISK);
        }
        // if read_none, get rid of the compressed bp's
        if (bft == read_none) {
            if ((*dn)->height == 0) {
                long bytes_freed = 0;
                toku_brtnode_pe_callback(*dn, 0xffffffff, &bytes_freed, NULL);
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
                fill_bfe_for_full_read(&bfe, brt_h, NULL, string_key_cmp);
                assert(toku_brtnode_pf_req_callback(*dn, &bfe));
                long size;
                r = toku_brtnode_pf_callback(*dn, &bfe, fd, &size);
                assert(r==0);
                // assert all bp's are available
                for (int i = 0; i < (*dn)->n_children; i++) {
                    assert(BP_STATE(*dn,i) == PT_AVAIL);
                }
                long bytes_freed = 0;
                toku_brtnode_pe_callback(*dn, 0xffffffff, &bytes_freed, NULL);
                for (int i = 0; i < (*dn)->n_children; i++) {
                    // assert all bp's are still available, because we touched the clock
                    assert(BP_STATE(*dn,i) == PT_AVAIL);
                    // now assert all should be evicted
                    assert(BP_SHOULD_EVICT(*dn, i));
                }
                toku_brtnode_pe_callback(*dn, 0xffffffff, &bytes_freed, NULL);
                for (int i = 0; i < (*dn)->n_children; i++) {
                    assert(BP_STATE(*dn,i) == PT_COMPRESSED);
                }
            }
        }
        // now decompress them
        fill_bfe_for_full_read(&bfe, brt_h, NULL, string_key_cmp);
        assert(toku_brtnode_pf_req_callback(*dn, &bfe));
        long size;
        r = toku_brtnode_pf_callback(*dn, &bfe, fd, &size);
        assert(r==0);
        // assert all bp's are available
        for (int i = 0; i < (*dn)->n_children; i++) {
            assert(BP_STATE(*dn,i) == PT_AVAIL);
        }
        // continue on with test
    }
    else {
        // if we get here, this is a test bug, NOT a bug in development code
        assert(FALSE);
    }
}

static void
test_serialize_leaf_check_msn(enum brtnode_verify_type bft) {
    //    struct brt source_brt;
    const int nodesize = 1024;
    struct brtnode sn, *dn;

    int fd = open(__FILE__ ".brt", O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    int r;

#define PRESERIALIZE_MSN_ON_DISK ((MSN) { MIN_MSN.msn + 42 })
#define POSTSERIALIZE_MSN_ON_DISK ((MSN) { MIN_MSN.msn + 84 })

    sn.max_msn_applied_to_node_on_disk = PRESERIALIZE_MSN_ON_DISK;
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
    set_BLB(&sn, 0, toku_create_empty_bn());
    set_BLB(&sn, 1, toku_create_empty_bn());
    r = toku_omt_insert(BLB_BUFFER(&sn, 0), elts[0], omt_cmp, elts[0], NULL); assert(r==0);
    r = toku_omt_insert(BLB_BUFFER(&sn, 0), elts[1], omt_cmp, elts[1], NULL); assert(r==0);
    r = toku_omt_insert(BLB_BUFFER(&sn, 1), elts[2], omt_cmp, elts[2], NULL); assert(r==0);
    BLB_NBYTESINBUF(&sn, 0) = 2*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(BLB_BUFFER(&sn, 0));
    BLB_NBYTESINBUF(&sn, 1) = 1*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(BLB_BUFFER(&sn, 1));
    BLB_MAX_MSN_APPLIED(&sn, 0) = ((MSN) { MIN_MSN.msn + 73 });
    BLB_MAX_MSN_APPLIED(&sn, 1) = POSTSERIALIZE_MSN_ON_DISK;

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

    setup_dn(bft, fd, brt_h, &dn);

    //
    // test that subtree estimates get set
    // rebalancing should make it 1 basement
    //
    assert(BP_SUBTREE_EST(&sn,0).nkeys == 3);
    assert(BP_SUBTREE_EST(dn,0).nkeys == 3);
    assert(BP_SUBTREE_EST(&sn,0).ndata == 3);
    assert(BP_SUBTREE_EST(dn,0).ndata == 3);


    assert(dn->thisnodename.b==20);

    assert(dn->layout_version ==BRT_LAYOUT_VERSION);
    assert(dn->layout_version_original ==BRT_LAYOUT_VERSION);
    assert(dn->layout_version_read_from_disk ==BRT_LAYOUT_VERSION);
    assert(dn->height == 0);
    assert(dn->optimized_for_upgrade == 1234);
    assert(dn->n_children>=1);
    assert(dn->max_msn_applied_to_node_on_disk.msn == POSTSERIALIZE_MSN_ON_DISK.msn);
    {
        const u_int32_t npartitions = dn->n_children;
        assert(dn->totalchildkeylens==(2*(npartitions-1)));
        struct check_leafentries_struct extra = { .nelts = 3, .elts = elts, .i = 0, .cmp = omt_cmp };
        u_int32_t last_i = 0;
        for (u_int32_t i = 0; i < npartitions; ++i) {
            assert(BLB_MAX_MSN_APPLIED(dn, i).msn == POSTSERIALIZE_MSN_ON_DISK.msn);
            assert(dn->bp[i].start > 0);
            assert(dn->bp[i].size  > 0);
            if (i > 0) {
                assert(dn->bp[i].start >= dn->bp[i-1].start + dn->bp[i-1].size);
            }
            toku_omt_iterate(BLB_BUFFER(dn, i), check_leafentries, &extra);
            u_int32_t keylen;
            if (i < npartitions-1) {
                assert(strcmp(kv_pair_key(dn->childkeys[i]), le_key_and_len(elts[extra.i-1], &keylen))==0);
            }
            // don't check soft_copy_is_up_to_date or seqinsert
            assert(BLB_NBYTESINBUF(dn, i) == (extra.i-last_i)*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(BLB_BUFFER(dn, i)));
            last_i = extra.i;
        }
        assert(extra.i == 3);
    }
    toku_brtnode_free(&dn);

    for (int i = 0; i < sn.n_children-1; ++i) {
        kv_pair_free(sn.childkeys[i]);
    }
    for (int i = 0; i < 3; ++i) {
        toku_free(elts[i]);
    }
    for (int i = 0; i < sn.n_children; i++) {
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

static void
test_serialize_leaf_with_large_pivots(enum brtnode_verify_type bft) {
    int r;
    struct brtnode sn, *dn;
    const int keylens = 256*1024, vallens = 0, nrows = 8;
    // assert(val_size > BN_MAX_SIZE);  // BN_MAX_SIZE isn't visible
    int fd = open(__FILE__ ".brt", O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    sn.max_msn_applied_to_node_on_disk.msn = 0;
    sn.nodesize = 4*(1<<20);
    sn.flags = 0x11223344;
    sn.thisnodename.b = 20;
    sn.layout_version = BRT_LAYOUT_VERSION;
    sn.layout_version_original = BRT_LAYOUT_VERSION;
    sn.height = 0;
    sn.optimized_for_upgrade = 1234;
    sn.n_children = nrows;
    sn.dirty = 1;

    LEAFENTRY les[nrows];
    {
        char key[keylens], val[vallens];
        key[keylens-1] = '\0';
        for (int i = 0; i < nrows; ++i) {
            char c = 'a' + i;
            memset(key, c, keylens-1);
            les[i] = le_fastmalloc((char *) &key, sizeof(key), (char *) &val, sizeof(val));
        }
    }
    MALLOC_N(sn.n_children, sn.bp);
    MALLOC_N(sn.n_children-1, sn.childkeys);
    sn.totalchildkeylens = (sn.n_children-1)*sizeof(int);
    for (int i = 0; i < sn.n_children; ++i) {
        BP_STATE(&sn,i) = PT_AVAIL;
        BP_SUBTREE_EST(&sn,i).ndata = random() + (((long long) random())<<32);
        BP_SUBTREE_EST(&sn,i).nkeys = random() + (((long long) random())<<32);
        BP_SUBTREE_EST(&sn,i).dsize = random() + (((long long) random())<<32);
        BP_SUBTREE_EST(&sn,i).exact =  (BOOL)(random()%2 != 0);
	set_BLB(&sn, i, toku_create_empty_bn());
    }
    for (int i = 0; i < nrows; ++i) {
        r = toku_omt_insert(BLB_BUFFER(&sn, i), les[i], omt_cmp, les[i], NULL); assert(r==0);
        BLB_NBYTESINBUF(&sn, i) = leafentry_disksize(les[i]);
        if (i < nrows-1) {
            u_int32_t keylen;
            char *key = le_key_and_len(les[i], &keylen);
            sn.childkeys[i] = kv_pair_malloc(key, keylen, 0, 0);
        }
    }

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

    setup_dn(bft, fd, brt_h, &dn);
    
    assert(dn->thisnodename.b==20);

    assert(dn->layout_version ==BRT_LAYOUT_VERSION);
    assert(dn->layout_version_original ==BRT_LAYOUT_VERSION);
    {
        const u_int32_t npartitions = dn->n_children;
        assert(dn->totalchildkeylens==(keylens*(npartitions-1)));
        struct check_leafentries_struct extra = { .nelts = nrows, .elts = les, .i = 0, .cmp = omt_cmp };
        u_int32_t last_i = 0;
        for (u_int32_t i = 0; i < npartitions; ++i) {
            assert(dn->bp[i].start > 0);
            assert(dn->bp[i].size  > 0);
            if (i > 0) {
                assert(dn->bp[i].start >= dn->bp[i-1].start + dn->bp[i-1].size);
            }
            assert(toku_omt_size(BLB_BUFFER(dn, i)) > 0);
            toku_omt_iterate(BLB_BUFFER(dn, i), check_leafentries, &extra);
            // don't check soft_copy_is_up_to_date or seqinsert
            assert(BLB_NBYTESINBUF(dn, i) == (extra.i-last_i)*(KEY_VALUE_OVERHEAD+keylens+vallens) + toku_omt_size(BLB_BUFFER(dn, i)));
            last_i = extra.i;
        }
        assert(extra.i == nrows);
    }

    toku_brtnode_free(&dn);
    for (int i = 0; i < sn.n_children-1; ++i) {
        kv_pair_free(sn.childkeys[i]);
    }
    for (int i = 0; i < nrows; ++i) {
        toku_free(les[i]);
    }
    toku_free(sn.childkeys);
    for (int i = 0; i < sn.n_children; i++) {
	destroy_basement_node(BLB(&sn, i));
    }
    toku_free(sn.bp);

    toku_block_free(brt_h->blocktable, BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
    toku_blocktable_destroy(&brt_h->blocktable);
    toku_free(brt_h);
    toku_free(brt);

    r = close(fd); assert(r != -1);
}

static void
test_serialize_leaf_with_many_rows(enum brtnode_verify_type bft) {
    int r;
    struct brtnode sn, *dn;
    const int keylens = sizeof(int), vallens = sizeof(int), nrows = 196*1024;
    // assert(val_size > BN_MAX_SIZE);  // BN_MAX_SIZE isn't visible
    int fd = open(__FILE__ ".brt", O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    sn.max_msn_applied_to_node_on_disk.msn = 0;
    sn.nodesize = 4*(1<<20);
    sn.flags = 0x11223344;
    sn.thisnodename.b = 20;
    sn.layout_version = BRT_LAYOUT_VERSION;
    sn.layout_version_original = BRT_LAYOUT_VERSION;
    sn.height = 0;
    sn.optimized_for_upgrade = 1234;
    sn.n_children = 1;
    sn.dirty = 1;

    LEAFENTRY les[nrows];
    {
        int key = 0, val = 0;
        for (int i = 0; i < nrows; ++i, key++, val++) {
            les[i] = le_fastmalloc((char *) &key, sizeof(key), (char *) &val, sizeof(val));
        }
    }
    MALLOC_N(sn.n_children, sn.bp);
    MALLOC_N(sn.n_children-1, sn.childkeys);
    sn.totalchildkeylens = (sn.n_children-1)*sizeof(int);
    for (int i = 0; i < sn.n_children; ++i) {
        BP_STATE(&sn,i) = PT_AVAIL;
        BP_SUBTREE_EST(&sn,i).ndata = random() + (((long long) random())<<32);
        BP_SUBTREE_EST(&sn,i).nkeys = random() + (((long long) random())<<32);
        BP_SUBTREE_EST(&sn,i).dsize = random() + (((long long) random())<<32);
        BP_SUBTREE_EST(&sn,i).exact =  (BOOL)(random()%2 != 0);
	set_BLB(&sn, i, toku_create_empty_bn()); 
    }
    BLB_NBYTESINBUF(&sn, 0) = 0;
    for (int i = 0; i < nrows; ++i) {
        r = toku_omt_insert(BLB_BUFFER(&sn, 0), les[i], omt_int_cmp, les[i], NULL); assert(r==0);
        BLB_NBYTESINBUF(&sn, 0) += leafentry_disksize(les[i]);
    }

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

    setup_dn(bft, fd, brt_h, &dn);

    assert(dn->thisnodename.b==20);

    assert(dn->layout_version ==BRT_LAYOUT_VERSION);
    assert(dn->layout_version_original ==BRT_LAYOUT_VERSION);
    {
        const u_int32_t npartitions = dn->n_children;
        assert(dn->totalchildkeylens==(sizeof(int)*(npartitions-1)));
        struct check_leafentries_struct extra = { .nelts = nrows, .elts = les, .i = 0, .cmp = omt_int_cmp };
        u_int32_t last_i = 0;
        for (u_int32_t i = 0; i < npartitions; ++i) {
            assert(dn->bp[i].start > 0);
            assert(dn->bp[i].size  > 0);
            if (i > 0) {
                assert(dn->bp[i].start >= dn->bp[i-1].start + dn->bp[i-1].size);
            }
            assert(toku_omt_size(BLB_BUFFER(dn, i)) > 0);
            toku_omt_iterate(BLB_BUFFER(dn, i), check_leafentries, &extra);
            // don't check soft_copy_is_up_to_date or seqinsert
            assert(BLB_NBYTESINBUF(dn, i) == (extra.i-last_i)*(KEY_VALUE_OVERHEAD+keylens+vallens) + toku_omt_size(BLB_BUFFER(dn, i)));
            assert(BLB_NBYTESINBUF(dn, i) < 128*1024);  // BN_MAX_SIZE, apt to change
            last_i = extra.i;
        }
        assert(extra.i == nrows);
    }

    toku_brtnode_free(&dn);
    for (int i = 0; i < sn.n_children-1; ++i) {
        kv_pair_free(sn.childkeys[i]);
    }
    for (int i = 0; i < nrows; ++i) {
        toku_free(les[i]);
    }
    for (int i = 0; i < sn.n_children; i++) {
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

static void
test_serialize_leaf_with_large_rows(enum brtnode_verify_type bft) {
    int r;
    struct brtnode sn, *dn;
    const size_t val_size = 512*1024;
    // assert(val_size > BN_MAX_SIZE);  // BN_MAX_SIZE isn't visible
    int fd = open(__FILE__ ".brt", O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    sn.max_msn_applied_to_node_on_disk.msn = 0;
    sn.nodesize = 4*(1<<20);
    sn.flags = 0x11223344;
    sn.thisnodename.b = 20;
    sn.layout_version = BRT_LAYOUT_VERSION;
    sn.layout_version_original = BRT_LAYOUT_VERSION;
    sn.height = 0;
    sn.optimized_for_upgrade = 1234;
    sn.n_children = 1;
    sn.dirty = 1;
    
    LEAFENTRY les[7];
    {
        char key[8], val[val_size];
        key[7] = '\0';
        val[val_size-1] = '\0';
        for (int i = 0; i < 7; ++i) {
            char c = 'a' + i;
            memset(key, c, 7);
            memset(val, c, val_size-1);
            les[i] = le_fastmalloc(key, 8, val, val_size);
        }
    }
    MALLOC_N(sn.n_children, sn.bp);
    MALLOC_N(sn.n_children-1, sn.childkeys);
    sn.totalchildkeylens = (sn.n_children-1)*8;
    for (int i = 0; i < sn.n_children; ++i) {
        BP_STATE(&sn,i) = PT_AVAIL;
        BP_SUBTREE_EST(&sn,i).ndata = random() + (((long long) random())<<32);
        BP_SUBTREE_EST(&sn,i).nkeys = random() + (((long long) random())<<32);
        BP_SUBTREE_EST(&sn,i).dsize = random() + (((long long) random())<<32);
        BP_SUBTREE_EST(&sn,i).exact =  (BOOL)(random()%2 != 0);
	set_BLB(&sn, i, toku_create_empty_bn());
    }
    BLB_NBYTESINBUF(&sn, 0) = 0;
    for (int i = 0; i < 7; ++i) {
        r = toku_omt_insert(BLB_BUFFER(&sn, 0), les[i], omt_cmp, les[i], NULL); assert(r==0);
        BLB_NBYTESINBUF(&sn, 0) += leafentry_disksize(les[i]);
    }

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

    setup_dn(bft, fd, brt_h, &dn);

    assert(dn->thisnodename.b==20);

    assert(dn->layout_version ==BRT_LAYOUT_VERSION);
    assert(dn->layout_version_original ==BRT_LAYOUT_VERSION);
    {
        const u_int32_t npartitions = dn->n_children;
        assert(npartitions == 7);
        assert(dn->totalchildkeylens==(8*(npartitions-1)));
        struct check_leafentries_struct extra = { .nelts = 7, .elts = les, .i = 0, .cmp = omt_cmp };
        u_int32_t last_i = 0;
        for (u_int32_t i = 0; i < npartitions; ++i) {
            assert(dn->bp[i].start > 0);
            assert(dn->bp[i].size  > 0);
            if (i > 0) {
                assert(dn->bp[i].start >= dn->bp[i-1].start + dn->bp[i-1].size);
            }
            assert(toku_omt_size(BLB_BUFFER(dn, i)) > 0);
            toku_omt_iterate(BLB_BUFFER(dn, i), check_leafentries, &extra);
            // don't check soft_copy_is_up_to_date or seqinsert
            assert(BLB_NBYTESINBUF(dn, i) == (extra.i-last_i)*(KEY_VALUE_OVERHEAD+8+val_size) + toku_omt_size(BLB_BUFFER(dn, i)));
            last_i = extra.i;
        }
        assert(extra.i == 7);
    }

    toku_brtnode_free(&dn);
    for (int i = 0; i < sn.n_children-1; ++i) {
        kv_pair_free(sn.childkeys[i]);
    }
    for (int i = 0; i < 7; ++i) {
        toku_free(les[i]);
    }
    for (int i = 0; i < sn.n_children; i++) {
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

static void
test_serialize_leaf_with_empty_basement_nodes(enum brtnode_verify_type bft) {
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
    sn.n_children = 7;
    sn.dirty = 1;
    LEAFENTRY elts[3];
    elts[0] = le_malloc("a", "aval");
    elts[1] = le_malloc("b", "bval");
    elts[2] = le_malloc("x", "xval");
    MALLOC_N(sn.n_children, sn.bp);
    MALLOC_N(sn.n_children-1, sn.childkeys);
    sn.childkeys[0] = kv_pair_malloc("A", 2, 0, 0);
    sn.childkeys[1] = kv_pair_malloc("a", 2, 0, 0);
    sn.childkeys[2] = kv_pair_malloc("a", 2, 0, 0);
    sn.childkeys[3] = kv_pair_malloc("b", 2, 0, 0);
    sn.childkeys[4] = kv_pair_malloc("b", 2, 0, 0);
    sn.childkeys[5] = kv_pair_malloc("x", 2, 0, 0);
    sn.totalchildkeylens = (sn.n_children-1)*2;
    for (int i = 0; i < sn.n_children; ++i) {
        BP_STATE(&sn,i) = PT_AVAIL;
        BP_SUBTREE_EST(&sn,i).ndata = random() + (((long long)random())<<32);
        BP_SUBTREE_EST(&sn,i).nkeys = random() + (((long long)random())<<32);
        BP_SUBTREE_EST(&sn,i).dsize = random() + (((long long)random())<<32);
        BP_SUBTREE_EST(&sn,i).exact =  (BOOL)(random()%2 != 0);
	set_BLB(&sn, i, toku_create_empty_bn());
        BLB_SEQINSERT(&sn, i) = 0;
    }
    r = toku_omt_insert(BLB_BUFFER(&sn, 1), elts[0], omt_cmp, elts[0], NULL); assert(r==0);
    r = toku_omt_insert(BLB_BUFFER(&sn, 3), elts[1], omt_cmp, elts[1], NULL); assert(r==0);
    r = toku_omt_insert(BLB_BUFFER(&sn, 5), elts[2], omt_cmp, elts[2], NULL); assert(r==0);
    BLB_NBYTESINBUF(&sn, 0) = 0*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(BLB_BUFFER(&sn, 0));
    BLB_NBYTESINBUF(&sn, 1) = 1*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(BLB_BUFFER(&sn, 1));
    BLB_NBYTESINBUF(&sn, 2) = 0*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(BLB_BUFFER(&sn, 2));
    BLB_NBYTESINBUF(&sn, 3) = 1*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(BLB_BUFFER(&sn, 3));
    BLB_NBYTESINBUF(&sn, 4) = 0*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(BLB_BUFFER(&sn, 4));
    BLB_NBYTESINBUF(&sn, 5) = 1*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(BLB_BUFFER(&sn, 5));
    BLB_NBYTESINBUF(&sn, 6) = 0*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(BLB_BUFFER(&sn, 6));

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

    setup_dn(bft, fd, brt_h, &dn);

    assert(dn->thisnodename.b==20);

    assert(dn->layout_version ==BRT_LAYOUT_VERSION);
    assert(dn->layout_version_original ==BRT_LAYOUT_VERSION);
    assert(dn->layout_version_read_from_disk ==BRT_LAYOUT_VERSION);
    assert(dn->height == 0);
    assert(dn->optimized_for_upgrade == 1234);
    assert(dn->n_children>0);
    {
        const u_int32_t npartitions = dn->n_children;
        assert(dn->totalchildkeylens==(2*(npartitions-1)));
        struct check_leafentries_struct extra = { .nelts = 3, .elts = elts, .i = 0, .cmp = omt_cmp };
        u_int32_t last_i = 0;
        for (u_int32_t i = 0; i < npartitions; ++i) {
            assert(dn->bp[i].start > 0);
            assert(dn->bp[i].size  > 0);
            if (i > 0) {
                assert(dn->bp[i].start >= dn->bp[i-1].start + dn->bp[i-1].size);
            }
            assert(toku_omt_size(BLB_BUFFER(dn, i)) > 0);
            toku_omt_iterate(BLB_BUFFER(dn, i), check_leafentries, &extra);
            // don't check soft_copy_is_up_to_date or seqinsert
            assert(BLB_NBYTESINBUF(dn, i) == (extra.i-last_i)*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(BLB_BUFFER(dn, i)));
            last_i = extra.i;
        }
        assert(extra.i == 3);
    }
    toku_brtnode_free(&dn);

    for (int i = 0; i < sn.n_children-1; ++i) {
        kv_pair_free(sn.childkeys[i]);
    }
    for (int i = 0; i < 3; ++i) {
        toku_free(elts[i]);
    }
    for (int i = 0; i < sn.n_children; i++) {
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

static void
test_serialize_leaf_with_multiple_empty_basement_nodes(enum brtnode_verify_type bft) {
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
    sn.n_children = 4;
    sn.dirty = 1;
    MALLOC_N(sn.n_children, sn.bp);
    MALLOC_N(sn.n_children-1, sn.childkeys);
    sn.childkeys[0] = kv_pair_malloc("A", 2, 0, 0);
    sn.childkeys[1] = kv_pair_malloc("A", 2, 0, 0);
    sn.childkeys[2] = kv_pair_malloc("A", 2, 0, 0);
    sn.totalchildkeylens = (sn.n_children-1)*2;
    for (int i = 0; i < sn.n_children; ++i) {
        BP_STATE(&sn,i) = PT_AVAIL;
        BP_SUBTREE_EST(&sn,i).ndata = random() + (((long long)random())<<32);
        BP_SUBTREE_EST(&sn,i).nkeys = random() + (((long long)random())<<32);
        BP_SUBTREE_EST(&sn,i).dsize = random() + (((long long)random())<<32);
        BP_SUBTREE_EST(&sn,i).exact =  (BOOL)(random()%2 != 0);
	set_BLB(&sn, i, toku_create_empty_bn());
    }
    BLB_NBYTESINBUF(&sn, 0) = 0*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(BLB_BUFFER(&sn, 0));
    BLB_NBYTESINBUF(&sn, 1) = 0*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(BLB_BUFFER(&sn, 1));
    BLB_NBYTESINBUF(&sn, 2) = 0*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(BLB_BUFFER(&sn, 2));
    BLB_NBYTESINBUF(&sn, 3) = 0*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(BLB_BUFFER(&sn, 3));

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

    setup_dn(bft, fd, brt_h, &dn);

    assert(dn->thisnodename.b==20);

    assert(dn->layout_version ==BRT_LAYOUT_VERSION);
    assert(dn->layout_version_original ==BRT_LAYOUT_VERSION);
    assert(dn->layout_version_read_from_disk ==BRT_LAYOUT_VERSION);
    assert(dn->height == 0);
    assert(dn->optimized_for_upgrade == 1234);
    assert(dn->n_children == 1);
    {
        const u_int32_t npartitions = dn->n_children;
        assert(dn->totalchildkeylens==(2*(npartitions-1)));
        struct check_leafentries_struct extra = { .nelts = 0, .elts = NULL, .i = 0, .cmp = omt_cmp };
        u_int32_t last_i = 0;
        for (u_int32_t i = 0; i < npartitions; ++i) {
            assert(dn->bp[i].start > 0);
            assert(dn->bp[i].size  > 0);
            if (i > 0) {
                assert(dn->bp[i].start >= dn->bp[i-1].start + dn->bp[i-1].size);
            }
            assert(toku_omt_size(BLB_BUFFER(dn, i)) == 0);
            toku_omt_iterate(BLB_BUFFER(dn, i), check_leafentries, &extra);
            // don't check soft_copy_is_up_to_date or seqinsert
            assert(BLB_NBYTESINBUF(dn, i) == (extra.i-last_i)*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(BLB_BUFFER(dn, i)));
            last_i = extra.i;
        }
        assert(extra.i == 0);
    }
    toku_brtnode_free(&dn);

    for (int i = 0; i < sn.n_children-1; ++i) {
        kv_pair_free(sn.childkeys[i]);
    }
    for (int i = 0; i < sn.n_children; i++) {
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

static void
test_serialize_leaf(enum brtnode_verify_type bft) {
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

    setup_dn(bft, fd, brt_h, &dn);

    assert(dn->thisnodename.b==20);

    assert(dn->layout_version ==BRT_LAYOUT_VERSION);
    assert(dn->layout_version_original ==BRT_LAYOUT_VERSION);
    assert(dn->layout_version_read_from_disk ==BRT_LAYOUT_VERSION);
    assert(dn->height == 0);
    assert(dn->optimized_for_upgrade == 1234);
    assert(dn->n_children>=1);
    {
        const u_int32_t npartitions = dn->n_children;
        assert(dn->totalchildkeylens==(2*(npartitions-1)));
        struct check_leafentries_struct extra = { .nelts = 3, .elts = elts, .i = 0, .cmp = omt_cmp };
        u_int32_t last_i = 0;
        for (u_int32_t i = 0; i < npartitions; ++i) {
            assert(dn->bp[i].start > 0);
            assert(dn->bp[i].size  > 0);
            if (i > 0) {
                assert(dn->bp[i].start >= dn->bp[i-1].start + dn->bp[i-1].size);
            }
            toku_omt_iterate(BLB_BUFFER(dn, i), check_leafentries, &extra);
            u_int32_t keylen;
            if (i < npartitions-1) {
                assert(strcmp(kv_pair_key(dn->childkeys[i]), le_key_and_len(elts[extra.i-1], &keylen))==0);
            }
            // don't check soft_copy_is_up_to_date or seqinsert
            assert(BLB_NBYTESINBUF(dn, i) == (extra.i-last_i)*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(BLB_BUFFER(dn, i)));
            last_i = extra.i;
        }
        assert(extra.i == 3);
    }
    toku_brtnode_free(&dn);

    for (int i = 0; i < sn.n_children-1; ++i) {
        kv_pair_free(sn.childkeys[i]);
    }
    for (int i = 0; i < 3; ++i) {
        toku_free(elts[i]);
    }
    for (int i = 0; i < sn.n_children; i++) {
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

static void
test_serialize_nonleaf(enum brtnode_verify_type bft) {
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

    setup_dn(bft, fd, brt_h, &dn);

    assert(dn->thisnodename.b==20);

    assert(dn->layout_version ==BRT_LAYOUT_VERSION);
    assert(dn->layout_version_original ==BRT_LAYOUT_VERSION);
    assert(dn->layout_version_read_from_disk ==BRT_LAYOUT_VERSION);
    assert(dn->height == 1);
    assert(dn->optimized_for_upgrade == 1234);
    assert(dn->n_children==2);
    assert(strcmp(kv_pair_key(dn->childkeys[0]), "hello")==0);
    assert(toku_brt_pivot_key_len(dn->childkeys[0])==6);
    assert(dn->totalchildkeylens==6);
    assert(BP_BLOCKNUM(dn,0).b==30);
    assert(BP_BLOCKNUM(dn,1).b==35);

    FIFO src_fifo_1 = BNC(&sn, 0)->buffer;
    FIFO src_fifo_2 = BNC(&sn, 1)->buffer;
    FIFO dest_fifo_1 = BNC(dn, 0)->buffer;
    FIFO dest_fifo_2 = BNC(dn, 1)->buffer;
    bytevec src_key,src_val, dest_key, dest_val;
    ITEMLEN src_keylen, src_vallen;
    u_int32_t src_type;
    MSN src_msn;
    XIDS src_xids;
    ITEMLEN dest_keylen, dest_vallen;
    u_int32_t dest_type;
    MSN dest_msn;
    XIDS dest_xids;
    bool src_is_fresh;
    bool dest_is_fresh;
    r = toku_fifo_peek(src_fifo_1, &src_key, &src_keylen, &src_val, &src_vallen, &src_type, &src_msn, &src_xids, &src_is_fresh);
    assert(r==0);
    r = toku_fifo_peek(dest_fifo_1, &dest_key, &dest_keylen, &dest_val, &dest_vallen, &dest_type, &dest_msn, &dest_xids, &dest_is_fresh);
    assert(r==0);
    assert(src_keylen == dest_keylen);
    assert(src_keylen == 2);
    assert(src_vallen == dest_vallen);
    assert(src_vallen == 5);
    assert(src_type == dest_type);
    assert(src_msn.msn == dest_msn.msn);
    assert(strcmp(src_key, "a") == 0);
    assert(strcmp(dest_key, "a") == 0);
    assert(strcmp(src_val, "aval") == 0);
    assert(strcmp(dest_val, "aval") == 0);
    assert(src_is_fresh == dest_is_fresh);
    r = toku_fifo_deq(src_fifo_1);
    assert(r==0);
    r = toku_fifo_deq(dest_fifo_1);
    assert(r==0);
    r = toku_fifo_peek(src_fifo_1, &src_key, &src_keylen, &src_val, &src_vallen, &src_type, &src_msn, &src_xids, &src_is_fresh);
    assert(r==0);
    r = toku_fifo_peek(dest_fifo_1, &dest_key, &dest_keylen, &dest_val, &dest_vallen, &dest_type, &dest_msn, &dest_xids, &dest_is_fresh);
    assert(r==0);
    assert(src_keylen == dest_keylen);
    assert(src_keylen == 2);
    assert(src_vallen == dest_vallen);
    assert(src_vallen == 5);
    assert(src_type == dest_type);
    assert(src_msn.msn == dest_msn.msn);
    assert(strcmp(src_key, "b") == 0);
    assert(strcmp(dest_key, "b") == 0);
    assert(strcmp(src_val, "bval") == 0);
    assert(strcmp(dest_val, "bval") == 0);
    assert(src_is_fresh == dest_is_fresh);
    r = toku_fifo_deq(src_fifo_1);
    assert(r==0);
    r = toku_fifo_deq(dest_fifo_1);
    assert(r==0);
    r = toku_fifo_peek(src_fifo_1, &src_key, &src_keylen, &src_val, &src_vallen, &src_type, &src_msn, &src_xids, &src_is_fresh);
    assert(r!=0);
    r = toku_fifo_peek(dest_fifo_1, &dest_key, &dest_keylen, &dest_val, &dest_vallen, &dest_type, &dest_msn, &dest_xids, &dest_is_fresh);
    assert(r!=0);

    r = toku_fifo_peek(src_fifo_2, &src_key, &src_keylen, &src_val, &src_vallen, &src_type, &src_msn, &src_xids, &src_is_fresh);
    assert(r==0);
    r = toku_fifo_peek(dest_fifo_2, &dest_key, &dest_keylen, &dest_val, &dest_vallen, &dest_type, &dest_msn, &dest_xids, &dest_is_fresh);
    assert(r==0);
    assert(src_keylen == dest_keylen);
    assert(src_keylen == 2);
    assert(src_vallen == dest_vallen);
    assert(src_vallen == 5);
    assert(src_type == dest_type);
    assert(src_msn.msn == dest_msn.msn);
    assert(strcmp(src_key, "x") == 0);
    assert(strcmp(dest_key, "x") == 0);
    assert(strcmp(src_val, "xval") == 0);
    assert(strcmp(dest_val, "xval") == 0);
    assert(src_is_fresh == dest_is_fresh);
    r = toku_fifo_deq(src_fifo_2);
    assert(r==0);
    r = toku_fifo_deq(dest_fifo_2);
    assert(r==0);
    r = toku_fifo_peek(src_fifo_2, &src_key, &src_keylen, &src_val, &src_vallen, &src_type, &src_msn, &src_xids, &src_is_fresh);
    assert(r!=0);
    r = toku_fifo_peek(dest_fifo_2, &dest_key, &dest_keylen, &dest_val, &dest_vallen, &dest_type, &dest_msn, &dest_xids, &dest_is_fresh);
    assert(r!=0);

    
    toku_brtnode_free(&dn);

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
    test_serialize_leaf(read_none);
    test_serialize_leaf(read_all);
    test_serialize_leaf(read_compressed);

    test_serialize_leaf_with_empty_basement_nodes(read_none);
    test_serialize_leaf_with_empty_basement_nodes(read_all);
    test_serialize_leaf_with_empty_basement_nodes(read_compressed);

    test_serialize_leaf_with_multiple_empty_basement_nodes(read_none);
    test_serialize_leaf_with_multiple_empty_basement_nodes(read_all);
    test_serialize_leaf_with_multiple_empty_basement_nodes(read_compressed);

    test_serialize_leaf_with_large_rows(read_none);
    test_serialize_leaf_with_large_rows(read_all);
    test_serialize_leaf_with_large_rows(read_compressed);

    test_serialize_leaf_with_many_rows(read_none);
    test_serialize_leaf_with_many_rows(read_all);
    test_serialize_leaf_with_many_rows(read_compressed);

    test_serialize_leaf_with_large_pivots(read_none);
    test_serialize_leaf_with_large_pivots(read_all);
    test_serialize_leaf_with_large_pivots(read_compressed);

    test_serialize_leaf_check_msn(read_none);
    test_serialize_leaf_check_msn(read_all);
    test_serialize_leaf_check_msn(read_compressed);

    test_serialize_nonleaf(read_none);
    test_serialize_nonleaf(read_all);
    test_serialize_nonleaf(read_compressed);

    return 0;
}
