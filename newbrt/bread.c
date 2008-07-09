/* Buffered read. */
#include <assert.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>

#include "bread.h"
#include "memory.h"

struct bread {
    off_t current_offset;   // The current offset to be read (in the file).  That offset includes anything that is unread in the buffer.
    int fd;
    size_t bufsize;
    char *buf;              // A buffer of size bufsize;
    int bufoff;             // The current offset buf.
};

BREAD create_bread_from_fd_initialize_at(int fd, off_t filesize, size_t bufsize) {
    BREAD MALLOC(result);
    result->current_offset=filesize;
    result->fd=fd;
    result->bufoff=0;
    result->bufsize=bufsize;
    MALLOC_N(bufsize, result->buf);
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
    while (nbytes > 0) {
	assert(br->current_offset >= (off_t)nbytes);
	// read whatever we can out of the buffer.
	{
	    size_t to_copy = ((size_t)br->bufoff >= nbytes) ? nbytes : (size_t)br->bufoff;
	    memcpy(buf+nbytes-to_copy, &br->buf[br->bufoff-to_copy], to_copy);
	    nbytes             -= to_copy;
	    br->current_offset -= to_copy;
	    result             += to_copy;
	    br->bufoff         -= to_copy;
	}
	if (nbytes>0) { 
	    assert(br->bufoff==0);
	    size_t to_read = ((size_t)br->current_offset >= br->bufsize) ? br->bufsize : (size_t)br->current_offset;
	    ssize_t r = pread(br->fd, br->buf, to_read, br->current_offset-to_read);
	    assert(r==(ssize_t)to_read);
	    br->bufoff = to_read;
	}
    }
    return result;
}

int bread_has_more(BREAD br) {
    return br->current_offset>0;
}
