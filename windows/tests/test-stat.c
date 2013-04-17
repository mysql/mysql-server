#include <toku_portability.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/stat.h>

void test_stat(char *dirname, int result) {
    int r;
    struct stat s;
    r = stat(dirname, &s);
    printf("stat %s %d\n", dirname, r); fflush(stdout);
    assert(r==result);
}

int main(void) {
    int r;

    test_stat(".", 0);
    test_stat("./", 0);

    r = system("rm -rf testdir"); assert(r==0);
    r = toku_os_mkdir("testdir", S_IRWXU);
    assert(r == 0);

    test_stat("testdir", 0);

    test_stat("./testdir", 0);

    test_stat("./testdir/", 0);
    
    return 0;
}
