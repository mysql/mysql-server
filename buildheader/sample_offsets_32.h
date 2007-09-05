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
  {0, 0, 484} /* size of whole struct */
};
struct fieldinfo dbt_fields32[] = {
  {"void*data", 0, 4},
  {"u_int32_t size", 4, 4},
  {"u_int32_t ulen", 8, 4},
  {"void*app_private", 20, 4},
  {"u_int32_t flags", 24, 4},
  {0, 0, 28} /* size of whole struct */
};
