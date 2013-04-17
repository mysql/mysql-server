/* Test bread_backwards to make sure it can read backwards even for large files. */

#include "toku_portability.h"

#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <zlib.h>

#include "../brttypes.h"
#include "../bread.h"
#include "test.h"

#define FNAME "test1305.data"

// THe buffer size in units of 64-bit integers.
#define N_BIGINTS (1<<20)
#define BIGINT_SIZE (sizeof(u_int64_t))
// How big is the readback buffer (in 8-bit integers)?
#define READBACK_BUFSIZE (1<<20)


static void
test (u_int64_t fsize) {
    unlink_file_and_bit(FNAME);
    // Create a file of size fsize.  Fill it with 8-byte values which are integers, in order)
    assert(fsize%(N_BIGINTS*sizeof(u_int64_t)) == 0); // Make sure the fsize is a multiple of the buffer size.
    u_int64_t i = 0;
    {
	int fd = open(FNAME, O_CREAT+O_RDWR+O_BINARY, 0777);
	assert(fd>=0);
	static u_int64_t buf[N_BIGINTS]; //windows cannot handle this on the stack
	static char compressed_buf[N_BIGINTS*2 + 1000]; // this is more than compressbound returns
	uLongf compressed_len;
	while (i*BIGINT_SIZE < fsize) {
            if (verbose>0 && i % (1<<25) == 0) {
                printf("   %s:test (%"PRIu64") forwards [%"PRIu64"%%]\n", __FILE__, fsize, 100*BIGINT_SIZE*((u_int64_t)i) / fsize);
                fflush(stdout);
            }

	    int j;
	    for (j=0; j<N_BIGINTS; j++) {
		buf[j] = i++;
	    }
assert(sizeof(buf) == N_BIGINTS * BIGINT_SIZE);
	    {
		compressed_len = sizeof(compressed_buf);
		int r = compress2((Bytef*)compressed_buf, &compressed_len, (Bytef*)buf, sizeof(buf), 1);
		assert(r==Z_OK);
	    }
	    {
		u_int32_t v = htonl(compressed_len);
		ssize_t r = write(fd, &v, sizeof(v));
		assert(r==sizeof(v));
	    }
	    {
		ssize_t r = write(fd, compressed_buf, compressed_len);
		assert(r==(ssize_t)compressed_len);
	    }
	    {
		u_int32_t v = htonl(sizeof(buf));
		ssize_t r = write(fd, &v, sizeof(v));
		assert(r==sizeof(v));
	    }
	    {
		u_int32_t v = htonl(compressed_len);
		ssize_t r = write(fd, &v, sizeof(v));
		assert(r==sizeof(v));
	    }
	}
	{ int r = close(fd); assert(r==0); }
    }
    assert(i*BIGINT_SIZE == fsize);
    // Now read it all backward
    {
	int fd = open(FNAME, O_RDONLY+O_BINARY);  	assert(fd>=0);
	BREAD br = create_bread_from_fd_initialize_at(fd);
	while (bread_has_more(br)) {
            if (verbose>0 && (fsize/BIGINT_SIZE - i) % (1<<25) == 0) {
                printf("   %s:test (%"PRIu64") backwards [%"PRIu64"%%]\n", __FILE__, fsize, 100*BIGINT_SIZE*((u_int64_t)i) / fsize);
                fflush(stdout);
            }
	    assert(i>0);
	    i--;
	    u_int64_t storedi;
	    { int r = bread_backwards(br, &storedi, sizeof(storedi)); assert(r==sizeof(storedi)); }
	    assert(storedi==i);
	}
	assert(i==0);
	{ int r=close_bread_without_closing_fd(br); assert(r==0); }
	{ int r=close(fd); assert(r==0); }
    }
    //printf("Did %" PRIu64 "\n", fsize);
    //system("ls -l " FNAME);
    unlink_file_and_bit(FNAME);
}

int
test_main (int argc, const char *argv[]) {
    default_parse_args(argc, argv);
    test(1LL<<23);
    test(1LL<<30);
    test(1LL<<31);
    test(1LL<<32);
    test(1LL<<33);
    return 0;
}

