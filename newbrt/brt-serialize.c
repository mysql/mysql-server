#define _XOPEN_SOURCE 500

#include "brt.h"
#include "memory.h"
//#include "pma.h"
#include "brt-internal.h"

#include <assert.h>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <arpa/inet.h>
#include <errno.h>

struct cursor {
    unsigned char *buf;
    unsigned int  size;
    unsigned int  ndone;
};

void wbuf_char (struct cursor *w, int ch) {
    assert(w->ndone<w->size);
    w->buf[w->ndone++]=ch;
}

void wbuf_int (struct cursor *w, unsigned int i) {
    wbuf_char(w, (i>>24)&0xff);
    wbuf_char(w, (i>>16)&0xff);
    wbuf_char(w, (i>>8)&0xff);
    wbuf_char(w, (i>>0)&0xff);
}

void wbuf_bytes (struct cursor *w, bytevec bytes_bv, int nbytes) {
    const unsigned char *bytes=bytes_bv; 
    int i;
    wbuf_int(w, nbytes);
    for (i=0; i<nbytes; i++) wbuf_char(w, bytes[i]);
}

void wbuf_diskoff (struct cursor *w, diskoff off) {
    wbuf_int(w, off>>32);
    wbuf_int(w, off&0xFFFFFFFF);
}

unsigned int rbuf_char (struct cursor *r) {
    assert(r->ndone<r->size);
    return r->buf[r->ndone++];
}

unsigned int rbuf_int (struct cursor *r) {
    unsigned char c0 = rbuf_char(r);
    unsigned char c1 = rbuf_char(r);
    unsigned char c2 = rbuf_char(r);
    unsigned char c3 = rbuf_char(r);
    return ((c0<<24)|
	    (c1<<16)|
	    (c2<<8)|
	    (c3<<0));
}

/* Return a pointer into the middle of the buffer. */
void rbuf_bytes (struct cursor *r, bytevec *bytes, unsigned int *n_bytes)
{
    *n_bytes = rbuf_int(r);
    *bytes =   &r->buf[r->ndone];
    r->ndone+=*n_bytes;
    assert(r->ndone<=r->size);
}

diskoff rbuf_diskoff (struct cursor *r) {
    unsigned i0 = rbuf_int(r);  
    unsigned i1 = rbuf_int(r);
    return ((unsigned long long)(i0)<<32) | ((unsigned long long)(i1));
}

static unsigned int serialize_brtnode_size_slow(BRTNODE node) {
    unsigned int size=4+4; /* size+height */
    if (node->height>0) {
	unsigned int hsize=0;
	unsigned int csize=0;
	int i;
	size+=4; /* n_children */
	for (i=0; i<node->u.n.n_children-1; i++) {
	    size+=4;
	    csize+=node->u.n.childkeylens[i];
	}
	for (i=0; i<node->u.n.n_children; i++) {
	    size+=8;
	}
	int n_hashtables = brtnode_n_hashtables(node);
	size+=4; /* n_entries */
	for (i=0; i< n_hashtables; i++) {
	    HASHTABLE_ITERATE(node->u.n.htables[i],
			      key __attribute__((__unused__)), keylen,
			      data __attribute__((__unused__)), datalen,
			      (hsize+=8+keylen+datalen));
	}
	assert(hsize==node->u.n.n_bytes_in_hashtables);
	assert(csize==node->u.n.totalchildkeylens);
	return size+hsize+csize;
    } else {
	unsigned int hsize=0;
	PMA_ITERATE(node->u.l.buffer,
		    key __attribute__((__unused__)), keylen,
		    data __attribute__((__unused__)), datalen,
		    (hsize+=8+keylen+datalen));
	assert(hsize==node->u.l.n_bytes_in_buffer);
	hsize+=4; /* add n entries in buffer table. */
	return size+hsize;
    }

}

unsigned int serialize_brtnode_size (BRTNODE node) {
    unsigned int result = 4+4; /* size+height */
    assert(sizeof(off_t)==8);
    if (node->height>0) {
	result+=4; /* n_children */
	result+=4*(node->u.n.n_children-1); /* key lengths */
	result+=node->u.n.totalchildkeylens; /* the lengths of the pivot keys, without their key lengths. */
	result+=8*(node->u.n.n_children); /* child offsets. */
	result+=4; /* n_entries in hash table. */
	result+=node->u.n.n_bytes_in_hashtables;
    } else {
	result+=4; /* n_entries in buffer table. */
	result+=node->u.l.n_bytes_in_buffer;
	if (memory_check) {
	    unsigned int slowresult = serialize_brtnode_size_slow(node);
	    if (result!=slowresult) printf("%s:%d result=%d slowresult=%d\n", __FILE__, __LINE__, result, slowresult);
	    assert(result==slowresult);
	}
    }
    return result;
}

