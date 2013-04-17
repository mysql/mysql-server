/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."

// it used to be the case that we copied the left and right keys of a
// range to be prelocked but never freed them, this test checks that they
// are freed (as of this time, this happens in destroy_bfe_for_prefetch)

#include "test.h"

#include "includes.h"

// Some constants to be used in calculations below
static const int nodesize = 1024; // Target max node size
static const int eltsize = 64;    // Element size (for most elements)
static const int bnsize = 256;    // Target basement node size
static const int eltsperbn = 256 / 64;  // bnsize / eltsize
static const int keylen = sizeof(long);
// vallen is eltsize - keylen and leafentry overhead
static const int vallen = 64 - sizeof(long) - (sizeof(((LEAFENTRY)NULL)->type)  // overhead from LE_CLEAN_MEMSIZE
                                               +sizeof(((LEAFENTRY)NULL)->keylen)
                                               +sizeof(((LEAFENTRY)NULL)->u.clean.vallen));
#define dummy_msn_3884 ((MSN) { (u_int64_t) 3884 * MIN_MSN.msn })

static TOKUTXN const null_txn = 0;
static DB * const null_db = 0;
static const char fname[]= __FILE__ ".brt";

static int omt_long_cmp(OMTVALUE p, void *q)
{
    LEAFENTRY a = p, b = q;
    void *ak, *bk;
    u_int32_t al, bl;
    ak = le_key_and_len(a, &al);
    bk = le_key_and_len(b, &bl);
    assert(al == sizeof(long) && bl == sizeof(long));
    long *ai = (long *) ak;
    long *bi = (long *) bk;
    return (*ai > *bi) - (*ai < *bi);
}

static size_t
calc_le_size(int key_size, int val_size) {
    size_t rval;
    LEAFENTRY le;
    rval = sizeof(le->type) + sizeof(le->keylen) + sizeof(le->u.clean.vallen) + key_size + val_size;
    return rval;
}

static LEAFENTRY
le_fastmalloc(struct mempool * mp, char *key, int key_size, char *val, int val_size)
{
    LEAFENTRY le;
    size_t le_size = calc_le_size(key_size, val_size);
    le = toku_mempool_malloc(mp, le_size, 1);
    resource_assert(le);
    le->type = LE_CLEAN;
    le->keylen = key_size;
    le->u.clean.vallen = val_size;
    memcpy(&le->u.clean.key_val[0], key, key_size);
    memcpy(&le->u.clean.key_val[keylen], val, val_size);
    return le;
}

static size_t
insert_dummy_value(BRTNODE node, int bn, long k)
{
    char val[vallen];
    memset(val, k, sizeof val);
    struct mempool *mp = &BLB(node, bn)->buffer_mempool;
    LEAFENTRY le = le_fastmalloc(mp, (char *) &k, keylen, val, vallen);
    int r = toku_omt_insert(BLB_BUFFER(node, bn), le, omt_long_cmp, le, NULL); assert(r == 0);
    BLB_NBYTESINBUF(node, bn) += leafentry_disksize(le);
    return leafentry_disksize(le);
}

static void
setup_brtnode_header(struct brtnode *node)
{
    node->nodesize = nodesize;
    node->flags = 0x11223344;
    node->thisnodename.b = 20;
    node->layout_version = BRT_LAYOUT_VERSION;
    node->layout_version_original = BRT_LAYOUT_VERSION;
    node->height = 0;
    node->optimized_for_upgrade = 1324;
    node->dirty = 1;
    node->totalchildkeylens = 0;
}

static void
setup_brtnode_partitions(struct brtnode *node, int n_children, const MSN msn, size_t maxbnsize)
{
    node->n_children = n_children;
    node->max_msn_applied_to_node_on_disk = msn;
    MALLOC_N(node->n_children, node->bp);
    MALLOC_N(node->n_children - 1, node->childkeys);
    for (int bn = 0; bn < node->n_children; ++bn) {
        BP_STATE(node, bn) = PT_AVAIL;
        set_BLB(node, bn, toku_create_empty_bn());
        BASEMENTNODE basement = BLB(node, bn);
        struct mempool *mp = &basement->buffer_mempool;
        toku_mempool_construct(mp, maxbnsize);
        BLB_NBYTESINBUF(node, bn) = 0;
        BLB_MAX_MSN_APPLIED(node, bn) = msn;
    }
}

