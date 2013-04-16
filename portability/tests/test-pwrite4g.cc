/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
/* Verify that toku_os_full_pwrite does the right thing when writing beyond 4GB.  */
#include <test.h>
#include <fcntl.h>
#include <toku_assert.h>
#include <memory.h>
#include <string.h>
#include <stdio.h>
#include <portability/toku_path.h>

static int iszero(char *cp, size_t n) {
    size_t i;
    for (i=0; i<n; i++)
        if (cp[i] != 0) 
	    return 0;
    return 1;
}

int test_main(int UU(argc), char *const UU(argv[])) {
    int r;
    unlink(TOKU_TEST_FILENAME);
    int fd = open(TOKU_TEST_FILENAME, O_RDWR | O_CREAT | O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO);
    assert(fd>=0);
    char *XMALLOC_N_ALIGNED(512, 512, buf);
    memset(buf, 0, 512);
    strcpy(buf, "hello");
    int64_t offset = (1LL<<32) + 512;
    toku_os_full_pwrite(fd, buf, 512, offset);
    char newbuf[512];
    r = pread(fd, newbuf, sizeof newbuf, 100);
    assert(r==sizeof newbuf);
    assert(iszero(newbuf, sizeof newbuf));
    r = pread(fd, newbuf, sizeof newbuf, offset);
    assert(r==sizeof newbuf);
    assert(memcmp(newbuf, buf, sizeof newbuf) == 0);
    int64_t fsize;
    r = toku_os_get_file_size(fd, &fsize);
    assert(r == 0);
    assert(fsize > 100 + 512);
    toku_free(buf);
    r = close(fd);
    assert(r==0);
    return 0;
}
