/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include "brt-internal.h"
#include <sys/types.h>
#include <unistd.h>
#include <assert.h>
#include <string.h>

int read_sint (int fd, int *result) {
    unsigned char b[4];
    int r = read(fd, b, 4);
    if (r!=4) return 1;
    *result = (b[0]<<24) | (b[1]<<16) | (b[2]<<8) | (b[3]<<0);
    return 0;
}
int read_uint (int fd, unsigned int *result) {
    int sresult;
    int r = read_sint(fd, &sresult);
    if (r==0) { *result = r; }
    return r;
}

int write_int (int fd, unsigned int v) {
    unsigned char b[4];
    int r;
    b[0] = (v>>24)&0xff;
    b[1] = (v>>16)&0xff;
    b[2] = (v>>8)&0xff;
    b[3] = (v>>0)&0xff;
    r = write(fd, b, 4);
    if (r!=4) return 1;
    return 0;
}

int read_diskoff (int fd, DISKOFF *result) {
    unsigned int i0,i1;
    int r;
    r = read_uint(fd, &i0);  if(r!=0) return r;
    r = read_uint(fd, &i1);  if(r!=0) return r;
    *result = ((unsigned long long)i0)<<32 | ((unsigned long long)i1);
    return 0;
}

int write_diskoff (int fd, DISKOFF v) {
    int r;
    r = write_int(fd, (unsigned int)(v>>32));        if (r!=0) return r;
    r = write_int(fd, (unsigned int)(v&0xffffffff)); if (r!=0) return r;
    return 0;
}

int read_bytes (int fd, int l, char *s) {
    int r = read(fd, s, l);
    if (r==l) return 0;
    return -1;
}

int write_bytes (int fd, int l, char *s) {
    int r= write(fd, s, l);
    if (r==l) return 0;
    return -1;
}

int read_brt_header (int fd, struct brt_header *header) {
    {
	off_t r = lseek(fd, 0, SEEK_SET);
	assert(r==0);
    }
    /* Ignore magic for now.  We'll need some magic at the beginning of the file. */
    {
	int r;
	r = read_uint(fd, &header->nodesize);
	if (r!=0) return -1;
	r = read_diskoff(fd, &header->freelist); assert(r==0); /* These asserts should do something smarter. */
	r = read_diskoff(fd, &header->unused_memory); assert(r==0);
	r = read_sint(fd, &header->n_named_roots); assert(r==0);
	if (header->n_named_roots>0) {
	    int i;
	    header->unnamed_root = -1;
	    MALLOC_N(header->n_named_roots, header->names);
	    MALLOC_N(header->n_named_roots, header->roots);
	    for (i=0; i<header->n_named_roots; i++) {
		unsigned int l;
		char *s;
		r = read_diskoff(fd, &header->roots[i]); assert(r==0);
		r = read_uint(fd, &l); assert(r==0); /* count includes the trailing null. */
		MALLOC_N(l, s);
		r = read_bytes(fd, l, s); assert(r==0);
		assert(l>0 && s[l-1]==0);
		header->names[i] = s;
	    }
	} else {
	    r = read_diskoff(fd, &header->unnamed_root); assert(r==0);
	    header->names = 0;
	    header->roots = 0;
	}
    }
    return 0;
}

int read_brt_h_unused_memory (int fd, DISKOFF *unused_memory) {
    off_t r = lseek(fd, 12, SEEK_SET);
    assert(r==12);
    r = read_diskoff(fd, unused_memory);
    return r;
}

int write_brt_h_unused_memory (int fd, DISKOFF unused_memory) {
    off_t r = lseek(fd, 12, SEEK_SET);
    assert(r==12);
    r = write_diskoff(fd, unused_memory);
    return r;
}
