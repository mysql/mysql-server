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

#pragma once

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "ft/serialize/block_table.h"
#include "ft/serialize/ft_layout_version.h"
#include "ft/txn/txn.h"

typedef struct tokulogger *TOKULOGGER;

enum {
    TOKU_LOG_VERSION_1 = 1,
    TOKU_LOG_VERSION_2 = 2,
    //After 2 we linked the log version to the FT_LAYOUT VERSION.
    //So it went from 2 to 13 (3-12 do not exist)
    TOKU_LOG_VERSION_24 = 24,
    TOKU_LOG_VERSION_25 = 25, // change rollinclude rollback log entry
    TOKU_LOG_VERSION_26 = 26, // no change from 25
    TOKU_LOG_VERSION_27 = 27, // no change from 26
    TOKU_LOG_VERSION   = FT_LAYOUT_VERSION, 
    TOKU_LOG_MIN_SUPPORTED_VERSION = FT_LAYOUT_MIN_SUPPORTED_VERSION,
};

int toku_logger_create (TOKULOGGER *resultp);
int toku_logger_open (const char *directory, TOKULOGGER logger);
int toku_logger_open_with_last_xid(const char *directory, TOKULOGGER logger, TXNID last_xid);
void toku_logger_shutdown(TOKULOGGER logger);
int toku_logger_close(TOKULOGGER *loggerp);
void toku_logger_initialize_rollback_cache(TOKULOGGER logger, struct ft *ft);
int toku_logger_open_rollback(TOKULOGGER logger, struct cachetable *ct, bool create);
void toku_logger_close_rollback(TOKULOGGER logger);
void toku_logger_close_rollback_check_empty(TOKULOGGER logger, bool clean_shutdown);
bool toku_logger_rollback_is_open (TOKULOGGER); // return true iff the rollback is open.

void toku_logger_fsync (TOKULOGGER logger);
void toku_logger_fsync_if_lsn_not_fsynced(TOKULOGGER logger, LSN lsn);
int toku_logger_is_open(TOKULOGGER logger);
void toku_logger_set_cachetable (TOKULOGGER logger, struct cachetable *ct);
int toku_logger_set_lg_max(TOKULOGGER logger, uint32_t lg_max);
int toku_logger_get_lg_max(TOKULOGGER logger, uint32_t *lg_maxp);
int toku_logger_set_lg_bsize(TOKULOGGER logger, uint32_t bsize);

void toku_logger_write_log_files (TOKULOGGER logger, bool write_log_files);
void toku_logger_trim_log_files(TOKULOGGER logger, bool trim_log_files);
bool toku_logger_txns_exist(TOKULOGGER logger);

// Restart the logger.  This function is used by recovery to really start
// logging.
// Effects: Flush the current log buffer, reset the logger's lastlsn, and
// open a new log file.
// Returns: 0 if success
int toku_logger_restart(TOKULOGGER logger, LSN lastlsn);

// Maybe trim the log entries from the log that are older than the given LSN
// Effect: find all of the log files whose largest LSN is smaller than the
// given LSN and delete them.
void toku_logger_maybe_trim_log(TOKULOGGER logger, LSN oldest_open_lsn);

// At the ft layer, a FILENUM uniquely identifies an open file.
struct FILENUM {
    uint32_t fileid;
};
static const FILENUM FILENUM_NONE = { .fileid = UINT32_MAX };

struct FILENUMS {
    uint32_t num;
    FILENUM *filenums;
};

void toku_logger_log_fcreate(TOKUTXN txn, const char *fname, FILENUM filenum, uint32_t mode, uint32_t flags, uint32_t nodesize, uint32_t basementnodesize, enum toku_compression_method compression_method);
void toku_logger_log_fdelete(TOKUTXN txn, FILENUM filenum);
void toku_logger_log_fopen(TOKUTXN txn, const char * fname, FILENUM filenum, uint32_t treeflags);

// the log generation code requires a typedef if we want to pass by pointer
typedef TOKU_XA_XID *XIDP;

