/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "test.h"

#include "includes.h"

#define TESTMSNVAL 0x1234567890123456    // arbitrary number

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

static void
test_serialize_leaf_with_large_pivots(void) {
    int r;
    struct brtnode sn, *dn;
    const int keylens = 256*1024, vallens = 0, nrows = 8;
    // assert(val_size > BN_MAX_SIZE);  // BN_MAX_SIZE isn't visible
    int fd = open(__FILE__ ".brt", O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    sn.nodesize = 4*(1<<20);
    sn.flags = 0x11223344;
    sn.thisnodename.b = 20;
    sn.layout_version = BRT_LAYOUT_VERSION;
    sn.layout_version_original = BRT_LAYOUT_VERSION;
    sn.height = 0;
    sn.n_children = nrows;
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
    MALLOC_N(sn.n_children, sn.u.l.bn);
    MALLOC_N(sn.n_children, sn.subtree_estimates);
    MALLOC_N(sn.n_children-1, sn.childkeys);
    sn.totalchildkeylens = (sn.n_children-1)*sizeof(int);
    for (int i = 0; i < sn.n_children; ++i) {
        sn.subtree_estimates[i].ndata = random() + (((long long) random())<<32);
        sn.subtree_estimates[i].nkeys = random() + (((long long) random())<<32);
        sn.subtree_estimates[i].dsize = random() + (((long long) random())<<32);
        sn.subtree_estimates[i].exact =  (BOOL)(random()%2 != 0);
        r = toku_omt_create(&sn.u.l.bn[i].buffer); assert(r==0);
        sn.u.l.bn[i].optimized_for_upgrade = BRT_LAYOUT_VERSION;
        sn.u.l.bn[i].soft_copy_is_up_to_date = TRUE;
        sn.u.l.bn[i].seqinsert = 0;
    }
    for (int i = 0; i < nrows; ++i) {
        r = toku_omt_insert(sn.u.l.bn[i].buffer, les[i], omt_cmp, les[i], NULL); assert(r==0);
        sn.u.l.bn[i].n_bytes_in_buffer = OMT_ITEM_OVERHEAD + leafentry_disksize(les[i]);
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

    r = toku_deserialize_brtnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, &dn, brt_h);
    assert(r==0);

    assert(dn->thisnodename.b==20);

    assert(dn->layout_version ==BRT_LAYOUT_VERSION);
    assert(dn->layout_version_original ==BRT_LAYOUT_VERSION);

    {
        const u_int32_t npartitions = dn->n_children;
        assert(dn->totalchildkeylens==(keylens*(npartitions-1)));
        struct check_leafentries_struct extra = { .nelts = nrows, .elts = les, .i = 0, .cmp = omt_cmp };
        u_int32_t last_i = 0;
        for (u_int32_t i = 0; i < npartitions; ++i) {
            assert(toku_omt_size(dn->u.l.bn[i].buffer) > 0);
            toku_omt_iterate(dn->u.l.bn[i].buffer, check_leafentries, &extra);
            assert(dn->u.l.bn[i].optimized_for_upgrade == BRT_LAYOUT_VERSION);
            // don't check soft_copy_is_up_to_date or seqinsert
            assert(dn->u.l.bn[i].n_bytes_in_buffer == (extra.i-last_i)*(KEY_VALUE_OVERHEAD+keylens+vallens) + toku_omt_size(dn->u.l.bn[i].buffer));
            last_i = extra.i;
        }
        assert(extra.i == nrows);
    }

    toku_brtnode_free(&dn);
    for (int i = 0; i < sn.n_children-1; ++i) {
        kv_pair_free(sn.childkeys[i]);
    }
    for (int i = 0; i < sn.n_children; ++i) {
        toku_omt_destroy(&sn.u.l.bn[i].buffer);
    }
    for (int i = 0; i < nrows; ++i) {
        toku_free(les[i]);
    }
    toku_free(sn.u.l.bn);
    toku_free(sn.childkeys);
    toku_free(sn.subtree_estimates);

    toku_block_free(brt_h->blocktable, BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
    toku_blocktable_destroy(&brt_h->blocktable);
    toku_free(brt_h);
    toku_free(brt);

    r = close(fd); assert(r != -1);
}

static void
test_serialize_leaf_with_many_rows(void) {
    int r;
    struct brtnode sn, *dn;
    const int keylens = sizeof(int), vallens = sizeof(int), nrows = 196*1024;
    // assert(val_size > BN_MAX_SIZE);  // BN_MAX_SIZE isn't visible
    int fd = open(__FILE__ ".brt", O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    sn.nodesize = 4*(1<<20);
    sn.flags = 0x11223344;
    sn.thisnodename.b = 20;
    sn.layout_version = BRT_LAYOUT_VERSION;
    sn.layout_version_original = BRT_LAYOUT_VERSION;
    sn.height = 0;
    sn.n_children = 1;
    LEAFENTRY les[nrows];
    {
        int key = 0, val = 0;
        for (int i = 0; i < nrows; ++i, key++, val++) {
            les[i] = le_fastmalloc((char *) &key, sizeof(key), (char *) &val, sizeof(val));
        }
    }
    MALLOC_N(sn.n_children, sn.u.l.bn);
    MALLOC_N(sn.n_children, sn.subtree_estimates);
    MALLOC_N(sn.n_children-1, sn.childkeys);
    sn.totalchildkeylens = (sn.n_children-1)*sizeof(int);
    for (int i = 0; i < sn.n_children; ++i) {
        sn.subtree_estimates[i].ndata = random() + (((long long) random())<<32);
        sn.subtree_estimates[i].nkeys = random() + (((long long) random())<<32);
        sn.subtree_estimates[i].dsize = random() + (((long long) random())<<32);
        sn.subtree_estimates[i].exact =  (BOOL)(random()%2 != 0);
        r = toku_omt_create(&sn.u.l.bn[i].buffer); assert(r==0);
        sn.u.l.bn[i].optimized_for_upgrade = BRT_LAYOUT_VERSION;
        sn.u.l.bn[i].soft_copy_is_up_to_date = TRUE;
        sn.u.l.bn[i].seqinsert = 0;
    }
    sn.u.l.bn[0].n_bytes_in_buffer = 0;
    for (int i = 0; i < nrows; ++i) {
        r = toku_omt_insert(sn.u.l.bn[0].buffer, les[i], omt_int_cmp, les[i], NULL); assert(r==0);
        sn.u.l.bn[0].n_bytes_in_buffer += OMT_ITEM_OVERHEAD + leafentry_disksize(les[i]);
    }

    struct brt *XMALLOC(brt);
    struct brt_header *XCALLOC(brt_h);
    brt->h = brt_h;
    brt_h->type = BRTHEADER_CURRENT;
    brt_h->panic = 0; brt_h->panic_string = 0;
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

    r = toku_deserialize_brtnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, &dn, brt_h);
    assert(r==0);

    assert(dn->thisnodename.b==20);

    assert(dn->layout_version ==BRT_LAYOUT_VERSION);
    assert(dn->layout_version_original ==BRT_LAYOUT_VERSION);

    {
        const u_int32_t npartitions = dn->n_children;
        assert(dn->totalchildkeylens==(sizeof(int)*(npartitions-1)));
        struct check_leafentries_struct extra = { .nelts = nrows, .elts = les, .i = 0, .cmp = omt_int_cmp };
        u_int32_t last_i = 0;
        for (u_int32_t i = 0; i < npartitions; ++i) {
            assert(toku_omt_size(dn->u.l.bn[i].buffer) > 0);
            toku_omt_iterate(dn->u.l.bn[i].buffer, check_leafentries, &extra);
            assert(dn->u.l.bn[i].optimized_for_upgrade == BRT_LAYOUT_VERSION);
            // don't check soft_copy_is_up_to_date or seqinsert
            assert(dn->u.l.bn[i].n_bytes_in_buffer == (extra.i-last_i)*(KEY_VALUE_OVERHEAD+keylens+vallens) + toku_omt_size(dn->u.l.bn[i].buffer));
            assert(dn->u.l.bn[i].n_bytes_in_buffer < 128*1024);  // BN_MAX_SIZE, apt to change
            last_i = extra.i;
        }
        assert(extra.i == nrows);
    }

    toku_brtnode_free(&dn);
    for (int i = 0; i < sn.n_children-1; ++i) {
        kv_pair_free(sn.childkeys[i]);
    }
    for (int i = 0; i < sn.n_children; ++i) {
        toku_omt_destroy(&sn.u.l.bn[i].buffer);
    }
    for (int i = 0; i < nrows; ++i) {
        toku_free(les[i]);
    }
    toku_free(sn.u.l.bn);
    toku_free(sn.childkeys);
    toku_free(sn.subtree_estimates);

    toku_block_free(brt_h->blocktable, BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
    toku_blocktable_destroy(&brt_h->blocktable);
    toku_free(brt_h);
    toku_free(brt);

    r = close(fd); assert(r != -1);
}

static void
test_serialize_leaf_with_large_rows(void) {
    int r;
    struct brtnode sn, *dn;
    const size_t val_size = 512*1024;
    // assert(val_size > BN_MAX_SIZE);  // BN_MAX_SIZE isn't visible
    int fd = open(__FILE__ ".brt", O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    sn.nodesize = 4*(1<<20);
    sn.flags = 0x11223344;
    sn.thisnodename.b = 20;
    sn.layout_version = BRT_LAYOUT_VERSION;
    sn.layout_version_original = BRT_LAYOUT_VERSION;
    sn.height = 0;
    sn.n_children = 1;
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
    MALLOC_N(sn.n_children, sn.u.l.bn);
    MALLOC_N(sn.n_children, sn.subtree_estimates);
    MALLOC_N(sn.n_children-1, sn.childkeys);
    sn.totalchildkeylens = (sn.n_children-1)*8;
    for (int i = 0; i < sn.n_children; ++i) {
        sn.subtree_estimates[i].ndata = random() + (((long long) random())<<32);
        sn.subtree_estimates[i].nkeys = random() + (((long long) random())<<32);
        sn.subtree_estimates[i].dsize = random() + (((long long) random())<<32);
        sn.subtree_estimates[i].exact =  (BOOL)(random()%2 != 0);
        r = toku_omt_create(&sn.u.l.bn[i].buffer); assert(r==0);
        sn.u.l.bn[i].optimized_for_upgrade = BRT_LAYOUT_VERSION;
        sn.u.l.bn[i].soft_copy_is_up_to_date = TRUE;
        sn.u.l.bn[i].seqinsert = 0;
    }
    sn.u.l.bn[0].n_bytes_in_buffer = 0;
    for (int i = 0; i < 7; ++i) {
        r = toku_omt_insert(sn.u.l.bn[0].buffer, les[i], omt_cmp, les[i], NULL); assert(r==0);
        sn.u.l.bn[0].n_bytes_in_buffer += OMT_ITEM_OVERHEAD + leafentry_disksize(les[i]);
    }

    struct brt *XMALLOC(brt);
    struct brt_header *XCALLOC(brt_h);
    brt->h = brt_h;
    brt_h->type = BRTHEADER_CURRENT;
    brt_h->panic = 0; brt_h->panic_string = 0;
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

    r = toku_deserialize_brtnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, &dn, brt_h);
    assert(r==0);

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
            assert(toku_omt_size(dn->u.l.bn[i].buffer) > 0);
            toku_omt_iterate(dn->u.l.bn[i].buffer, check_leafentries, &extra);
            assert(dn->u.l.bn[i].optimized_for_upgrade == BRT_LAYOUT_VERSION);
            // don't check soft_copy_is_up_to_date or seqinsert
            assert(dn->u.l.bn[i].n_bytes_in_buffer == (extra.i-last_i)*(KEY_VALUE_OVERHEAD+8+val_size) + toku_omt_size(dn->u.l.bn[i].buffer));
            last_i = extra.i;
        }
        assert(extra.i == 7);
    }

    toku_brtnode_free(&dn);
    for (int i = 0; i < sn.n_children-1; ++i) {
        kv_pair_free(sn.childkeys[i]);
    }
    for (int i = 0; i < sn.n_children; ++i) {
        toku_omt_destroy(&sn.u.l.bn[i].buffer);
    }
    for (int i = 0; i < 7; ++i) {
        toku_free(les[i]);
    }
    toku_free(sn.u.l.bn);
    toku_free(sn.childkeys);
    toku_free(sn.subtree_estimates);

    toku_block_free(brt_h->blocktable, BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
    toku_blocktable_destroy(&brt_h->blocktable);
    toku_free(brt_h);
    toku_free(brt);

    r = close(fd); assert(r != -1);
}

static void
test_serialize_leaf_with_empty_basement_nodes(void) {
    const int nodesize = 1024;
    struct brtnode sn, *dn;

    int fd = open(__FILE__ ".brt", O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    int r;

    sn.nodesize = nodesize;
    sn.flags = 0x11223344;
    sn.thisnodename.b = 20;
    sn.layout_version = BRT_LAYOUT_VERSION;
    sn.layout_version_original = BRT_LAYOUT_VERSION;
    sn.height = 0;
    sn.n_children = 7;
    LEAFENTRY elts[3];
    elts[0] = le_malloc("a", "aval");
    elts[1] = le_malloc("b", "bval");
    elts[2] = le_malloc("x", "xval");
    MALLOC_N(sn.n_children, sn.u.l.bn);
    MALLOC_N(sn.n_children, sn.subtree_estimates);
    MALLOC_N(sn.n_children-1, sn.childkeys);
    sn.childkeys[0] = kv_pair_malloc("A", 2, 0, 0);
    sn.childkeys[1] = kv_pair_malloc("a", 2, 0, 0);
    sn.childkeys[2] = kv_pair_malloc("a", 2, 0, 0);
    sn.childkeys[3] = kv_pair_malloc("b", 2, 0, 0);
    sn.childkeys[4] = kv_pair_malloc("b", 2, 0, 0);
    sn.childkeys[5] = kv_pair_malloc("x", 2, 0, 0);
    sn.totalchildkeylens = (sn.n_children-1)*2;
    for (int i = 0; i < sn.n_children; ++i) {
        sn.subtree_estimates[i].ndata = random() + (((long long)random())<<32);
        sn.subtree_estimates[i].nkeys = random() + (((long long)random())<<32);
        sn.subtree_estimates[i].dsize = random() + (((long long)random())<<32);
        sn.subtree_estimates[i].exact =  (BOOL)(random()%2 != 0);
        r = toku_omt_create(&sn.u.l.bn[i].buffer); assert(r==0);
        sn.u.l.bn[i].optimized_for_upgrade = BRT_LAYOUT_VERSION;
        sn.u.l.bn[i].soft_copy_is_up_to_date = TRUE;
        sn.u.l.bn[i].seqinsert = 0;
    }
    r = toku_omt_insert(sn.u.l.bn[1].buffer, elts[0], omt_cmp, elts[0], NULL); assert(r==0);
    r = toku_omt_insert(sn.u.l.bn[3].buffer, elts[1], omt_cmp, elts[1], NULL); assert(r==0);
    r = toku_omt_insert(sn.u.l.bn[5].buffer, elts[2], omt_cmp, elts[2], NULL); assert(r==0);
    sn.u.l.bn[0].n_bytes_in_buffer = 0*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(sn.u.l.bn[0].buffer);
    sn.u.l.bn[1].n_bytes_in_buffer = 1*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(sn.u.l.bn[1].buffer);
    sn.u.l.bn[2].n_bytes_in_buffer = 0*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(sn.u.l.bn[2].buffer);
    sn.u.l.bn[3].n_bytes_in_buffer = 1*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(sn.u.l.bn[3].buffer);
    sn.u.l.bn[4].n_bytes_in_buffer = 0*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(sn.u.l.bn[4].buffer);
    sn.u.l.bn[5].n_bytes_in_buffer = 1*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(sn.u.l.bn[5].buffer);
    sn.u.l.bn[6].n_bytes_in_buffer = 0*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(sn.u.l.bn[6].buffer);

    struct brt *XMALLOC(brt);
    struct brt_header *XCALLOC(brt_h);
    brt->h = brt_h;
    brt_h->type = BRTHEADER_CURRENT;
    brt_h->panic = 0; brt_h->panic_string = 0;
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

    r = toku_deserialize_brtnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, &dn, brt_h);
    assert(r==0);

    assert(dn->thisnodename.b==20);

    assert(dn->layout_version ==BRT_LAYOUT_VERSION);
    assert(dn->layout_version_original ==BRT_LAYOUT_VERSION);
    assert(dn->layout_version_read_from_disk ==BRT_LAYOUT_VERSION);
    assert(dn->height == 0);
    assert(dn->n_children>0);
    {
        const u_int32_t npartitions = dn->n_children;
        assert(dn->totalchildkeylens==(2*(npartitions-1)));
        struct check_leafentries_struct extra = { .nelts = 3, .elts = elts, .i = 0, .cmp = omt_cmp };
        u_int32_t last_i = 0;
        for (u_int32_t i = 0; i < npartitions; ++i) {
            assert(toku_omt_size(dn->u.l.bn[i].buffer) > 0);
            toku_omt_iterate(dn->u.l.bn[i].buffer, check_leafentries, &extra);
            assert(dn->u.l.bn[i].optimized_for_upgrade == BRT_LAYOUT_VERSION);
            // don't check soft_copy_is_up_to_date or seqinsert
            assert(dn->u.l.bn[i].n_bytes_in_buffer == (extra.i-last_i)*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(dn->u.l.bn[i].buffer));
            last_i = extra.i;
        }
        assert(extra.i == 3);
    }
    toku_brtnode_free(&dn);

    for (int i = 0; i < sn.n_children-1; ++i) {
        kv_pair_free(sn.childkeys[i]);
    }
    for (int i = 0; i < sn.n_children; ++i) {
        toku_omt_destroy(&sn.u.l.bn[i].buffer);
    }
    for (int i = 0; i < 3; ++i) {
        toku_free(elts[i]);
    }
    toku_free(sn.u.l.bn);
    toku_free(sn.childkeys);
    toku_free(sn.subtree_estimates);

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

    sn.nodesize = nodesize;
    sn.flags = 0x11223344;
    sn.thisnodename.b = 20;
    sn.layout_version = BRT_LAYOUT_VERSION;
    sn.layout_version_original = BRT_LAYOUT_VERSION;
    sn.height = 0;
    sn.n_children = 2;
    LEAFENTRY elts[3];
    elts[0] = le_malloc("a", "aval");
    elts[1] = le_malloc("b", "bval");
    elts[2] = le_malloc("x", "xval");
    MALLOC_N(2, sn.u.l.bn);
    MALLOC_N(2, sn.subtree_estimates);
    MALLOC_N(1, sn.childkeys);
    sn.childkeys[0] = kv_pair_malloc("b", 2, 0, 0);
    sn.totalchildkeylens = 2;
    sn.subtree_estimates[0].ndata = random() + (((long long)random())<<32);
    sn.subtree_estimates[1].ndata = random() + (((long long)random())<<32);
    sn.subtree_estimates[0].nkeys = random() + (((long long)random())<<32);
    sn.subtree_estimates[1].nkeys = random() + (((long long)random())<<32);
    sn.subtree_estimates[0].dsize = random() + (((long long)random())<<32);
    sn.subtree_estimates[1].dsize = random() + (((long long)random())<<32);
    sn.subtree_estimates[0].exact =  (BOOL)(random()%2 != 0);
    sn.subtree_estimates[1].exact =  (BOOL)(random()%2 != 0);
    r = toku_omt_create(&sn.u.l.bn[0].buffer); assert(r==0);
    r = toku_omt_create(&sn.u.l.bn[1].buffer); assert(r==0);
    r = toku_omt_insert(sn.u.l.bn[0].buffer, elts[0], omt_cmp, elts[0], NULL); assert(r==0);
    r = toku_omt_insert(sn.u.l.bn[0].buffer, elts[1], omt_cmp, elts[1], NULL); assert(r==0);
    r = toku_omt_insert(sn.u.l.bn[1].buffer, elts[2], omt_cmp, elts[2], NULL); assert(r==0);
    sn.u.l.bn[0].n_bytes_in_buffer = 2*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(sn.u.l.bn[0].buffer);
    sn.u.l.bn[1].n_bytes_in_buffer = 1*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(sn.u.l.bn[1].buffer);
    for (int i = 0; i < 2; ++i) {
        sn.u.l.bn[i].optimized_for_upgrade = BRT_LAYOUT_VERSION;
        sn.u.l.bn[i].soft_copy_is_up_to_date = TRUE;
        sn.u.l.bn[i].seqinsert = 0;
    }

    struct brt *XMALLOC(brt);
    struct brt_header *XCALLOC(brt_h);
    brt->h = brt_h;
    brt_h->type = BRTHEADER_CURRENT;
    brt_h->panic = 0; brt_h->panic_string = 0;
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

    r = toku_deserialize_brtnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, &dn, brt_h);
    assert(r==0);

    assert(dn->thisnodename.b==20);

    assert(dn->layout_version ==BRT_LAYOUT_VERSION);
    assert(dn->layout_version_original ==BRT_LAYOUT_VERSION);
    assert(dn->layout_version_read_from_disk ==BRT_LAYOUT_VERSION);
    assert(dn->height == 0);
    assert(dn->n_children>=1);
    {
        const u_int32_t npartitions = dn->n_children;
        assert(dn->totalchildkeylens==(2*(npartitions-1)));
        struct check_leafentries_struct extra = { .nelts = 3, .elts = elts, .i = 0, .cmp = omt_cmp };
        u_int32_t last_i = 0;
        for (u_int32_t i = 0; i < npartitions; ++i) {
            toku_omt_iterate(dn->u.l.bn[i].buffer, check_leafentries, &extra);
            u_int32_t keylen;
            if (i < npartitions-1) {
                assert(strcmp(kv_pair_key(dn->childkeys[i]), le_key_and_len(elts[extra.i-1], &keylen))==0);
            }
            assert(dn->u.l.bn[i].optimized_for_upgrade == BRT_LAYOUT_VERSION);
            // don't check soft_copy_is_up_to_date or seqinsert
            assert(dn->u.l.bn[i].n_bytes_in_buffer == (extra.i-last_i)*(KEY_VALUE_OVERHEAD+2+5) + toku_omt_size(dn->u.l.bn[i].buffer));
            last_i = extra.i;
        }
        assert(extra.i == 3);
    }
    toku_brtnode_free(&dn);

    for (int i = 0; i < sn.n_children-1; ++i) {
        kv_pair_free(sn.childkeys[i]);
    }
    for (int i = 0; i < sn.n_children; ++i) {
        toku_omt_destroy(&sn.u.l.bn[i].buffer);
    }
    for (int i = 0; i < 3; ++i) {
        toku_free(elts[i]);
    }
    toku_free(sn.u.l.bn);
    toku_free(sn.childkeys);
    toku_free(sn.subtree_estimates);

    toku_block_free(brt_h->blocktable, BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
    toku_blocktable_destroy(&brt_h->blocktable);
    toku_free(brt_h);
    toku_free(brt);

    r = close(fd); assert(r != -1);
}

static void
test_serialize_nonleaf(void) {
    //    struct brt source_brt;
    const int nodesize = 1024;
    struct brtnode sn, *dn;

    int fd = open(__FILE__ ".brt", O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    int r;

    //    source_brt.fd=fd;
    char *hello_string;
    sn.max_msn_applied_to_node = (MSN) {TESTMSNVAL};
    sn.nodesize = nodesize;
    sn.flags = 0x11223344;
    sn.thisnodename.b = 20;
    sn.layout_version = BRT_LAYOUT_VERSION;
    sn.layout_version_original = BRT_LAYOUT_VERSION;
    sn.height = 1;
    sn.n_children = 2;
    hello_string = toku_strdup("hello");
    MALLOC_N(2, sn.u.n.childinfos);
    MALLOC_N(2, sn.subtree_estimates);
    MALLOC_N(1, sn.childkeys);
    sn.childkeys[0] = kv_pair_malloc(hello_string, 6, 0, 0);
    sn.totalchildkeylens = 6;
    BNC_BLOCKNUM(&sn, 0).b = 30;
    BNC_BLOCKNUM(&sn, 1).b = 35;
    sn.subtree_estimates[0].ndata = random() + (((long long)random())<<32);
    sn.subtree_estimates[1].ndata = random() + (((long long)random())<<32);
    sn.subtree_estimates[0].nkeys = random() + (((long long)random())<<32);
    sn.subtree_estimates[1].nkeys = random() + (((long long)random())<<32);
    sn.subtree_estimates[0].dsize = random() + (((long long)random())<<32);
    sn.subtree_estimates[1].dsize = random() + (((long long)random())<<32);
    sn.subtree_estimates[0].exact =  (BOOL)(random()%2 != 0);
    sn.subtree_estimates[1].exact =  (BOOL)(random()%2 != 0);
    r = toku_fifo_create(&BNC_BUFFER(&sn,0)); assert(r==0);
    r = toku_fifo_create(&BNC_BUFFER(&sn,1)); assert(r==0);
    //Create XIDS
    XIDS xids_0 = xids_get_root_xids();
    XIDS xids_123;
    XIDS xids_234;
    r = xids_create_child(xids_0, &xids_123, (TXNID)123);
    CKERR(r);
    r = xids_create_child(xids_123, &xids_234, (TXNID)234);
    CKERR(r);

    r = toku_fifo_enq(BNC_BUFFER(&sn,0), "a", 2, "aval", 5, BRT_NONE, next_dummymsn(), xids_0);    assert(r==0);
    r = toku_fifo_enq(BNC_BUFFER(&sn,0), "b", 2, "bval", 5, BRT_NONE, next_dummymsn(), xids_123);  assert(r==0);
    r = toku_fifo_enq(BNC_BUFFER(&sn,1), "x", 2, "xval", 5, BRT_NONE, next_dummymsn(), xids_234);  assert(r==0);
    BNC_NBYTESINBUF(&sn, 0) = 2*(BRT_CMD_OVERHEAD+KEY_VALUE_OVERHEAD+2+5) + xids_get_serialize_size(xids_0) + xids_get_serialize_size(xids_123);
    BNC_NBYTESINBUF(&sn, 1) = 1*(BRT_CMD_OVERHEAD+KEY_VALUE_OVERHEAD+2+5) + xids_get_serialize_size(xids_234);
    sn.u.n.n_bytes_in_buffers = 3*(BRT_CMD_OVERHEAD+KEY_VALUE_OVERHEAD+2+5) + xids_get_serialize_size(xids_0) + xids_get_serialize_size(xids_123) + xids_get_serialize_size(xids_234);
    //Cleanup:
    xids_destroy(&xids_0);
    xids_destroy(&xids_123);
    xids_destroy(&xids_234);

    struct brt *XMALLOC(brt);
    struct brt_header *XCALLOC(brt_h);
    brt->h = brt_h;
    brt_h->type = BRTHEADER_CURRENT;
    brt_h->panic = 0; brt_h->panic_string = 0;
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

    r = toku_deserialize_brtnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, &dn, brt_h);
    assert(r==0);

    assert(dn->thisnodename.b==20);
    assert(dn->max_msn_applied_to_node.msn == TESTMSNVAL);

    assert(dn->layout_version ==BRT_LAYOUT_VERSION);
    assert(dn->layout_version_original ==BRT_LAYOUT_VERSION);
    assert(dn->layout_version_read_from_disk ==BRT_LAYOUT_VERSION);
    assert(dn->height == 1);
    assert(dn->n_children==2);
    assert(strcmp(kv_pair_key(dn->childkeys[0]), "hello")==0);
    assert(toku_brt_pivot_key_len(dn->childkeys[0])==6);
    assert(dn->totalchildkeylens==6);
    assert(BNC_BLOCKNUM(dn,0).b==30);
    assert(BNC_BLOCKNUM(dn,1).b==35);
    toku_brtnode_free(&dn);

    kv_pair_free(sn.childkeys[0]);
    toku_free(hello_string);
    toku_fifo_free(&BNC_BUFFER(&sn,0));
    toku_fifo_free(&BNC_BUFFER(&sn,1));
    toku_free(sn.u.n.childinfos);
    toku_free(sn.childkeys);
    toku_free(sn.subtree_estimates);

    toku_block_free(brt_h->blocktable, BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
    toku_blocktable_destroy(&brt_h->blocktable);
    toku_free(brt_h);
    toku_free(brt);

    r = close(fd); assert(r != -1);
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    toku_memory_check = 1;
    test_serialize_leaf();
    test_serialize_leaf_with_empty_basement_nodes();
    test_serialize_leaf_with_large_rows();
    test_serialize_leaf_with_many_rows();
    test_serialize_leaf_with_large_pivots();
    test_serialize_nonleaf();
    return 0;
}