void serialize_brtnode_to(int fd, diskoff off, diskoff size, BRTNODE node) {
    struct cursor w;
    int i;
    unsigned int calculated_size = serialize_brtnode_size(node);
    assert(size>0);
    w.buf=my_malloc(size);
    w.size=size;
    w.ndone=0;
    //printf("%s:%d serializing %lld w height=%d p0=%p\n", __FILE__, __LINE__, off, node->height, node->mdicts[0]);
    wbuf_int(&w, calculated_size);
    wbuf_int(&w, node->height);
    //printf("%s:%d w.ndone=%d n_children=%d\n", __FILE__, __LINE__, w.ndone, node->n_children);
    if (node->height>0) { 
	wbuf_int(&w, node->u.n.n_children);
	//printf("%s:%d w.ndone=%d\n", __FILE__, __LINE__, w.ndone);
	for (i=0; i<node->u.n.n_children-1; i++) {
	    wbuf_bytes(&w, node->u.n.childkeys[i], node->u.n.childkeylens[i]);
	    //printf("%s:%d w.ndone=%d (childkeylen[%d]=%d\n", __FILE__, __LINE__, w.ndone, i, node->childkeylens[i]);
	}
	for (i=0; i<node->u.n.n_children; i++) {
	    wbuf_diskoff(&w, node->u.n.children[i]);
	    //printf("%s:%d w.ndone=%d\n", __FILE__, __LINE__, w.ndone);
	}

	{
	    int n_entries=0;
	    int n_hash_tables = brtnode_n_hashtables(node);
	    for (i=0; i< n_hash_tables; i++) {
		//printf("%s:%d p%d=%p n_entries=%d\n", __FILE__, __LINE__, i, node->mdicts[i], mdict_n_entries(node->mdicts[i]));
		n_entries += hashtable_n_entries(node->u.n.htables[i]);
	    }
	    //printf("%s:%d n_entries=%d\n", __FILE__, __LINE__, n_entries);
	    wbuf_int(&w, n_entries);
	    for (i=0; i< n_hash_tables; i++) {
		HASHTABLE_ITERATE(node->u.n.htables[i], key, keylen, data, datalen,
				  (wbuf_bytes(&w, key, keylen),
				   wbuf_bytes(&w, data, datalen)));
	    }
	}
    } else {
	wbuf_int(&w, pma_n_entries(node->u.l.buffer));
	PMA_ITERATE(node->u.l.buffer, key, keylen, data, datalen,
		    (wbuf_bytes(&w, key, keylen),
		     wbuf_bytes(&w, data, datalen)));
    }
    assert(w.ndone<=w.size);
    {
	ssize_t r=pwrite(fd, w.buf, w.ndone, off);
	if (r<0) printf("r=%d errno=%d\n", r, errno);
	assert((size_t)r==w.ndone);
    }

    //printf("%s:%d w.done=%d r=%d\n", __FILE__, __LINE__, w.ndone, r);
    assert(calculated_size==w.ndone);

    //printf("%s:%d wrote %d bytes for %lld size=%lld\n", __FILE__, __LINE__, w.ndone, off, size);
    assert(w.ndone<=size);
    my_free(w.buf);
}

