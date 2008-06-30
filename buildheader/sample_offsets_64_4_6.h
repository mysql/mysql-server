/* BDB offsets on a 64-bit machine */
#define DB_VERSION_MAJOR_64 4
#define DB_VERSION_MINOR_64 6
#define DB_VERSION_STRING_64 "Berkeley DB Compatability Header 4.6"
struct fieldinfo db_btree_stat_fields64[] = {
  {"u_int32_t bt_nkeys", 12, 4},
  {"u_int32_t bt_ndata", 16, 4},
  {0, 84, 84} /* size of whole struct */
};
struct fieldinfo db_env_fields64[] = {
  {"void *app_private", 104, 8},
  {"void *api1_internal", 568, 8},
  {"int  (*close) (DB_ENV *, u_int32_t)", 640, 8},
  {"void (*err) (const DB_ENV *, int, const char *, ...)", 664, 8},
  {"int (*get_cachesize) (DB_ENV *, u_int32_t *, u_int32_t *, int *)", 696, 8},
  {"int (*get_flags) (DB_ENV *, u_int32_t *)", 744, 8},
  {"int  (*get_lg_max) (DB_ENV *, u_int32_t*)", 784, 8},
  {"int  (*get_lk_max_locks) (DB_ENV *, u_int32_t *)", 824, 8},
  {"int  (*log_archive) (DB_ENV *, char **[], u_int32_t)", 1008, 8},
  {"int  (*log_flush) (DB_ENV *, const DB_LSN *)", 1032, 8},
  {"int  (*open) (DB_ENV *, const char *, u_int32_t, int)", 1240, 8},
  {"int  (*set_cachesize) (DB_ENV *, u_int32_t, u_int32_t, int)", 1488, 8},
  {"int  (*set_data_dir) (DB_ENV *, const char *)", 1504, 8},
  {"void (*set_errcall) (DB_ENV *, void (*)(const DB_ENV *, const char *, const char *))", 1520, 8},
  {"void (*set_errfile) (DB_ENV *, FILE*)", 1528, 8},
  {"void (*set_errpfx) (DB_ENV *, const char *)", 1536, 8},
  {"int  (*set_flags) (DB_ENV *, u_int32_t, int)", 1560, 8},
  {"int  (*set_lg_bsize) (DB_ENV *, u_int32_t)", 1584, 8},
  {"int  (*set_lg_dir) (DB_ENV *, const char *)", 1592, 8},
  {"int  (*set_lg_max) (DB_ENV *, u_int32_t)", 1608, 8},
  {"int  (*set_lk_detect) (DB_ENV *, u_int32_t)", 1632, 8},
  {"int  (*set_lk_max_locks) (DB_ENV *, u_int32_t)", 1648, 8},
  {"int  (*set_tmp_dir) (DB_ENV *, const char *)", 1768, 8},
  {"int  (*set_verbose) (DB_ENV *, u_int32_t, int)", 1792, 8},
  {"int  (*txn_begin) (DB_ENV *, DB_TXN *, DB_TXN **, u_int32_t)", 1808, 8},
  {"int  (*txn_checkpoint) (DB_ENV *, u_int32_t, u_int32_t, u_int32_t)", 1816, 8},
  {"int  (*txn_stat) (DB_ENV *, DB_TXN_STAT **, u_int32_t)", 1832, 8},
  {0, 1872, 1872} /* size of whole struct */
};
struct fieldinfo db_key_range_fields64[] = {
  {"double less", 0, 8},
  {"double equal", 8, 8},
  {"double greater", 16, 8},
  {0, 1872, 1872} /* size of whole struct */
};
struct fieldinfo db_lsn_fields64[] = {
  {0, 8, 8} /* size of whole struct */
};
struct fieldinfo db_fields64[] = {
  {"void *app_private", 32, 8},
  {"DB_ENV *dbenv", 40, 8},
  {"void *api_internal", 464, 8},
  {"int (*associate) (DB*, DB_TXN*, DB*, int(*)(DB*, const DBT*, const DBT*, DBT*), u_int32_t)", 504, 8},
  {"int (*close) (DB*, u_int32_t)", 512, 8},
  {"int (*cursor) (DB *, DB_TXN *, DBC **, u_int32_t)", 528, 8},
  {"int (*del) (DB *, DB_TXN *, DBT *, u_int32_t)", 536, 8},
  {"int (*fd) (DB *, int *)", 568, 8},
  {"int (*get) (DB *, DB_TXN *, DBT *, DBT *, u_int32_t)", 576, 8},
  {"int (*get_flags) (DB *, u_int32_t *)", 648, 8},
  {"int (*get_pagesize) (DB *, u_int32_t *)", 712, 8},
  {"int (*key_range) (DB *, DB_TXN *, DBT *, DB_KEY_RANGE *, u_int32_t)", 792, 8},
  {"int (*open) (DB *, DB_TXN *, const char *, const char *, DBTYPE, u_int32_t, int)", 800, 8},
  {"int (*pget) (DB *, DB_TXN *, DBT *, DBT *, DBT *, u_int32_t)", 808, 8},
  {"int (*put) (DB *, DB_TXN *, DBT *, DBT *, u_int32_t)", 816, 8},
  {"int (*remove) (DB *, const char *, const char *, u_int32_t)", 824, 8},
  {"int (*rename) (DB *, const char *, const char *, const char *, u_int32_t)", 832, 8},
  {"int (*set_bt_compare) (DB *, int (*)(DB *, const DBT *, const DBT *))", 856, 8},
  {"int (*set_dup_compare) (DB *, int (*)(DB *, const DBT *, const DBT *))", 888, 8},
  {"void (*set_errfile) (DB *, FILE*)", 912, 8},
  {"int (*set_flags) (DB *, u_int32_t)", 936, 8},
  {"int (*set_pagesize) (DB *, u_int32_t)", 1000, 8},
  {"int (*stat) (DB *, void *, u_int32_t)", 1064, 8},
  {"int (*truncate) (DB *, DB_TXN *, u_int32_t *, u_int32_t)", 1088, 8},
  {"int (*verify) (DB *, const char *, const char *, FILE *, u_int32_t)", 1104, 8},
  {0, 1168, 1168} /* size of whole struct */
};
struct fieldinfo db_txn_active_fields64[] = {
  {"u_int32_t txnid", 0, 4},
  {"DB_LSN lsn", 24, 8},
  {0, 232, 232} /* size of whole struct */
};
struct fieldinfo db_txn_fields64[] = {
  {"DB_ENV *mgrp /*In TokuDB, mgrp is a DB_ENV not a DB_TXNMGR*/", 0, 8},
  {"void *api_internal", 168, 8},
  {"int (*abort) (DB_TXN *)", 192, 8},
  {"int (*commit) (DB_TXN*, u_int32_t)", 200, 8},
  {"u_int32_t (*id) (DB_TXN *)", 224, 8},
  {0, 272, 272} /* size of whole struct */
};
struct fieldinfo db_txn_stat_fields64[] = {
  {"u_int32_t st_nactive", 44, 4},
  {"DB_TXN_ACTIVE *st_txnarray", 64, 8},
  {0, 88, 88} /* size of whole struct */
};
struct fieldinfo dbc_fields64[] = {
  {"DB *dbp", 0, 8},
  {"int (*c_close) (DBC *)", 384, 8},
  {"int (*c_count) (DBC *, db_recno_t *, u_int32_t)", 392, 8},
  {"int (*c_del) (DBC *, u_int32_t)", 400, 8},
  {"int (*c_get) (DBC *, DBT *, DBT *, u_int32_t)", 416, 8},
  {"int (*c_pget) (DBC *, DBT *, DBT *, DBT *, u_int32_t)", 424, 8},
  {"int (*c_put) (DBC *, DBT *, DBT *, u_int32_t)", 432, 8},
  {0, 504, 504} /* size of whole struct */
};
struct fieldinfo dbt_fields64[] = {
  {"void*data", 0, 8},
  {"u_int32_t size", 8, 4},
  {"u_int32_t ulen", 12, 4},
  {"u_int32_t flags", 32, 4},
  {0, 40, 40} /* size of whole struct */
};