int toku_fread_uint8_t (FILE *f, uint8_t *v, struct x1764 *mm, uint32_t *len);
int toku_fread_uint32_t_nocrclen (FILE *f, uint32_t *v);
int toku_fread_uint32_t (FILE *f, uint32_t *v, struct x1764 *checksum, uint32_t *len);
int toku_fread_uint64_t (FILE *f, uint64_t *v, struct x1764 *checksum, uint32_t *len);
int toku_fread_bool (FILE *f, bool *v, struct x1764 *checksum, uint32_t *len);
int toku_fread_LSN     (FILE *f, LSN *lsn, struct x1764 *checksum, uint32_t *len);
int toku_fread_BLOCKNUM (FILE *f, BLOCKNUM *lsn, struct x1764 *checksum, uint32_t *len);
int toku_fread_FILENUM (FILE *f, FILENUM *filenum, struct x1764 *checksum, uint32_t *len);
int toku_fread_TXNID   (FILE *f, TXNID *txnid, struct x1764 *checksum, uint32_t *len);
int toku_fread_TXNID_PAIR   (FILE *f, TXNID_PAIR *txnid, struct x1764 *checksum, uint32_t *len);
int toku_fread_XIDP    (FILE *f, XIDP  *xidp,  struct x1764 *checksum, uint32_t *len);
int toku_fread_BYTESTRING (FILE *f, BYTESTRING *bs, struct x1764 *checksum, uint32_t *len);
int toku_fread_FILENUMS (FILE *f, FILENUMS *fs, struct x1764 *checksum, uint32_t *len);

int toku_logprint_LSN (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, uint32_t *len, const char *format __attribute__((__unused__)));
int toku_logprint_TXNID (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, uint32_t *len, const char *format __attribute__((__unused__)));
int toku_logprint_TXNID_PAIR (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, uint32_t *len, const char *format __attribute__((__unused__)));
int toku_logprint_XIDP (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, uint32_t *len, const char *format __attribute__((__unused__)));
int toku_logprint_uint8_t (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, uint32_t *len, const char *format);
int toku_logprint_uint32_t (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, uint32_t *len, const char *format);
int toku_logprint_BLOCKNUM (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, uint32_t *len, const char *format);
int toku_logprint_uint64_t (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, uint32_t *len, const char *format);
int toku_logprint_bool (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, uint32_t *len, const char *format __attribute__((__unused__)));
void toku_print_BYTESTRING (FILE *outf, uint32_t len, char *data);
int toku_logprint_BYTESTRING (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, uint32_t *len, const char *format __attribute__((__unused__)));
int toku_logprint_FILENUM (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, uint32_t *len, const char *format);
int toku_logprint_FILENUMS (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, uint32_t *len, const char *format);
int toku_read_and_print_logmagic (FILE *f, uint32_t *versionp);
int toku_read_logmagic (FILE *f, uint32_t *versionp);

TXNID_PAIR toku_txn_get_txnid (TOKUTXN txn);
LSN toku_logger_last_lsn(TOKULOGGER logger);
TOKULOGGER toku_txn_logger (TOKUTXN txn);

void toku_txnid2txn (TOKULOGGER logger, TXNID_PAIR txnid, TOKUTXN *result);

int toku_logger_log_archive (TOKULOGGER logger, char ***logs_p, int flags);

TOKUTXN toku_logger_txn_parent (TOKUTXN txn);
void toku_logger_note_checkpoint(TOKULOGGER logger, LSN lsn);

void toku_logger_make_space_in_inbuf (TOKULOGGER logger, int n_bytes_needed);

int toku_logger_write_inbuf (TOKULOGGER logger);
// Effect: Write the buffered data (from the inbuf) to a file.  No fsync, however.
// As a side effect, the inbuf will be made empty.
// Return 0 on success, otherwise return an error number.
// Requires: The inbuf lock is currently held, and the outbuf lock is not held.
//  Upon return, the inbuf lock will be held, and the outbuf lock is not held.
//  However, no side effects should have been made to the logger.  The lock was acquired simply to determine that the buffer will overflow if we try to put something into it.
//  The inbuf lock will be released, so the operations before and after this function call will not be atomic.
// Rationale:  When the buffer becomes nearly full, call this function so that more can be put in.
// Implementation note:  Since the output lock is acquired first, we must release the input lock, and then grab both in the right order.

void toku_logger_maybe_fsync (TOKULOGGER logger, LSN lsn, int do_fsync, bool holds_input_lock);
// Effect: If fsync is nonzero, then make sure that the log is flushed and synced at least up to lsn.
// Entry: Holds input lock iff 'holds_input_lock'.
// Exit:  Holds no locks.