static void
destroy_brtnode_and_internals(struct brtnode *node)
{
    for (int i = 0; i < node->n_children - 1; ++i) {
        kv_pair_free(node->childkeys[i]);
    }
    for (int i = 0; i < node->n_children; ++i) {
        BASEMENTNODE bn = BLB(node, i);
        struct mempool * mp = &bn->buffer_mempool;
        toku_mempool_destroy(mp);
        destroy_basement_node(BLB(node, i));
    }
    toku_free(node->bp);
    toku_free(node->childkeys);
}

static void
verify_basement_node_msns(BRTNODE node, MSN expected)
{
    for(int i = 0; i < node->n_children; ++i) {
        assert(expected.msn == BLB_MAX_MSN_APPLIED(node, i).msn);
    }
}

//
// Maximum node size according to the BRT: 1024 (expected node size after split)
// Maximum basement node size: 256
// Actual node size before split: 2048
// Actual basement node size before split: 256
// Start by creating 8 basements, then split node, expected result of two nodes with 4 basements each.
static void
test_split_on_boundary(void)
{
    struct brtnode sn;

    int fd = open(__FILE__ ".brt", O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    int r;

    setup_brtnode_header(&sn);
    const int nelts = 2 * nodesize / eltsize;
    setup_brtnode_partitions(&sn, nelts * eltsize / bnsize, dummy_msn_3884, bnsize);
    for (int bn = 0; bn < sn.n_children; ++bn) {
        long k;
        for (int i = 0; i < eltsperbn; ++i) {
            k = bn * eltsperbn + i;
            insert_dummy_value(&sn, bn, k);
        }
        if (bn < sn.n_children - 1) {
            sn.childkeys[bn] = kv_pair_malloc(&k, sizeof k, 0, 0);
            sn.totalchildkeylens += (sizeof k);
        }
    }

    unlink(fname);
    CACHETABLE ct;
    BRT brt;
    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);   assert(r==0);
    r = toku_open_brt(fname, 1, &brt, nodesize, bnsize, ct, null_txn, toku_builtin_compare_fun, null_db); assert(r==0);

    BRTNODE nodea, nodeb;
    DBT splitk;
    // if we haven't done it right, we should hit the assert in the top of move_leafentries
    brtleaf_split(brt->h, &sn, &nodea, &nodeb, &splitk, TRUE, 0, NULL);

    verify_basement_node_msns(nodea, dummy_msn_3884);
    verify_basement_node_msns(nodeb, dummy_msn_3884);

    toku_unpin_brtnode(brt, nodeb);
    r = toku_close_brt(brt, NULL); assert(r == 0);
    r = toku_cachetable_close(&ct); assert(r == 0);

    if (splitk.data) {
        toku_free(splitk.data);
    }

    destroy_brtnode_and_internals(&sn);
}

