/* Test the expunge method. */

#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "fifo.h"
#include "memory.h"

int count;
int callback (bytevec key, ITEMLEN keylen, bytevec data, ITEMLEN datalen, int type, TXNID xid, void *v) {
    TXNID which=(long)v;
    assert(xid==which);
    int actual_row = count;
    assert(strlen(key)+1==keylen);
    assert(strlen(data)+1==datalen);
    //printf("count=%d which=%ld deleting %s %s\n", count, (long)which, (char*)key, (char*)data);
    switch (which) {
    case 23: break;
    case 24: actual_row++; break;
    case 26: actual_row+=3;
    }
    switch (actual_row) {
    case 0: assert(strcmp(key, "hello")==0); assert(strcmp(data, "thera")==0); assert(xid==23); assert(type==0); break;
    case 1: assert(strcmp(key, "hello")==0); assert(strcmp(data, "therb")==0); assert(xid==24); assert(type==0); break;
    case 2: assert(strcmp(key, "hell1")==0); assert(strcmp(data, "therc")==0); assert(xid==24); assert(type==1); break;
    case 3: assert(strcmp(key, "hell1")==0); assert(strcmp(data, "therd")==0); assert(xid==26); assert(type==1); break;
    default: assert(0);
    }
    count++;
    return 0;
}

void doit (int which) {
    int r;
    FIFO f;
    r = toku_fifo_create(&f);                                  assert(r==0);
    r = toku_fifo_enq(f, "hello", 6, "thera", 6, 0, 23);       assert(r==0);
    r = toku_fifo_enq(f, "hello", 6, "therb", 6, 0, 24);       assert(r==0);
    r = toku_fifo_enq(f, "hell1", 6, "therc", 6, 1, 24);       assert(r==0);
    r = toku_fifo_enq(f, "hell1", 6, "therd", 6, 1, 26);       assert(r==0);
    int i=0;
    FIFO_ITERATE(f, k, kl, d, dl, t, x,
		 ({
		     assert(strlen(k)+1==kl);
		     assert(strlen(d)+1==dl);
		     switch(i) {
		     case 0: assert(strcmp(k, "hello")==0); assert(strcmp(d, "thera")==0); assert(x==23); assert(t==0); i++; break;
		     case 1: assert(strcmp(k, "hello")==0); assert(strcmp(d, "therb")==0); assert(x==24); assert(t==0); i++; break;
		     case 2: assert(strcmp(k, "hell1")==0); assert(strcmp(d, "therc")==0); assert(x==24); assert(t==1); i++; break;
		     case 3: assert(strcmp(k, "hell1")==0); assert(strcmp(d, "therd")==0); assert(x==26); assert(t==1); i++; break;
		     default: assert(0);
		     }
		 }));
    count=0;
    r = toku_fifo_expunge_xaction(f, which, callback, (void*)(long)which);
    switch (which) {
    case 23: assert(count==1); break;
    case 24: assert(count==2); break;
    case 26: assert(count==1); break;
    }
    toku_fifo_free(&f);
    toku_malloc_cleanup();
}

int main (int argc __attribute__((__unused__)), char *argv[]  __attribute__((__unused__))) {
    doit(23);
    doit(24);
    doit(26);
    doit(27);
    return 0;
}
