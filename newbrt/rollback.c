/* -*- mode: C; c-basic-offset: 4 -*- */
#ident "$Id: rollback.c 12375 2009-05-28 14:14:47Z yfogel $"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "includes.h"

static void note_txn_closing (TOKUTXN txn);

int toku_commit_rollback_item (TOKUTXN txn, struct roll_entry *item, YIELDF yield, void*yieldv) {
    int r=0;
    rolltype_dispatch_assign(item, toku_commit_, r, txn, yield, yieldv);
    return r;
}

int toku_abort_rollback_item (TOKUTXN txn, struct roll_entry *item, YIELDF yield, void*yieldv) {
    int r=0;
    rolltype_dispatch_assign(item, toku_rollback_, r, txn, yield, yieldv);
    if (r!=0) return r;
    return 0;
}

void toku_rollback_txn_close (TOKUTXN txn) {
    memarena_close(&txn->rollentry_arena);
    if (txn->rollentry_filename!=0) {
	int r = close(txn->rollentry_fd);
	assert(r==0);
	r = unlink(txn->rollentry_filename);
	assert(r==0);
	toku_free(txn->rollentry_filename);
    }

    list_remove(&txn->live_txns_link);
    note_txn_closing(txn);
    toku_free(txn);
    return;
}

// wbuf points into logbytes
int toku_logger_finish (TOKULOGGER logger, struct logbytes *logbytes, struct wbuf *wbuf, int do_fsync) {
    if (logger->is_panicked) return EINVAL;
    u_int32_t checksum = x1764_finish(&wbuf->checksum);
    wbuf_int(wbuf, checksum);
    wbuf_int(wbuf, 4+wbuf->ndone);
    logbytes->nbytes=wbuf->ndone;
    return toku_logger_log_bytes(logger, logbytes, do_fsync);
}

void* toku_malloc_in_rollback(TOKUTXN txn, size_t size) {
    return malloc_in_memarena(txn->rollentry_arena, size);
}

void *toku_memdup_in_rollback(TOKUTXN txn, const void *v, size_t len) {
    void *r=toku_malloc_in_rollback(txn, len);
    memcpy(r,v,len);
    return r;
}

char *toku_strdup_in_rollback(TOKUTXN txn, const char *s) {
    return toku_memdup_in_rollback(txn, s, strlen(s)+1);
}

static int note_brt_used_in_parent_txn(OMTVALUE brtv, u_int32_t UU(index), void*parentv) {
    return toku_txn_note_brt(parentv, brtv);
}

