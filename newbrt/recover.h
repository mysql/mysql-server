#ifndef TOKURECOVER_H
#define TOKURECOVER_H

#ident "$Id: recover.h 13182 2009-07-09 20:57:11Z dwells $"
#ident "Copyright (c) 2007, 2008, 2009 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."

#include <toku_portability.h>
#include <errno.h>

#include "../include/db.h"
#include "brttypes.h"
#include "memory.h"
#include "bread.h"
#include "x1764.h"

int toku_recover_init(void);
void toku_recover_cleanup(void);
int tokudb_recover(const char *datadir, const char *logdir);

#endif // TOKURECOVER_H
