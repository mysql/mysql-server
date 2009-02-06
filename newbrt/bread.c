/* Buffered read. */

#include "includes.h"

struct bread {
    int64_t fileoff;        // The byte before this offset is the next byte we will read (since we are reading backward)
    int fd;
    int bufoff;             // The current offset in the buf.   The next byte we will read is buf[bufoff-1] (assuming that bufoff>0).
    char *buf;              // A buffer with at least bufoff bytes in it.
};

BREAD create_bread_from_fd_initialize_at(int fd) {
    BREAD XMALLOC(result);
    int r = toku_os_get_file_size(fd, &result->fileoff); 
    assert(r==0);
    result->fd=fd;
    result->bufoff=0;
    result->buf = 0;
    return result;
}

int close_bread_without_closing_fd(BREAD br) {
    toku_free(br->buf);
    toku_free(br);
    return 0;
}


ssize_t bread_backwards(BREAD br, void *vbuf, size_t nbytes) {
    char *buf=vbuf;
    ssize_t result=0;
    const int i4 = sizeof(u_int32_t);
    while (nbytes > 0) {
	// read whatever we can out of the buffer.
	if (br->bufoff>0) {
	    size_t to_copy = ((size_t)br->bufoff >= nbytes) ? nbytes : (size_t)br->bufoff;
	    memcpy(buf+nbytes-to_copy, &br->buf[br->bufoff-to_copy], to_copy);
	    nbytes             -= to_copy;
	    result             += to_copy;
	    br->bufoff         -= to_copy;
	}
	if (nbytes>0) {
	    assert(br->bufoff==0);
	    u_int32_t compressed_length_n, uncompressed_length_n;
	    assert(br->fileoff>=i4); // there better be the three lengths plus the compressed data.
	    { ssize_t r = pread(br->fd, &compressed_length_n,   i4, br->fileoff-  i4); assert(r==i4); }
	    u_int32_t compressed_length = ntohl(compressed_length_n);
	    assert(br->fileoff >= compressed_length + 3*i4);
	    { ssize_t r = pread(br->fd, &uncompressed_length_n, i4, br->fileoff-2*i4); assert(r==i4); }
	    u_int32_t uncompressed_length = ntohl(uncompressed_length_n);
	    char *XMALLOC_N(compressed_length, zbuf);
	    {
		ssize_t r = pread(br->fd, zbuf,               compressed_length, br->fileoff- compressed_length -2*i4);
		assert(r==(ssize_t)compressed_length);
	    }
	    {
		u_int32_t compressed_length_n_again;
		ssize_t r = pread(br->fd, &compressed_length_n_again,   i4, br->fileoff-compressed_length-3*i4); assert(r==i4);
		assert(compressed_length_n_again == compressed_length_n);
	    }
	    uLongf destlen = uncompressed_length;
	    XREALLOC_N(uncompressed_length, br->buf);
	    uncompress((Bytef*)br->buf, &destlen, (Bytef*)zbuf, compressed_length);
	    assert(destlen==uncompressed_length);
	    toku_free(zbuf);
	    
	    br->bufoff = uncompressed_length;
	    br->fileoff -= (compressed_length + 3*i4);
	}
    }
    return result;
}

int bread_has_more(BREAD br) {
    return (br->fileoff>0) || (br->bufoff>0);
}
