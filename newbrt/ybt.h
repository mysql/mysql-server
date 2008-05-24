#ifndef YBT_H
#define YBT_H

#ident "Copyright (c) 2007 Tokutek Inc.  All rights reserved."

// brttypes.h must be first to make 64-bit file mode work right in linux.
#include "brttypes.h"
#include "../include/db.h"


DBT* toku_init_dbt (DBT *);
DBT *toku_fill_dbt(DBT *dbt, bytevec k, ITEMLEN len);
int toku_dbt_set_value (DBT *, bytevec *val, ITEMLEN vallen, void **staticptrp, BOOL ybt1_disposable);
int toku_dbt_set_two_values(DBT* key, bytevec *key_data, ITEMLEN key_len, void** key_staticptrp, BOOL key_disposable,
                            DBT* val, bytevec *val_data, ITEMLEN val_len, void** val_staticptrp, BOOL val_disposable);
int toku_dbt_set_three_values(
        DBT* ybt1, bytevec *ybt1_data, ITEMLEN ybt1_len, void** ybt1_staticptrp, BOOL ybt1_disposable,
        DBT* ybt2, bytevec *ybt2_data, ITEMLEN ybt2_len, void** ybt2_staticptrp, BOOL ybt2_disposable,
        DBT* ybt3, bytevec *ybt3_data, ITEMLEN ybt3_len, void** ybt3_staticptrp, BOOL ybt3_disposable);
 


#endif
