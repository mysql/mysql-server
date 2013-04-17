/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id$"
#ident "Copyright (c) 2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "dbufio.h"
#include "brttypes.h"
#include <toku_assert.h>
#include <errno.h>
#include <unistd.h>
#include "memory.h"
#include <string.h>

struct dbufio_file {
    // i/o thread owns these
    int fd;

    // consumers own these
    size_t offset_in_buf;
    toku_off_t  offset_in_file;

    // need the mutex to modify these
    struct dbufio_file *next;
    BOOL   second_buf_ready; // if true, the i/o thread is not touching anything.

    // consumers own [0], i/o thread owns [1], they are swapped by the consumer only when the condition mutex is held and second_buf_ready is true.
    char *buf[2];
    size_t n_in_buf[2];
    int    error_code[2]; // includes errno or eof. [0] is the error code associated with buf[0], [1] is the code for buf[1]

    BOOL io_done;

};


/* A dbufio_fileset  */
struct dbufio_fileset {
    // The mutex/condition variables protect 
    //   the singly-linked list of files that need I/O (head/tail in the fileset, and next in each file)
    //   in each file:
    //     the second_buf_ready boolean (which says the second buffer is full of data).
    //     the swapping of the buf[], n_in_buf[], and error_code[] values.
    toku_pthread_mutex_t mutex;
    toku_pthread_cond_t  cond;
    int N; // How many files.  This is constant once established.
    int n_not_done; // how many of the files require more I/O?  Owned by the i/o thread.
    struct dbufio_file *files;     // an array of length N.
    struct dbufio_file *head, *tail; // must have the mutex to fiddle with these.
    size_t bufsize; // the bufsize is the constant (the same for all buffers).

    BOOL panic;
    int  panic_errno;
    toku_pthread_t iothread;
};


static void enq (DBUFIO_FILESET bfs, struct dbufio_file *f) {
    if (bfs->tail==NULL) {
	bfs->head = f;
    } else {
	bfs->tail->next = f;
    }
    bfs->tail = f;
    f->next = NULL;
}

static void panic (DBUFIO_FILESET bfs, int r) {
    if (bfs->panic) return;
    // may need a cilk fake mutex here to convince the race detector that it's OK.
    bfs->panic_errno = r; // Don't really care about a race on this variable...  Writes to it are atomic, so at least one good panic reason will be stored.
    bfs->panic = TRUE;
    return;
}

static BOOL paniced (DBUFIO_FILESET bfs) {
    // may need a cilk fake mutex here to convince the race detector that it's OK.
    return bfs->panic;
}

static void* io_thread (void *v)
// The dbuf_thread does all the asynchronous I/O.
{
    DBUFIO_FILESET bfs = (DBUFIO_FILESET)v;
    {
	int r = toku_pthread_mutex_lock(&bfs->mutex);
	if (r!=0) { panic(bfs, r); return 0; }
    }
    //printf("%s:%d Locked\n", __FILE__, __LINE__);
    while (1) {

	if (paniced(bfs)) {
	    toku_pthread_mutex_unlock(&bfs->mutex); // ignore any error
	    return 0;
	}
	//printf("n_not_done=%d\n", bfs->n_not_done);
	if (bfs->n_not_done==0) {
	    // all done (meaning we stored EOF (or another error) in error_code[0] for the file.
	    //printf("unlocked\n");
	    int r = toku_pthread_mutex_unlock(&bfs->mutex);
	    if (r!=0) { panic(bfs, r); }
	    return 0;
	}

	struct dbufio_file *dbf = bfs->head;
	if (dbf==NULL) {
	    // No I/O needs to be done yet. 
	    // Wait until something happens that will wake us up.
	    int r = toku_pthread_cond_wait(&bfs->cond, &bfs->mutex);
	    if (r!=0) { panic(bfs, r); return 0; }
	    if (paniced(bfs)) {
		toku_pthread_mutex_unlock(&bfs->mutex); // ignore any error
		return 0;
	    }
	    // Have the lock so go around.
	} else {
	    // Some I/O needs to be done.
	    //printf("%s:%d Need I/O\n", __FILE__, __LINE__);
	    assert(dbf->second_buf_ready == FALSE);
	    assert(!dbf->io_done);
	    bfs->head = dbf->next;
	    if (bfs->head==NULL) bfs->tail=NULL;

	    // Unlock the mutex now that we have ownership of dbf to allow consumers to get the mutex and perform swaps.  They won't swap
	    // this buffer because second_buf_ready is false.
	    {
		int r = toku_pthread_mutex_unlock(&bfs->mutex);
		if (r!=0) { panic(bfs, r); return 0; }
	    }
	    //printf("%s:%d Doing read fd=%d\n", __FILE__, __LINE__, dbf->fd);
	    {
		ssize_t readcode = read(dbf->fd, dbf->buf[1], bfs->bufsize);
		//printf("%s:%d readcode=%ld\n", __FILE__, __LINE__, readcode);
		if (readcode==-1) {
		    // a real error.  Save the real error.
		    dbf->error_code[1] = errno;
		    dbf->n_in_buf[1] = 0;
		} else if (readcode==0) {
		    // End of file.  Save it.
		    dbf->error_code[1] = EOF;
		    dbf->n_in_buf[1] = 0;
		    dbf->io_done = TRUE;
		    
		} else {
		    dbf->error_code[1] = 0;
		    dbf->n_in_buf[1] = readcode;
		}

		//printf("%s:%d locking mutex again=%ld\n", __FILE__, __LINE__, readcode);
		{
		    int r = toku_pthread_mutex_lock(&bfs->mutex);
		    if (r!=0) { panic(bfs, r); return 0; }
		    if (paniced(bfs)) {
                        toku_pthread_mutex_unlock(&bfs->mutex); // ignore any error
                        return 0;
                    }
		}
		// Now that we have the mutex, we can decrement n_not_done (if applicable) and set second_buf_ready
		if (readcode<=0) {
		    bfs->n_not_done--;
		}
		//printf("%s:%d n_not_done=%d\n", __FILE__, __LINE__, bfs->n_not_done);
		dbf->second_buf_ready = TRUE;
		{
		    int r = toku_pthread_cond_broadcast(&bfs->cond);
		    if (r!=0) {
			panic(bfs, r);
			toku_pthread_mutex_unlock(&bfs->mutex); // ignore any error
			return 0;
		    }
		}
		//printf("%s:%d did broadcast=%d\n", __FILE__, __LINE__, bfs->n_not_done);
		// Still have the lock so go around the loop
	    }
	}
    }
}

