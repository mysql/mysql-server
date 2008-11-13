#include <portability.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <sys/stat.h>

void test_stat(char *dirname) {
    int r;
    struct stat s;
    r = stat(dirname, &s);
    printf("stat %s %d\n", dirname, r);
}

int main(void) {
    int r;

    test_stat(".");

    r = toku_os_mkdir("testdir", S_IRWXU);
    assert(r == 0);

    test_stat("testdir");

    test_stat("./testdir");

    test_stat("./testdir/");
    
    return 0;
}
