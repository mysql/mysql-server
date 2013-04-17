/* Verify that toku_os_pwrite does the right thing when writing beyond 4GB.  */
#include <fcntl.h>
#include "../toku_include/toku_portability.h"
#include <assert.h>

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    char fname[] = "pwrite4g.data";
    int r = unlink(fname);
    assert(r==0);
    int fd = open(fname, O_RDWR | O_CREAT | O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO);
    assert(fd>=0);
    char buf[] = "hello";
    r = toku_os_pwrite(fd, buf, sizeof(buf), (1LL<<32)+100);
    assert(r==sizeof(buf));
    r = close(fd);
    assert(r==0);
    struct stat statbuf;
    r = stat(fname, &statbuf);
    assert(r==0);
    assert(statbuf.st_size > 100 + (signed)sizeof(buf));
    return 0;
}
