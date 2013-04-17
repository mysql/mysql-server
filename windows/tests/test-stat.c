#include <test.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <toku_assert.h>
#include <sys/stat.h>
#include <errno.h>

void test_stat(char *dirname, int result, int ex_errno) {
    int r;
    toku_struct_stat buf;
    r = toku_stat(dirname, &buf);
    printf("stat %s %d %d\n", dirname, r, errno); fflush(stdout);
    assert(r==result);
    if (r!=0) assert(errno == ex_errno);
}

int test_main(int argc, char *argv[]) {
    int r;

    test_stat(".", 0, 0);
    test_stat("./", 0, 0);

    r = system("rm -rf testdir"); assert(r==0);
    test_stat("testdir", -1, ENOENT);
    test_stat("testdir/", -1, ENOENT);
    test_stat("testdir/foo", -1, ENOENT);
    test_stat("testdir/foo/", -1, ENOENT);
    r = toku_os_mkdir("testdir", S_IRWXU);
    assert(r == 0);
    test_stat("testdir/foo", -1, ENOENT);
    test_stat("testdir/foo/", -1, ENOENT);
    r = system("touch testdir/foo"); assert(r==0);
    test_stat("testdir/foo", 0, 0);
    test_stat("testdir/foo/", -1, ENOENT);

    test_stat("testdir", 0, 0);

    test_stat("./testdir", 0, 0);

    test_stat("./testdir/", 0, 0);

    return 0;
}
