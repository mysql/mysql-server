/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuFT, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2010-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <config.h>

#include <errno.h>
#include <string.h>
#include <unistd.h>

#include "portability/toku_assert.h"
#include "portability/memory.h"

#include "ft/ft-internal.h"
#include "ft/serialize/ft_node-serialize.h"
#include "loader/dbufio.h"
#include "loader/loader-internal.h"

struct dbufio_file {
    // i/o thread owns these
    int fd;

    // consumers own these
    size_t offset_in_buf;
    toku_off_t  offset_in_uncompressed_file;

    // need the mutex to modify these
    struct dbufio_file *next;
    bool   second_buf_ready; // if true, the i/o thread is not touching anything.

    // consumers own [0], i/o thread owns [1], they are swapped by the consumer only when the condition mutex is held and second_buf_ready is true.
    char *buf[2];
    size_t n_in_buf[2];
    int    error_code[2]; // includes errno or eof. [0] is the error code associated with buf[0], [1] is the code for buf[1]

    bool io_done;
};


/* A dbufio_fileset  */
struct dbufio_fileset {
    // The mutex/condition variables protect 
    //   the singly-linked list of files that need I/O (head/tail in the fileset, and next in each file)
    //   in each file:
    //     the second_buf_ready boolean (which says the second buffer is full of data).
    //     the swapping of the buf[], n_in_buf[], and error_code[] values.
    toku_mutex_t mutex;
    toku_cond_t  cond;
    int N; // How many files.  This is constant once established.
    int n_not_done; // how many of the files require more I/O?  Owned by the i/o thread.
    struct dbufio_file *files;     // an array of length N.
    struct dbufio_file *head, *tail; // must have the mutex to fiddle with these.
    size_t bufsize; // the bufsize is the constant (the same for all buffers).

    bool panic;
    bool compressed;
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
    bfs->panic_errno = r; // Don't really care about a race on this variable...  Writes to it are atomic, so at least one good panic reason will be stored.
    bfs->panic = true;
    return;
}

static bool paniced (DBUFIO_FILESET bfs) {
    return bfs->panic;
}

