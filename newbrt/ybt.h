#ifndef YBT_H
#define YBT_H

// brttypes.h must be first to make 64-bit file mode work right in linux.
#include "brttypes.h"
#include "../include/db.h"


DBT* init_dbt (DBT *);
DBT *fill_dbt(DBT *dbt, bytevec k, ITEMLEN len);
int ybt_set_value (DBT *, bytevec val, ITEMLEN vallen, void **staticptrp);

#endif
