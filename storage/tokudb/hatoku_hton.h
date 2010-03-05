#ifndef _HATOKU_HTON
#define _HATOKU_HTON

#include "db.h"


extern handlerton *tokudb_hton;

extern DB_ENV *db_env;
extern DB *metadata_db;


// thread variables
ulonglong get_write_lock_wait_time (THD* thd);
ulonglong get_read_lock_wait_time (THD* thd);
uint get_pk_insert_mode(THD* thd);

extern HASH tokudb_open_tables;
extern pthread_mutex_t tokudb_mutex;
extern pthread_mutex_t tokudb_meta_mutex;
extern my_bool tokudb_prelock_empty;
extern u_int32_t tokudb_write_status_frequency;
extern u_int32_t tokudb_read_status_frequency;
#endif //#ifdef _HATOKU_HTON
