/* Verify that toku_os_full_pwrite does the right thing when writing beyond 4GB.  */
#include <fcntl.h>
#include <string.h>
#include <toku_assert.h>
#include <test.h>

static int iszero(char *cp, size_t n) {
    size_t i;
    for (i=0; i<n; i++)
        if (cp[i] != 0) 
	    return 0;
    return 1;
}

int test_main(int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    char fname[] = "pwrite4g.data";
    int r;
    unlink(fname);
    int fd = open(fname, O_RDWR | O_CREAT | O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO);
    assert(fd>=0);
    char buf[] = "hello";
    int64_t offset = (1LL<<32) + 100;
    toku_os_full_pwrite(fd, buf, sizeof buf, offset);
    char newbuf[sizeof buf];
    r = pread(fd, newbuf, sizeof newbuf, 100);
    assert(r==sizeof newbuf);
    assert(iszero(newbuf, sizeof newbuf));
    r = pread(fd, newbuf, sizeof newbuf, offset);
    assert(r==sizeof newbuf);
    assert(memcmp(newbuf, buf, sizeof newbuf) == 0);
    int64_t fsize;
    r = toku_os_get_file_size(fd, &fsize);
    assert(r == 0);
    assert(fsize > 100 + (signed)sizeof(buf));
    r = close(fd);
    assert(r==0);
    return 0;
}
