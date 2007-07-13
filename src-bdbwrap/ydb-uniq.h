#ifndef _YDB_WRAP_H
#define _YDB_WRAP_H

#define DB_BTREE DB_BTREE_ydb
#define DB_NOTICE_LOGFILE_CHANGED DB_NOTICE_LOGFILE_CHANGED_ydb
#define DBTYPE DBTYPE_ydb
#define db_notices db_notices_ydb
#define txn_abort txn_abort_ydb
#define txn_begin txn_begin_ydb
#define txn_commit txn_commit_ydb
#define DB_VERB_CHKPOINT DB_VERB_CHKPOINT_ydb
#define DB_VERB_DEADLOCK DB_VERB_DEADLOCK_ydb
#define DB_VERB_RECOVERY DB_VERB_RECOVERY_ydb
#define DB DB_ydb
#define DB_BTREE_STAT DB_BTREE_STAT_ydb
#define DB_ENV DB_ENV_ydb
#define DB_KEY_RANGE DB_KEY_RANGE_ydb
#define DB_LSN DB_LSN_ydb
#define DB_TXN DB_TXN_ydb
#define DB_TXN_ACTIVE DB_TXN_ACTIVE_ydb
#define DB_TXN_STAT DB_TXN_STAT_ydb
#define DBC DBC_ydb
#define DBT DBT_ydb
#define DB_DBT_MALLOC  B_DBT_MALLOC_ydb
#define DB_DBT_REALLOC DB_DBT_REALLOC_ydb
#define DB_DBT_USERMEM DB_DBT_USERMEM_ydb
#define DB_DBT_DUPOK DB_DBT_DUPOK_ydb
#define DB_VERSION_STRING DB_VERSION_STRING_ydb
#define DB_ARCH_ABS DB_ARCH_ABS_ydb
#define DB_ARCH_LOG DB_ARCH_LOG_ydb
#define DB_FIRST DB_FIRST_ydb
#define DB_GET_BOTH DB_GET_BOTH_ydb
#define DB_LAST DB_LAST_ydb
#define DB_NEXT DB_NEXT_ydb
#define DB_NEXT_DUP DB_NEXT_DUP_ydb
#define DB_PREV DB_PREV_ydb
#define DB_SET DB_SET_ydb
#define DB_SET_RANGE DB_SET_RANGE_ydb
#define DB_RMW DB_RMW_ydb
#define DB_KEYEMPTY DB_KEYEMPTY_ydb
#define DB_KEYEXIST DB_KEYEXIST_ydb
#define DB_LOCK_DEADLOCK DB_LOCK_DEADLOCK_ydb
#define DB_NOTFOUND DB_NOTFOUND_ydb
#define DB_CREATE DB_CREATE_ydb
#define DB_RDONLY DB_RDONLY_ydb
#define DB_RECOVER DB_RECOVER_ydb
#define DB_THREAD DB_THREAD_ydb
#define DB_TXN_NOSYNC DB_TXN_NOSYNC_ydb
#define DB_PRIVATE DB_PRIVATE_ydb
#define DB_LOCK_DEFAULT DB_LOCK_DEFAULT_ydb
#define DB_LOCK_OLDEST DB_LOCK_OLDEST_ydb
#define DB_LOCK_RANDOM DB_LOCK_RANDOM_ydb
#define DB_DUP DB_DUP_ydb
#define DB_NOOVERWRITE DB_NOOVERWRITE_ydb
#define DB_INIT_LOCK DB_INIT_LOCK_ydb
#define DB_INIT_LOG DB_INIT_LOG_ydb
#define DB_INIT_MPOOL DB_INIT_MPOOL_ydb
#define DB_INIT_TXN DB_INIT_TXN_ydb
#define db_create db_create_ydb
#define db_env_create db_env_create_ydb
#define txn_begin txn_begin_ydb
#define txn_commit txn_commit_ydb
#define txn_abort txn_abort_ydb
#define log_compare log_compare_ydb

#include "../include/db.h"
#undef DB_BTREE
#undef DB_NOTICE_LOGFILE_CHANGED
#undef DBTYPE
#undef db_notices
#undef txn_abort
#undef txn_begin
#undef txn_commit
#undef DB_VERB_CHKPOINT
#undef DB_VERB_DEADLOCK
#undef DB_VERB_RECOVERY
#undef DB
#undef DB_BTREE_STAT
#undef DB_ENV
#undef DB_KEY_RANGE
#undef DB_LSN
#undef DB_TXN
#undef DB_TXN_ACTIVE
#undef DB_TXN_STAT
#undef DBC
#undef DBT
#undef DB_DBT_MALLOC
#undef DB_DBT_REALLOC
#undef DB_DBT_USERMEM
#undef DB_DBT_DUPOK
#undef DB_VERSION_STRING
#undef DB_ARCH_ABS
#undef DB_ARCH_LOG
#undef DB_FIRST
#undef DB_GET_BOTH
#undef DB_LAST
#undef DB_NEXT
#undef DB_NEXT_DUP
#undef DB_PREV
#undef DB_SET
#undef DB_SET_RANGE
#undef DB_RMW
#undef DB_KEYEMPTY
#undef DB_KEYEXIST
#undef DB_LOCK_DEADLOCK
#undef DB_NOTFOUND
#undef DB_CREATE
#undef DB_RDONLY
#undef DB_RECOVER
#undef DB_THREAD
#undef DB_TXN_NOSYNC
#undef DB_PRIVATE
#undef DB_LOCK_DEFAULT
#undef DB_LOCK_OLDEST
#undef DB_LOCK_RANDOM
#undef DB_DUP
#undef DB_NOOVERWRITE
#undef DB_INIT_LOCK
#undef DB_INIT_LOG
#undef DB_INIT_MPOOL
#undef DB_INIT_TXN
#undef db_create
#undef db_env_create
#undef txn_begin
#undef txn_commit
#undef txn_abort
#undef log_compare

#endif
