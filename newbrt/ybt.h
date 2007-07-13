#ifndef YBT_H
#define YBT_H

// brttypes.h must be first to make 64-bit file mode work right in linux.
#include "brttypes.h"
#include "../include/db.h"


int ybt_init (DBT *);
int ybt_set_value (DBT *, bytevec val, ITEMLEN vallen, void **staticptrp);

#endif
