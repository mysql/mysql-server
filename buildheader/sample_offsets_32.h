/* BDB offsets on a 32-bit machine */
struct fieldinfo fields32[] = {
  {"void *app_private", 16, 4},
  {"int (*close) (DB*, u_int32_t)", 272, 4},
  {"int (*cursor) (DB *, DB_TXN *, DBC **, u_int32_t)", 276, 4},
  {"int (*del) (DB *, DB_TXN *, DBT *, u_int32_t)", 280, 4},
  {"int (*get) (DB *, DB_TXN *, DBT *, DBT *, u_int32_t)", 300, 4},
  {"int (*key_range) (DB *, DB_TXN *, DBT *, DB_KEY_RANGE *, u_int32_t)", 364, 4},
  {"int (*open) (DB *, DB_TXN *, const char *, const char *, DBTYPE, u_int32_t, int)", 368, 4},
  {"int (*put) (DB *, DB_TXN *, DBT *, DBT *, u_int32_t)", 372, 4},
  {"int (*remove) (DB *, const char *, const char *, u_int32_t)", 376, 4},
  {"int (*rename) (DB *, const char *, const char *, const char *, u_int32_t)", 380, 4},
  {"int (*set_flags) (DB *, u_int32_t)", 424, 4},
  {"int (*stat) (DB *, void *, u_int32_t)", 452, 4},
  {"int (*set_bt_compare) (DB *, int (*)(DB *, const DBT *, const DBT *))", 476, 4}
};
