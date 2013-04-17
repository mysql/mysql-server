#ifndef BREAD_H
#define BREAD_H
#ident "$Id$"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

// A BREAD reads a file backwards using buffered I/O.  BREAD stands for Buffered Read or Backwards Read.
// Conceivably, we could read forward too.
// The buffered I/O is buffered using a large buffer (e.g., something like a megabyte).
// Furthermore, data is compressed into blocks.  Each block is a 4-byte compressed length (in network order), followed by compressed data, followed by a 4-byte uncompressed-length (in network order), followed by  a 4-byte compressed length
// The compressed-length appears twice so that the file can be read backward or forward.
// If not for the large-buffer requirement, as well as compression, as well as reading backward, we could have used a FILE.

#include <sys/types.h>
typedef struct bread *BREAD;

BREAD create_bread_from_fd_initialize_at(int fd);
// Effect:  Given a file descriptor, fd, create a BREAD.
// Requires: The fd must be an open fd.

int close_bread_without_closing_fd(BREAD);
// Effect: Close the BREAD, but don't close the underlying fd.

ssize_t bread_backwards(BREAD, void *buf, size_t nbytes);
// Read nbytes into buf, reading backwards.

int bread_has_more(BREAD);
// Is there more to read?

#endif