int create_dbufio_fileset (DBUFIO_FILESET *bfsp, int N, int fds[/*N*/], size_t bufsize) {
    //printf("%s:%d here\n", __FILE__, __LINE__);
    int result = 0;
    DBUFIO_FILESET bfs=MALLOC(bfs);
    if (bfs==0) { result = errno; }
    BOOL mutex_inited = FALSE, cond_inited = FALSE;
    if (result==0) {
	MALLOC_N(N, bfs->files);
	if (bfs->files==NULL) { result = errno; }
	else {
	    for (int i=0; i<N; i++) {
		bfs->files[i].buf[0] = bfs->files[i].buf[1] = NULL;
	    }
	}
    }
    //printf("%s:%d here\n", __FILE__, __LINE__);
    if (result==0) {
	result = toku_pthread_mutex_init(&bfs->mutex, NULL);
	if (result==0) mutex_inited = TRUE;
    }
    if (result==0) {
	result = toku_pthread_cond_init(&bfs->cond, NULL);
	if (result==0) cond_inited = TRUE;
    }
    if (result==0) {
	bfs->N = N;
	bfs->n_not_done = N;
	bfs->head = bfs->tail = NULL;
	for (int i=0; i<N; i++) {
	    bfs->files[i].fd = fds[i];
	    bfs->files[i].offset_in_buf = 0;
	    bfs->files[i].offset_in_file = 0;
	    bfs->files[i].next = NULL;
	    bfs->files[i].second_buf_ready = FALSE;
	    for (int j=0; j<2; j++) {
		if (result==0) {
		    MALLOC_N(bufsize, bfs->files[i].buf[j]);
		    if (bfs->files[i].buf[j]==NULL) { result=errno; }
		}
		bfs->files[i].n_in_buf[j] = 0;
		bfs->files[i].error_code[j] = 0;
	    }
	    bfs->files[i].io_done = FALSE;
	    {
		ssize_t r = read(bfs->files[i].fd, bfs->files[i].buf[0], bufsize);
		if (r<0) {
		    result=errno;
		    break;
		} else if (r==0) {
		    // it's EOF
		    bfs->files[i].io_done = TRUE;
		    bfs->n_not_done--;
		    bfs->files[i].error_code[0] = EOF;
		} else {
		    bfs->files[i].n_in_buf[0] = r;
		    //printf("%s:%d enq [%d]\n", __FILE__, __LINE__, i);
		    enq(bfs, &bfs->files[i]);
		}
	    }
	}
	bfs->bufsize = bufsize;
	bfs->panic = FALSE;
	bfs->panic_errno = 0;
    }
    //printf("Creating IO thread\n");
    if (result==0) {
	result = toku_pthread_create(&bfs->iothread, NULL, io_thread, (void*)bfs);
    }
    if (result==0) { *bfsp = bfs; return 0; }
    // Now undo everything.
    // If we got here, there is no thread (either result was zero before the thread was created, or else the thread creation itself failed.
    if (bfs) {
	if (bfs->files) {
	    // the files were allocated, so we have to free all the bufs.
	    for (int i=0; i<N; i++) {
		for (int j=0; j<2; j++) {
		    if (bfs->files[i].buf[j])
			toku_free(bfs->files[i].buf[j]);
		    bfs->files[i].buf[j]=NULL;
		}
	    }
	    toku_free(bfs->files);
	    bfs->files=NULL;
	}
	if (cond_inited) {
	    toku_pthread_cond_destroy(&bfs->cond);  // don't check error status
	}
	if (mutex_inited) {
	    toku_pthread_mutex_destroy(&bfs->mutex); // don't check error status
	}
	toku_free(bfs);
    }
    return result;
}

