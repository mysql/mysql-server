#ifndef TOKULOGGGER_H
#define TOKULOGGGER_H

#ident "$Id$"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "toku_portability.h"
#include <errno.h>

#include "../include/db.h"
#include "brttypes.h"
#include "memory.h"
#include "bread.h"
#include "x1764.h"

struct logbytes;
struct logbytes {
    struct logbytes *next;
    int nbytes;
    LSN lsn;
    char bytes[1];
};

#define MALLOC_LOGBYTES(n) toku_malloc(sizeof(struct logbytes)+n -1)

void toku_logger_lock_init(void);
void toku_logger_lock_destroy(void);
int toku_logger_create(TOKULOGGER */*resultp*/);
void toku_logger_set_cachetable (TOKULOGGER, CACHETABLE);
void toku_logger_write_log_files (TOKULOGGER, int do_write_log_files);
int toku_logger_open(const char */*directory*/, TOKULOGGER);
int toku_logger_log_bytes(TOKULOGGER logger, struct logbytes *bytes, int do_fsync);
int toku_logger_close(TOKULOGGER *logger);
int toku_logger_log_checkpoint (TOKULOGGER);
void toku_logger_panic(TOKULOGGER, int/*err*/);
int toku_logger_panicked(TOKULOGGER /*logger*/);
int toku_logger_is_open(TOKULOGGER);
LSN toku_logger_last_lsn(TOKULOGGER);

int toku_logger_set_lg_max (TOKULOGGER logger, u_int32_t);
int toku_logger_get_lg_max (TOKULOGGER logger, u_int32_t *);
int toku_logger_set_lg_bsize(TOKULOGGER, u_int32_t);

typedef void(*voidfp)(void);
typedef void(*YIELDF)(voidfp, void*);
// Doesn't close the txn, just performs the commit operations.
int toku_logger_commit (TOKUTXN txn, int no_sync, YIELDF yield, void*yield_v);

// Doesn't close the txn, just performs the abort operations.
int toku_logger_abort(TOKUTXN, YIELDF, void*/*yield_v*/);

// Closes a txn.  Call after commiting or aborting.
void toku_logger_txn_close (TOKUTXN);

// Return the number of bytes that went into the rollback data structure (the uncompressed count if there is compression)
int toku_logger_txn_rolltmp_raw_count(TOKUTXN, u_int64_t *count);

int toku_logger_txn_begin (TOKUTXN /*parent*/,TOKUTXN *, TOKULOGGER /*logger*/);

int toku_logger_log_fcreate (TOKUTXN, const char */*fname*/, FILENUM /*filenum*/, int /*mode*/);

int toku_logger_log_fopen (TOKUTXN, const char * /*fname*/, FILENUM);

int toku_logger_log_header (TOKUTXN, FILENUM, struct brt_header *);

int toku_logger_log_newbrtnode (TOKUTXN txn, FILENUM filenum, DISKOFF offset, u_int32_t height, u_int32_t nodesize, char is_dup_sort_mode, u_int32_t rand4fingerprint);

int toku_logger_fsync (TOKULOGGER logger);

int toku_fread_u_int8_t (FILE *f, u_int8_t *v, struct x1764 *, u_int32_t *len);

int toku_fread_u_int32_t_nocrclen (FILE *f, u_int32_t *v);
int toku_fread_u_int32_t (FILE *f, u_int32_t *v, struct x1764 *, u_int32_t *len);
int toku_fread_LSN     (FILE *f, LSN *lsn, struct x1764 *, u_int32_t *len);
int toku_fread_FILENUM (FILE *f, FILENUM *filenum, struct x1764 *, u_int32_t *len);
int toku_fread_DISKOFF (FILE *f, DISKOFF *diskoff, struct x1764 *, u_int32_t *len);
int toku_fread_BLOCKNUM (FILE *f, BLOCKNUM *, struct x1764 *, u_int32_t *len);
int toku_fread_TXNID   (FILE *f, TXNID *txnid, struct x1764 *, u_int32_t *len);
// fills in the bs with malloced data.
int toku_fread_BYTESTRING (FILE *f, BYTESTRING *bs, struct x1764 *, u_int32_t *len);
int toku_fread_LOGGEDBRTHEADER(FILE *f, LOGGEDBRTHEADER *v, struct x1764 *, u_int32_t *len);
int toku_fread_INTPAIRARRAY (FILE *f, INTPAIRARRAY *v, struct x1764 *, u_int32_t *len);

