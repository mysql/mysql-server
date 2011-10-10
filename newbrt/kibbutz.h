#ifndef KIBBUTZ_H
#ident "$Id$"
#ident "Copyright (c) 2007-2010 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include "c_dialects.h"

C_BEGIN

typedef struct kibbutz *KIBBUTZ;
KIBBUTZ toku_kibbutz_create (int n_workers);
void toku_kibbutz_enq (KIBBUTZ k, void (*f)(void*), void *extra);
void toku_kibbutz_destroy (KIBBUTZ k);

C_END

#endif