//
// Maximum node size according to the BRT: 1024 (expected node size after split)
// Maximum basement node size: 256 (except the last)
// Actual node size before split: 4095
// Actual basement node size before split: 256 (except the last, of size 2K)
// 
// Start by creating 9 basements, the first 8 being of 256 bytes each,
// and the last with one row of size 2047 bytes.  Then split node,
// expected result is two nodes, one with 8 basement nodes and one
// with 1 basement node.
static void
test_split_with_everything_on_the_left(void)
{
    struct brtnode sn;

    int fd = open(__FILE__ ".brt", O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    int r;

    setup_brtnode_header(&sn);
    const int nelts = 2 * nodesize / eltsize;
    setup_brtnode_partitions(&sn, nelts * eltsize / bnsize + 1, dummy_msn_3884, 2 * nodesize);
    size_t big_val_size = 0;
    for (int bn = 0; bn < sn.n_children; ++bn) {
        long k;
        if (bn < sn.n_children - 1) {
            for (int i = 0; i < eltsperbn; ++i) {
                k = bn * eltsperbn + i;
                big_val_size += insert_dummy_value(&sn, bn, k);
            }
            sn.childkeys[bn] = kv_pair_malloc(&k, sizeof k, 0, 0);
            sn.totalchildkeylens += (sizeof k);
        } else {
            k = bn * eltsperbn;
            // we want this to be as big as the rest of our data and a
            // little bigger, so the halfway mark will land inside this
            // value and it will be split to the left
            big_val_size += 100;
            char * big_val = toku_xmalloc(big_val_size);
            memset(big_val, k, big_val_size);
            struct mempool *mp = &BLB(&sn, bn)->buffer_mempool;
            LEAFENTRY big_element = le_fastmalloc(mp, (char *) &k, keylen, big_val, big_val_size);
            toku_free(big_val);
            r = toku_omt_insert(BLB_BUFFER(&sn, bn), big_element, omt_long_cmp, big_element, NULL); assert(r == 0);
            BLB_NBYTESINBUF(&sn, bn) += leafentry_disksize(big_element);
        }
    }

    unlink(fname);
    CACHETABLE ct;
    BRT brt;
    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);   assert(r==0);
    r = toku_open_brt(fname, 1, &brt, nodesize, bnsize, ct, null_txn, toku_builtin_compare_fun, null_db); assert(r==0);

    BRTNODE nodea, nodeb;
    DBT splitk;
    // if we haven't done it right, we should hit the assert in the top of move_leafentries
    brtleaf_split(brt->h, &sn, &nodea, &nodeb, &splitk, TRUE, 0, NULL);

    toku_unpin_brtnode(brt, nodeb);
    r = toku_close_brt(brt, NULL); assert(r == 0);
    r = toku_cachetable_close(&ct); assert(r == 0);

    if (splitk.data) {
        toku_free(splitk.data);
    }

    destroy_brtnode_and_internals(&sn);
}


//
// Maximum node size according to the BRT: 1024 (expected node size after split)
// Maximum basement node size: 256 (except the last)
// Actual node size before split: 4095
// Actual basement node size before split: 256 (except the last, of size 2K)
// 
// Start by creating 9 basements, the first 8 being of 256 bytes each,
// and the last with one row of size 2047 bytes.  Then split node,
// expected result is two nodes, one with 8 basement nodes and one
// with 1 basement node.
static void
test_split_on_boundary_of_last_node(void)
{
    struct brtnode sn;

    int fd = open(__FILE__ ".brt", O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    int r;

    setup_brtnode_header(&sn);
    const int nelts = 2 * nodesize / eltsize;
    const size_t maxbnsize = 2 * nodesize;
    setup_brtnode_partitions(&sn, nelts * eltsize / bnsize + 1, dummy_msn_3884, maxbnsize);
    size_t big_val_size = 0;
    for (int bn = 0; bn < sn.n_children; ++bn) {
        long k;
        if (bn < sn.n_children - 1) {
            for (int i = 0; i < eltsperbn; ++i) {
                k = bn * eltsperbn + i;
                big_val_size += insert_dummy_value(&sn, bn, k);
            }
            sn.childkeys[bn] = kv_pair_malloc(&k, sizeof k, 0, 0);
            sn.totalchildkeylens += (sizeof k);
        } else {
            k = bn * eltsperbn;
            // we want this to be slightly smaller than all the rest of
            // the data combined, so the halfway mark will be just to its
            // left and just this element will end up on the right of the split
            big_val_size -= 1 + (sizeof(((LEAFENTRY)NULL)->type)  // overhead from LE_CLEAN_MEMSIZE
                                 +sizeof(((LEAFENTRY)NULL)->keylen)
                                 +sizeof(((LEAFENTRY)NULL)->u.clean.vallen));
            invariant(big_val_size <= maxbnsize);
            char * big_val = toku_xmalloc(big_val_size);
            memset(big_val, k, big_val_size);
            struct mempool *mp = &BLB(&sn, bn)->buffer_mempool;
            LEAFENTRY big_element = le_fastmalloc(mp, (char *) &k, keylen, big_val, big_val_size);
            toku_free(big_val);
            r = toku_omt_insert(BLB_BUFFER(&sn, bn), big_element, omt_long_cmp, big_element, NULL); assert(r == 0);
            BLB_NBYTESINBUF(&sn, bn) += leafentry_disksize(big_element);
        }
    }

    unlink(fname);
    CACHETABLE ct;
    BRT brt;
    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);   assert(r==0);
    r = toku_open_brt(fname, 1, &brt, nodesize, bnsize, ct, null_txn, toku_builtin_compare_fun, null_db); assert(r==0);

    BRTNODE nodea, nodeb;
    DBT splitk;
    // if we haven't done it right, we should hit the assert in the top of move_leafentries
    brtleaf_split(brt->h, &sn, &nodea, &nodeb, &splitk, TRUE, 0, NULL);

    toku_unpin_brtnode(brt, nodeb);
    r = toku_close_brt(brt, NULL); assert(r == 0);
    r = toku_cachetable_close(&ct); assert(r == 0);

    if (splitk.data) {
        toku_free(splitk.data);
    }

    destroy_brtnode_and_internals(&sn);
}

