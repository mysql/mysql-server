#ifndef TOKULOGGGER_H
#define TOKULOGGGER_H

#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

#include "../include/db.h"
#include "brttypes.h"
#include "kv-pair.h"
#include <errno.h>

int toku_logger_create(TOKULOGGER */*resultp*/);
int toku_logger_open(const char */*directory*/, TOKULOGGER);
int toku_logger_log_bytes(TOKULOGGER logger, int nbytes, void *bytes);
int toku_logger_close(TOKULOGGER *logger);
int toku_logger_log_checkpoint (TOKULOGGER, LSN*);
void toku_logger_panic(TOKULOGGER, int/*err*/);
int toku_logger_panicked(TOKULOGGER /*logger*/);
int toku_logger_is_open(TOKULOGGER);

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

static inline int toku_copy_FILENUM(FILENUM *target, FILENUM val) { *target = val; return 0; }
static inline void toku_free_FILENUM(FILENUM val __attribute__((__unused__))) {}

static inline int toku_copy_DISKOFF(DISKOFF *target, DISKOFF val) { *target = val; return 0; }
static inline void toku_free_DISKOFF(DISKOFF val __attribute__((__unused__))) {}

static inline int toku_copy_TXNID(TXNID *target, TXNID val) { *target = val; return 0; }
static inline void toku_free_TXNID(TXNID val __attribute__((__unused__))) {}

static inline int toku_copy_u_int8_t(u_int8_t *target, u_int8_t val) { *target = val; return 0; }
static inline void toku_free_u_int8_t(u_int8_t val __attribute__((__unused__))) {}

static inline int toku_copy_u_int32_t(u_int32_t *target, u_int32_t val) { *target = val; return 0; }
static inline void toku_free_u_int32_t(u_int32_t val __attribute__((__unused__))) {}

static inline int toku_copy_INTPAIRARRAY(INTPAIRARRAY *target, INTPAIRARRAY val) {
    target->size = val.size;
    target->array = toku_memdup(val.array, val.size*sizeof(val.array[0]));
    if (target->array==0) return errno;
    return 0;
}
static inline void toku_free_INTPAIRARRAY(INTPAIRARRAY val) {
    toku_free(val.array);
}

static inline int toku_copy_BYTESTRING(BYTESTRING *target, BYTESTRING val) {
    target->len = val.len;
    target->data = toku_memdup(val.data, val.len);
    if (target->data==0) return errno;
    return 0;
}
static inline void toku_free_BYTESTRING(BYTESTRING val) {
    toku_free(val.data);
}

static inline int toku_copy_LOGGEDBRTHEADER(LOGGEDBRTHEADER *target, LOGGEDBRTHEADER val) {
    *target = val;
    if ((int32_t)val.n_named_roots!=-1) {
	int r;
	target->u.many.names = toku_memdup(target->u.many.names, val.n_named_roots*sizeof(target->u.many.names[0]));
	if (target->u.many.names==0) { r=errno; if (0) { died0: toku_free(target->u.many.names); } return r; }
	target->u.many.roots = toku_memdup(target->u.many.roots, val.n_named_roots*sizeof(target->u.many.roots[0]));
	if (target->u.many.roots==0) { r=errno; if (0) { died1: toku_free(target->u.many.names); } goto died0; }
	u_int32_t i;
	for (i=0; i<val.n_named_roots; i++) {
	    target->u.many.names[i] = toku_strdup(target->u.many.names[i]);
	    if (target->u.many.names[i]==0) {
		u_int32_t j;
		r=errno;
		for (j=0; j<i; j++) toku_free(target->u.many.names[j]);
		goto died1;
	    }
	}
    }
    return 0;
}
static inline void toku_free_LOGGEDBRTHEADER(LOGGEDBRTHEADER val) {
    if ((int32_t)val.n_named_roots==-1) return;
    u_int32_t i;
    for (i=0; i<val.n_named_roots; i++) {
	toku_free(val.u.many.names[i]);
    }
    toku_free(val.u.many.names);
    toku_free(val.u.many.roots);
}

int toku_recover_init(void);
void toku_recover_cleanup(void);

#endif
