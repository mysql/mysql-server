#include <test.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <toku_assert.h>
#include <fcntl.h>
#include <string.h>
#include <sys/stat.h>

#if 0 && defined _WIN32
#include <windows.h>
static int ftruncate(int fd, uint64_t offset) {
    HANDLE h = (HANDLE) _get_osfhandle(fd);
    printf("%s:%d %p\n", __FILE__, __LINE__, h); fflush(stdout);
    if (h == INVALID_HANDLE_VALUE)
        return -1;
    int r = _lseeki64(fd, 0, SEEK_SET);
    printf("%s:%d %d\n", __FILE__, __LINE__, r); fflush(stdout);
    if (r != 0)
        return -2;
    BOOL b = SetEndOfFile(h);
    printf("%s:%d %d\n", __FILE__, __LINE__, b); fflush(stdout);
    if (!b)
        return -3;
    return 0;
}
#endif

int test_main(int argc, char *argv[]) {
    int r;
    int fd;

    fd = open("test-file-truncate", O_CREAT+O_RDWR+O_TRUNC, S_IREAD+S_IWRITE);
    assert(fd != -1);

    int i;
    for (i=0; i<32; i++) {
        char junk[4096];
        memset(junk, 0, sizeof junk);
        r = write(fd, junk, sizeof junk);
        assert(r == sizeof junk);
    }
    
    toku_struct_stat filestat;
    r = toku_fstat(fd, &filestat);
    assert(r == 0);

    printf("orig size %lu\n", (unsigned long) filestat.st_size); fflush(stdout);

    r = ftruncate(fd, 0);
    assert(r == 0);

    r = toku_fstat(fd, &filestat);
    assert(r == 0);

    printf("truncated size %lu\n", (unsigned long) filestat.st_size); fflush(stdout);
    assert(filestat.st_size == 0);

    return 0;
}
