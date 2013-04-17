#include <test.h>
#include <stdio.h>
#include <stdlib.h>
#include <toku_assert.h>
#include <fcntl.h>
#include "toku_os.h"

int verbose=0;
enum {NUM_IDS=4};
struct fileid old_ids[NUM_IDS];
BOOL valid[NUM_IDS];

//TODO: Test that different files are different,
//      other stuff
static void test_handles(const char *fname, unsigned which) {
    unlink(fname);
    int fd = open(fname,  O_RDWR | O_CREAT | O_BINARY, S_IRWXU|S_IRWXG|S_IRWXO);
    assert(fd!=-1);
    int i;
    struct fileid id_base;
    struct fileid id;
    int r = toku_os_get_unique_file_id(fd, &id_base);
    CKERR(r);
    assert(which < NUM_IDS);
    for (i = 0; i < NUM_IDS; i++) {
        if (valid[i]) {
            if (which==i) {
                //Assert same
                assert(memcmp(&id_base, &old_ids[i], sizeof(id_base))==0);
            }
            else {
                //Assert different
                assert(memcmp(&id_base, &old_ids[i], sizeof(id_base))!=0);
            }
        }
    }
    memcpy(&old_ids[which], &id_base, sizeof(id_base));
    valid[which] = TRUE;

    if (verbose) printf("[%s] : r=[%d] errno=[%d] id=[0x%"PRIx32"/0x%"PRIx64"]\n", fname, r, errno, id_base.st_dev, id_base.st_ino);
    for (i=0; i < 1<<16; i++) {
        r = toku_os_get_unique_file_id(fd, &id);
        CKERR(r);
        assert(memcmp(&id, &id_base, sizeof(id))==0);
    }
    r = close(fd);
    CKERR(r);
}

int test_main(int argc, char *const argv[]) {
    int i;

    for (i=1; i<argc; i++) {
        char *arg = argv[i];
        if (strcmp(arg, "-v") == 0 || strcmp(arg, "--verbose") == 0)
            verbose++;
    }

    test_handles("junk1", 0);
    test_handles("junk2", 1);
    test_handles("junk3", 2);
    test_handles("NUL", 3);
    test_handles(".\\NUL", 3);
    test_handles("\\NUL", 3);
    test_handles("C:\\NUL", 3);

    return 0;
}
