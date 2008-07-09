#ifndef BREAD_H
#define BREAD_H

// A BREAD reads a file backwards using buffered I/O.  BREAD stands for Buffered Read or Backwards Read.
// Conceivably, we could read forward too.
// The buffered I/O is buffered using a large buffer (e.g., something like a megabyte).
// If not for the large-buffer requirement, we could have used a FILE.

#include <sys/types.h>
typedef struct bread *BREAD;

BREAD create_bread_from_fd_initialize_at(int fd, off_t filesize, size_t bufsize);
// Effect:  Given a file descriptor, fd, that points at a file of size filesize, create a BREAD.
// Requires: The filesize must be correct.  The fd must be an open fd.

int close_bread_without_closing_fd(BREAD);
// Effect: Close the BREAD, but don't close the underlying fd.

ssize_t bread_backwards(BREAD, void *buf, size_t nbytes);
// Read nbytes into buf, reading backwards.  

int bread_has_more(BREAD);
// Is there more to read?

#endif