int toku_logprint_LSN             (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *, u_int32_t *len, const char *);
int toku_logprint_TXNID           (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *, u_int32_t *len, const char *);
int toku_logprint_BYTESTRING      (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *, u_int32_t *len, const char *);
int toku_logprint_FILENUM         (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *, u_int32_t *len, const char *);
int toku_logprint_DISKOFF         (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *, u_int32_t *len, const char *);
int toku_logprint_BLOCKNUM        (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *, u_int32_t *len, const char *);
int toku_logprint_u_int8_t        (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *, u_int32_t *len, const char *);
int toku_logprint_u_int32_t       (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *, u_int32_t *len, const char *);
int toku_logprint_LOGGEDBRTHEADER (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *, u_int32_t *len, const char *);
int toku_logprint_INTPAIRARRAY    (FILE *outf, FILE *inf, const char *fieldname, struct x1764 *, u_int32_t *len, const char *);

// Useful thing for printing a bytestring.
void toku_print_BYTESTRING (FILE *outf, u_int32_t len, char *data);

int toku_read_and_print_logmagic (FILE *f, u_int32_t *version);

TXNID toku_txn_get_txnid (TOKUTXN);
LSN   toku_txn_get_last_lsn (TOKUTXN);
TOKULOGGER toku_txn_logger (TOKUTXN txn);

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
    target->data = toku_memdup(val.data, (size_t)val.len);
    if (target->data==0) return errno;
    return 0;
}
static inline void toku_free_BYTESTRING(BYTESTRING val) {
    toku_free(val.data);
}

static inline int toku_copy_LOGGEDBRTHEADER(LOGGEDBRTHEADER *target, LOGGEDBRTHEADER val) {
    *target = val;
    return 0;
}
static inline void toku_free_LOGGEDBRTHEADER(LOGGEDBRTHEADER UU(val)) {
    return;
}

int toku_recover_init(void);
void toku_recover_cleanup(void);

// Find the txn that belongs to a txnid.
// Return nonzero if no such txn is live (either didn't exist ever, or it is committed or aborted.)
// Return 0 if there is a live txn with that txnid.
int toku_txnid2txn (TOKULOGGER logger, TXNID txnid, TOKUTXN *result);

int tokudb_recover(const char *datadir, const char *logdir);
int toku_logger_log_archive (TOKULOGGER logger, char ***logs_p, int flags);

int toku_maybe_spill_rollbacks (TOKUTXN txn);

struct roll_entry;
int toku_rollback_fileentries (int        fd,
			       TOKUTXN    txn,
			       YIELDF yield,
			       void *     yieldv);
int toku_commit_fileentries (int        fd,
			     TOKUTXN    txn,
			     YIELDF yield,
			     void *     yieldv);

// do the commit items.  Call yield(yield_v) once in a while.
int toku_commit_rollback_item (TOKUTXN txn, struct roll_entry *item, YIELDF yield, void*yield_v);
int toku_abort_rollback_item (TOKUTXN txn, struct roll_entry *item, YIELDF yield, void*yield_v);

int toku_txn_note_brt (TOKUTXN txn, BRT brt);
int toku_txn_note_close_brt (BRT brt);
int toku_txn_note_swap_brt (BRT t, BRT deadbeat);

// find the TOKUTXN object by xid
// if found then return 0 and set txnptr to the address of the TOKUTXN object
int toku_txn_find_by_xid (BRT brt, TXNID xid, TOKUTXN *txnptr);


// Allocate memory as part of a rollback structure.  It will be freed when the transaction completes.
void* toku_malloc_in_rollback(TOKUTXN txn, size_t size);
void *toku_memdup_in_rollback(TOKUTXN txn, const void *v, size_t len);
char *toku_strdup_in_rollback(TOKUTXN txn, const char *s);

// A hook for testing #1572.  Sets the amount that txn's are incremented by.
void toku_set_lsn_increment (uint64_t incr) __attribute__((__visibility__("default")));

int toku_logger_iterate_over_live_txns (TOKULOGGER logger, int (*f)(TOKULOGGER, TOKUTXN, void*), void *v);
// Effect: Call f on each open transaction (root transactions and non-root transactions) in no particular order.
// If f returns nonzero, then this function returns immediately (without calling f any more), and returns that nonzero value.
// Otherwise returns 0.

TOKUTXN toku_logger_txn_parent (TOKUTXN);

void toku_logger_note_checkpoint(TOKULOGGER, LSN);

#endif
