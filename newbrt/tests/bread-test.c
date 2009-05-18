/* Test bread by writing random data and then reading it using bread_backwards() to see if it gives the right answer.
 * See test_1305 for another bread test (testing to see if it can read 1GB files) */

#include "test.h"
#include <toku_portability.h>

#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <zlib.h>

#include "../brttypes.h"
#include "../bread.h"

#define FNAME "bread-test.data"

#define RECORDS 20
#define RECORDLEN 100

char buf[RECORDS][RECORDLEN];
int sizes[RECORDS];
int sizesn[RECORDS];
int nwrote=0;
char wrotedata[RECORDS*RECORDLEN];

static void
test (int seed) {
    srandom(seed);
    unlink(FNAME);
    int i;
    {
        int fd = open(FNAME, O_CREAT+O_RDWR+O_BINARY, 0777);
	assert(fd>=0);
	for (i=0; i<RECORDS; i++) {
	    sizes[i]  = 1+ random()%RECORDLEN;
	    sizesn[i] = toku_htod32(sizes[i]);
	    int j;
	    for (j=0; j<sizes[i]; j++) {
		buf[i][j] = wrotedata[nwrote++] = (char)random();
	    }
	    uLongf compressed_size = compressBound(sizes[i]);
	    Bytef compressed_buf[compressed_size];
	    { int r = compress2(compressed_buf, &compressed_size, (Bytef*)(buf[i]), sizes[i], 1); assert(r==Z_OK); }
	    u_int32_t compressed_size_n = toku_htod32(compressed_size);
	    { int r = write(fd, &compressed_size_n, 4); assert(r==4); }
	    { int r = write(fd, compressed_buf, compressed_size);    assert(r==(int)compressed_size); }
	    { int r = write(fd, &sizesn[i], 4);         assert(r==4); } // the uncompressed size
	    { int r = write(fd, &compressed_size_n, 4); assert(r==4); }
	}
	{ int r=close(fd); assert(r==0); }
    }
    int fd = open(FNAME, O_RDONLY+O_BINARY);  	assert(fd>=0);
    // Now read it all backward
    BREAD br = create_bread_from_fd_initialize_at(fd);
    while (bread_has_more(br)) {
	assert(nwrote>0);
	int to_read = 1+(random()%RECORDLEN); // read from 1 to 100 (if RECORDLEN is 100)
	if (to_read>nwrote) to_read=nwrote;
	char rbuf[to_read];
	int r = bread_backwards(br, rbuf, to_read);
	assert(r==to_read);
	assert(memcmp(rbuf, &wrotedata[nwrote-to_read], to_read)==0);
	nwrote-=to_read;
    }
    assert(nwrote==0);

    { int r=close_bread_without_closing_fd(br); assert(r==0); }
    { int r=close(fd); assert(r==0); }
    unlink(FNAME);
}

int
test_main (int argc __attribute__((__unused__)), const char *argv[] __attribute__((__unused__))) {
    int i;
    for (i=0; i<10; i++) test(i);
    return 0;
}
