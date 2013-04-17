#ifndef _HATOKU_HTON
#define _HATOKU_HTON

#include "db.h"


extern handlerton *tokudb_hton;

extern DB_ENV *db_env;
extern DB *metadata_db;


// thread variables


extern HASH tokudb_open_tables;
extern pthread_mutex_t tokudb_mutex;
extern pthread_mutex_t tokudb_meta_mutex;





#endif //#ifdef _HATOKU_HTON
