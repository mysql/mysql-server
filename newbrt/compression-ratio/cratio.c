/* Measure the extent to which we can compress a file.
 * Works on version 8. */

#define _XOPEN_SOURCE 500
#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <zlib.h>


off_t fd_size (int fd) {
    struct stat buf;
    int r = fstat(fd, &buf);
    assert(r==0);
    return buf.st_size;
}

#define NSIZE (1<<20)
unsigned char fbuf[NSIZE];
unsigned char cbuf[NSIZE+500];

void
measure_header (int fd, off_t off, // read header from this offset
		off_t *usize,      // size uncompressed (but not including any padding)
		off_t *csize)      // compressed size
{
    int r;
    r=pread(fd, fbuf, 12, off);
    assert(r==12);
    assert(memcmp(fbuf,"tokudata",8)==0);
    int bsize = ntohl(*(u_int32_t*)(fbuf+8));
    //printf("Bsize=%d\n", bsize);
    (*usize)+=bsize;
    assert(bsize<=NSIZE);
    r=pread(fd, fbuf, bsize, off);
    assert(r==bsize);
    uLongf destLen=sizeof(cbuf);
    r=compress2(cbuf, &destLen,
		fbuf+20, bsize-=20, // skip magic nodesize and version
		1);
    assert(r==Z_OK);
    destLen+=16; // account for the size and magic and version
    //printf("Csize=%ld\n", destLen);
    (*csize)+=destLen;
}

void
measure_node (int fd, off_t off, // read header from this offset
	      off_t *usize,      // size uncompressed (but not including any padding)
	      off_t *csize)      // compressed size
{
    int r;
    r=pread(fd, fbuf, 24, off);
    assert(r==24);
    //printf("fbuf[0..7]=%c%c%c%c%c%c%c%c\n", fbuf[0], fbuf[1], fbuf[2], fbuf[3], fbuf[4], fbuf[5], fbuf[6], fbuf[7]);
    assert(memcmp(fbuf,"tokuleaf",8)==0 || memcmp(fbuf, "tokunode", 8)==0);
    assert(8==ntohl(*(u_int32_t*)(fbuf+8))); // check file version
    int bsize = ntohl(*(u_int32_t*)(fbuf+20));
    //printf("Bsize=%d\n", bsize);
    (*usize)+=bsize;

    assert(bsize<=NSIZE);
    r=pread(fd, fbuf, bsize, off);
    assert(r==bsize);
    uLongf destLen=sizeof(cbuf);
    r=compress2(cbuf, &destLen,
		fbuf+28, bsize-=28, // skip constant header stuff
		1);
    destLen += 24; // add in magic (8), version(4), lsn (8), and size (4).  Actually lsn will be compressed, but ignore that for now.
    assert(r==Z_OK);
    //printf("Csize=%ld\n", destLen);
    (*csize)+=destLen;

}



/* The single argument is the filename to measure. */
int main (int argc, const char *argv[]) {
    assert(argc==2);
    const char *fname=argv[1];
    int fd = open(fname, O_RDONLY);
    assert(fd>=0);
    off_t fsize = fd_size(fd);
    printf("fsize (uncompressed with   padding)=%lld\n", (long long)fsize);

    off_t usize=0, csize=0;
    measure_header(fd, 0, &usize, &csize);

    off_t i;
    for (i=NSIZE; i+24<fsize; i+=NSIZE) {
	measure_node(fd, i, &usize, &csize);
    }

    printf("usize (uncompressed with no padding)=%10lld  (ratio=%5.2f)\n", (long long)usize, (double)fsize/(double)usize);
    printf("csize (compressed)                  =%10lld  (ratio=%5.2f)\n", (long long)csize, (double)fsize/(double)csize);

    close(fd);
    return 0;
}