// Discussion: How does the logger work:
//  The logger has two buffers: an inbuf and an outbuf.  
//  There are two locks, called the inlock, and the outlock.  To write, both locks must be held, and the outlock is acquired first.
//  Roughly speaking, the inbuf is used to accumulate logged data, and the outbuf is used to write to disk.
//  When something is to be logged we do the following: 
//    acquire the inlock.
//    Make sure there is space in the inbuf for the logentry. (We know the size of the logentry in advance):
//      if the inbuf doesn't have enough space then
//      release the inlock
//      acquire the outlock
//      acquire the inlock
//      it's possible that some other thread made space.
//      if there still isn't space
//        swap the inbuf and the outbuf
//        release the inlock
//        write the outbuf
//        acquire the inlock
//        release the outlock
//        if the inbuf is still too small, then increase the size of the inbuf
//    Increment the LSN and fill the inbuf.
//    If fsync is required then
//      release the inlock
//      acquire the outlock
//      acquire the inlock
//      if the LSN has been flushed and fsynced (if so we are done.  Some other thread did the flush.)  
//        release the locks
//      if the LSN has been flushed but not fsynced up to the LSN:
//        release the inlock
//        fsync
//        release the outlock
//      otherwise:
//        swap the outbuf and the inbuf
//        release the inlock
//        write the outbuf
//        fsync
//        release the outlock

typedef enum {
    LOGGER_NEXT_LSN = 0,
    LOGGER_NUM_WRITES,
    LOGGER_BYTES_WRITTEN,
    LOGGER_UNCOMPRESSED_BYTES_WRITTEN,
    LOGGER_TOKUTIME_WRITES,
    LOGGER_WAIT_BUF_LONG,
    LOGGER_STATUS_NUM_ROWS
} logger_status_entry;

typedef struct {
    bool initialized;
    TOKU_ENGINE_STATUS_ROW_S status[LOGGER_STATUS_NUM_ROWS];
} LOGGER_STATUS_S, *LOGGER_STATUS;

void toku_logger_get_status(TOKULOGGER logger, LOGGER_STATUS s);

int toku_get_version_of_logs_on_disk(const char *log_dir, bool *found_any_logs, uint32_t *version_found);

struct txn_manager *toku_logger_get_txn_manager(TOKULOGGER logger);

// For serialize / deserialize

#include "ft/serialize/wbuf.h"

static inline void wbuf_nocrc_FILENUM(struct wbuf *wb, FILENUM fileid) {
    wbuf_nocrc_uint(wb, fileid.fileid);
}

static inline void wbuf_FILENUM(struct wbuf *wb, FILENUM fileid) {
    wbuf_uint(wb, fileid.fileid);
}

static inline void wbuf_nocrc_FILENUMS(struct wbuf *wb, FILENUMS v) {
    wbuf_nocrc_uint(wb, v.num);
    for (uint32_t i = 0; i < v.num; i++) {
        wbuf_nocrc_FILENUM(wb, v.filenums[i]);
    }
}

static inline void wbuf_FILENUMS(struct wbuf *wb, FILENUMS v) {
    wbuf_uint(wb, v.num);
    for (uint32_t i = 0; i < v.num; i++) {
        wbuf_FILENUM(wb, v.filenums[i]);
    }
}

static inline void wbuf_nocrc_XIDP (struct wbuf *w, TOKU_XA_XID *xid) {
    wbuf_nocrc_uint32_t(w, xid->formatID);
    wbuf_nocrc_uint8_t(w, xid->gtrid_length);
    wbuf_nocrc_uint8_t(w, xid->bqual_length);
    wbuf_nocrc_literal_bytes(w, xid->data, xid->gtrid_length+xid->bqual_length);
}

#include "ft/serialize/rbuf.h"

static inline void rbuf_FILENUM(struct rbuf *rb, FILENUM *filenum) {
    filenum->fileid = rbuf_int(rb);
}
static inline void rbuf_ma_FILENUM(struct rbuf *rb, memarena *UU(ma), FILENUM *filenum) {
    rbuf_FILENUM(rb, filenum);
}

static inline void rbuf_FILENUMS(struct rbuf *rb, FILENUMS *filenums) {
    filenums->num = rbuf_int(rb);
    XMALLOC_N(filenums->num, filenums->filenums);
    for (uint32_t i = 0; i < filenums->num; i++) {
        rbuf_FILENUM(rb, &(filenums->filenums[i]));
    }
}

static inline void rbuf_ma_FILENUMS(struct rbuf *rb, memarena *ma, FILENUMS *filenums) {
    rbuf_ma_uint32_t(rb, ma, &(filenums->num));
    filenums->filenums = (FILENUM *) ma->malloc_from_arena(filenums->num * sizeof(FILENUM));
    assert(filenums->filenums != NULL);
    for (uint32_t i = 0; i < filenums->num; i++) {
        rbuf_ma_FILENUM(rb, ma, &(filenums->filenums[i]));
    }
}
