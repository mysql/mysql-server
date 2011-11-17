/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007-2011 Tokutek Inc.  All rights reserved."

// it used to be the case that we copied the left and right keys of a
// range to be prelocked but never freed them, this test checks that they
// are freed (as of this time, this happens in destroy_bfe_for_prefetch)

#include "test.h"

#include "includes.h"

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
calc_le_size(int keylen, int vallen) {
    size_t rval;
    LEAFENTRY le;
    rval = sizeof(le->type) + sizeof(le->keylen) + sizeof(le->u.clean.vallen) + keylen + vallen;
    return rval;
}

static LEAFENTRY
le_fastmalloc(struct mempool * mp, char *key, int keylen, char *val, int vallen)
{
    LEAFENTRY le;
    size_t le_size = calc_le_size(keylen, vallen);
    le = toku_mempool_malloc(mp, le_size, 1);
    resource_assert(le);
    le->type = LE_CLEAN;
    le->keylen = keylen;
    le->u.clean.vallen = vallen;
    memcpy(&le->u.clean.key_val[0], key, keylen);
    memcpy(&le->u.clean.key_val[keylen], val, vallen);
    return le;
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
    const int nodesize = 1024, eltsize = 64, bnsize = 256;
    const size_t maxbnsize = bnsize;
    const int keylen = sizeof(long), vallen = eltsize - keylen - (sizeof(((LEAFENTRY)NULL)->type)  // overhead from LE_CLEAN_MEMSIZE
                                                                  +sizeof(((LEAFENTRY)NULL)->keylen)
                                                                  +sizeof(((LEAFENTRY)NULL)->u.clean.vallen));
    const int eltsperbn = bnsize / eltsize;
    struct brtnode sn;

    int fd = open(__FILE__ ".brt", O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    int r;

    sn.max_msn_applied_to_node_on_disk.msn = 0;
    sn.nodesize = nodesize;
    sn.flags = 0x11223344;
    sn.thisnodename.b = 20;
    sn.layout_version = BRT_LAYOUT_VERSION;
    sn.layout_version_original = BRT_LAYOUT_VERSION;
    sn.height = 0;
    sn.optimized_for_upgrade = 1324;
    const int nelts = 2 * nodesize / eltsize;
    sn.n_children = nelts * eltsize / bnsize;
    sn.dirty = 1;
    MALLOC_N(sn.n_children, sn.bp);
    MALLOC_N(sn.n_children - 1, sn.childkeys);
    sn.totalchildkeylens = 0;
    const MSN new_msn = { .msn = 2 * MIN_MSN.msn };
    for (int bn = 0; bn < sn.n_children; ++bn) {
        BP_STATE(&sn,bn) = PT_AVAIL;
        set_BLB(&sn, bn, toku_create_empty_bn());
	BASEMENTNODE basement = BLB(&sn, bn);
        // Write an MSN to this basement node.
        BLB_MAX_MSN_APPLIED(&sn, bn) = new_msn;
	struct mempool * mp = &basement->buffer_mempool;
	toku_mempool_construct(mp, maxbnsize);
        BLB_NBYTESINBUF(&sn,bn) = 0;
        long k;
        for (int i = 0; i < eltsperbn; ++i) {
            k = bn * eltsperbn + i;
            char val[vallen];
            memset(val, k, sizeof val);
            LEAFENTRY le = le_fastmalloc(mp, (char *) &k, keylen, val, vallen);
            r = toku_omt_insert(BLB_BUFFER(&sn, bn), le, omt_long_cmp, le, NULL); assert(r == 0);
            BLB_NBYTESINBUF(&sn, bn) += leafentry_disksize(le);
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

    // Verify that the MSN is the same on the basement nodes after the split.
    int a_children = nodea->n_children;
    int b_children = nodeb->n_children;
    for(int i = 0; i < a_children; ++i)
    {
        assert(new_msn.msn == BLB_MAX_MSN_APPLIED(nodea, i).msn);
    }

    for(int i = 0; i < b_children; ++i)
    {
        assert(new_msn.msn == BLB_MAX_MSN_APPLIED(nodeb, i).msn);
    }

    toku_unpin_brtnode(brt, nodeb);
    r = toku_close_brt(brt, NULL); assert(r == 0);
    r = toku_cachetable_close(&ct); assert(r == 0);

    if (splitk.data) {
        toku_free(splitk.data);
    }

    for (int i = 0; i < sn.n_children - 1; ++i) {
        kv_pair_free(sn.childkeys[i]);
    }
    for (int i = 0; i < sn.n_children; ++i) {
	BASEMENTNODE bn = BLB(&sn, i);
	struct mempool * mp = &bn->buffer_mempool;
	toku_mempool_destroy(mp);
        destroy_basement_node(BLB(&sn, i));
    }
    toku_free(sn.bp);
    toku_free(sn.childkeys);
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
    const int nodesize = 1024, eltsize = 64, bnsize = 256;
    const size_t maxbnsize = 1024 * 2;
    const int keylen = sizeof(long), vallen = eltsize - keylen - (sizeof(((LEAFENTRY)NULL)->type)  // overhead from LE_CLEAN_MEMSIZE
                                                                  +sizeof(((LEAFENTRY)NULL)->keylen)
                                                                  +sizeof(((LEAFENTRY)NULL)->u.clean.vallen));
    const int eltsperbn = bnsize / eltsize;
    struct brtnode sn;

    int fd = open(__FILE__ ".brt", O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    int r;

    sn.max_msn_applied_to_node_on_disk.msn = 0;
    sn.nodesize = nodesize;
    sn.flags = 0x11223344;
    sn.thisnodename.b = 20;
    sn.layout_version = BRT_LAYOUT_VERSION;
    sn.layout_version_original = BRT_LAYOUT_VERSION;
    sn.height = 0;
    sn.optimized_for_upgrade = 1324;
    const int nelts = 2 * nodesize / eltsize;
    sn.n_children = nelts * eltsize / bnsize + 1;
    sn.dirty = 1;
    MALLOC_N(sn.n_children, sn.bp);
    MALLOC_N(sn.n_children - 1, sn.childkeys);
    sn.totalchildkeylens = 0;
    for (int bn = 0; bn < sn.n_children; ++bn) {
        BP_STATE(&sn,bn) = PT_AVAIL;
        set_BLB(&sn, bn, toku_create_empty_bn());
	BASEMENTNODE basement = BLB(&sn, bn);
	struct mempool * mp = &basement->buffer_mempool;
	toku_mempool_construct(mp, maxbnsize);
        BLB_NBYTESINBUF(&sn,bn) = 0;
        long k;
        if (bn < sn.n_children - 1) {
            for (int i = 0; i < eltsperbn; ++i) {
                k = bn * eltsperbn + i;
                char val[vallen];
                memset(val, k, sizeof val);
                LEAFENTRY le = le_fastmalloc(mp, (char *) &k, keylen, val, vallen);
                r = toku_omt_insert(BLB_BUFFER(&sn, bn), le, omt_long_cmp, le, NULL); assert(r == 0);
                BLB_NBYTESINBUF(&sn, bn) += leafentry_disksize(le);
            }
            sn.childkeys[bn] = kv_pair_malloc(&k, sizeof k, 0, 0);
            sn.totalchildkeylens += (sizeof k);
        } else {
            k = bn * eltsperbn;
	    size_t big_val_size = (nelts * eltsize - 1);   // TODO: Explain this
            char * big_val = toku_xmalloc(big_val_size);
            memset(big_val, k, big_val_size);
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

    for (int i = 0; i < sn.n_children - 1; ++i) {
        kv_pair_free(sn.childkeys[i]);
    }
    for (int i = 0; i < sn.n_children; ++i) {
	BASEMENTNODE bn = BLB(&sn, i);
	struct mempool * mp = &bn->buffer_mempool;
	toku_mempool_destroy(mp);
        destroy_basement_node(BLB(&sn, i));
    }
    toku_free(sn.bp);
    toku_free(sn.childkeys);
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
    const int nodesize = 1024, eltsize = 64, bnsize = 256;
    const size_t maxbnsize = 1024 * 2;
    const int keylen = sizeof(long), vallen = eltsize - keylen - (sizeof(((LEAFENTRY)NULL)->type)  // overhead from LE_CLEAN_MEMSIZE
                                                                  +sizeof(((LEAFENTRY)NULL)->keylen)
                                                                  +sizeof(((LEAFENTRY)NULL)->u.clean.vallen));
    const int eltsperbn = bnsize / eltsize;
    struct brtnode sn;

    int fd = open(__FILE__ ".brt", O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    int r;

    sn.max_msn_applied_to_node_on_disk.msn = 0;
    sn.nodesize = nodesize;
    sn.flags = 0x11223344;
    sn.thisnodename.b = 20;
    sn.layout_version = BRT_LAYOUT_VERSION;
    sn.layout_version_original = BRT_LAYOUT_VERSION;
    sn.height = 0;
    sn.optimized_for_upgrade = 1324;
    const int nelts = 2 * nodesize / eltsize;
    sn.n_children = nelts * eltsize / bnsize + 1;
    sn.dirty = 1;
    MALLOC_N(sn.n_children, sn.bp);
    MALLOC_N(sn.n_children - 1, sn.childkeys);
    sn.totalchildkeylens = 0;
    for (int bn = 0; bn < sn.n_children; ++bn) {
        BP_STATE(&sn,bn) = PT_AVAIL;
        set_BLB(&sn, bn, toku_create_empty_bn());
	BASEMENTNODE basement = BLB(&sn, bn);
	struct mempool * mp = &basement->buffer_mempool;
	toku_mempool_construct(mp, maxbnsize);
        BLB_NBYTESINBUF(&sn,bn) = 0;
        long k;
        if (bn < sn.n_children - 1) {
            for (int i = 0; i < eltsperbn; ++i) {
                k = bn * eltsperbn + i;
                char val[vallen];
                memset(val, k, sizeof val);
                LEAFENTRY le = le_fastmalloc(mp, (char *) &k, keylen, val, vallen);
                r = toku_omt_insert(BLB_BUFFER(&sn, bn), le, omt_long_cmp, le, NULL); assert(r == 0);
                BLB_NBYTESINBUF(&sn, bn) += leafentry_disksize(le);
            }
            sn.childkeys[bn] = kv_pair_malloc(&k, sizeof k, 0, 0);
            sn.totalchildkeylens += (sizeof k);
        } else {
            k = bn * eltsperbn;
	    size_t big_val_size = (nelts * eltsize - 100);    // TODO: This looks wrong, should perhaps be +100?
	    invariant(big_val_size <=  maxbnsize);
            char * big_val = toku_xmalloc(big_val_size);
            memset(big_val, k, big_val_size);
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

    for (int i = 0; i < sn.n_children - 1; ++i) {
        kv_pair_free(sn.childkeys[i]);
    }
    for (int i = 0; i < sn.n_children; ++i) {
	BASEMENTNODE bn = BLB(&sn, i);
	struct mempool * mp = &bn->buffer_mempool;
	toku_mempool_destroy(mp);
        destroy_basement_node(BLB(&sn, i));
    }
    toku_free(sn.bp);
    toku_free(sn.childkeys);
}

static void
test_split_at_begin(void)
{
    const int nodesize = 1024, eltsize = 64, bnsize = 256;
    const size_t maxbnsize = 1024 * 2;
    const int keylen = sizeof(long), vallen = eltsize - keylen - (sizeof(((LEAFENTRY)NULL)->type)  // overhead from LE_CLEAN_MEMSIZE
                                                                  +sizeof(((LEAFENTRY)NULL)->keylen)
                                                                  +sizeof(((LEAFENTRY)NULL)->u.clean.vallen));
    const int eltsperbn = bnsize / eltsize;
    struct brtnode sn;

    int fd = open(__FILE__ ".brt", O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    int r;

    sn.max_msn_applied_to_node_on_disk.msn = 0;
    sn.nodesize = nodesize;
    sn.flags = 0x11223344;
    sn.thisnodename.b = 20;
    sn.layout_version = BRT_LAYOUT_VERSION;
    sn.layout_version_original = BRT_LAYOUT_VERSION;
    sn.height = 0;
    sn.optimized_for_upgrade = 1324;
    const int nelts = 2 * nodesize / eltsize;
    sn.n_children = nelts * eltsize / bnsize;
    sn.dirty = 1;
    MALLOC_N(sn.n_children, sn.bp);
    MALLOC_N(sn.n_children - 1, sn.childkeys);
    sn.totalchildkeylens = 0;
    size_t totalbytes = 0;
    for (int bn = 0; bn < sn.n_children; ++bn) {
        BP_STATE(&sn,bn) = PT_AVAIL;
        set_BLB(&sn, bn, toku_create_empty_bn());
	BASEMENTNODE basement = BLB(&sn, bn);
	struct mempool * mp = &basement->buffer_mempool;
	toku_mempool_construct(mp, maxbnsize);
        BLB_NBYTESINBUF(&sn,bn) = 0;
        long k;
        for (int i = 0; i < eltsperbn; ++i) {
            k = bn * eltsperbn + i;
            if (bn == 0 && i == 0) {
                // we'll add the first element later when we know how big
                // to make it
                continue;
            }
            char val[vallen];
            memset(val, k, sizeof val);
            LEAFENTRY le = le_fastmalloc(mp, (char *) &k, keylen, val, vallen);
            r = toku_omt_insert(BLB_BUFFER(&sn, bn), le, omt_long_cmp, le, NULL); assert(r == 0);
            BLB_NBYTESINBUF(&sn, bn) += leafentry_disksize(le);
            totalbytes += leafentry_disksize(le);
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

    for (int i = 0; i < sn.n_children - 1; ++i) {
        kv_pair_free(sn.childkeys[i]);
    }
    for (int i = 0; i < sn.n_children; ++i) {
	BASEMENTNODE bn = BLB(&sn, i);
	struct mempool * mp = &bn->buffer_mempool;
	toku_mempool_destroy(mp);
        destroy_basement_node(BLB(&sn, i));
    }
    toku_free(sn.bp);
    toku_free(sn.childkeys);
}

static void
test_split_at_end(void)
{
    const int nodesize = 1024, eltsize = 64, bnsize = 256;
    const size_t maxbnsize = 1024 * 2;
    const int keylen = sizeof(long), vallen = eltsize - keylen - (sizeof(((LEAFENTRY)NULL)->type)  // overhead from LE_CLEAN_MEMSIZE
                                                                  +sizeof(((LEAFENTRY)NULL)->keylen)
                                                                  +sizeof(((LEAFENTRY)NULL)->u.clean.vallen));
    const int eltsperbn = bnsize / eltsize;
    struct brtnode sn;

    int fd = open(__FILE__ ".brt", O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    int r;

    sn.max_msn_applied_to_node_on_disk.msn = 0;
    sn.nodesize = nodesize;
    sn.flags = 0x11223344;
    sn.thisnodename.b = 20;
    sn.layout_version = BRT_LAYOUT_VERSION;
    sn.layout_version_original = BRT_LAYOUT_VERSION;
    sn.height = 0;
    sn.optimized_for_upgrade = 1324;
    const int nelts = 2 * nodesize / eltsize;
    sn.n_children = nelts * eltsize / bnsize;
    sn.dirty = 1;
    MALLOC_N(sn.n_children, sn.bp);
    MALLOC_N(sn.n_children - 1, sn.childkeys);
    sn.totalchildkeylens = 0;
    long totalbytes = 0;
    for (int bn = 0; bn < sn.n_children; ++bn) {
        BP_STATE(&sn,bn) = PT_AVAIL;
        set_BLB(&sn, bn, toku_create_empty_bn());
	BASEMENTNODE basement = BLB(&sn, bn);
	struct mempool * mp = &basement->buffer_mempool;
	toku_mempool_construct(mp, maxbnsize);
        BLB_NBYTESINBUF(&sn,bn) = 0;
        long k;
        for (int i = 0; i < eltsperbn; ++i) {
	    LEAFENTRY le;
            k = bn * eltsperbn + i;
            if (bn < sn.n_children - 1 || i < eltsperbn - 1) {
                char val[vallen];
                memset(val, k, sizeof val);
                le = le_fastmalloc(mp, (char *) &k, keylen, val, vallen);
            } else {  // the last element
                char val[totalbytes + 3];  // just to be sure
                memset(val, k, sizeof val);
                le = le_fastmalloc(mp, (char *) &k, keylen, val, totalbytes + 3);
            }
            r = toku_omt_insert(BLB_BUFFER(&sn, bn), le, omt_long_cmp, le, NULL); assert(r == 0);
            BLB_NBYTESINBUF(&sn, bn) += leafentry_disksize(le);
            totalbytes += leafentry_disksize(le);
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

    for (int i = 0; i < sn.n_children - 1; ++i) {
        kv_pair_free(sn.childkeys[i]);
    }
    for (int i = 0; i < sn.n_children; ++i) {
	BASEMENTNODE bn = BLB(&sn, i);
	struct mempool * mp = &bn->buffer_mempool;
	toku_mempool_destroy(mp);
        destroy_basement_node(BLB(&sn, i));
    }
    toku_free(sn.bp);
    toku_free(sn.childkeys);
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {

    test_split_on_boundary();
    test_split_with_everything_on_the_left();
    test_split_on_boundary_of_last_node();
    test_split_at_begin();
    test_split_at_end();

    return 0;
}