int panic_dbufio_fileset(DBUFIO_FILESET bfs, int error) {
    int r;
    r = toku_pthread_mutex_lock(&bfs->mutex); assert(r==0);
    panic(bfs, error);
    r = toku_pthread_cond_broadcast(&bfs->cond); assert(r==0);
    r = toku_pthread_mutex_unlock(&bfs->mutex); assert(r==0);
    return 0;
}

int destroy_dbufio_fileset (DBUFIO_FILESET bfs) {
    int result = 0;
    {
	void *retval;
	int r = toku_pthread_join(bfs->iothread, &retval);
	assert(r==0);
	assert(retval==NULL);
    }
    {
	int r = toku_pthread_mutex_destroy(&bfs->mutex);
	if (result==0 && r!=0) result=r;
    }
    {
	int r = toku_pthread_cond_destroy(&bfs->cond);
	if (result==0 && r!=0) result=r;
    }
    if (bfs->files) {
	for (int i=0; i<bfs->N; i++) {
	    for (int j=0; j<2; j++) {
		//printf("%s:%d free([%d][%d]=%p\n", __FILE__, __LINE__, i,j, bfs->files[i].buf[j]);
		toku_free(bfs->files[i].buf[j]);
	    }
	}
	toku_free(bfs->files);
    }
    toku_free(bfs);
    return result;
}

int dbufio_fileset_read (DBUFIO_FILESET bfs, int filenum, void *buf_v, size_t count, size_t *n_read) {
    char *buf = (char*)buf_v;
    struct dbufio_file *dbf = &bfs->files[filenum];
    if (dbf->error_code[0]!=0) return dbf->error_code[0];
    if (dbf->offset_in_buf + count <= dbf->n_in_buf[0]) {
	// Enough data is present to do it all now
	memcpy(buf, dbf->buf[0]+dbf->offset_in_buf, count);
	dbf->offset_in_buf += count;
	dbf->offset_in_file += count;
	*n_read = count;
	return 0;
    } else if (dbf->n_in_buf[0] > dbf->offset_in_buf) {
	// There is something in buf[0] 
	size_t this_count = dbf->n_in_buf[0]-dbf->offset_in_buf;
	assert(dbf->offset_in_buf + this_count <= bfs->bufsize);
	memcpy(buf, dbf->buf[0]+dbf->offset_in_buf, this_count);
	dbf->offset_in_buf += this_count;
	dbf->offset_in_file += this_count;
	size_t sub_n_read;
	int r = dbufio_fileset_read(bfs, filenum, buf+this_count, count-this_count, &sub_n_read);
	if (r==0) {
	    *n_read = this_count + sub_n_read;
	    return 0;
	} else {
	    // The error code will have been saved.  We got some data so return that
	    *n_read = this_count;
	    return 0;
	}
    } else {
	// There is nothing in buf[0].  So we need to swap buffers
	{ int r = toku_pthread_mutex_lock(&bfs->mutex); assert(r==0);  }
	while (1) {
	    if (dbf->second_buf_ready) {
		dbf->n_in_buf[0] = dbf->n_in_buf[1];
		{
		    char *tmp = dbf->buf[0];
		    dbf->buf[0]      = dbf->buf[1];
		    dbf->buf[1]      = tmp;
		}
		dbf->error_code[0] = dbf->error_code[1];
		dbf->second_buf_ready = FALSE;
		dbf->offset_in_buf = 0;
		if (!dbf->io_done) {
		    // Don't enqueue it if the I/O is all done.
		    //printf("%s:%d enq [%ld]\n", __FILE__, __LINE__, dbf-&bfs->files[0]);
		    enq(bfs, dbf);
		}
		{ int r = toku_pthread_cond_broadcast(&bfs->cond); assert(r==0); }
		{ int r = toku_pthread_mutex_unlock(&bfs->mutex); assert(r==0); }
		if (dbf->error_code[0]==0) {
		    assert(dbf->n_in_buf[0]>0);
		    return dbufio_fileset_read(bfs, filenum, buf_v, count, n_read);
		} else {
		    *n_read = 0;
		    return dbf->error_code[0];
		}
	    } else {
		{ int r = toku_pthread_cond_wait(&bfs->cond, &bfs->mutex); assert(r==0); }
	    }
	}
	assert(0); // cannot get here.
    }
}
