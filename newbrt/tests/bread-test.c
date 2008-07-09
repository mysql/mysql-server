#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>

#include "../bread.h"

#define FNAME "bread-test.data"

#define RECORDS 2

void test (int seed) {
    srandom(seed);
    unlink(FNAME);
    int i;
    char buf[RECORDS][100];
    int sizes[RECORDS];
    int sizesn[RECORDS];
    size_t off = 0;
    {
	int fd = creat(FNAME, 0777);
	assert(fd>=0);
	for (i=0; i<RECORDS; i++) {
	    sizes[i]  = random()%100;
	    sizesn[i] = htonl(sizes[i]);
	    int j;
	    for (j=0; j<sizes[i]; j++) {
		buf[i][j]=random();
	    }
	    int r = write(fd, buf[i], sizes[i]);
	    assert(r==sizes[i]);
	    off+=r;
	    r = write(fd, &sizesn[i], 4);
	    assert(r==4);
	    off+=4;
	}
	{ int r=close(fd); assert(r==0); }
    }
    int fd = open(FNAME, O_RDONLY);  	assert(fd>=0);
    // Now read it all backward
    BREAD br = create_bread_from_fd_initialize_at(fd, off, 50);
    while (bread_has_more(br)) {
	assert(i>0);
	i--;
	int sizen;
	{ int r = bread_backwards(br, &sizen, 4); assert(r==4); }
	int sizeh=ntohl(sizen);
	assert(sizeh==sizes[i]);
	assert(0<=sizeh && sizeh<100);
	{
	    char rbuf[100];
	    int r = bread_backwards(br, rbuf,sizeh);
	    assert(r==sizeh);
	    assert(memcmp(rbuf, &buf[i][0], sizes[i])==0);
	}
    }
    assert(i==0);
    { int r=close_bread_without_closing_fd(br); assert(r==0); }
    { int r=close(fd); assert(r==0); }
    unlink(FNAME);
}

int main (int argc __attribute__((__unused__)), char *argv[] __attribute__((__unused__))) {
    int i;
    for (i=0; i<10; i++) test(i);
    return 0;
}