static ssize_t dbf_read_some_compressed(struct dbufio_file *dbf, char *buf, size_t bufsize) {
    ssize_t ret;
    invariant(bufsize >= MAX_UNCOMPRESSED_BUF);
    unsigned char *raw_block = NULL;

    // deserialize the sub block header

    // total_size
    // num_sub_blocks
    // compressed_size,uncompressed_size,xsum (repeated num_sub_blocks times)
    ssize_t readcode;
    const uint32_t header_size = sizeof(uint32_t);
    char header[header_size];

    readcode = toku_os_read(dbf->fd, &header, header_size);
    if (readcode < 0) {
        ret = -1;
        goto exit;
    }
    if (readcode == 0) {
        ret = 0;
        goto exit;
    }
    if (readcode < header_size) {
        errno = TOKUDB_NO_DATA;
        ret = -1;
        goto exit;
    }
    uint32_t total_size;
    {
        uint32_t *p = (uint32_t *) &header[0];
        total_size = toku_dtoh32(p[0]);
    }
    if (total_size == 0 || total_size > (1<<30)) {
        errno = toku_db_badformat(); 
        ret = -1;
        goto exit;
    }

    //Cannot use XMALLOC
    MALLOC_N(total_size, raw_block);
    if (raw_block == nullptr) {
        errno = ENOMEM;
        ret = -1;
        goto exit;
    }
    readcode = toku_os_read(dbf->fd, raw_block, total_size);
    if (readcode < 0) {
        ret = -1;
        goto exit;
    }
    if (readcode < total_size) {
        errno = TOKUDB_NO_DATA;
        ret = -1;
        goto exit;
    }

    struct sub_block sub_block[max_sub_blocks];
    uint32_t *sub_block_header;
    sub_block_header = (uint32_t *) &raw_block[0];
    int32_t n_sub_blocks;
    n_sub_blocks = toku_dtoh32(sub_block_header[0]);
    sub_block_header++;
    size_t size_subblock_header;
    size_subblock_header = sub_block_header_size(n_sub_blocks);
    if (n_sub_blocks == 0 || n_sub_blocks > max_sub_blocks || size_subblock_header > total_size) {
        errno = toku_db_badformat(); 
        ret = -1;
        goto exit;
    }
    for (int i = 0; i < n_sub_blocks; i++) {
        sub_block_init(&sub_block[i]);
        sub_block[i].compressed_size = toku_dtoh32(sub_block_header[0]);
        sub_block[i].uncompressed_size = toku_dtoh32(sub_block_header[1]);
        sub_block[i].xsum = toku_dtoh32(sub_block_header[2]);
        sub_block_header += 3;
    }

    // verify sub block sizes
    size_t total_compressed_size;
    total_compressed_size = 0;
    for (int i = 0; i < n_sub_blocks; i++) {
        uint32_t compressed_size = sub_block[i].compressed_size;
        if (compressed_size<=0   || compressed_size>(1<<30)) { 
            errno = toku_db_badformat(); 
            ret = -1;
            goto exit;
        }

        uint32_t uncompressed_size = sub_block[i].uncompressed_size;
        if (uncompressed_size<=0 || uncompressed_size>(1<<30)) { 
            errno = toku_db_badformat(); 
            ret = -1;
            goto exit;
        }
        total_compressed_size += compressed_size;
    }
    if (total_size != total_compressed_size + size_subblock_header) {
        errno = toku_db_badformat(); 
        ret = -1;
        goto exit;
    }

    // sum up the uncompressed size of the sub blocks
    size_t uncompressed_size;
    uncompressed_size = get_sum_uncompressed_size(n_sub_blocks, sub_block);
    if (uncompressed_size > bufsize || uncompressed_size > MAX_UNCOMPRESSED_BUF) {
        errno = toku_db_badformat(); 
        ret = -1;
        goto exit;
    }

    unsigned char *uncompressed_data;
    uncompressed_data = (unsigned char *)buf;

    // point at the start of the compressed data (past the node header, the sub block header, and the header checksum)
    unsigned char *compressed_data;
    compressed_data = raw_block + size_subblock_header;

    // decompress all the compressed sub blocks into the uncompressed buffer
    {
        int r;
        r = decompress_all_sub_blocks(n_sub_blocks, sub_block, compressed_data, uncompressed_data, get_num_cores(), get_ft_pool());
        if (r != 0) {
            fprintf(stderr, "%s:%d loader failed %d at %p size %" PRIu32"\n", __FUNCTION__, __LINE__, r, raw_block, total_size);
            dump_bad_block(raw_block, total_size);
            errno = r;
            ret = -1;
            goto exit;
        }
    }
    ret = uncompressed_size;
exit:
    if (raw_block) {
        toku_free(raw_block);
    }
    return ret;
}

static ssize_t dbf_read_compressed(struct dbufio_file *dbf, char *buf, size_t bufsize) {
    invariant(bufsize >= MAX_UNCOMPRESSED_BUF);
    size_t count = 0;

    while (count + MAX_UNCOMPRESSED_BUF <= bufsize) {
        ssize_t readcode = dbf_read_some_compressed(dbf, buf + count, bufsize - count);
        if (readcode < 0) {
            return readcode;
        }
        count += readcode;
        if (readcode == 0) {
            break;
        }
    }
    return count;
}