int deserialize_brtnode_from (int fd, diskoff off, BRTNODE *brtnode, int nodesize) {
    TAGMALLOC(BRTNODE, result);
    struct cursor rc;
    int i;
    uint32_t datasize;
    int r;
    if (errno!=0) {
	r=errno;
	if (0) { died0: my_free(result); }
	return r;
    }
    {
	uint32_t datasize_n;
	int r = pread(fd, &datasize_n, sizeof(datasize_n), off);
	//printf("%s:%d r=%d the datasize=%d\n", __FILE__, __LINE__, r, ntohl(datasize_n));
	if (r!=sizeof(datasize_n)) {
	    if (r==-1) r=errno;
	    else r = DB_BADFORMAT;
	    goto died0;
	}
	datasize = ntohl(datasize_n);
	if (datasize<=0 || datasize>(1<<30)) { r = DB_BADFORMAT; goto died0; }
    }
    rc.buf=my_malloc(datasize);
    if (errno!=0) {
	if (0) { died1: my_free(rc.buf); }
	r=errno;
	goto died0;
    }
    rc.size=datasize;
    assert(rc.size>0);
    rc.ndone=0;
    //printf("Deserializing %lld datasize=%d\n", off, datasize);
    {
	ssize_t r=pread(fd, rc.buf, datasize, off);
	if ((size_t)r!=datasize) { r=errno; goto died1; }
	//printf("Got %d %d %d %d\n", rc.buf[0], rc.buf[1], rc.buf[2], rc.buf[3]);
    }
    {
	unsigned int stored_size = rbuf_int(&rc);
	if (stored_size!=datasize) { r=DB_BADFORMAT; goto died1; }
    }
    result->nodesize = nodesize; // How to compute the nodesize?
    result->thisnodename = off;
    result->height = rbuf_int(&rc);
    //printf("height==%d\n", result->height);
    if (result->height>0) {
	result->u.n.totalchildkeylens=0;
	for (i=0; i<TREE_FANOUT; i++) { result->u.n.childkeys[i]=0; result->u.n.childkeylens[i]=0; }
	for (i=0; i<TREE_FANOUT+1; i++) { result->u.n.children[i]=0; result->u.n.htables[i]=0; result->u.n.n_bytes_in_hashtable[i]=0; }
	result->u.n.n_children = rbuf_int(&rc);
	//printf("n_children=%d\n", result->n_children);
	assert(result->u.n.n_children>=0 && result->u.n.n_children<=TREE_FANOUT);
	for (i=0; i<result->u.n.n_children-1; i++) {
	    bytevec childkeyptr;
	    rbuf_bytes(&rc, &childkeyptr, &result->u.n.childkeylens[i]); /* Returns a pointer into the rbuf. */
	    result->u.n.childkeys[i] = memdup(childkeyptr, result->u.n.childkeylens[i]);
	    //printf(" key %d length=%d data=%s\n", i, result->childkeylens[i], result->childkeys[i]);
	    result->u.n.totalchildkeylens+=result->u.n.childkeylens[i];
	}
	for (i=0; i<result->u.n.n_children; i++) {
	    result->u.n.children[i] = rbuf_diskoff(&rc);
	    //printf("Child %d at %lld\n", i, result->children[i]);
	}
	for (i=0; i<TREE_FANOUT+1; i++) {
	    result->u.n.n_bytes_in_hashtable[i] = 0;
	}
	result->u.n.n_bytes_in_hashtables = 0; 
	for (i=0; i<brtnode_n_hashtables(result); i++) {
	    int r=hashtable_create(&result->u.n.htables[i]);
	    if (r!=0) {
		int j;
		if (0) { died_12: j=brtnode_n_hashtables(result); }
		for (j=0; j<i; j++) hashtable_free(&result->u.n.htables[j]);
		goto died1;
	    }
	}
	{
	    int n_in_hash = rbuf_int(&rc);
	    //printf("%d in hash\n", n_in_hash);

	    for (i=0; i<n_in_hash; i++) {
		int childnum, diff;
		bytevec key; ITEMLEN keylen; 
		bytevec val; ITEMLEN vallen;
		verify_counts(result);
		rbuf_bytes(&rc, &key, &keylen); /* Returns a pointer into the rbuf. */
		rbuf_bytes(&rc, &val, &vallen);
		//printf("Found %s,%s\n", key, val);
		childnum = brtnode_which_child(result, key, keylen);
		{
		    int r=hash_insert(result->u.n.htables[childnum], key, keylen, val, vallen); /* Copies the data into the hash table. */
		    if (r!=0) { goto died_12; }
		}
		diff =  keylen + vallen + KEY_VALUE_OVERHEAD;
		result->u.n.n_bytes_in_hashtables += diff;
		result->u.n.n_bytes_in_hashtable[childnum] += diff;
	    //printf("Inserted\n");
	    }
	}
    } else {
	int n_in_buf = rbuf_int(&rc);
	result->u.l.n_bytes_in_buffer = 0;
	int r=pma_create(&result->u.l.buffer);
	if (r!=0) {
	    if (0) { died_21: pma_free(&result->u.l.buffer); }
	    goto died1;
	}
	//printf("%s:%d r PMA= %p\n", __FILE__, __LINE__, result->u.l.buffer); 
	for (i=0; i<n_in_buf; i++) {
	    bytevec key; ITEMLEN keylen; 
	    bytevec val; ITEMLEN vallen;
	    verify_counts(result);
	    rbuf_bytes(&rc, &key, &keylen); /* Returns a pointer into the rbuf. */
	    rbuf_bytes(&rc, &val, &vallen);
	    {
		int r = pma_insert(result->u.l.buffer, key, keylen, val, vallen);
		if (r!=0) goto died_21;
	    }
	    result->u.l.n_bytes_in_buffer += keylen + vallen + KEY_VALUE_OVERHEAD;
	}
    }
    //printf("%s:%d Ok got %lld n_children=%d\n", __FILE__, __LINE__, result->thisnodename, result->n_children);
    my_free(rc.buf);
    *brtnode = result;
    verify_counts(result);
    return 0;
}

