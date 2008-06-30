/* BDB offsets on a 64-bit machine */
#define DB_VERSION_MAJOR_64 4
#define DB_VERSION_MINOR_64 3
#define DB_VERSION_STRING_64 "Berkeley DB Compatability Header 4.3"
struct fieldinfo db_btree_stat_fields64[] = {
  {"u_int32_t bt_nkeys", 12, 4},
  {"u_int32_t bt_ndata", 16, 4},
  {0, 84, 84} /* size of whole struct */
};
struct fieldinfo db_env_fields64[] = {
  {"void *app_private", 88, 8},
  {"void *api1_internal", 392, 8},
  {"int  (*close) (DB_ENV *, u_int32_t)", 456, 8},
  {"void (*err) (const DB_ENV *, int, const char *, ...)", 480, 8},
  {"int  (*open) (DB_ENV *, const char *, u_int32_t, int)", 496, 8},
  {"int  (*set_data_dir) (DB_ENV *, const char *)", 576, 8},
  {"void (*set_errcall) (DB_ENV *, void (*)(const DB_ENV *, const char *, const char *))", 600, 8},
  {"void (*set_errfile) (DB_ENV *, FILE*)", 616, 8},
  {"void (*set_errpfx) (DB_ENV *, const char *)", 632, 8},
  {"int (*get_flags) (DB_ENV *, u_int32_t *)", 648, 8},
  {"int  (*set_flags) (DB_ENV *, u_int32_t, int)", 656, 8},
  {"int  (*set_tmp_dir) (DB_ENV *, const char *)", 768, 8},
  {"int  (*set_verbose) (DB_ENV *, u_int32_t, int)", 784, 8},
  {"int  (*set_lg_bsize) (DB_ENV *, u_int32_t)", 808, 8},
  {"int  (*set_lg_dir) (DB_ENV *, const char *)", 824, 8},
  {"int  (*get_lg_max) (DB_ENV *, u_int32_t*)", 832, 8},
  {"int  (*set_lg_max) (DB_ENV *, u_int32_t)", 840, 8},
  {"int  (*log_archive) (DB_ENV *, char **[], u_int32_t)", 864, 8},
  {"int  (*log_flush) (DB_ENV *, const DB_LSN *)", 888, 8},
  {"int  (*set_lk_detect) (DB_ENV *, u_int32_t)", 952, 8},
  {"int  (*set_lk_max) (DB_ENV *, u_int32_t)", 960, 8},
  {"int  (*get_lk_max_locks) (DB_ENV *, u_int32_t *)", 968, 8},
  {"int  (*set_lk_max_locks) (DB_ENV *, u_int32_t)", 976, 8},
  {"int (*get_cachesize) (DB_ENV *, u_int32_t *, u_int32_t *, int *)", 1088, 8},
  {"int  (*set_cachesize) (DB_ENV *, u_int32_t, u_int32_t, int)", 1096, 8},
  {"int  (*txn_begin) (DB_ENV *, DB_TXN *, DB_TXN **, u_int32_t)", 1328, 8},
  {"int  (*txn_checkpoint) (DB_ENV *, u_int32_t, u_int32_t, u_int32_t)", 1336, 8},
  {"int  (*txn_stat) (DB_ENV *, DB_TXN_STAT **, u_int32_t)", 1352, 8},
  {0, 1400, 1400} /* size of whole struct */
};
struct fieldinfo db_key_range_fields64[] = {
  {"double less", 0, 8},
  {"double equal", 8, 8},
  {"double greater", 16, 8},
  {0, 1400, 1400} /* size of whole struct */
};
struct fieldinfo db_lsn_fields64[] = {
  {0, 8, 8} /* size of whole struct */
};
struct fieldinfo db_fields64[] = {
  {"void *app_private", 32, 8},
  {"DB_ENV *dbenv", 40, 8},
  {"void *api_internal", 400, 8},
  {"int (*associate) (DB*, DB_TXN*, DB*, int(*)(DB*, const DBT*, const DBT*, DBT*), u_int32_t)", 440, 8},
  {"int (*close) (DB*, u_int32_t)", 448, 8},
  {"int (*cursor) (DB *, DB_TXN *, DBC **, u_int32_t)", 456, 8},
  {"int (*del) (DB *, DB_TXN *, DBT *, u_int32_t)", 464, 8},
  {"int (*fd) (DB *, int *)", 496, 8},
  {"int (*get) (DB *, DB_TXN *, DBT *, DBT *, u_int32_t)", 504, 8},
  {"int (*pget) (DB *, DB_TXN *, DBT *, DBT *, DBT *, u_int32_t)", 512, 8},
  {"int (*get_flags) (DB *, u_int32_t *)", 576, 8},
  {"int (*get_pagesize) (DB *, u_int32_t *)", 600, 8},
  {"int (*key_range) (DB *, DB_TXN *, DBT *, DB_KEY_RANGE *, u_int32_t)", 632, 8},
  {"int (*open) (DB *, DB_TXN *, const char *, const char *, DBTYPE, u_int32_t, int)", 640, 8},
  {"int (*put) (DB *, DB_TXN *, DBT *, DBT *, u_int32_t)", 648, 8},
  {"int (*remove) (DB *, const char *, const char *, u_int32_t)", 656, 8},
  {"int (*rename) (DB *, const char *, const char *, const char *, u_int32_t)", 664, 8},
  {"int (*truncate) (DB *, DB_TXN *, u_int32_t *, u_int32_t)", 672, 8},
  {"int (*set_dup_compare) (DB *, int (*)(DB *, const DBT *, const DBT *))", 704, 8},
  {"void (*set_errfile) (DB *, FILE*)", 728, 8},
  {"int (*set_flags) (DB *, u_int32_t)", 752, 8},
  {"int (*set_pagesize) (DB *, u_int32_t)", 792, 8},
  {"int (*stat) (DB *, void *, u_int32_t)", 808, 8},
  {"int (*verify) (DB *, const char *, const char *, FILE *, u_int32_t)", 840, 8},
  {"int (*set_bt_compare) (DB *, int (*)(DB *, const DBT *, const DBT *))", 856, 8},
  {0, 1056, 1056} /* size of whole struct */
};
struct fieldinfo db_txn_active_fields64[] = {
  {"u_int32_t txnid", 0, 4},
  {"DB_LSN lsn", 8, 8},
  {0, 148, 148} /* size of whole struct */
};
struct fieldinfo db_txn_fields64[] = {
  {"DB_ENV *mgrp /*In TokuDB, mgrp is a DB_ENV not a DB_TXNMGR*/", 0, 8},
  {"void *api_internal", 152, 8},
  {"int (*abort) (DB_TXN *)", 176, 8},
  {"int (*commit) (DB_TXN*, u_int32_t)", 184, 8},
  {"u_int32_t (*id) (DB_TXN *)", 200, 8},
  {0, 240, 240} /* size of whole struct */
};
struct fieldinfo db_txn_stat_fields64[] = {
  {"u_int32_t st_nactive", 36, 4},
  {"DB_TXN_ACTIVE *st_txnarray", 48, 8},
  {0, 72, 72} /* size of whole struct */
};
struct fieldinfo dbc_fields64[] = {
  {"DB *dbp", 0, 8},
  {"int (*c_close) (DBC *)", 264, 8},
  {"int (*c_count) (DBC *, db_recno_t *, u_int32_t)", 272, 8},
  {"int (*c_del) (DBC *, u_int32_t)", 280, 8},
  {"int (*c_get) (DBC *, DBT *, DBT *, u_int32_t)", 296, 8},
  {"int (*c_pget) (DBC *, DBT *, DBT *, DBT *, u_int32_t)", 304, 8},
  {"int (*c_put) (DBC *, DBT *, DBT *, u_int32_t)", 312, 8},
  {0, 384, 384} /* size of whole struct */
};
struct fieldinfo dbt_fields64[] = {
  {"void*data", 0, 8},
  {"u_int32_t size", 8, 4},
  {"u_int32_t ulen", 12, 4},
  {"u_int32_t flags", 24, 4},
  {0, 32, 32} /* size of whole struct */
};