static void* io_thread (void *v)
// The dbuf_thread does all the asynchronous I/O.
{
    DBUFIO_FILESET bfs = (DBUFIO_FILESET)v;
    toku_mutex_lock(&bfs->mutex);
    //printf("%s:%d Locked\n", __FILE__, __LINE__);
    while (1) {

	if (paniced(bfs)) {
	    toku_mutex_unlock(&bfs->mutex); // ignore any error
	    return 0;
	}
	//printf("n_not_done=%d\n", bfs->n_not_done);
	if (bfs->n_not_done==0) {
	    // all done (meaning we stored EOF (or another error) in error_code[0] for the file.
	    //printf("unlocked\n");
	    toku_mutex_unlock(&bfs->mutex);
	    return 0;
	}

	struct dbufio_file *dbf = bfs->head;
	if (dbf==NULL) {
	    // No I/O needs to be done yet. 
	    // Wait until something happens that will wake us up.
	    toku_cond_wait(&bfs->cond, &bfs->mutex);
	    if (paniced(bfs)) {
		toku_mutex_unlock(&bfs->mutex); // ignore any error
		return 0;
	    }
	    // Have the lock so go around.
	} else {
	    // Some I/O needs to be done.
	    //printf("%s:%d Need I/O\n", __FILE__, __LINE__);
	    assert(dbf->second_buf_ready == false);
	    assert(!dbf->io_done);
	    bfs->head = dbf->next;
	    if (bfs->head==NULL) bfs->tail=NULL;

	    // Unlock the mutex now that we have ownership of dbf to allow consumers to get the mutex and perform swaps.  They won't swap
	    // this buffer because second_buf_ready is false.
            toku_mutex_unlock(&bfs->mutex);
	    //printf("%s:%d Doing read fd=%d\n", __FILE__, __LINE__, dbf->fd);
	    {
		ssize_t readcode;
                if (bfs->compressed) {
                    readcode = dbf_read_compressed(dbf, dbf->buf[1], bfs->bufsize);
                }
                else {
                    readcode = toku_os_read(dbf->fd, dbf->buf[1], bfs->bufsize);
                }
		//printf("%s:%d readcode=%ld\n", __FILE__, __LINE__, readcode);
		if (readcode==-1) {
		    // a real error.  Save the real error.
                    int the_errno = get_error_errno();
                    fprintf(stderr, "%s:%d dbf=%p fd=%d errno=%d\n", __FILE__, __LINE__, dbf, dbf->fd, the_errno);
		    dbf->error_code[1] = the_errno;
		    dbf->n_in_buf[1] = 0;
		} else if (readcode==0) {
		    // End of file.  Save it.
		    dbf->error_code[1] = EOF;
		    dbf->n_in_buf[1] = 0;
		    dbf->io_done = true;
		    
		} else {
		    dbf->error_code[1] = 0;
		    dbf->n_in_buf[1] = readcode;
		}

		//printf("%s:%d locking mutex again=%ld\n", __FILE__, __LINE__, readcode);
		{
		    toku_mutex_lock(&bfs->mutex);
		    if (paniced(bfs)) {
                        toku_mutex_unlock(&bfs->mutex); // ignore any error
                        return 0;
                    }
		}
		// Now that we have the mutex, we can decrement n_not_done (if applicable) and set second_buf_ready
		if (readcode<=0) {
		    bfs->n_not_done--;
		}
		//printf("%s:%d n_not_done=%d\n", __FILE__, __LINE__, bfs->n_not_done);
		dbf->second_buf_ready = true;
                toku_cond_broadcast(&bfs->cond);
		//printf("%s:%d did broadcast=%d\n", __FILE__, __LINE__, bfs->n_not_done);
		// Still have the lock so go around the loop
	    }
	}
    }
}

