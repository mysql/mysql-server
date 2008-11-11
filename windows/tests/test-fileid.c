
#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include "portability.h"
#include "os.h"

int verbose=0;

//TODO: Test that different files are different,
//      other stuff
static void test_handles(const char *fname) {
    unlink(fname);
    int fd = open(fname,  O_RDWR | O_CREAT | O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO);
    assert(fd!=-1);
    int i;
    struct fileid id_base;
    struct fileid id;
    int r = os_get_unique_file_id(fd, &id_base);
    assert(r==0);
    for (i=0; i < 1<<16; i++) {
        r = os_get_unique_file_id(fd, &id);
        assert(r==0);
        assert(memcmp(&id, &id_base, sizeof(id))==0);
    }
    r = close(fd);
    assert(r==0);
}

int main(int argc, char *argv[]) {
    int i;

    for (i=1; i<argc; i++) {
        char *arg = argv[i];
        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0)
            verbose++;
    }

    test_handles("junk");

    return 0;
}
