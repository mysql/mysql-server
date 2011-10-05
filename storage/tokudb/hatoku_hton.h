#ifndef _HATOKU_HTON
#define _HATOKU_HTON

#include "db.h"


extern handlerton *tokudb_hton;

extern DB_ENV *db_env;
extern DB *metadata_db;

// thread variables
uint get_pk_insert_mode(THD* thd);
bool get_load_save_space(THD* thd);
bool get_disable_slow_alter(THD* thd);
bool get_create_index_online(THD* thd);
bool get_prelock_empty(THD* thd);
uint get_tokudb_block_size(THD* thd);
uint get_tokudb_read_block_size(THD* thd);
uint get_tokudb_read_buf_size(THD* thd);

extern HASH tokudb_open_tables;
extern pthread_mutex_t tokudb_mutex;
extern pthread_mutex_t tokudb_meta_mutex;
extern u_int32_t tokudb_write_status_frequency;
extern u_int32_t tokudb_read_status_frequency;
#endif //#ifdef _HATOKU_HTON
