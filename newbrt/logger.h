#ifndef TOKULOGGER_H
#define TOKULOGGER_H

#ident "$Id: logger.h 12375 2009-05-28 14:14:47Z yfogel $"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

enum { TOKU_LOG_VERSION = 1 };

int toku_logger_create (TOKULOGGER *resultp);
int toku_logger_open (const char *directory, TOKULOGGER logger);
int toku_logger_log_bytes (TOKULOGGER logger, struct logbytes *bytes, int do_fsync);
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

int toku_logger_log_fcreate (TOKUTXN txn, const char *fname, FILENUM filenum, u_int32_t mode, u_int32_t flags);
int toku_logger_log_fopen (TOKUTXN txn, const char * fname, FILENUM filenum);

int toku_fread_u_int8_t (FILE *f, u_int8_t *v, struct x1764 *mm, u_int32_t *len);
int toku_fread_u_int32_t_nocrclen (FILE *f, u_int32_t *v);
int toku_fread_u_int32_t (FILE *f, u_int32_t *v, struct x1764 *checksum, u_int32_t *len);
int toku_fread_u_int64_t (FILE *f, u_int64_t *v, struct x1764 *checksum, u_int32_t *len);
int toku_fread_LSN     (FILE *f, LSN *lsn, struct x1764 *checksum, u_int32_t *len);
int toku_fread_FILENUM (FILE *f, FILENUM *filenum, struct x1764 *checksum, u_int32_t *len);
int toku_fread_DISKOFF (FILE *f, DISKOFF *diskoff, struct x1764 *checksum, u_int32_t *len);
int toku_fread_BLOCKNUM (FILE *f, BLOCKNUM *blocknum, struct x1764 *checksum, u_int32_t *len);
int toku_fread_TXNID   (FILE *f, TXNID *txnid, struct x1764 *checksum, u_int32_t *len);
int toku_fread_BYTESTRING (FILE *f, BYTESTRING *bs, struct x1764 *checksum, u_int32_t *len);
int toku_fread_LOGGEDBRTHEADER (FILE *f, LOGGEDBRTHEADER *v, struct x1764 *checksum, u_int32_t *len);
int toku_fread_INTPAIRARRAY (FILE *f, INTPAIRARRAY *v, struct x1764 *checksum, u_int32_t *len);

int toku_logprint_LSN (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, u_int32_t *len, const char *format __attribute__((__unused__)));
int toku_logprint_TXNID (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, u_int32_t *len, const char *format __attribute__((__unused__)));
int toku_logprint_u_int8_t (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, u_int32_t *len, const char *format);
int toku_logprint_u_int32_t (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, u_int32_t *len, const char *format);
int toku_logprint_u_int64_t (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, u_int32_t *len, const char *format);
void toku_print_BYTESTRING (FILE *outf, u_int32_t len, char *data);
int toku_logprint_BYTESTRING (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, u_int32_t *len, const char *format __attribute__((__unused__)));
int toku_logprint_FILENUM (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, u_int32_t *len, const char *format);
int toku_logprint_DISKOFF (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, u_int32_t *len, const char *format __attribute__((__unused__)));
int toku_logprint_BLOCKNUM (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, u_int32_t *len, const char *format __attribute__((__unused__)));
int toku_logprint_LOGGEDBRTHEADER (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, u_int32_t *len, const char *format __attribute__((__unused__)));
int toku_logprint_INTPAIRARRAY (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *checksum, u_int32_t *len, const char *format __attribute__((__unused__)));
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

#endif
