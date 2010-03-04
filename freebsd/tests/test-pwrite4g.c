/* Verify that toku_os_pwrite does the right thing when writing beyond 4GB.  */
#include <fcntl.h>
#include <test.h>
#include <toku_assert.h>

int test_main(int argc, char *const argv[]) {
    char fname[] = "pwrite4g.data";
    int r;
    unlink(fname);
    int fd = open(fname, O_RDWR | O_CREAT | O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO);
    assert(fd>=0);
    char buf[] = "hello";
    int64_t offset = (1LL<<32) + 100;
    r = toku_os_pwrite(fd, buf, sizeof(buf), offset);
    assert(r==sizeof(buf));
    int64_t fsize;
    r = toku_os_get_file_size(fd, &fsize);
    assert(r == 0);
    assert(fsize > 100 + (signed)sizeof(buf));
    r = close(fd);
    assert(r==0);
    return 0;
}
