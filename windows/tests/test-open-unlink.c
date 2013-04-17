#include <test.h>
#include <stdio.h>
#include <stdlib.h>
#include <toku_assert.h>
#include <errno.h>
#include <fcntl.h>
#if TOKU_WINDOWS
#include <io.h>
#endif
#include <sys/stat.h>

#ifndef S_IRUSR
#define S_IRUSR S_IREAD
#endif
#ifndef S_IWUSR
#define S_IWUSR S_IWRITE
#endif

const char TESTFILE[] = "test-open-unlink-file";

int test_main(int argc, char *const argv[]) {
    int r;
    int fd;

    system("rm -rf test-open-unlink-file");
    
    fd = open(TESTFILE, O_CREAT+O_RDWR, S_IRUSR+S_IWUSR);
    assert(fd != -1);

    r = unlink(TESTFILE);
    printf("%s:%d unlink %d %d\n", __FILE__, __LINE__, r, errno); fflush(stdout);
#if defined(__linux__)
    assert(r == 0);

    r = close(fd);
    assert(r == 0);
#endif
#if TOKU_WINDOWS
    assert(r == -1);
    
    r = close(fd);
    assert(r == 0);
    
    r = unlink(TESTFILE);
    printf("%s:%d unlink %d %d\n", __FILE__, __LINE__, r, errno); fflush(stdout);
    assert(r == 0);
#endif

    return 0;
}
