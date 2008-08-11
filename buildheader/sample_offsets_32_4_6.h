/* BDB offsets on a 32-bit machine */
#define DB_VERSION_MAJOR_32 4
#define DB_VERSION_MINOR_32 6
#define DB_VERSION_STRING_32 "Berkeley DB Compatability Header 4.6"
struct fieldinfo db_btree_stat_fields32[] = {
  {"u_int32_t bt_nkeys", 12, 4},
  {"u_int32_t bt_ndata", 16, 4},
  {0, 84, 84} /* size of whole struct */
};
struct fieldinfo db_env_fields32[] = {
  {"void *app_private", 52, 4},
  {"void *api1_internal", 356, 4},
  {"int  (*close) (DB_ENV *, u_int32_t)", 392, 4},
  {"void (*err) (const DB_ENV *, int, const char *, ...)", 404, 4},
  {"int (*get_cachesize) (DB_ENV *, u_int32_t *, u_int32_t *, int *)", 420, 4},
  {"int (*get_flags) (DB_ENV *, u_int32_t *)", 444, 4},
  {"int  (*get_lg_max) (DB_ENV *, u_int32_t*)", 464, 4},
  {"int  (*get_lk_max_locks) (DB_ENV *, u_int32_t *)", 484, 4},
  {"int  (*log_archive) (DB_ENV *, char **[], u_int32_t)", 576, 4},
  {"int  (*log_flush) (DB_ENV *, const DB_LSN *)", 588, 4},
  {"int  (*open) (DB_ENV *, const char *, u_int32_t, int)", 692, 4},
  {"int  (*set_cachesize) (DB_ENV *, u_int32_t, u_int32_t, int)", 816, 4},
  {"int  (*set_data_dir) (DB_ENV *, const char *)", 824, 4},
  {"void (*set_errcall) (DB_ENV *, void (*)(const DB_ENV *, const char *, const char *))", 832, 4},
  {"void (*set_errfile) (DB_ENV *, FILE*)", 836, 4},
  {"void (*set_errpfx) (DB_ENV *, const char *)", 840, 4},
  {"int  (*set_flags) (DB_ENV *, u_int32_t, int)", 852, 4},
  {"int  (*set_lg_bsize) (DB_ENV *, u_int32_t)", 864, 4},
  {"int  (*set_lg_dir) (DB_ENV *, const char *)", 868, 4},
  {"int  (*set_lg_max) (DB_ENV *, u_int32_t)", 876, 4},
  {"int  (*set_lk_detect) (DB_ENV *, u_int32_t)", 888, 4},
  {"int  (*set_lk_max_locks) (DB_ENV *, u_int32_t)", 896, 4},
  {"int  (*set_tmp_dir) (DB_ENV *, const char *)", 956, 4},
  {"int  (*set_verbose) (DB_ENV *, u_int32_t, int)", 968, 4},
  {"int  (*txn_begin) (DB_ENV *, DB_TXN *, DB_TXN **, u_int32_t)", 976, 4},
  {"int  (*txn_checkpoint) (DB_ENV *, u_int32_t, u_int32_t, u_int32_t)", 980, 4},
  {"int  (*txn_stat) (DB_ENV *, DB_TXN_STAT **, u_int32_t)", 988, 4},
  {0, 1016, 1016} /* size of whole struct */
};
struct fieldinfo db_key_range_fields32[] = {
  {"double less", 0, 8},
  {"double equal", 8, 8},
  {"double greater", 16, 8},
  {0, 1016, 1016} /* size of whole struct */
};
struct fieldinfo db_lsn_fields32[] = {
  {0, 8, 8} /* size of whole struct */
};
struct fieldinfo db_fields32[] = {
  {"void *app_private", 20, 4},
  {"DB_ENV *dbenv", 24, 4},
  {"void *api_internal", 276, 4},
  {"int (*associate) (DB*, DB_TXN*, DB*, int(*)(DB*, const DBT*, const DBT*, DBT*), u_int32_t)", 296, 4},
  {"int (*close) (DB*, u_int32_t)", 300, 4},
  {"int (*cursor) (DB *, DB_TXN *, DBC **, u_int32_t)", 308, 4},
  {"int (*del) (DB *, DB_TXN *, DBT *, u_int32_t)", 312, 4},
  {"int (*fd) (DB *, int *)", 328, 4},
  {"int (*get) (DB *, DB_TXN *, DBT *, DBT *, u_int32_t)", 332, 4},
  {"int (*get_flags) (DB *, u_int32_t *)", 368, 4},
  {"int (*get_pagesize) (DB *, u_int32_t *)", 400, 4},
  {"int (*key_range) (DB *, DB_TXN *, DBT *, DB_KEY_RANGE *, u_int32_t)", 440, 4},
  {"int (*open) (DB *, DB_TXN *, const char *, const char *, DBTYPE, u_int32_t, int)", 444, 4},
  {"int (*pget) (DB *, DB_TXN *, DBT *, DBT *, DBT *, u_int32_t)", 448, 4},
  {"int (*put) (DB *, DB_TXN *, DBT *, DBT *, u_int32_t)", 452, 4},
  {"int (*remove) (DB *, const char *, const char *, u_int32_t)", 456, 4},
  {"int (*rename) (DB *, const char *, const char *, const char *, u_int32_t)", 460, 4},
  {"int (*set_bt_compare) (DB *, int (*)(DB *, const DBT *, const DBT *))", 472, 4},
  {"int (*set_dup_compare) (DB *, int (*)(DB *, const DBT *, const DBT *))", 488, 4},
  {"void (*set_errfile) (DB *, FILE*)", 500, 4},
  {"int (*set_flags) (DB *, u_int32_t)", 512, 4},
  {"int (*set_pagesize) (DB *, u_int32_t)", 544, 4},
  {"int (*stat) (DB *, void *, u_int32_t)", 576, 4},
  {"int (*truncate) (DB *, DB_TXN *, u_int32_t *, u_int32_t)", 588, 4},
  {"int (*verify) (DB *, const char *, const char *, FILE *, u_int32_t)", 596, 4},
  {0, 636, 636} /* size of whole struct */
};
struct fieldinfo db_txn_active_fields32[] = {
  {"u_int32_t txnid", 0, 4},
  {"DB_LSN lsn", 16, 8},
  {0, 224, 224} /* size of whole struct */
};
struct fieldinfo db_txn_fields32[] = {
  {"DB_ENV *mgrp /*In TokuDB, mgrp is a DB_ENV not a DB_TXNMGR*/", 0, 4},
  {"DB_TXN *parent", 4, 4},
  {"void *api_internal", 88, 4},
  {"int (*abort) (DB_TXN *)", 100, 4},
  {"int (*commit) (DB_TXN*, u_int32_t)", 104, 4},
  {"u_int32_t (*id) (DB_TXN *)", 116, 4},
  {0, 140, 140} /* size of whole struct */
};
struct fieldinfo db_txn_stat_fields32[] = {
  {"u_int32_t st_nactive", 36, 4},
  {"DB_TXN_ACTIVE *st_txnarray", 52, 4},
  {0, 68, 68} /* size of whole struct */
};
struct fieldinfo dbc_fields32[] = {
  {"DB *dbp", 0, 4},
  {"int (*c_close) (DBC *)", 244, 4},
  {"int (*c_count) (DBC *, db_recno_t *, u_int32_t)", 248, 4},
  {"int (*c_del) (DBC *, u_int32_t)", 252, 4},
  {"int (*c_get) (DBC *, DBT *, DBT *, u_int32_t)", 260, 4},
  {"int (*c_pget) (DBC *, DBT *, DBT *, DBT *, u_int32_t)", 264, 4},
  {"int (*c_put) (DBC *, DBT *, DBT *, u_int32_t)", 268, 4},
  {0, 304, 304} /* size of whole struct */
};
struct fieldinfo dbt_fields32[] = {
  {"void*data", 0, 4},
  {"u_int32_t size", 4, 4},
  {"u_int32_t ulen", 8, 4},
  {"u_int32_t flags", 24, 4},
  {0, 28, 28} /* size of whole struct */
};
