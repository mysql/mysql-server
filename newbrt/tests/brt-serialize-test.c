/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007, 2008 Tokutek Inc.  All rights reserved."

#include "test.h"

#include "includes.h"

static void
test_serialize_leaf(void) {
    int r;

    int fd = open(__FILE__ ".brt", O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    r = close(fd); assert(r != -1);
}

static void 
test_serialize_nonleaf(void) {
    //    struct brt source_brt;
    const int nodesize = 1024;
    struct brtnode sn, *dn;

    int fd = open(__FILE__ ".brt", O_RDWR|O_CREAT|O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO); assert(fd >= 0);

    int r;
    const u_int32_t randval = random();

    //    source_brt.fd=fd;
    char *hello_string;
    sn.nodesize = nodesize;
    sn.ever_been_written = 0;
    sn.flags = 0x11223344;
    sn.thisnodename.b = 20;
    sn.layout_version = BRT_LAYOUT_VERSION;
    sn.layout_version_original = BRT_LAYOUT_VERSION;
    sn.height = 1;
    sn.rand4fingerprint = randval;
    sn.local_fingerprint = 0;
    sn.u.n.n_children = 2;
    hello_string = toku_strdup("hello");
    MALLOC_N(2, sn.u.n.childinfos);
    MALLOC_N(1, sn.u.n.childkeys);
    sn.u.n.childkeys[0] = kv_pair_malloc(hello_string, 6, 0, 0); 
    sn.u.n.totalchildkeylens = 6;
    BNC_BLOCKNUM(&sn, 0).b = 30;
    BNC_BLOCKNUM(&sn, 1).b = 35;
    BNC_SUBTREE_FINGERPRINT(&sn, 0) = random();
    BNC_SUBTREE_FINGERPRINT(&sn, 1) = random();
    BNC_SUBTREE_ESTIMATES(&sn, 0).ndata = random() + (((long long)random())<<32);
    BNC_SUBTREE_ESTIMATES(&sn, 1).ndata = random() + (((long long)random())<<32);
    BNC_SUBTREE_ESTIMATES(&sn, 0).nkeys = random() + (((long long)random())<<32);
    BNC_SUBTREE_ESTIMATES(&sn, 1).nkeys = random() + (((long long)random())<<32);
    BNC_SUBTREE_ESTIMATES(&sn, 0).dsize = random() + (((long long)random())<<32);
    BNC_SUBTREE_ESTIMATES(&sn, 1).dsize = random() + (((long long)random())<<32);
    BNC_SUBTREE_ESTIMATES(&sn, 0).exact = (BOOL)(random()%2 != 0);
    BNC_SUBTREE_ESTIMATES(&sn, 1).exact = (BOOL)(random()%2 != 0);
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

    r = toku_fifo_enq(BNC_BUFFER(&sn,0), "a", 2, "aval", 5, BRT_NONE, xids_0);   assert(r==0);    sn.local_fingerprint += randval*toku_calc_fingerprint_cmd(BRT_NONE, xids_0, "a", 2, "aval", 5);
    r = toku_fifo_enq(BNC_BUFFER(&sn,0), "b", 2, "bval", 5, BRT_NONE, xids_123); assert(r==0);    sn.local_fingerprint += randval*toku_calc_fingerprint_cmd(BRT_NONE, xids_123,  "b", 2, "bval", 5);
    r = toku_fifo_enq(BNC_BUFFER(&sn,1), "x", 2, "xval", 5, BRT_NONE, xids_234); assert(r==0);    sn.local_fingerprint += randval*toku_calc_fingerprint_cmd(BRT_NONE, xids_234, "x", 2, "xval", 5);
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
    
    sn.desc = &brt->h->descriptor;
    r = toku_serialize_brtnode_to(fd, make_blocknum(20), &sn, brt->h, 1, 1, FALSE);  
    assert(r==0);
    
    r = toku_deserialize_brtnode_from(fd, make_blocknum(20), 0/*pass zero for hash*/, &dn, brt_h);
    assert(r==0);

    assert(dn->thisnodename.b==20);

    assert(dn->layout_version ==BRT_LAYOUT_VERSION);
    assert(dn->layout_version_original ==BRT_LAYOUT_VERSION);
    assert(dn->layout_version_read_from_disk ==BRT_LAYOUT_VERSION);
    assert(dn->height == 1);
    assert(dn->rand4fingerprint==randval);
    assert(dn->u.n.n_children==2);
    assert(strcmp(kv_pair_key(dn->u.n.childkeys[0]), "hello")==0);
    assert(toku_brtnode_pivot_key_len(dn, dn->u.n.childkeys[0])==6);
    assert(dn->u.n.totalchildkeylens==6);
    assert(BNC_BLOCKNUM(dn,0).b==30);
    assert(BNC_BLOCKNUM(dn,1).b==35);
    {
	int i;
	for (i=0; i<2; i++) {
	    assert(BNC_SUBTREE_FINGERPRINT(dn, i)==BNC_SUBTREE_FINGERPRINT(&sn, i));
	    assert(BNC_SUBTREE_ESTIMATES(dn, i).nkeys==BNC_SUBTREE_ESTIMATES(&sn, i).nkeys);
	    assert(BNC_SUBTREE_ESTIMATES(dn, i).ndata==BNC_SUBTREE_ESTIMATES(&sn, i).ndata);
	    assert(BNC_SUBTREE_ESTIMATES(dn, i).dsize==BNC_SUBTREE_ESTIMATES(&sn, i).dsize);
	}
	assert(dn->local_fingerprint==sn.local_fingerprint);
    }
    toku_brtnode_free(&dn);

    kv_pair_free(sn.u.n.childkeys[0]);
    toku_free(hello_string);
    toku_fifo_free(&BNC_BUFFER(&sn,0));
    toku_fifo_free(&BNC_BUFFER(&sn,1));
    toku_free(sn.u.n.childinfos);
    toku_free(sn.u.n.childkeys);

    toku_block_free(brt_h->blocktable, BLOCK_ALLOCATOR_TOTAL_HEADER_RESERVE);
    toku_blocktable_destroy(&brt_h->blocktable);
    toku_free(brt_h);
    toku_free(brt);
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    toku_memory_check = 1;
    test_serialize_leaf();
    test_serialize_nonleaf();
    toku_malloc_cleanup();
    return 0;
}
