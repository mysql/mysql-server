/* BDB offsets on a 64-bit machine */
struct fieldinfo fields64[] = {
  {"void *app_private", 32, 8},
  {"int (*close) (DB*, u_int32_t)", 448, 8},
  {"int (*cursor) (DB *, DB_TXN *, DBC **, u_int32_t)", 456, 8},
  {"int (*del) (DB *, DB_TXN *, DBT *, u_int32_t)", 464, 8},
  {"int (*get) (DB *, DB_TXN *, DBT *, DBT *, u_int32_t)", 504, 8},
  {"int (*key_range) (DB *, DB_TXN *, DBT *, DB_KEY_RANGE *, u_int32_t)", 632, 8},
  {"int (*open) (DB *, DB_TXN *, const char *, const char *, DBTYPE, u_int32_t, int)", 640, 8},
  {"int (*put) (DB *, DB_TXN *, DBT *, DBT *, u_int32_t)", 648, 8},
  {"int (*remove) (DB *, const char *, const char *, u_int32_t)", 656, 8},
  {"int (*rename) (DB *, const char *, const char *, const char *, u_int32_t)", 664, 8},
  {"int (*set_flags) (DB *, u_int32_t)", 752, 8},
  {"int (*stat) (DB *, void *, u_int32_t)", 808, 8},
  {"int (*set_bt_compare) (DB *, int (*)(DB *, const DBT *, const DBT *))", 856, 8}
};