int toku_rollback_commit(TOKUTXN txn, YIELDF yield, void*yieldv) {
    int r=0;
    if (txn->parent!=0) {
        // First we must put a rollinclude entry into the parent if we have a rollentry file.
        if (txn->rollentry_filename) {
            int len = strlen(txn->rollentry_filename);
            // Don't have to strdup the rollentry_filename because
            // we take ownership of it.
            BYTESTRING fname = {len, toku_strdup_in_rollback(txn, txn->rollentry_filename)};
            r = toku_logger_save_rollback_rollinclude(txn->parent, fname);
            if (r!=0) return r;
            r = close(txn->rollentry_fd);
            if (r!=0) {
                // We have to do the unlink ourselves, and then
                // set txn->rollentry_filename=0 so that the cleanup
                // won't try to close the fd again.
                unlink(txn->rollentry_filename);
                toku_free(txn->rollentry_filename);
                txn->rollentry_filename = 0;
                return r;
            }
            // Stop the cleanup from closing and unlinking the file.
            toku_free(txn->rollentry_filename);
            txn->rollentry_filename = 0;
        }
        // Append the list to the front of the parent.
        if (txn->oldest_logentry) {
            // There are some entries, so link them in.
            txn->oldest_logentry->prev = txn->parent->newest_logentry;
            if (txn->parent->newest_logentry) {
                txn->parent->newest_logentry->next = txn->oldest_logentry;
            } else {
                txn->parent->oldest_logentry = txn->oldest_logentry;
            }
            txn->parent->newest_logentry = txn->newest_logentry;
            txn->parent->rollentry_resident_bytecount += txn->rollentry_resident_bytecount;
            txn->parent->rollentry_raw_count          += txn->rollentry_raw_count;
            txn->rollentry_resident_bytecount = 0;
        }
        if (txn->parent->oldest_logentry==0) {
            txn->parent->oldest_logentry = txn->oldest_logentry;
        }
        txn->newest_logentry = txn->oldest_logentry = 0;
        // Put all the memarena data into the parent.
        if (memarena_total_size_in_use(txn->rollentry_arena) > 0) {
            // If there are no bytes to move, then just leave things alone, and let the memory be reclaimed on txn is closed.
            memarena_move_buffers(txn->parent->rollentry_arena, txn->rollentry_arena);
        }

        // Note the open brts, the omts must be merged
        r = toku_omt_iterate(txn->open_brts, note_brt_used_in_parent_txn, txn->parent);
        assert(r==0);

        r = toku_maybe_spill_rollbacks(txn->parent);
        assert(r==0);

    } else {
        // do the commit calls and free everything
        // we do the commit calls in reverse order too.
        {
            struct roll_entry *item;
            //printf("%s:%d abort\n", __FILE__, __LINE__);
            int count=0;
            while ((item=txn->newest_logentry)) {
                txn->newest_logentry = item->prev;
                r = toku_commit_rollback_item(txn, item, yield, yieldv);
                if (r!=0) return r;
                count++;
                if (count%2 == 0) yield(NULL, yieldv);
            }
        }

        // Read stuff out of the file and execute it.
        if (txn->rollentry_filename) {
            r = toku_commit_fileentries(txn->rollentry_fd, txn, yield, yieldv);
        }
    }
    return r;
}

int toku_rollback_abort(TOKUTXN txn, YIELDF yield, void*yieldv) {
    struct roll_entry *item;
    int count=0;
    int r=0;
    while ((item=txn->newest_logentry)) {
        txn->newest_logentry = item->prev;
        r = toku_abort_rollback_item(txn, item, yield, yieldv);
        if (r!=0) 
            return r;
        count++;
        if (count%2 == 0) yield(NULL, yieldv);
    }
    list_remove(&txn->live_txns_link);
    // Read stuff out of the file and roll it back.
    if (txn->rollentry_filename) {
        r = toku_rollback_fileentries(txn->rollentry_fd, txn, yield, yieldv);
        assert(r==0);
    }
    return 0;
}
                         
// Write something out.  Keep trying even if partial writes occur.
// On error: Return negative with errno set.
// On success return nbytes.

// NOTE : duplicated from logger.c - FIX THIS!!!
static int write_it (int fd, const void *bufv, int nbytes) {
    int org_nbytes=nbytes;
    const char *buf=bufv;
    while (nbytes>0) {
	int r = write(fd, buf, nbytes);
	if (r<0 || errno!=EAGAIN) return r;
	buf+=r;
	nbytes-=r;
    }
    return org_nbytes;
}

