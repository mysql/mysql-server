#ifndef TOKULOGGGER_H
#define TOKULOGGGER_H
#include "../include/db.h"
#include "brttypes.h"
#include "kv-pair.h"
int tokulogger_create_and_open_logger (const char *directory, TOKULOGGER *resultp);
int tokulogger_log_bytes(TOKULOGGER logger, int nbytes, void *bytes);
int tokulogger_log_close(TOKULOGGER *logger);
int tokulogger_log_checkpoint (TOKULOGGER, LSN*);

int tokulogger_log_phys_add_or_delete_in_leaf    (DB *db, TOKUTXN txn, DISKOFF diskoff, int is_add, const struct kv_pair *pair);

int tokulogger_commit (TOKUTXN txn);

int tokulogger_log_block_rename (TOKULOGGER /*logger*/, FILENUM /*fileid*/, DISKOFF /*olddiskoff*/, DISKOFF /*newdiskoff*/, DISKOFF /*parentdiskoff*/, int /*childnum*/);

int tokutxn_begin (TOKUTXN /*parent*/,TOKUTXN *, TXNID /*txnid64*/, TOKULOGGER /*logger*/);

int tokulogger_log_fcreate (TOKUTXN, const char */*fname*/, int /*mode*/);

int tokulogger_log_fopen (TOKUTXN, const char * /*fname*/, FILENUM);

int tokulogger_log_unlink (TOKUTXN, const char */*fname*/);

int tokulogger_log_header (TOKUTXN, FILENUM, struct brt_header *);

int tokulogger_log_newbrtnode (TOKUTXN txn, FILENUM filenum, DISKOFF offset, u_int32_t height, u_int32_t nodesize, char is_dup_sort_mode, u_int32_t rand4fingerprint);

int tokulogger_fsync (TOKULOGGER logger);

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

int toku_logprint_LSN             (FILE *outf, FILE *inf, const char *fieldname, u_int32_t *crc, u_int32_t *len); 
int toku_logprint_TXNID           (FILE *outf, FILE *inf, const char *fieldname, u_int32_t *crc, u_int32_t *len); 
int toku_logprint_BYTESTRING      (FILE *outf, FILE *inf, const char *fieldname, u_int32_t *crc, u_int32_t *len); 
int toku_logprint_FILENUM         (FILE *outf, FILE *inf, const char *fieldname, u_int32_t *crc, u_int32_t *len); 
int toku_logprint_DISKOFF         (FILE *outf, FILE *inf, const char *fieldname, u_int32_t *crc, u_int32_t *len); 
int toku_logprint_u_int8_t        (FILE *outf, FILE *inf, const char *fieldname, u_int32_t *crc, u_int32_t *len);
int toku_logprint_u_int32_t       (FILE *outf, FILE *inf, const char *fieldname, u_int32_t *crc, u_int32_t *len); 
int toku_logprint_LOGGEDBRTHEADER (FILE *outf, FILE *inf, const char *fieldname, u_int32_t *crc, u_int32_t *len); 

#endif
