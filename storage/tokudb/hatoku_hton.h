#ifndef _HATOKU_HTON
#define _HATOKU_HTON

#include "db.h"

extern handlerton *tokudb_hton;

extern DB_ENV *db_env;
extern DB *metadata_db;

enum srv_row_format_enum {
    SRV_ROW_FORMAT_UNCOMPRESSED = 0,
    SRV_ROW_FORMAT_ZLIB = 1,
    SRV_ROW_FORMAT_QUICKLZ = 2,
    SRV_ROW_FORMAT_LZMA = 3,
    SRV_ROW_FORMAT_FAST = 4,
    SRV_ROW_FORMAT_SMALL = 5,
    SRV_ROW_FORMAT_DEFAULT = 6
};
typedef enum srv_row_format_enum srv_row_format_t;

// thread variables
uint get_pk_insert_mode(THD* thd);
bool get_load_save_space(THD* thd);
bool get_disable_slow_alter(THD* thd);
bool get_disable_hot_alter(THD* thd);
bool get_create_index_online(THD* thd);
bool get_disable_prefetching(THD* thd);
bool get_prelock_empty(THD* thd);
bool get_log_client_errors(THD* thd);
uint get_tokudb_block_size(THD* thd);
uint get_tokudb_read_block_size(THD* thd);
uint get_tokudb_read_buf_size(THD* thd);
srv_row_format_t get_row_format(THD *thd);
#if TOKU_INCLUDE_UPSERT
bool get_enable_fast_update(THD *thd);
bool get_disable_slow_update(THD *thd);
bool get_enable_fast_upsert(THD *thd);
bool get_disable_slow_upsert(THD *thd);
#endif
uint get_analyze_time(THD *thd);

extern HASH tokudb_open_tables;
extern pthread_mutex_t tokudb_mutex;
extern pthread_mutex_t tokudb_meta_mutex;
extern uint32_t tokudb_write_status_frequency;
extern uint32_t tokudb_read_status_frequency;

void toku_hton_update_primary_key_bytes_inserted(uint64_t row_size);

#endif //#ifdef _HATOKU_HTON
