#ifndef TOKULOGGGER_H
#define TOKULOGGGER_H
#include "../include/db.h"
#include "brttypes.h"
#include "kv-pair.h"
typedef struct tokulogger *TOKULOGGER;
typedef struct tokutxn    *TOKUTXN;
int tokulogger_create_and_open_logger (const char *directory, TOKULOGGER *resultp);
int tokulogger_log_bytes(TOKULOGGER logger, int nbytes, void *bytes);
int tokulogger_log_close(TOKULOGGER *logger);

int tokulogger_log_phys_add_or_delete_in_leaf    (DB *db, TOKUTXN txn, diskoff diskoff, int is_add, const struct kv_pair *pair);

int tokulogger_log_commit (TOKUTXN txn);

int tokutxn_begin (TOKUTXN /*parent*/,TOKUTXN *, TXNID txnid64, TOKULOGGER logger);

#endif