int toku_maybe_spill_rollbacks (TOKUTXN txn) {
    // Previously:
    // if (txn->rollentry_resident_bytecount>txn->logger->write_block_size) {
    // But now we use t
    if (memarena_total_memory_size(txn->rollentry_arena) > txn->logger->write_block_size) {
	struct roll_entry *item;
	ssize_t bufsize = txn->rollentry_resident_bytecount;
	char *MALLOC_N(bufsize, buf);
	if (bufsize==0) return errno;
	struct wbuf w;
	wbuf_init(&w, buf, bufsize);
	while ((item=txn->oldest_logentry)) {
	    assert(item->prev==0);
	    u_int32_t rollback_fsize = toku_logger_rollback_fsize(item);
	    txn->rollentry_resident_bytecount -= rollback_fsize;
	    txn->oldest_logentry = item->next;
	    if (item->next) { item->next->prev=0; }
	    toku_logger_rollback_wbufwrite(&w, item);
	}
	assert(txn->rollentry_resident_bytecount==0);
	assert((ssize_t)w.ndone==bufsize);
	txn->oldest_logentry = txn->newest_logentry = 0;
	if (txn->rollentry_fd<0) {
	    const char filenamepart[] = "/__rolltmp.";
	    int fnamelen = strlen(txn->logger->directory)+sizeof(filenamepart)+16;
	    assert(txn->rollentry_filename==0);
	    txn->rollentry_filename = toku_malloc(fnamelen);
	    if (txn->rollentry_filename==0) return errno;
	    snprintf(txn->rollentry_filename, fnamelen, "%s%s%.16"PRIx64, txn->logger->directory, filenamepart, txn->txnid64);
	    txn->rollentry_fd = open(txn->rollentry_filename, O_CREAT+O_RDWR+O_EXCL+O_BINARY, 0600);
	    if (txn->rollentry_fd==-1) return errno;
	}
	uLongf compressed_len = compressBound(w.ndone);
	char *MALLOC_N(compressed_len, compressed_buf);
	{
	    int r = compress2((Bytef*)compressed_buf, &compressed_len,
			      (Bytef*)buf,            w.ndone,
			      1);
	    assert(r==Z_OK);
	}
	{
	    u_int32_t v = toku_htod32(compressed_len);
	    ssize_t r = write_it(txn->rollentry_fd, &v, sizeof(v)); assert(r==sizeof(v));
	}
	{
	    ssize_t r = write_it(txn->rollentry_fd, compressed_buf, compressed_len);
	    if (r<0) return r;
	    assert(r==(ssize_t)compressed_len);
	}
	{
	    u_int32_t v = toku_htod32(w.ndone);
	    ssize_t r = write_it(txn->rollentry_fd, &v, sizeof(v)); assert(r==sizeof(v));
	}
	{
	    u_int32_t v = toku_htod32(compressed_len);
	    ssize_t r = write_it(txn->rollentry_fd, &v, sizeof(v)); assert(r==sizeof(v));
	}
	toku_free(compressed_buf);
	txn->rollentry_filesize+=w.ndone;
	toku_free(buf);

	// Cleanup the rollback memory
	memarena_clear(txn->rollentry_arena);
    }
    return 0;
}

int toku_read_rollback_backwards(BREAD br, struct roll_entry **item, MEMARENA ma) {
    u_int32_t nbytes_n; ssize_t sr;
    if ((sr=bread_backwards(br, &nbytes_n, 4))!=4) { assert(sr<0); return errno; }
    u_int32_t n_bytes=toku_dtoh32(nbytes_n);
    unsigned char *buf = malloc_in_memarena(ma, n_bytes);
    if (buf==0) return errno;
    if ((sr=bread_backwards(br, buf, n_bytes-4))!=(ssize_t)n_bytes-4) { assert(sr<0); return errno; }
    int r = toku_parse_rollback(buf, n_bytes, item, ma);
    if (r!=0) return r;
    return 0;
}


static int find_xid (OMTVALUE v, void *txnv) {
    TOKUTXN txn = v;
    TOKUTXN txnfind = txnv;
    if (txn->txnid64<txnfind->txnid64) return -1;
    if (txn->txnid64>txnfind->txnid64) return +1;
    return 0;
}

static int find_filenum (OMTVALUE v, void *brtv) {
    BRT brt     = v;
    BRT brtfind = brtv;
    FILENUM fnum     = toku_cachefile_filenum(brt    ->cf);
    FILENUM fnumfind = toku_cachefile_filenum(brtfind->cf);
    if (fnum.fileid<fnumfind.fileid) return -1;
    if (fnum.fileid>fnumfind.fileid) return +1;
    if (brt < brtfind) return -1;
    if (brt > brtfind) return +1;
    return 0;
}

//Notify a transaction that it has touched a brt.
int toku_txn_note_brt (TOKUTXN txn, BRT brt) {
    OMTVALUE txnv;
    u_int32_t index;
    // Does brt already know about transaction txn?
    int r = toku_omt_find_zero(brt->txns, find_xid, txn, &txnv, &index, NULL);
    if (r==0) {
	// It's already there.
	assert((TOKUTXN)txnv==txn);
	return 0;
    }
    // Otherwise it's not there.
    // Insert reference to transaction into brt
    r = toku_omt_insert_at(brt->txns, txn, index);
    assert(r==0);
    // Insert reference to brt into transaction
    r = toku_omt_insert(txn->open_brts, brt, find_filenum, brt, 0);
    assert(r==0);
    return 0;
}

