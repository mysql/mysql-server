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

#define TESTFILE "test-open-unlink-file"
#define NEWNAME TESTFILE ".junk"

int test_main(int argc, char *argv[]) {
    int r;
    int fd;

    r = system("rm -rf " TESTFILE); assert(r==0);
    r = system("rm -rf " NEWNAME); assert(r==0);
    
    fd = open(TESTFILE, O_CREAT+O_RDWR, S_IRUSR+S_IWUSR);
    assert(fd != -1);

    r = rename(TESTFILE, NEWNAME);
    printf("%s:%d rename %d %d\n", __FILE__, __LINE__, r, errno); fflush(stdout);
#if defined(__linux__)
    assert(r == 0);

    r = close(fd);
    assert(r == 0);
#endif
#if TOKU_WINDOWS
    assert(r == -1);
    
    r = close(fd);
    assert(r == 0);
    
    r = rename(TESTFILE, NEWNAME);
    printf("%s:%d rename %d %d\n", __FILE__, __LINE__, r, errno); fflush(stdout);
    assert(r == 0);
#endif

    return 0;
}
