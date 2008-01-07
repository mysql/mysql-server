/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include "brt-internal.h"

#include <fcntl.h>
#include <assert.h>
#include <string.h>
#include <zlib.h>
#include <arpa/inet.h>
#include <stdlib.h>

static void test_serialize(void) {
    //    struct brt source_brt;
    int nodesize = 1024;
    struct brtnode sn, *dn;
    int fd = open("brt-serialize-test.brt", O_RDWR|O_CREAT, 0777);
    int r;
    const u_int32_t randval = random();
    assert(fd>=0);

    //    source_brt.fd=fd;
    char *hello_string;
    sn.nodesize = nodesize;
    sn.flags = 0x11223344;
    sn.thisnodename = sn.nodesize*20;
    sn.disk_lsn.lsn = 789;
    sn.log_lsn.lsn  = 123456;
    sn.layout_version = 1;
    sn.height = 1;
    sn.rand4fingerprint = randval;
    sn.local_fingerprint = 0;
    sn.u.n.n_children = 2;
    hello_string = toku_strdup("hello");
    sn.u.n.childkeys[0] = kv_pair_malloc(hello_string, 6, 0, 0); 
    sn.u.n.totalchildkeylens = 6;
    sn.u.n.pivotflags[0] = 42;
    sn.u.n.children[0] = sn.nodesize*30;
    sn.u.n.children[1] = sn.nodesize*35;
    BRTNODE_CHILD_SUBTREE_FINGERPRINTS(&sn, 0) = random();
    BRTNODE_CHILD_SUBTREE_FINGERPRINTS(&sn, 1) = random();
    r = toku_hashtable_create(&sn.u.n.htables[0]); assert(r==0);
    r = toku_hashtable_create(&sn.u.n.htables[1]); assert(r==0);
    r = toku_hash_insert(sn.u.n.htables[0], "a", 2, "aval", 5, BRT_NONE); assert(r==0);    sn.local_fingerprint += randval*toku_calccrc32_cmd(BRT_NONE,   "a", 2, "aval", 5);
    r = toku_hash_insert(sn.u.n.htables[0], "b", 2, "bval", 5, BRT_NONE); assert(r==0);    sn.local_fingerprint += randval*toku_calccrc32_cmd(BRT_NONE,   "b", 2, "bval", 5);
    r = toku_hash_insert(sn.u.n.htables[1], "x", 2, "xval", 5, BRT_NONE); assert(r==0);    sn.local_fingerprint += randval*toku_calccrc32_cmd(BRT_NONE,   "x", 2, "xval", 5);
    sn.u.n.n_bytes_in_hashtable[0] = 2*(BRT_CMD_OVERHEAD+KEY_VALUE_OVERHEAD+2+5);
    sn.u.n.n_bytes_in_hashtable[1] = 1*(BRT_CMD_OVERHEAD+KEY_VALUE_OVERHEAD+2+5);
    {
	int i;
	for (i=2; i<TREE_FANOUT+1; i++)
	    sn.u.n.n_bytes_in_hashtable[i]=0;
    }
    sn.u.n.n_bytes_in_hashtables = 3*(BRT_CMD_OVERHEAD+KEY_VALUE_OVERHEAD+2+5);

    toku_serialize_brtnode_to(fd, sn.nodesize*20, sn.nodesize, &sn);  assert(r==0);

    r = toku_deserialize_brtnode_from(fd, nodesize*20, &dn, sn.flags, nodesize, 0, 0, 0, (FILENUM){0});
    assert(r==0);

    assert(dn->thisnodename==nodesize*20);
    assert(dn->disk_lsn.lsn==123456);
    assert(dn->layout_version ==1);
    assert(dn->height == 1);
    assert(dn->rand4fingerprint==randval);
    assert(dn->u.n.n_children==2);
    assert(strcmp(kv_pair_key(dn->u.n.childkeys[0]), "hello")==0);
    assert(toku_brtnode_pivot_key_len(dn, dn->u.n.childkeys[0])==6);
    assert(dn->u.n.totalchildkeylens==6);
    assert(dn->u.n.pivotflags[0]==42);
    assert(dn->u.n.children[0]==nodesize*30);
    assert(dn->u.n.children[1]==nodesize*35);
    {
	int i;
	for (i=0; i<2; i++) {
	    assert(BRTNODE_CHILD_SUBTREE_FINGERPRINTS(dn, i)==BRTNODE_CHILD_SUBTREE_FINGERPRINTS(&sn, i));
	}
	assert(dn->local_fingerprint==sn.local_fingerprint);
    }
    {
	bytevec data; ITEMLEN datalen; int type;
	r = toku_hash_find(dn->u.n.htables[0], "a", 2, &data, &datalen, &type);
	assert(r==0);
	assert(strcmp(data,"aval")==0);
	assert(datalen==5);
        assert(type == BRT_NONE);

	r=toku_hash_find(dn->u.n.htables[0], "b", 2, &data, &datalen, &type);
	assert(r==0);
	assert(strcmp(data,"bval")==0);
	assert(datalen==5);
        assert(type == BRT_NONE);

	r=toku_hash_find(dn->u.n.htables[1], "x", 2, &data, &datalen, &type);
	assert(r==0);
	assert(strcmp(data,"xval")==0);
	assert(datalen==5);
        assert(type == BRT_NONE);
    }
    toku_brtnode_free(&dn);

    kv_pair_free(sn.u.n.childkeys[0]);
    toku_free(hello_string);
    toku_hashtable_free(&sn.u.n.htables[0]);
    toku_hashtable_free(&sn.u.n.htables[1]);
}

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    toku_memory_check = 1;
    test_serialize();
    toku_malloc_cleanup();
    return 0;
}
