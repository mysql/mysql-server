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

int tokulogger_log_commit (TOKUTXN txn);

int tokulogger_log_block_rename (TOKULOGGER logger, FILENUM fileid, DISKOFF olddiskoff, DISKOFF newdiskoff, DISKOFF parentdiskoff, int childnum);

int tokutxn_begin (TOKUTXN /*parent*/,TOKUTXN *, TXNID txnid64, TOKULOGGER logger);

int tokulogger_log_fcreate (TOKUTXN, const char */*fname*/, int /*mode*/);

int tokulogger_log_fopen (TOKUTXN, const char * /*fname*/, FILENUM filenum);

int tokulogger_log_unlink (TOKUTXN, const char */*fname*/);

#endif
