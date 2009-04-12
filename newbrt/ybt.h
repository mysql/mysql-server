#ifndef YBT_H
#define YBT_H
#ident "$Id$"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

// brttypes.h must be first to make 64-bit file mode work right in linux.
#include "brttypes.h"
#include <db.h>


DBT* toku_init_dbt (DBT *);
DBT *toku_fill_dbt(DBT *dbt, bytevec k, ITEMLEN len);
int toku_dbt_set (ITEMLEN len, bytevec val, DBT *d, struct simple_dbt *sdbt);
int toku_dbt_set_value (DBT *, bytevec *val, ITEMLEN vallen, void **staticptrp, BOOL ybt1_disposable);
void toku_sdbt_cleanup(struct simple_dbt *sdbt);

#endif
