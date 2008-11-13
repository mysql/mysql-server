#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <fcntl.h>
#include "portability.h"

int main(void) {
    int r;
    int fd;
    struct fileid fid;

    fd = open(DEV_NULL_FILE, O_RDWR);
    assert(fd != -1);

    r = toku_os_get_unique_file_id(fd, &fid);
    printf("%s:%d %d\n", __FILE__, __LINE__, r);

    r = close(fd);
    assert(r != -1);
    return 0;
}
