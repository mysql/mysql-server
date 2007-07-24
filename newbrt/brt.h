#ifndef BRT_H
#define BRT_H

// This must be first to make the 64-bit file mode work right in Linux
#define _FILE_OFFSET_BITS 64
#include "brttypes.h"

#include "ybt.h"
#include "../include/ydb-constants.h"
#include "cachetable.h"
typedef struct brt *BRT;
int open_brt (const char *fname, const char *dbname, int is_create, BRT *, int nodesize, CACHETABLE, int(*)(DB*,DBT*,DBT*));
//int brt_create (BRT **, int nodesize, int n_nodes_in_cache); /* the nodesize and n_nodes in cache really should be separately configured. */
//int brt_open (BRT *, char *fname, char *dbname);
int brt_insert (BRT brt, DBT *k, DBT *v, DB*db);
int brt_lookup (BRT brt, DBT *k, DBT *v, DB*db);
int close_brt (BRT);
int dump_brt (BRT brt);
void brt_fsync (BRT); /* fsync, but don't clear the caches. */

void brt_flush (BRT); /* fsync and clear the caches. */ 

int brt_create_cachetable (CACHETABLE *t, int n_cachlines /* Pass 0 if you want the default. */);

extern int brt_debug_mode;
int verify_brt (BRT brt);

int show_brt_blocknumbers(BRT);

typedef struct brt_cursor *BRT_CURSOR;
int brt_cursor (BRT, BRT_CURSOR*);
int brt_c_get (BRT_CURSOR cursor, DBT *kbt, DBT *vbt, int brtc_flags);
int brt_cursor_close (BRT_CURSOR curs);

#endif