struct swap_brt_extra {
    BRT live;
    BRT zombie;
};

static int swap_brt (OMTVALUE txnv, u_int32_t UU(idx), void *extra) {
    struct swap_brt_extra *info = extra;

    TOKUTXN txn = txnv;
    OMTVALUE zombie_again=NULL;
    u_int32_t index;

    int r;
    r = toku_txn_note_brt(txn, info->live); //Add new brt.
    assert(r==0);
    r = toku_omt_find_zero(txn->open_brts, find_filenum, info->zombie, &zombie_again, &index, NULL);
    assert(r==0);
    assert((void*)zombie_again==info->zombie);
    r = toku_omt_delete_at(txn->open_brts, index); //Delete old brt.
    assert(r==0);
    return 0;
}

int toku_txn_note_swap_brt (BRT live, BRT zombie) {
    struct swap_brt_extra swap = {.live = live, .zombie = zombie};
    int r = toku_omt_iterate(zombie->txns, swap_brt, &swap);
    assert(r==0);
    toku_omt_clear(zombie->txns);

    //Close immediately.
    assert(zombie->close_db);
    r = zombie->close_db(zombie->db, zombie->close_flags);
    return r;
}


static int remove_brt (OMTVALUE txnv, u_int32_t UU(idx), void *brtv) {
    TOKUTXN txn = txnv;
    BRT     brt = brtv;
    OMTVALUE brtv_again=NULL;
    u_int32_t index;
    int r = toku_omt_find_zero(txn->open_brts, find_filenum, brt, &brtv_again, &index, NULL);
    assert(r==0);
    assert((void*)brtv_again==brtv);
    r = toku_omt_delete_at(txn->open_brts, index);
    assert(r==0);
    return 0;
}

int toku_txn_note_close_brt (BRT brt) {
    assert(toku_omt_size(brt->txns)==0);
    int r = toku_omt_iterate(brt->txns, remove_brt, brt);
    assert(r==0);
    return 0;
}

static int remove_txn (OMTVALUE brtv, u_int32_t UU(idx), void *txnv) {
    BRT brt     = brtv;
    TOKUTXN txn = txnv;
    OMTVALUE txnv_again=NULL;
    u_int32_t index;
    int r = toku_omt_find_zero(brt->txns, find_xid, txn, &txnv_again, &index, NULL);
    assert(r==0);
    assert((void*)txnv_again==txnv);
    r = toku_omt_delete_at(brt->txns, index);
    assert(r==0);
    if (txn->txnid64==brt->h->txnid_that_created_or_locked_when_empty) {
        brt->h->txnid_that_created_or_locked_when_empty = 0;
    }
    if (toku_omt_size(brt->txns)==0 && brt->was_closed) {
        //Close immediately.
        assert(brt->close_db);
        r = brt->close_db(brt->db, brt->close_flags);
    }
    return r;
}

// for every BRT in txn, remove it.
static void note_txn_closing (TOKUTXN txn) {
    toku_omt_iterate(txn->open_brts, remove_txn, txn);
    toku_omt_destroy(&txn->open_brts);
}

// Return the number of bytes that went into the rollback data structure (the uncompressed count if there is compression)
int toku_logger_txn_rolltmp_raw_count(TOKUTXN txn, u_int64_t *raw_count)
{
    *raw_count = txn->rollentry_raw_count;
    return 0;
}

int toku_txn_find_by_xid (BRT brt, TXNID xid, TOKUTXN *txnptr) {
    struct tokutxn fake_txn; fake_txn.txnid64 = xid;
    u_int32_t index;
    OMTVALUE txnv;
    int r = toku_omt_find_zero(brt->txns, find_xid, &fake_txn, &txnv, &index, NULL);
    if (r == 0) *txnptr = txnv;
    return r;
}
