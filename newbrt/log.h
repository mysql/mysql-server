#ifndef TOKULOGGGER_H
#define TOKULOGGGER_H

#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include "../include/db.h"
#include "brttypes.h"
#include "kv-pair.h"
int toku_logger_create(TOKULOGGER */*resultp*/);
int toku_logger_open(const char */*directory*/, TOKULOGGER);
int toku_logger_log_bytes(TOKULOGGER logger, int nbytes, void *bytes);
int toku_logger_close(TOKULOGGER *logger);
int toku_logger_log_checkpoint (TOKULOGGER, LSN*);
void toku_logger_panic(TOKULOGGER, int/*err*/);
int toku_logger_panicked(TOKULOGGER /*logger*/);

int toku_logger_log_phys_add_or_delete_in_leaf    (DB *db, TOKUTXN txn, DISKOFF diskoff, int is_add, const struct kv_pair *pair);

int toku_logger_commit (TOKUTXN txn, int no_sync);

int toku_logger_log_block_rename (TOKULOGGER /*logger*/, FILENUM /*fileid*/, DISKOFF /*olddiskoff*/, DISKOFF /*newdiskoff*/, DISKOFF /*parentdiskoff*/, int /*childnum*/);

int toku_logger_txn_begin (TOKUTXN /*parent*/,TOKUTXN *, TXNID /*txnid64*/, TOKULOGGER /*logger*/);

int toku_logger_log_fcreate (TOKUTXN, const char */*fname*/, int /*mode*/);

int toku_logger_log_fopen (TOKUTXN, const char * /*fname*/, FILENUM);

int toku_logger_log_unlink (TOKUTXN, const char */*fname*/);

int toku_logger_log_header (TOKUTXN, FILENUM, struct brt_header *);

int toku_logger_log_newbrtnode (TOKUTXN txn, FILENUM filenum, DISKOFF offset, u_int32_t height, u_int32_t nodesize, char is_dup_sort_mode, u_int32_t rand4fingerprint);

int toku_logger_fsync (TOKULOGGER logger);

int toku_fread_u_int8_t (FILE *f, u_int8_t *v, u_int32_t *crc, u_int32_t *len);

int toku_fread_u_int32_t_nocrclen (FILE *f, u_int32_t *v);
int toku_fread_u_int32_t (FILE *f, u_int32_t *v, u_int32_t *crc, u_int32_t *len);
int toku_fread_LSN     (FILE *f, LSN *lsn, u_int32_t *crc, u_int32_t *len);
int toku_fread_FILENUM (FILE *f, FILENUM *filenum, u_int32_t *crc, u_int32_t *len);
int toku_fread_DISKOFF (FILE *f, DISKOFF *diskoff, u_int32_t *crc, u_int32_t *len);
int toku_fread_TXNID   (FILE *f, TXNID *txnid, u_int32_t *crc, u_int32_t *len);
// fills in the bs with malloced data.
int toku_fread_BYTESTRING (FILE *f, BYTESTRING *bs, u_int32_t *crc, u_int32_t *len);
int toku_fread_LOGGEDBRTHEADER(FILE *f, LOGGEDBRTHEADER *v, u_int32_t *crc, u_int32_t *len);
int toku_fread_INTPAIRARRAY (FILE *f, INTPAIRARRAY *v, u_int32_t *crc, u_int32_t *len);

int toku_logprint_LSN             (FILE *outf, FILE *inf, const char *fieldname, u_int32_t *crc, u_int32_t *len, const char *); 
int toku_logprint_TXNID           (FILE *outf, FILE *inf, const char *fieldname, u_int32_t *crc, u_int32_t *len, const char *); 
int toku_logprint_BYTESTRING      (FILE *outf, FILE *inf, const char *fieldname, u_int32_t *crc, u_int32_t *len, const char *); 
int toku_logprint_FILENUM         (FILE *outf, FILE *inf, const char *fieldname, u_int32_t *crc, u_int32_t *len, const char *); 
int toku_logprint_DISKOFF         (FILE *outf, FILE *inf, const char *fieldname, u_int32_t *crc, u_int32_t *len, const char *); 
int toku_logprint_u_int8_t        (FILE *outf, FILE *inf, const char *fieldname, u_int32_t *crc, u_int32_t *len, const char *);
int toku_logprint_u_int32_t       (FILE *outf, FILE *inf, const char *fieldname, u_int32_t *crc, u_int32_t *len, const char *); 
int toku_logprint_LOGGEDBRTHEADER (FILE *outf, FILE *inf, const char *fieldname, u_int32_t *crc, u_int32_t *len, const char *); 
int toku_logprint_INTPAIRARRAY    (FILE *outf, FILE *inf, const char *fieldname, u_int32_t *crc, u_int32_t *len, const char *); 

int toku_read_and_print_logmagic (FILE *f, u_int32_t *version);

TXNID toku_txn_get_txnid (TOKUTXN);
LSN   toku_txn_get_last_lsn (TOKUTXN);

#endif