unsigned int brtnode_which_child (BRTNODE node, bytevec key, ITEMLEN keylen) {
    int i;
    assert(node->height>0);
    for (i=0; i<node->u.n.n_children-1; i++) {
	if (keycompare(key, keylen, node->u.n.childkeys[i], node->u.n.childkeylens[i])<=0) {
	    return i;
	}
    }
    return node->u.n.n_children-1;
}

void verify_counts (BRTNODE node) {
    if (node->height==0) {
	assert(node->u.l.buffer);
    } else {
	unsigned int sum = 0;
	int i;
	for (i=0; i<node->u.n.n_children; i++)
	    sum += node->u.n.n_bytes_in_hashtable[i];
	for (; i<TREE_FANOUT+1; i++) {
	    assert(node->u.n.n_bytes_in_hashtable[i]==0);
	}
	assert(sum==node->u.n.n_bytes_in_hashtables);
    }
}
    
int serialize_brt_header_to (int fd, struct brt_header *h) {
    struct cursor w;
    int i;
    unsigned int size=0; /* I don't want to mess around calculating it exactly. */ 
    size += 4+4+8+8+4; /* this size, the tree's nodesize, freelist, unused_memory, nnamed_rootse. */
    if (h->n_named_roots<0) {
	size+=8;
    } else {
	for (i=0; i<h->n_named_roots; i++) {
	    size+=12 + 1 + strlen(h->names[i]);
	}
    }
    w.buf = my_malloc(size);
    w.size = size;
    w.ndone = 0;
    wbuf_int    (&w, size);
    wbuf_int    (&w, h->nodesize);
    wbuf_diskoff(&w, h->freelist);
    wbuf_diskoff(&w, h->unused_memory);
    wbuf_int    (&w, h->n_named_roots);
    if (h->n_named_roots>0) {
	for (i=0; i<h->n_named_roots; i++) {
	    char *s = h->names[i];
	    unsigned int l = 1+strlen(s);
	    wbuf_diskoff(&w, h->roots[i]);
	    wbuf_bytes  (&w,  s, l);
	    assert(l>0 && s[l-1]==0);
	}
    } else {
	wbuf_diskoff(&w, h->unnamed_root);
    }
    assert(w.ndone==size);
    {
	ssize_t r = pwrite(fd, w.buf, w.ndone, 0);
	assert((size_t)r==w.ndone);
    }
    my_free(w.buf);
    return 0;
}

int deserialize_brtheader_from (int fd, diskoff off, struct brt_header **brth) {
    struct brt_header *MALLOC(h);
    struct cursor rc;
    int size;
    int sizeagain;
    assert(off==0);
    {
	uint32_t size_n;
	ssize_t r = pread(fd, &size_n, sizeof(size_n), off);
	if (r==0) { my_free(h); return -1; }
	assert(r==sizeof(size_n));
	size = ntohl(size_n);
    }
    rc.buf = my_malloc(size);
    rc.size=size;
    assert(rc.size>0);
    rc.ndone=0;
    {
	ssize_t r = pread(fd, rc.buf, size, off);
	assert(r==size);
    }
    h->dirty=0;
    sizeagain        = rbuf_int(&rc);
    assert(sizeagain==size);
    h->nodesize      = rbuf_int(&rc);
    h->freelist      = rbuf_diskoff(&rc);
    h->unused_memory = rbuf_diskoff(&rc);
    h->n_named_roots = rbuf_int(&rc);
    if (h->n_named_roots>=0) {
	int i;
	MALLOC_N(h->n_named_roots, h->roots);
	MALLOC_N(h->n_named_roots, h->names);
	for (i=0; i<h->n_named_roots; i++) {
	    bytevec nameptr;
	    unsigned int len;
	    h->roots[i] = rbuf_diskoff(&rc);
	    rbuf_bytes(&rc, &nameptr, &len);
	    assert(strlen(nameptr)+1==len);
	    h->names[i] = memdup(nameptr,len);
	}
	h->unnamed_root = -1;
    } else {
	h->roots = 0;
	h->names = 0;
	h->unnamed_root = rbuf_diskoff(&rc);
    }
    assert(rc.ndone==rc.size);
    my_free(rc.buf);
    *brth = h;
    return 0;
}
