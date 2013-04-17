#ifndef TOKULOGGER_H
#define TOKULOGGER_H

#ident "$Id: logger.h 12375 2009-05-28 14:14:47Z yfogel $"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

enum { TOKU_LOG_VERSION = 1 };

int toku_logger_create (TOKULOGGER *resultp);
int toku_logger_open (const char *directory, TOKULOGGER logger);
int toku_logger_shutdown(TOKULOGGER logger);
int toku_logger_close(TOKULOGGER *loggerp);

u_int32_t toku_logger_get_lock_ctr(void);

int toku_logger_fsync (TOKULOGGER logger);
void toku_logger_panic (TOKULOGGER logger, int err);
int toku_logger_panicked(TOKULOGGER logger);
int toku_logger_is_open(TOKULOGGER logger);
void toku_logger_set_cachetable (TOKULOGGER logger, CACHETABLE ct);
int toku_logger_set_lg_max(TOKULOGGER logger, u_int32_t lg_max);
int toku_logger_get_lg_max(TOKULOGGER logger, u_int32_t *lg_maxp);
int toku_logger_set_lg_bsize(TOKULOGGER logger, u_int32_t bsize);

int toku_logger_lock_init(void);
int toku_logger_lock_destroy(void);

void toku_logger_write_log_files (TOKULOGGER logger, BOOL write_log_files);
void toku_logger_trim_log_files(TOKULOGGER logger, BOOL trim_log_files);

// Restart the logger.  This function is used by recovery to really start
// logging.
// Effects: Flush the current log buffer, reset the logger's lastlsn, and
// open a new log file.
// Returns: 0 if success
int toku_logger_restart(TOKULOGGER logger, LSN lastlsn);

// Maybe trim the log entries from the log that are older than the given LSN
// Effect: find all of the log files whose largest LSN is smaller than the
// given LSN and delete them.
// Returns: 0 if success
int toku_logger_maybe_trim_log(TOKULOGGER logger, LSN oldest_open_lsn);

int toku_logger_log_fcreate (TOKUTXN txn, const char *fname, FILENUM filenum, u_int32_t mode, u_int32_t flags, DESCRIPTOR descriptor_p);
int toku_logger_log_fdelete (TOKUTXN txn, const char *fname);
int toku_logger_log_fopen (TOKUTXN txn, const char * fname, FILENUM filenum, uint32_t treeflags);

int toku_fread_u_int8_t (FILE *f, u_int8_t *v, struct x1764 *mm, u_int32_t *len);
int toku_fread_u_int32_t_nocrclen (FILE *f, u_int32_t *v);
int toku_fread_u_int32_t (FILE *f, u_int32_t *v, struct x1764 *checksum, u_int32_t *len);
int toku_fread_u_int64_t (FILE *f, u_int64_t *v, struct x1764 *checksum, u_int32_t *len);
int toku_fread_LSN     (FILE *f, LSN *lsn, struct x1764 *checksum, u_int32_t *len);
int toku_fread_FILENUM (FILE *f, FILENUM *filenum, struct x1764 *checksum, u_int32_t *len);
int toku_fread_TXNID   (FILE *f, TXNID *txnid, struct x1764 *checksum, u_int32_t *len);
int toku_fread_BYTESTRING (FILE *f, BYTESTRING *bs, struct x1764 *checksum, u_int32_t *len);

int toku_logprint_LSN (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, u_int32_t *len, const char *format __attribute__((__unused__)));
int toku_logprint_TXNID (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, u_int32_t *len, const char *format __attribute__((__unused__)));
int toku_logprint_u_int8_t (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, u_int32_t *len, const char *format);
int toku_logprint_u_int32_t (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, u_int32_t *len, const char *format);
int toku_logprint_u_int64_t (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, u_int32_t *len, const char *format);
void toku_print_BYTESTRING (FILE *outf, u_int32_t len, char *data);
int toku_logprint_BYTESTRING (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, u_int32_t *len, const char *format __attribute__((__unused__)));
int toku_logprint_FILENUM (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, u_int32_t *len, const char *format);
int toku_read_and_print_logmagic (FILE *f, u_int32_t *versionp);
int toku_read_logmagic (FILE *f, u_int32_t *versionp);

TXNID toku_txn_get_txnid (TOKUTXN txn);
LSN toku_txn_get_last_lsn (TOKUTXN txn);
LSN toku_logger_last_lsn(TOKULOGGER logger);
TOKULOGGER toku_txn_logger (TOKUTXN txn);

int toku_txnid2txn (TOKULOGGER logger, TXNID txnid, TOKUTXN *result);
//int toku_logger_log_checkpoint (TOKULOGGER);
//int toku_set_func_fsync (int (*fsync_function)(int));
int toku_logger_log_archive (TOKULOGGER logger, char ***logs_p, int flags);

TOKUTXN toku_logger_txn_parent (TOKUTXN txn);
void toku_logger_note_checkpoint(TOKULOGGER logger, LSN lsn);

TXNID toku_logger_get_oldest_living_xid(TOKULOGGER logger);
LSN toku_logger_get_oldest_living_lsn(TOKULOGGER logger);
LSN toku_logger_get_next_lsn(TOKULOGGER logger);
void toku_logger_set_remove_finalize_callback(TOKULOGGER logger, void (*funcp)(int, void *), void * extra);
void toku_logger_call_remove_finalize_callback(TOKULOGGER logger, int fd);

int toku_logger_make_space_in_inbuf (TOKULOGGER logger, int n_bytes_needed);

int
toku_logger_write_inbuf (TOKULOGGER logger);
// Effect: Write the buffered data (from the inbuf) to a file.  No fsync, however.
// As a side effect, the inbuf will be made empty.
// Return 0 on success, otherwise return an error number.
// Requires: The inbuf lock is currently held, and the outbuf lock is not held.
//  Upon return, the inbuf lock will be held, and the outbuf lock is not held.
//  However, no side effects should have been made to the logger.  The lock was acquired simply to determine that the buffer will overflow if we try to put something into it.
//  The inbuf lock will be released, so the operations before and after this function call will not be atomic.
// Rationale:  When the buffer becomes nearly full, call this function so that more can be put in.
// Implementation note:  Since the output lock is acquired first, we must release the input lock, and then grab both in the right order.


int
toku_logger_maybe_fsync (TOKULOGGER logger, LSN lsn, int do_fsync);
// Effect: If fsync is nonzero, then make sure that the log is flushed and synced at least up to lsn.
// Entry: Holds input lock.
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

#endif