static void
test_split_at_begin(void)
{
    struct brtnode sn;

    int fd = open(__FILE__ ".brt", O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    int r;

    setup_brtnode_header(&sn);
    const int nelts = 2 * nodesize / eltsize;
    const size_t maxbnsize = 2 * nodesize;
    setup_brtnode_partitions(&sn, nelts * eltsize / bnsize, dummy_msn_3884, maxbnsize);
    size_t totalbytes = 0;
    for (int bn = 0; bn < sn.n_children; ++bn) {
        long k;
        for (int i = 0; i < eltsperbn; ++i) {
            k = bn * eltsperbn + i;
            if (bn == 0 && i == 0) {
                // we'll add the first element later when we know how big
                // to make it
                continue;
            }
            totalbytes += insert_dummy_value(&sn, bn, k);
        }
        if (bn < sn.n_children - 1) {
            sn.childkeys[bn] = kv_pair_malloc(&k, sizeof k, 0, 0);
            sn.totalchildkeylens += (sizeof k);
        }
    }
    {  // now add the first element
        int bn = 0; long k = 0;
        BASEMENTNODE basement = BLB(&sn, bn);
        struct mempool * mp = &basement->buffer_mempool;
        // add a few bytes so the halfway mark is definitely inside this
        // val, which will make it go to the left and everything else to
        // the right
        char val[totalbytes + 3];
        invariant(totalbytes + 3 <= maxbnsize);
        memset(val, k, sizeof val);
        LEAFENTRY le = le_fastmalloc(mp, (char *) &k, keylen, val, totalbytes + 3);
        r = toku_omt_insert(BLB_BUFFER(&sn, bn), le, omt_long_cmp, le, NULL); assert(r == 0);
        BLB_NBYTESINBUF(&sn, bn) += leafentry_disksize(le);
        totalbytes += leafentry_disksize(le);
    }

    unlink(fname);
    CACHETABLE ct;
    BRT brt;
    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);   assert(r==0);
    r = toku_open_brt(fname, 1, &brt, nodesize, bnsize, ct, null_txn, toku_builtin_compare_fun, null_db); assert(r==0);

    BRTNODE nodea, nodeb;
    DBT splitk;
    // if we haven't done it right, we should hit the assert in the top of move_leafentries
    brtleaf_split(brt->h, &sn, &nodea, &nodeb, &splitk, TRUE, 0, NULL);

    toku_unpin_brtnode(brt, nodeb);
    r = toku_close_brt(brt, NULL); assert(r == 0);
    r = toku_cachetable_close(&ct); assert(r == 0);

    if (splitk.data) {
        toku_free(splitk.data);
    }

    destroy_brtnode_and_internals(&sn);
}

