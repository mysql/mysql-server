#include "brt.h"
#include "memory.h"
#include "brt-internal.h"

#include <fcntl.h>
#include <assert.h>
#include <string.h>

void test_serialize(void) {
    //    struct brt source_brt;
    int nodesize = 1024;
    struct brtnode sn, *dn;
    int fd = open("brt-serialize-test.brt", O_RDWR|O_CREAT, 0777);
    int r;
    assert(fd>=0);

    //    source_brt.fd=fd;
    char *hello_string;
    sn.nodesize = nodesize;
    sn.thisnodename = sn.nodesize*20;
    sn.height = 1;
    sn.u.n.n_children = 2;
    sn.u.n.childkeys[0]    = hello_string = toku_strdup("hello");
    sn.u.n.childkeylens[0] = 6;
    sn.u.n.totalchildkeylens = 6;
    sn.u.n.children[0] = sn.nodesize*30;
    sn.u.n.children[1] = sn.nodesize*35;
    r = toku_hashtable_create(&sn.u.n.htables[0]); assert(r==0);
    r = toku_hashtable_create(&sn.u.n.htables[1]); assert(r==0);
    r = toku_hash_insert(sn.u.n.htables[0], "a", 2, "aval", 5); assert(r==0);
    r = toku_hash_insert(sn.u.n.htables[0], "b", 2, "bval", 5); assert(r==0);
    r = toku_hash_insert(sn.u.n.htables[1], "x", 2, "xval", 5); assert(r==0);
    sn.u.n.n_bytes_in_hashtables = 3*(KEY_VALUE_OVERHEAD+2+5);

    deserialize_brtnode_from(fd, nodesize*20, &dn, nodesize);

    serialize_brtnode_to(fd, sn.nodesize*20, sn.nodesize, &sn);


    assert(dn->thisnodename==nodesize*20);
    assert(dn->height == 1);
    assert(dn->u.n.n_children==2);
    assert(strcmp(dn->u.n.childkeys[0], "hello")==0);
    assert(dn->u.n.childkeylens[0]==6);
    assert(dn->u.n.totalchildkeylens==6);
    assert(dn->u.n.children[0]==nodesize*30);
    assert(dn->u.n.children[1]==nodesize*35);
    {
	bytevec data; ITEMLEN datalen;
	int r = toku_hash_find(dn->u.n.htables[0], "a", 2, &data, &datalen);
	assert(r==0);
	assert(strcmp(data,"aval")==0);
	assert(datalen==5);

	r=toku_hash_find(dn->u.n.htables[0], "b", 2, &data, &datalen);
	assert(r==0);
	assert(strcmp(data,"bval")==0);
	assert(datalen==5);

	r=toku_hash_find(dn->u.n.htables[1], "x", 2, &data, &datalen);
	assert(r==0);
	assert(strcmp(data,"xval")==0);
	assert(datalen==5);

    }
    brtnode_free(&dn);

    toku_free(hello_string);
    toku_hashtable_free(&sn.u.n.htables[0]);
    toku_hashtable_free(&sn.u.n.htables[1]);
}

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    memory_check = 1;
    test_serialize();
    return 0;
}
