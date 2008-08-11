/* BDB offsets on a 32-bit machine */
#define DB_VERSION_MAJOR_32 4
#define DB_VERSION_MINOR_32 3
#define DB_VERSION_STRING_32 "Berkeley DB Compatability Header 4.3"
struct fieldinfo db_btree_stat_fields32[] = {
  {"u_int32_t bt_nkeys", 12, 4},
  {"u_int32_t bt_ndata", 16, 4},
  {0, 84, 84} /* size of whole struct */
};
struct fieldinfo db_env_fields32[] = {
  {"void *app_private", 44, 4},
  {"void *api1_internal", 244, 4},
  {"int  (*close) (DB_ENV *, u_int32_t)", 276, 4},
  {"void (*err) (const DB_ENV *, int, const char *, ...)", 288, 4},
  {"int  (*open) (DB_ENV *, const char *, u_int32_t, int)", 296, 4},
  {"int  (*set_data_dir) (DB_ENV *, const char *)", 336, 4},
  {"void (*set_errcall) (DB_ENV *, void (*)(const DB_ENV *, const char *, const char *))", 348, 4},
  {"void (*set_errfile) (DB_ENV *, FILE*)", 356, 4},
  {"void (*set_errpfx) (DB_ENV *, const char *)", 364, 4},
  {"int (*get_flags) (DB_ENV *, u_int32_t *)", 372, 4},
  {"int  (*set_flags) (DB_ENV *, u_int32_t, int)", 376, 4},
  {"int  (*set_tmp_dir) (DB_ENV *, const char *)", 432, 4},
  {"int  (*set_verbose) (DB_ENV *, u_int32_t, int)", 440, 4},
  {"int  (*set_lg_bsize) (DB_ENV *, u_int32_t)", 452, 4},
  {"int  (*set_lg_dir) (DB_ENV *, const char *)", 460, 4},
  {"int  (*get_lg_max) (DB_ENV *, u_int32_t*)", 464, 4},
  {"int  (*set_lg_max) (DB_ENV *, u_int32_t)", 468, 4},
  {"int  (*log_archive) (DB_ENV *, char **[], u_int32_t)", 480, 4},
  {"int  (*log_flush) (DB_ENV *, const DB_LSN *)", 492, 4},
  {"int  (*set_lk_detect) (DB_ENV *, u_int32_t)", 524, 4},
  {"int  (*set_lk_max) (DB_ENV *, u_int32_t)", 528, 4},
  {"int  (*get_lk_max_locks) (DB_ENV *, u_int32_t *)", 532, 4},
  {"int  (*set_lk_max_locks) (DB_ENV *, u_int32_t)", 536, 4},
  {"int (*get_cachesize) (DB_ENV *, u_int32_t *, u_int32_t *, int *)", 592, 4},
  {"int  (*set_cachesize) (DB_ENV *, u_int32_t, u_int32_t, int)", 596, 4},
  {"int  (*txn_begin) (DB_ENV *, DB_TXN *, DB_TXN **, u_int32_t)", 712, 4},
  {"int  (*txn_checkpoint) (DB_ENV *, u_int32_t, u_int32_t, u_int32_t)", 716, 4},
  {"int  (*txn_stat) (DB_ENV *, DB_TXN_STAT **, u_int32_t)", 724, 4},
  {0, 756, 756} /* size of whole struct */
};
struct fieldinfo db_key_range_fields32[] = {
  {"double less", 0, 8},
  {"double equal", 8, 8},
  {"double greater", 16, 8},
  {0, 756, 756} /* size of whole struct */
};
struct fieldinfo db_lsn_fields32[] = {
  {0, 8, 8} /* size of whole struct */
};
struct fieldinfo db_fields32[] = {
  {"void *app_private", 16, 4},
  {"DB_ENV *dbenv", 20, 4},
  {"void *api_internal", 248, 4},
  {"int (*associate) (DB*, DB_TXN*, DB*, int(*)(DB*, const DBT*, const DBT*, DBT*), u_int32_t)", 268, 4},
  {"int (*close) (DB*, u_int32_t)", 272, 4},
  {"int (*cursor) (DB *, DB_TXN *, DBC **, u_int32_t)", 276, 4},
  {"int (*del) (DB *, DB_TXN *, DBT *, u_int32_t)", 280, 4},
  {"int (*fd) (DB *, int *)", 296, 4},
  {"int (*get) (DB *, DB_TXN *, DBT *, DBT *, u_int32_t)", 300, 4},
  {"int (*pget) (DB *, DB_TXN *, DBT *, DBT *, DBT *, u_int32_t)", 304, 4},
  {"int (*get_flags) (DB *, u_int32_t *)", 336, 4},
  {"int (*get_pagesize) (DB *, u_int32_t *)", 348, 4},
  {"int (*key_range) (DB *, DB_TXN *, DBT *, DB_KEY_RANGE *, u_int32_t)", 364, 4},
  {"int (*open) (DB *, DB_TXN *, const char *, const char *, DBTYPE, u_int32_t, int)", 368, 4},
  {"int (*put) (DB *, DB_TXN *, DBT *, DBT *, u_int32_t)", 372, 4},
  {"int (*remove) (DB *, const char *, const char *, u_int32_t)", 376, 4},
  {"int (*rename) (DB *, const char *, const char *, const char *, u_int32_t)", 380, 4},
  {"int (*truncate) (DB *, DB_TXN *, u_int32_t *, u_int32_t)", 384, 4},
  {"int (*set_dup_compare) (DB *, int (*)(DB *, const DBT *, const DBT *))", 400, 4},
  {"void (*set_errfile) (DB *, FILE*)", 412, 4},
  {"int (*set_flags) (DB *, u_int32_t)", 424, 4},
  {"int (*set_pagesize) (DB *, u_int32_t)", 444, 4},
  {"int (*stat) (DB *, void *, u_int32_t)", 452, 4},
  {"int (*verify) (DB *, const char *, const char *, FILE *, u_int32_t)", 468, 4},
  {"int (*set_bt_compare) (DB *, int (*)(DB *, const DBT *, const DBT *))", 476, 4},
  {0, 580, 580} /* size of whole struct */
};
struct fieldinfo db_txn_active_fields32[] = {
  {"u_int32_t txnid", 0, 4},
  {"DB_LSN lsn", 8, 8},
  {0, 148, 148} /* size of whole struct */
};
struct fieldinfo db_txn_fields32[] = {
  {"DB_ENV *mgrp /*In TokuDB, mgrp is a DB_ENV not a DB_TXNMGR*/", 0, 4},
  {"DB_TXN *parent", 4, 4},
  {"void *api_internal", 88, 4},
  {"int (*abort) (DB_TXN *)", 100, 4},
  {"int (*commit) (DB_TXN*, u_int32_t)", 104, 4},
  {"u_int32_t (*id) (DB_TXN *)", 112, 4},
  {0, 132, 132} /* size of whole struct */
};
struct fieldinfo db_txn_stat_fields32[] = {
  {"u_int32_t st_nactive", 32, 4},
  {"DB_TXN_ACTIVE *st_txnarray", 44, 4},
  {0, 60, 60} /* size of whole struct */
};
struct fieldinfo dbc_fields32[] = {
  {"DB *dbp", 0, 4},
  {"int (*c_close) (DBC *)", 188, 4},
  {"int (*c_count) (DBC *, db_recno_t *, u_int32_t)", 192, 4},
  {"int (*c_del) (DBC *, u_int32_t)", 196, 4},
  {"int (*c_get) (DBC *, DBT *, DBT *, u_int32_t)", 204, 4},
  {"int (*c_pget) (DBC *, DBT *, DBT *, DBT *, u_int32_t)", 208, 4},
  {"int (*c_put) (DBC *, DBT *, DBT *, u_int32_t)", 212, 4},
  {0, 248, 248} /* size of whole struct */
};
struct fieldinfo dbt_fields32[] = {
  {"void*data", 0, 4},
  {"u_int32_t size", 4, 4},
  {"u_int32_t ulen", 8, 4},
  {"u_int32_t flags", 20, 4},
  {0, 24, 24} /* size of whole struct */
};
