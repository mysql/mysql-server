/* BDB offsets on a 32-bit machine */
struct fieldinfo db_fields32[] = {
  {"void *app_private", 16, 4},
  {"int (*close) (DB*, u_int32_t)", 272, 4},
  {"int (*cursor) (DB *, DB_TXN *, DBC **, u_int32_t)", 276, 4},
  {"int (*del) (DB *, DB_TXN *, DBT *, u_int32_t)", 280, 4},
  {"int (*get) (DB *, DB_TXN *, DBT *, DBT *, u_int32_t)", 296, 4},
  {"int (*key_range) (DB *, DB_TXN *, DBT *, DB_KEY_RANGE *, u_int32_t)", 316, 4},
  {"int (*open) (DB *, DB_TXN *, const char *, const char *, DBTYPE, u_int32_t, int)", 320, 4},
  {"int (*put) (DB *, DB_TXN *, DBT *, DBT *, u_int32_t)", 324, 4},
  {"int (*remove) (DB *, const char *, const char *, u_int32_t)", 328, 4},
  {"int (*rename) (DB *, const char *, const char *, const char *, u_int32_t)", 332, 4},
  {"int (*set_flags) (DB *, u_int32_t)", 380, 4},
  {"int (*stat) (DB *, void *, u_int32_t)", 396, 4},
  {"int (*set_bt_compare) (DB *, int (*)(DB *, const DBT *, const DBT *))", 412, 4},
  {0, 484, 484} /* size of whole struct */
};
struct fieldinfo dbt_fields32[] = {
  {"void*data", 0, 4},
  {"u_int32_t size", 4, 4},
  {"u_int32_t ulen", 8, 4},
  {"void*app_private", 20, 4},
  {"u_int32_t flags", 24, 4},
  {0, 28, 28} /* size of whole struct */
};
struct fieldinfo db_txn_fields32[] = {
  {"int (*commit) (DB_TXN*, u_int32_t)", 80, 4},
  {"u_int32_t (*id) (DB_TXN *)", 88, 4},
  {0, 104, 104} /* size of whole struct */
};
struct fieldinfo dbc_fields32[] = {
  {"int (*c_close) (DBC *)", 204, 4},
  {"int (*c_del) (DBC *, u_int32_t)", 212, 4},
  {"int (*c_get) (DBC *, DBT *, DBT *, u_int32_t)", 220, 4},
  {0, 268, 268} /* size of whole struct */
};
struct fieldinfo db_env_fields32[] = {
  {"int  (*close) (DB_ENV *, u_int32_t)", 248, 4},
  {"void (*err) (const DB_ENV *, int, const char *, ...)", 260, 4},
  {"int  (*open) (DB_ENV *, const char *, u_int32_t, int)", 268, 4},
  {"int  (*set_data_dir) (DB_ENV *, const char *)", 276, 4},
  {"void (*set_errcall) (DB_ENV *, void (*)(const char *, char *))", 292, 4},
  {"void (*set_errpfx) (DB_ENV *, const char *)", 300, 4},
  {"int  (*set_flags) (DB_ENV *, u_int32_t, int)", 308, 4},
  {"void (*set_noticecall) (DB_ENV *, void (*)(DB_ENV *, db_notices))", 312, 4},
  {"int  (*set_tmp_dir) (DB_ENV *, const char *)", 332, 4},
  {"int  (*set_verbose) (DB_ENV *, u_int32_t, int)", 336, 4},
  {"int  (*set_lg_bsize) (DB_ENV *, u_int32_t)", 344, 4},
  {"int  (*set_lg_dir) (DB_ENV *, const char *)", 348, 4},
  {"int  (*set_lg_max) (DB_ENV *, u_int32_t)", 352, 4},
  {"int  (*log_archive) (DB_ENV *, char **[], u_int32_t)", 360, 4},
  {"int  (*log_flush) (DB_ENV *, const DB_LSN *)", 372, 4},
  {"int  (*set_lk_detect) (DB_ENV *, u_int32_t)", 392, 4},
  {"int  (*set_lk_max) (DB_ENV *, u_int32_t)", 396, 4},
  {"int  (*set_cachesize) (DB_ENV *, u_int32_t, u_int32_t, int)", 460, 4},
  {"int  (*txn_checkpoint) (DB_ENV *, u_int32_t, u_int32_t, u_int32_t)", 552, 4},
  {"int  (*txn_stat) (DB_ENV *, DB_TXN_STAT **, u_int32_t)", 564, 4},
  {0, 584, 584} /* size of whole struct */
};
