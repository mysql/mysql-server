/* BDB offsets on a 64-bit machine */
struct fieldinfo db_fields64[] = {
  {"void *app_private", 32, 8},
  {"int (*close) (DB*, u_int32_t)", 448, 8},
  {"int (*cursor) (DB *, DB_TXN *, DBC **, u_int32_t)", 456, 8},
  {"int (*del) (DB *, DB_TXN *, DBT *, u_int32_t)", 464, 8},
  {"int (*get) (DB *, DB_TXN *, DBT *, DBT *, u_int32_t)", 496, 8},
  {"int (*key_range) (DB *, DB_TXN *, DBT *, DB_KEY_RANGE *, u_int32_t)", 536, 8},
  {"int (*open) (DB *, DB_TXN *, const char *, const char *, DBTYPE, u_int32_t, int)", 544, 8},
  {"int (*put) (DB *, DB_TXN *, DBT *, DBT *, u_int32_t)", 552, 8},
  {"int (*remove) (DB *, const char *, const char *, u_int32_t)", 560, 8},
  {"int (*rename) (DB *, const char *, const char *, const char *, u_int32_t)", 568, 8},
  {"int (*set_flags) (DB *, u_int32_t)", 664, 8},
  {"int (*stat) (DB *, void *, u_int32_t)", 696, 8},
  {"int (*set_bt_compare) (DB *, int (*)(DB *, const DBT *, const DBT *))", 728, 8},
  {0, 0, 864} /* size of whole struct */
};
struct fieldinfo dbt_fields64[] = {
  {"void*data", 0, 8},
  {"u_int32_t size", 8, 4},
  {"u_int32_t ulen", 12, 4},
  {"void*app_private", 24, 8},
  {"u_int32_t flags", 32, 4},
  {0, 0, 40} /* size of whole struct */
};
struct fieldinfo db_txn_fields64[] = {
  {"int (*commit) (DB_TXN*, u_int32_t)", 136, 8},
  {"u_int32_t (*id) (DB_TXN *)", 152, 8},
  {0, 0, 184} /* size of whole struct */
};
struct fieldinfo dbc_fields64[] = {
  {"int (*c_close) (DBC *)", 304, 8},
  {"int (*c_del) (DBC *, u_int32_t)", 320, 8},
  {"int (*c_get) (DBC *, DBT *, DBT *, u_int32_t)", 336, 8},
  {0, 0, 432} /* size of whole struct */
};
struct fieldinfo db_env_fields64[] = {
  {"int  (*close) (DB_ENV *, u_int32_t)", 432, 8},
  {"void (*err) (const DB_ENV *, int, const char *, ...)", 456, 8},
  {"int  (*open) (DB_ENV *, const char *, u_int32_t, int)", 472, 8},
  {"int  (*set_data_dir) (DB_ENV *, const char *)", 488, 8},
  {"void (*set_errcall) (DB_ENV *, void (*)(const char *, char *))", 520, 8},
  {"void (*set_errpfx) (DB_ENV *, const char *)", 536, 8},
  {"int  (*set_flags) (DB_ENV *, u_int32_t, int)", 552, 8},
  {"void (*set_noticecall) (DB_ENV *, void (*)(DB_ENV *, db_notices))", 560, 8},
  {"int  (*set_tmp_dir) (DB_ENV *, const char *)", 600, 8},
  {"int  (*set_verbose) (DB_ENV *, u_int32_t, int)", 608, 8},
  {"int  (*set_lg_bsize) (DB_ENV *, u_int32_t)", 624, 8},
  {"int  (*set_lg_dir) (DB_ENV *, const char *)", 632, 8},
  {"int  (*set_lg_max) (DB_ENV *, u_int32_t)", 640, 8},
  {"int  (*log_archive) (DB_ENV *, char **[], u_int32_t)", 656, 8},
  {"int  (*log_flush) (DB_ENV *, const DB_LSN *)", 680, 8},
  {"int  (*set_lk_detect) (DB_ENV *, u_int32_t)", 720, 8},
  {"int  (*set_lk_max) (DB_ENV *, u_int32_t)", 728, 8},
  {"int  (*set_cachesize) (DB_ENV *, u_int32_t, u_int32_t, int)", 856, 8},
  {"int  (*txn_checkpoint) (DB_ENV *, u_int32_t, u_int32_t, u_int32_t)", 1040, 8},
  {"int  (*txn_stat) (DB_ENV *, DB_TXN_STAT **, u_int32_t)", 1064, 8},
  {0, 0, 1096} /* size of whole struct */
};