static void
test_split_at_end(void)
{
    struct brtnode sn;

    int fd = open(__FILE__ ".brt", O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    int r;

    setup_brtnode_header(&sn);
    const int nelts = 2 * nodesize / eltsize;
    const size_t maxbnsize = 2 * nodesize;
    setup_brtnode_partitions(&sn, nelts * eltsize / bnsize, dummy_msn_3884, maxbnsize);
    long totalbytes = 0;
    int bn, i;
    for (bn = 0; bn < sn.n_children; ++bn) {
        long k;
        for (i = 0; i < eltsperbn; ++i) {
            k = bn * eltsperbn + i;
            if (bn == sn.n_children - 1 && i == eltsperbn - 1) {
                BASEMENTNODE basement = BLB(&sn, bn);
                struct mempool * mp = &basement->buffer_mempool;
                // add a few bytes so the halfway mark is definitely inside this
                // val, which will make it go to the left and everything else to
                // the right, which is nothing, so we actually split at the very end
                char val[totalbytes + 3];
                invariant(totalbytes + 3 <= (long) maxbnsize);
                memset(val, k, sizeof val);
                LEAFENTRY le = le_fastmalloc(mp, (char *) &k, keylen, val, totalbytes + 3);
                r = toku_omt_insert(BLB_BUFFER(&sn, bn), le, omt_long_cmp, le, NULL); assert(r == 0);
                BLB_NBYTESINBUF(&sn, bn) += leafentry_disksize(le);
                totalbytes += leafentry_disksize(le);
            } else {
                totalbytes += insert_dummy_value(&sn, bn, k);
            }
        }
        if (bn < sn.n_children - 1) {
            sn.childkeys[bn] = kv_pair_malloc(&k, sizeof k, 0, 0);
            sn.totalchildkeylens += (sizeof k);
        }
    }

    unlink(fname);
    CACHETABLE ct;
    BRT brt;
    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);   assert(r==0);
    r = toku_open_brt(fname, 1, &brt, nodesize, bnsize, ct, null_txn, toku_builtin_compare_fun, null_db); assert(r==0);

    BRTNODE nodea, nodeb;
    DBT splitk;
    // if we haven't done it right, we should hit the assert in the top of move_leafentries
    brtleaf_split(brt->h, &sn, &nodea, &nodeb, &splitk, TRUE, 0, NULL);

    toku_unpin_brtnode(brt, nodeb);
    r = toku_close_brt(brt, NULL); assert(r == 0);
    r = toku_cachetable_close(&ct); assert(r == 0);

    if (splitk.data) {
        toku_free(splitk.data);
    }

    destroy_brtnode_and_internals(&sn);
}

// Maximum node size according to the BRT: 1024 (expected node size after split)
// Maximum basement node size: 256
// Actual node size before split: 2048
// Actual basement node size before split: 256
// Start by creating 9 basements, then split node.
// Expected result of two nodes with 5 basements each.
static void
test_split_odd_nodes(void)
{
    struct brtnode sn;

    int fd = open(__FILE__ ".brt", O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO);
    assert(fd >= 0);

    int r;

    setup_brtnode_header(&sn);
    // This will give us 9 children.
    const int nelts = 2 * (nodesize + 128) / eltsize;
    setup_brtnode_partitions(&sn, nelts * eltsize / bnsize, dummy_msn_3884, bnsize);
    for (int bn = 0; bn < sn.n_children; ++bn) {
        long k;
        for (int i = 0; i < eltsperbn; ++i) {
            k = bn * eltsperbn + i;
            insert_dummy_value(&sn, bn, k);
        }
        if (bn < sn.n_children - 1) {
            sn.childkeys[bn] = kv_pair_malloc(&k, sizeof k, 0, 0);
            sn.totalchildkeylens += (sizeof k);
        }
    }

    unlink(fname);
    CACHETABLE ct;
    BRT brt;
    r = toku_brt_create_cachetable(&ct, 0, ZERO_LSN, NULL_LOGGER);   assert(r==0);
    r = toku_open_brt(fname, 1, &brt, nodesize, bnsize, ct, null_txn, toku_builtin_compare_fun, null_db); assert(r==0);

    BRTNODE nodea, nodeb;
    DBT splitk;
    // if we haven't done it right, we should hit the assert in the top of move_leafentries
    brtleaf_split(brt->h, &sn, &nodea, &nodeb, &splitk, TRUE, 0, NULL);

    verify_basement_node_msns(nodea, dummy_msn_3884);
    verify_basement_node_msns(nodeb, dummy_msn_3884);

    toku_unpin_brtnode(brt, nodeb);
    r = toku_close_brt(brt, NULL); assert(r == 0);
    r = toku_cachetable_close(&ct); assert(r == 0);

    if (splitk.data) {
        toku_free(splitk.data);
    }

    destroy_brtnode_and_internals(&sn);
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {

    test_split_on_boundary();
    test_split_with_everything_on_the_left();
    test_split_on_boundary_of_last_node();
    test_split_at_begin();
    test_split_at_end();
    test_split_odd_nodes();

    return 0;
}
