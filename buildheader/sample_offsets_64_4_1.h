/* BDB offsets on a 64-bit machine */
#define DB_VERSION_MAJOR_64 4
#define DB_VERSION_MINOR_64 1
#define DB_VERSION_STRING_64 "Berkeley DB Compatability Header 4.1"
struct fieldinfo db_btree_stat_fields64[] = {
  {"u_int32_t bt_nkeys", 12, 4},
  {"u_int32_t bt_ndata", 16, 4},
  {0, 80, 80} /* size of whole struct */
};
struct fieldinfo db_env_fields64[] = {
  {"void *app_private", 72, 8},
  {"void *api1_internal", 360, 8},
  {"int  (*close) (DB_ENV *, u_int32_t)", 424, 8},
  {"void (*err) (const DB_ENV *, int, const char *, ...)", 448, 8},
  {"int  (*open) (DB_ENV *, const char *, u_int32_t, int)", 464, 8},
  {"int  (*set_data_dir) (DB_ENV *, const char *)", 480, 8},
  {"void (*set_errcall) (DB_ENV *, void (*)(const char *, char *))", 512, 8},
  {"void (*set_errfile) (DB_ENV *, FILE*)", 520, 8},
  {"void (*set_errpfx) (DB_ENV *, const char *)", 528, 8},
  {"int  (*set_flags) (DB_ENV *, u_int32_t, int)", 544, 8},
  {"int  (*set_tmp_dir) (DB_ENV *, const char *)", 584, 8},
  {"int  (*set_verbose) (DB_ENV *, u_int32_t, int)", 592, 8},
  {"int  (*set_lg_bsize) (DB_ENV *, u_int32_t)", 608, 8},
  {"int  (*set_lg_dir) (DB_ENV *, const char *)", 616, 8},
  {"int  (*set_lg_max) (DB_ENV *, u_int32_t)", 624, 8},
  {"int  (*log_archive) (DB_ENV *, char **[], u_int32_t)", 640, 8},
  {"int  (*log_flush) (DB_ENV *, const DB_LSN *)", 664, 8},
  {"int  (*set_lk_detect) (DB_ENV *, u_int32_t)", 704, 8},
  {"int  (*set_lk_max) (DB_ENV *, u_int32_t)", 712, 8},
  {"int  (*set_lk_max_locks) (DB_ENV *, u_int32_t)", 720, 8},
  {"int  (*set_cachesize) (DB_ENV *, u_int32_t, u_int32_t, int)", 840, 8},
  {"int  (*txn_begin) (DB_ENV *, DB_TXN *, DB_TXN **, u_int32_t)", 1016, 8},
  {"int  (*txn_checkpoint) (DB_ENV *, u_int32_t, u_int32_t, u_int32_t)", 1024, 8},
  {"int  (*txn_stat) (DB_ENV *, DB_TXN_STAT **, u_int32_t)", 1048, 8},
  {0, 1080, 1080} /* size of whole struct */
};
struct fieldinfo db_key_range_fields64[] = {
  {"double less", 0, 8},
  {"double equal", 8, 8},
  {"double greater", 16, 8},
  {0, 1080, 1080} /* size of whole struct */
};
struct fieldinfo db_lsn_fields64[] = {
  {0, 8, 8} /* size of whole struct */
};
struct fieldinfo db_fields64[] = {
  {"void *app_private", 32, 8},
  {"DB_ENV *dbenv", 40, 8},
  {"void *api_internal", 376, 8},
  {"int (*associate) (DB*, DB_TXN*, DB*, int(*)(DB*, const DBT*, const DBT*, DBT*), u_int32_t)", 416, 8},
  {"int (*close) (DB*, u_int32_t)", 424, 8},
  {"int (*cursor) (DB *, DB_TXN *, DBC **, u_int32_t)", 432, 8},
  {"int (*del) (DB *, DB_TXN *, DBT *, u_int32_t)", 440, 8},
  {"int (*fd) (DB *, int *)", 464, 8},
  {"int (*get) (DB *, DB_TXN *, DBT *, DBT *, u_int32_t)", 472, 8},
  {"int (*pget) (DB *, DB_TXN *, DBT *, DBT *, DBT *, u_int32_t)", 480, 8},
  {"int (*key_range) (DB *, DB_TXN *, DBT *, DB_KEY_RANGE *, u_int32_t)", 512, 8},
  {"int (*open) (DB *, DB_TXN *, const char *, const char *, DBTYPE, u_int32_t, int)", 520, 8},
  {"int (*put) (DB *, DB_TXN *, DBT *, DBT *, u_int32_t)", 528, 8},
  {"int (*remove) (DB *, const char *, const char *, u_int32_t)", 536, 8},
  {"int (*rename) (DB *, const char *, const char *, const char *, u_int32_t)", 544, 8},
  {"int (*truncate) (DB *, DB_TXN *, u_int32_t *, u_int32_t)", 552, 8},
  {"int (*set_dup_compare) (DB *, int (*)(DB *, const DBT *, const DBT *))", 592, 8},
  {"void (*set_errfile) (DB *, FILE*)", 616, 8},
  {"int (*set_flags) (DB *, u_int32_t)", 640, 8},
  {"int (*set_pagesize) (DB *, u_int32_t)", 656, 8},
  {"int (*stat) (DB *, void *, u_int32_t)", 672, 8},
  {"int (*verify) (DB *, const char *, const char *, FILE *, u_int32_t)", 696, 8},
  {"int (*set_bt_compare) (DB *, int (*)(DB *, const DBT *, const DBT *))", 704, 8},
  {0, 840, 840} /* size of whole struct */
};
struct fieldinfo db_txn_active_fields64[] = {
  {"u_int32_t txnid", 0, 4},
  {"DB_LSN lsn", 8, 8},
  {0, 16, 16} /* size of whole struct */
};
struct fieldinfo db_txn_fields64[] = {
  {"DB_ENV *mgrp /*In TokuDB, mgrp is a DB_ENV not a DB_TXNMGR*/", 0, 8},
  {"void *api_internal", 112, 8},
  {"int (*abort) (DB_TXN *)", 128, 8},
  {"int (*commit) (DB_TXN*, u_int32_t)", 136, 8},
  {"u_int32_t (*id) (DB_TXN *)", 152, 8},
  {0, 184, 184} /* size of whole struct */
};
struct fieldinfo db_txn_stat_fields64[] = {
  {"u_int32_t st_nactive", 36, 4},
  {"DB_TXN_ACTIVE *st_txnarray", 48, 8},
  {0, 72, 72} /* size of whole struct */
};
struct fieldinfo dbc_fields64[] = {
  {"DB *dbp", 0, 8},
  {"int (*c_close) (DBC *)", 272, 8},
  {"int (*c_count) (DBC *, db_recno_t *, u_int32_t)", 280, 8},
  {"int (*c_del) (DBC *, u_int32_t)", 288, 8},
  {"int (*c_get) (DBC *, DBT *, DBT *, u_int32_t)", 304, 8},
  {"int (*c_pget) (DBC *, DBT *, DBT *, DBT *, u_int32_t)", 312, 8},
  {"int (*c_put) (DBC *, DBT *, DBT *, u_int32_t)", 320, 8},
  {0, 400, 400} /* size of whole struct */
};
struct fieldinfo dbt_fields64[] = {
  {"void*data", 0, 8},
  {"u_int32_t size", 8, 4},
  {"u_int32_t ulen", 12, 4},
  {"u_int32_t flags", 24, 4},
  {0, 32, 32} /* size of whole struct */
};
