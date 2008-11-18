#ifndef _HATOKU_HTON
#define _HATOKU_HTON

#include "db.h"


extern handlerton *tokudb_hton;

extern const char *ha_tokudb_ext;
extern char *tokudb_data_dir;
extern DB_ENV *db_env;


// thread variables


extern HASH tokudb_open_tables;
extern pthread_mutex_t tokudb_mutex;





#endif //#ifdef _HATOKU_HTON
