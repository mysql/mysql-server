/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include "test.h"

#include "includes.h"

#define MIN(x, y) (((x) < (y)) ? (x) : (y))
const double USECS_PER_SEC = 1000000.0;

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

static int
long_key_cmp(DB *UU(e), const DBT *a, const DBT *b)
{
    const long *x = a->data, *y = b->data;
    return (*x > *y) - (*x < *y);
}

static void
test_serialize_leaf(int valsize, int nelts, double entropy) {
    //    struct brt source_brt;
    const int nodesize = (1<<22);
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
    sn.n_children = 8;
    sn.dirty = 1;
    MALLOC_N(sn.n_children, sn.bp);
    MALLOC_N(sn.n_children-1, sn.childkeys);
    sn.totalchildkeylens = 0;
    for (int i = 0; i < sn.n_children; ++i) {
        BP_SUBTREE_EST(&sn,i).ndata = random() + (((long long)random())<<32);
        BP_SUBTREE_EST(&sn,i).nkeys = random() + (((long long)random())<<32);
        BP_SUBTREE_EST(&sn,i).dsize = random() + (((long long)random())<<32);
        BP_SUBTREE_EST(&sn,i).exact =  (BOOL)(random()%2 != 0);
        BP_STATE(&sn,i) = PT_AVAIL;
        set_BLB(&sn, i, toku_create_empty_bn());
        BLB_OPTIMIZEDFORUPGRADE(&sn, 0) = BRT_LAYOUT_VERSION;
    }
    int nperbn = nelts / sn.n_children;
    LEAFENTRY les[nelts];
    memset(les, 0, sizeof les);
    for (int ck = 0; ck < sn.n_children; ++ck) {
        long k;
        for (long i = 0; i < nperbn; ++i) {
            k = ck * nperbn + i;
            char buf[valsize];
            int c;
            for (c = 0; c < valsize * entropy; ) {
                *((int *) &buf[c]) = rand();
                c += sizeof(int);
            }
            memset(&buf[c], 0, valsize - c);
            les[k] = le_fastmalloc((char *)&k, sizeof k, buf, sizeof buf);
            r = toku_omt_insert(BLB_BUFFER(&sn, ck), les[k], omt_cmp, les[k], NULL); assert(r==0);
        }
        BLB_NBYTESINBUF(&sn, ck) = nperbn*(KEY_VALUE_OVERHEAD+(sizeof(long)+valsize)) + toku_omt_size(BLB_BUFFER(&sn, ck));
        if (ck < 7) {
            sn.childkeys[ck] = kv_pair_malloc(&k, sizeof k, 0, 0);
            sn.totalchildkeylens += sizeof k;
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

    struct timeval t[2];
    gettimeofday(&t[0], NULL);
    r = toku_serialize_brtnode_to(fd, make_blocknum(20), &sn, brt->h, 1, 1, FALSE);
    assert(r==0);
    gettimeofday(&t[1], NULL);
    double dt;
    dt = (t[1].tv_sec - t[0].tv_sec) + ((t[1].tv_usec - t[0].tv_usec) / USECS_PER_SEC);
    printf("serialize leaf:   %0.05lf\n", dt);

    struct brtnode_fetch_extra bfe;
    fill_bfe_for_full_read(&bfe, brt_h, NULL, long_key_cmp);
    gettimeofday(&t[0], NULL);
    r = toku_deserialize_brtnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, &dn, &bfe);
    assert(r==0);
    gettimeofday(&t[1], NULL);
    dt = (t[1].tv_sec - t[0].tv_sec) + ((t[1].tv_usec - t[0].tv_usec) / USECS_PER_SEC);
    printf("deserialize leaf: %0.05lf\n", dt);

    toku_brtnode_free(&dn);

    for (int i = 0; i < sn.n_children-1; ++i) {
        kv_pair_free(sn.childkeys[i]);
    }
    for (int i = 0; i < nelts; ++i) {
        if (les[i]) { toku_free(les[i]); }
    }
    for (int ck = 0; ck < sn.n_children; ++ck) {
        destroy_basement_node(BLB(&sn, ck));
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
test_serialize_nonleaf(int valsize, int nelts, double entropy) {
    //    struct brt source_brt;
    const int nodesize = (1<<22);
    struct brtnode sn, *dn;

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
    sn.n_children = 8;
    sn.dirty = 1;
    MALLOC_N(sn.n_children, sn.bp);
    MALLOC_N(sn.n_children-1, sn.childkeys);
    sn.totalchildkeylens = 0;
    for (int i = 0; i < sn.n_children; ++i) {
        BP_BLOCKNUM(&sn, i).b = 30 + (i*5);
        BP_SUBTREE_EST(&sn,i).ndata = random() + (((long long)random())<<32);
        BP_SUBTREE_EST(&sn,i).nkeys = random() + (((long long)random())<<32);
        BP_SUBTREE_EST(&sn,i).dsize = random() + (((long long)random())<<32);
        BP_SUBTREE_EST(&sn,i).exact =  (BOOL)(random()%2 != 0);
        BP_STATE(&sn,i) = PT_AVAIL;
        set_BNC(&sn, i, toku_create_empty_nl());
    }
    //Create XIDS
    XIDS xids_0 = xids_get_root_xids();
    XIDS xids_123;
    r = xids_create_child(xids_0, &xids_123, (TXNID)123);
    CKERR(r);
    int nperchild = nelts / 8;
    for (int ck = 0; ck < sn.n_children; ++ck) {
        long k;
        for (long i = 0; i < nperchild; ++i) {
            k = ck * nperchild + i;
            char buf[valsize];
            int c;
            for (c = 0; c < valsize * entropy; ) {
                *((int *) &buf[c]) = rand();
                c += sizeof(int);
            }
            memset(&buf[c], 0, valsize - c);
            r = toku_fifo_enq(BNC_BUFFER(&sn,ck), &k, sizeof k, buf, sizeof buf, BRT_NONE, next_dummymsn(), xids_123, NULL);    assert(r==0);
        }
        BNC_NBYTESINBUF(&sn, ck) = nperchild*(BRT_CMD_OVERHEAD+KEY_VALUE_OVERHEAD+(sizeof k)+valsize+xids_get_serialize_size(xids_123));
        if (ck < 7) {
            sn.childkeys[ck] = kv_pair_malloc(&k, sizeof k, 0, 0);
            sn.totalchildkeylens += sizeof k;
        }
    }

    //Cleanup:
    xids_destroy(&xids_0);
    xids_destroy(&xids_123);

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

    struct timeval t[2];
    gettimeofday(&t[0], NULL);
    r = toku_serialize_brtnode_to(fd, make_blocknum(20), &sn, brt->h, 1, 1, FALSE);
    assert(r==0);
    gettimeofday(&t[1], NULL);
    double dt;
    dt = (t[1].tv_sec - t[0].tv_sec) + ((t[1].tv_usec - t[0].tv_usec) / USECS_PER_SEC);
    printf("serialize nonleaf:   %0.05lf\n", dt);

    struct brtnode_fetch_extra bfe;
    fill_bfe_for_full_read(&bfe, brt_h, NULL, long_key_cmp);
    gettimeofday(&t[0], NULL);
    r = toku_deserialize_brtnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, &dn, &bfe);
    assert(r==0);
    gettimeofday(&t[1], NULL);
    dt = (t[1].tv_sec - t[0].tv_sec) + ((t[1].tv_usec - t[0].tv_usec) / USECS_PER_SEC);
    printf("deserialize nonleaf: %0.05lf\n", dt);

    toku_brtnode_free(&dn);

    for (int i = 0; i < sn.n_children-1; ++i) {
        kv_pair_free(sn.childkeys[i]);
    }
    for (int i = 0; i < sn.n_children; ++i) {
        destroy_nonleaf_childinfo(BNC(&sn, i));
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
    long valsize, nelts;
    double entropy = 0.3;

    if (argc != 3) {
        fprintf(stderr, "Usage: %s <valsize> <nelts>\n", argv[0]);
        return 2;
    }
    valsize = strtol(argv[1], NULL, 0);
    nelts = strtol(argv[2], NULL, 0);

    test_serialize_leaf(valsize, nelts, entropy);
    test_serialize_nonleaf(valsize, nelts, entropy);

    return 0;
}
