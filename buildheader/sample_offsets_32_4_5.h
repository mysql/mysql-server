/* BDB offsets on a 32-bit machine */
#define DB_VERSION_MAJOR_32 4
#define DB_VERSION_MINOR_32 5
#define DB_VERSION_STRING_32 "Berkeley DB Compatability Header 4.5"
struct fieldinfo db_btree_stat_fields32[] = {
  {"u_int32_t bt_nkeys", 12, 4},
  {"u_int32_t bt_ndata", 16, 4},
  {0, 80, 80} /* size of whole struct */
};
struct fieldinfo db_env_fields32[] = {
  {"void *app_private", 52, 4},
  {"void *api1_internal", 336, 4},
  {"int  (*close) (DB_ENV *, u_int32_t)", 372, 4},
  {"void (*err) (const DB_ENV *, int, const char *, ...)", 384, 4},
  {"int (*get_cachesize) (DB_ENV *, u_int32_t *, u_int32_t *, int *)", 400, 4},
  {"int (*get_flags) (DB_ENV *, u_int32_t *)", 420, 4},
  {"int  (*get_lg_max) (DB_ENV *, u_int32_t*)", 440, 4},
  {"int  (*get_lk_max_locks) (DB_ENV *, u_int32_t *)", 460, 4},
  {"int  (*log_archive) (DB_ENV *, char **[], u_int32_t)", 548, 4},
  {"int  (*log_flush) (DB_ENV *, const DB_LSN *)", 560, 4},
  {"int  (*open) (DB_ENV *, const char *, u_int32_t, int)", 664, 4},
  {"int  (*set_cachesize) (DB_ENV *, u_int32_t, u_int32_t, int)", 776, 4},
  {"int  (*set_data_dir) (DB_ENV *, const char *)", 780, 4},
  {"void (*set_errcall) (DB_ENV *, void (*)(const DB_ENV *, const char *, const char *))", 788, 4},
  {"void (*set_errfile) (DB_ENV *, FILE*)", 792, 4},
  {"void (*set_errpfx) (DB_ENV *, const char *)", 796, 4},
  {"int  (*set_flags) (DB_ENV *, u_int32_t, int)", 808, 4},
  {"int  (*set_lg_bsize) (DB_ENV *, u_int32_t)", 820, 4},
  {"int  (*set_lg_dir) (DB_ENV *, const char *)", 824, 4},
  {"int  (*set_lg_max) (DB_ENV *, u_int32_t)", 832, 4},
  {"int  (*set_lk_detect) (DB_ENV *, u_int32_t)", 844, 4},
  {"int  (*set_lk_max_locks) (DB_ENV *, u_int32_t)", 852, 4},
  {"int  (*set_tmp_dir) (DB_ENV *, const char *)", 912, 4},
  {"int  (*set_verbose) (DB_ENV *, u_int32_t, int)", 924, 4},
  {"int  (*txn_begin) (DB_ENV *, DB_TXN *, DB_TXN **, u_int32_t)", 932, 4},
  {"int  (*txn_checkpoint) (DB_ENV *, u_int32_t, u_int32_t, u_int32_t)", 936, 4},
  {"int  (*txn_stat) (DB_ENV *, DB_TXN_STAT **, u_int32_t)", 944, 4},
  {0, 972, 972} /* size of whole struct */
};
struct fieldinfo db_key_range_fields32[] = {
  {"double less", 0, 8},
  {"double equal", 8, 8},
  {"double greater", 16, 8},
  {0, 972, 972} /* size of whole struct */
};
struct fieldinfo db_lsn_fields32[] = {
  {0, 8, 8} /* size of whole struct */
};
struct fieldinfo db_fields32[] = {
  {"void *app_private", 16, 4},
  {"DB_ENV *dbenv", 20, 4},
  {"void *api_internal", 268, 4},
  {"int (*associate) (DB*, DB_TXN*, DB*, int(*)(DB*, const DBT*, const DBT*, DBT*), u_int32_t)", 288, 4},
  {"int (*close) (DB*, u_int32_t)", 292, 4},
  {"int (*cursor) (DB *, DB_TXN *, DBC **, u_int32_t)", 300, 4},
  {"int (*del) (DB *, DB_TXN *, DBT *, u_int32_t)", 304, 4},
  {"int (*fd) (DB *, int *)", 316, 4},
  {"int (*get) (DB *, DB_TXN *, DBT *, DBT *, u_int32_t)", 320, 4},
  {"int (*get_flags) (DB *, u_int32_t *)", 356, 4},
  {"int (*get_pagesize) (DB *, u_int32_t *)", 384, 4},
  {"int (*key_range) (DB *, DB_TXN *, DBT *, DB_KEY_RANGE *, u_int32_t)", 420, 4},
  {"int (*open) (DB *, DB_TXN *, const char *, const char *, DBTYPE, u_int32_t, int)", 424, 4},
  {"int (*pget) (DB *, DB_TXN *, DBT *, DBT *, DBT *, u_int32_t)", 428, 4},
  {"int (*put) (DB *, DB_TXN *, DBT *, DBT *, u_int32_t)", 432, 4},
  {"int (*remove) (DB *, const char *, const char *, u_int32_t)", 436, 4},
  {"int (*rename) (DB *, const char *, const char *, const char *, u_int32_t)", 440, 4},
  {"int (*set_bt_compare) (DB *, int (*)(DB *, const DBT *, const DBT *))", 452, 4},
  {"int (*set_dup_compare) (DB *, int (*)(DB *, const DBT *, const DBT *))", 468, 4},
  {"void (*set_errfile) (DB *, FILE*)", 480, 4},
  {"int (*set_flags) (DB *, u_int32_t)", 492, 4},
  {"int (*set_pagesize) (DB *, u_int32_t)", 520, 4},
  {"int (*stat) (DB *, void *, u_int32_t)", 548, 4},
  {"int (*truncate) (DB *, DB_TXN *, u_int32_t *, u_int32_t)", 560, 4},
  {"int (*verify) (DB *, const char *, const char *, FILE *, u_int32_t)", 568, 4},
  {0, 608, 608} /* size of whole struct */
};
struct fieldinfo db_txn_active_fields32[] = {
  {"u_int32_t txnid", 0, 4},
  {"DB_LSN lsn", 16, 8},
  {0, 224, 224} /* size of whole struct */
};
struct fieldinfo db_txn_fields32[] = {
  {"DB_ENV *mgrp /*In TokuDB, mgrp is a DB_ENV not a DB_TXNMGR*/", 0, 4},
  {"void *api_internal", 84, 4},
  {"int (*abort) (DB_TXN *)", 96, 4},
  {"int (*commit) (DB_TXN*, u_int32_t)", 100, 4},
  {"u_int32_t (*id) (DB_TXN *)", 112, 4},
  {0, 136, 136} /* size of whole struct */
};
struct fieldinfo db_txn_stat_fields32[] = {
  {"u_int32_t st_nactive", 32, 4},
  {"DB_TXN_ACTIVE *st_txnarray", 52, 4},
  {0, 68, 68} /* size of whole struct */
};
struct fieldinfo dbc_fields32[] = {
  {"DB *dbp", 0, 4},
  {"int (*c_close) (DBC *)", 204, 4},
  {"int (*c_count) (DBC *, db_recno_t *, u_int32_t)", 208, 4},
  {"int (*c_del) (DBC *, u_int32_t)", 212, 4},
  {"int (*c_get) (DBC *, DBT *, DBT *, u_int32_t)", 220, 4},
  {"int (*c_pget) (DBC *, DBT *, DBT *, DBT *, u_int32_t)", 224, 4},
  {"int (*c_put) (DBC *, DBT *, DBT *, u_int32_t)", 228, 4},
  {0, 264, 264} /* size of whole struct */
};
struct fieldinfo dbt_fields32[] = {
  {"void*data", 0, 4},
  {"u_int32_t size", 4, 4},
  {"u_int32_t ulen", 8, 4},
  {"u_int32_t flags", 24, 4},
  {0, 28, 28} /* size of whole struct */
};