int create_dbufio_fileset (DBUFIO_FILESET *bfsp, int N, int fds[/*N*/], size_t bufsize, bool compressed) {
    //printf("%s:%d here\n", __FILE__, __LINE__);
    int result = 0;
    DBUFIO_FILESET CALLOC(bfs);
    if (bfs==0) { result = get_error_errno(); }

    bfs->compressed = compressed;

    bool mutex_inited = false, cond_inited = false;
    if (result==0) {
	CALLOC_N(N, bfs->files);
	if (bfs->files==NULL) { result = get_error_errno(); }
	else {
	    for (int i=0; i<N; i++) {
		bfs->files[i].buf[0] = bfs->files[i].buf[1] = NULL;
	    }
	}
    }
    //printf("%s:%d here\n", __FILE__, __LINE__);
    if (result==0) {
	toku_mutex_init(&bfs->mutex, NULL);
	mutex_inited = true;
    }
    if (result==0) {
	toku_cond_init(&bfs->cond, NULL);
	cond_inited = true;
    }
    if (result==0) {
	bfs->N = N;
	bfs->n_not_done = N;
	bfs->head = bfs->tail = NULL;
	for (int i=0; i<N; i++) {
	    bfs->files[i].fd = fds[i];
	    bfs->files[i].offset_in_buf = 0;
	    bfs->files[i].offset_in_uncompressed_file = 0;
	    bfs->files[i].next = NULL;
	    bfs->files[i].second_buf_ready = false;
	    for (int j=0; j<2; j++) {
		if (result==0) {
		    MALLOC_N(bufsize, bfs->files[i].buf[j]);
		    if (bfs->files[i].buf[j]==NULL) { result=get_error_errno(); }
		}
		bfs->files[i].n_in_buf[j] = 0;
		bfs->files[i].error_code[j] = 0;
	    }
	    bfs->files[i].io_done = false;
            ssize_t r;
            if (bfs->compressed) {
                r = dbf_read_compressed(&bfs->files[i], bfs->files[i].buf[0], bufsize);
            } else {
		r = toku_os_read(bfs->files[i].fd, bfs->files[i].buf[0], bufsize);
            }
            {
		if (r<0) {
		    result=get_error_errno();
		    break;
                } else if (r==0) {
		    // it's EOF
		    bfs->files[i].io_done = true;
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
	bfs->panic = false;
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
	    toku_cond_destroy(&bfs->cond);  // don't check error status
	}
	if (mutex_inited) {
	    toku_mutex_destroy(&bfs->mutex); // don't check error status
	}
	toku_free(bfs);
    }
    return result;
}

int panic_dbufio_fileset(DBUFIO_FILESET bfs, int error) {
    toku_mutex_lock(&bfs->mutex);
    panic(bfs, error);
    toku_cond_broadcast(&bfs->cond);
    toku_mutex_unlock(&bfs->mutex);
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
	toku_mutex_destroy(&bfs->mutex);
    }
    {
	toku_cond_destroy(&bfs->cond);
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
	dbf->offset_in_uncompressed_file += count;
	*n_read = count;
	return 0;
    } else if (dbf->n_in_buf[0] > dbf->offset_in_buf) {
	// There is something in buf[0] 
	size_t this_count = dbf->n_in_buf[0]-dbf->offset_in_buf;
	assert(dbf->offset_in_buf + this_count <= bfs->bufsize);
	memcpy(buf, dbf->buf[0]+dbf->offset_in_buf, this_count);
	dbf->offset_in_buf += this_count;
	dbf->offset_in_uncompressed_file += this_count;
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
	toku_mutex_lock(&bfs->mutex);
	while (1) {
	    if (dbf->second_buf_ready) {
		dbf->n_in_buf[0] = dbf->n_in_buf[1];
		{
		    char *tmp = dbf->buf[0];
		    dbf->buf[0]      = dbf->buf[1];
		    dbf->buf[1]      = tmp;
		}
		dbf->error_code[0] = dbf->error_code[1];
		dbf->second_buf_ready = false;
		dbf->offset_in_buf = 0;
		if (!dbf->io_done) {
		    // Don't enqueue it if the I/O is all done.
		    //printf("%s:%d enq [%ld]\n", __FILE__, __LINE__, dbf-&bfs->files[0]);
		    enq(bfs, dbf);
		}
		toku_cond_broadcast(&bfs->cond);
		toku_mutex_unlock(&bfs->mutex);
		if (dbf->error_code[0]==0) {
		    assert(dbf->n_in_buf[0]>0);
		    return dbufio_fileset_read(bfs, filenum, buf_v, count, n_read);
		} else {
		    *n_read = 0;
		    return dbf->error_code[0];
		}
	    } else {
		toku_cond_wait(&bfs->cond, &bfs->mutex);
	    }
	}
	assert(0); // cannot get here.
    }
}

void
dbufio_print(DBUFIO_FILESET bfs) {
    fprintf(stderr, "%s:%d bfs=%p", __FILE__, __LINE__, bfs);
    if (bfs->panic)
        fprintf(stderr, " panic=%d", bfs->panic_errno);
    fprintf(stderr, " N=%d %d %" PRIuMAX, bfs->N, bfs->n_not_done, bfs->bufsize);
    for (int i = 0; i < bfs->N; i++) {
        struct dbufio_file *dbf = &bfs->files[i];
        if (dbf->error_code[0] || dbf->error_code[1])
            fprintf(stderr, " %d=[%d,%d]", i, dbf->error_code[0], dbf->error_code[1]);
    }
    fprintf(stderr, "\n");
                
}
