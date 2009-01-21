// Test the first case for the bug in #1308 (brt-serialize.c:33 does the cast wrong)


#include "test.h"
#include <assert.h>
#include <string.h>

#include "toku_portability.h"
#include "../brt.h" 

#define FNAME "test1308a.data"

#define BUFSIZE (16<<20)

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__)))
{
    unlink_file_and_bit(FNAME);
    
    int fd;
    {

	static u_int64_t buf [BUFSIZE]; // make this static because it's too big to fit on the stack.

	fd = open(FNAME, O_CREAT+O_RDWR+O_BINARY, 0777);
	assert(fd>=0);
	memset(buf, 0, sizeof(buf));
	u_int64_t i;
	for (i=0; i<(1LL<<32); i+=BUFSIZE) {
	    int r = write(fd, buf, BUFSIZE);
	    assert(r==BUFSIZE);
	}
    }
    int64_t file_size;
    {
        int r = toku_os_get_file_size(fd, &file_size);
        assert(r==0);
    }
    maybe_preallocate_in_file(fd, 1000);
    int64_t file_size2;
    {
        int r = toku_os_get_file_size(fd, &file_size2);
        assert(r==0);
    }
    assert(file_size==file_size2);
    close(fd);

    unlink_file_and_bit(FNAME);
    return 0;
}
