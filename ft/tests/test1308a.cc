/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
// Test the first case for the bug in #1308 (brt-serialize.c:33 does the cast wrong)
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."

#include "test.h"

#include <string.h>

#include <toku_portability.h>
#include "../ft-ops.h" 

#define FNAME "test1308a.data"

#define BUFSIZE (16<<20)

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__)))
{
    unlink(FNAME);
    
    int fd;
    {

	static uint64_t buf [BUFSIZE]; // make this static because it's too big to fit on the stack.

	fd = open(FNAME, O_CREAT+O_RDWR+O_BINARY, 0777);
	assert(fd>=0);
	memset(buf, 0, sizeof(buf));
	uint64_t i;
	for (i=0; i<(1LL<<32); i+=BUFSIZE) {
	    toku_os_full_write(fd, buf, BUFSIZE);
	}
    }
    int64_t file_size;
    {
        int r = toku_os_get_file_size(fd, &file_size);
        assert(r==0);
    }
    {
        int64_t size_after;
	toku_maybe_preallocate_in_file(fd, 1000, file_size, &size_after);
        assert(size_after == file_size);
    }
    int64_t file_size2;
    {
        int r = toku_os_get_file_size(fd, &file_size2);
        assert(r==0);
    }
    assert(file_size==file_size2);
    close(fd);

    unlink(FNAME);
    return 0;
}
