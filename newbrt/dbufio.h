/* -*- mode: C; c-basic-offset: 4 -*- */
#ifndef TOKU_DBUFIO_H
#define TOKU_DBUFIO_H
#ident "$Id: queue.c 20104 2010-05-12 17:22:40Z bkuszmaul $"
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."

#include <toku_portability.h>
#include <toku_pthread.h>
#include "c_dialects.h"

C_BEGIN

/* Maintain a set of files for reading, with double buffering for the reads. */

/* A DBUFIO_FILESET is a set of files.  The files are indexed from 0 to N-1, where N is specified when the set is created (and the files are also provided when the set is creaed). */
/* An implementation would typically use a separate thread or asynchronous I/O to fetch ahead data for each file.  The system will typically fill two buffers of size M for each file.  One buffer is being read out of using dbuf_read(), and the other buffer is either empty (waiting on the asynchronous I/O to start), being filled in by the asynchronous I/O mechanism, or is waiting for the caller to read data from it. */
typedef struct dbufio_fileset *DBUFIO_FILESET; 

int create_dbufio_fileset (DBUFIO_FILESET *bfsp, int N, int fds[/*N*/], size_t bufsize);

int destroy_dbufio_fileset(DBUFIO_FILESET);

int dbufio_fileset_read (DBUFIO_FILESET bfs, int filenum, void *buf_v, size_t count, size_t *n_read);

int panic_dbufio_fileset(DBUFIO_FILESET, int error);

void dbufio_print(DBUFIO_FILESET);

C_END

#endif
